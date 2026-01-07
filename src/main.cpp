/**
 * main.cpp - Screen Time Tracker Main Application
 * 
 * Entry point for the M5StickC Plus2 Screen Time Tracker.
 * 
 * Hardware: M5StickC Plus2
 * Libraries: M5Unified, M5GFX
 * 
 * Button Mapping (M5Stick C/CPlus/CPlus2 - landscape, rotated 180°):
 * - Button A (BtnA): Front button - activate menu item / toggle timer
 * - Button B (BtnB): Top side button - open menu / cycle menu items
 * - Button C / Power (BtnPWR): Bottom button - back (close menu)
 * 
 * Architecture: Uses ScreenManager for screen orchestration.
 * See docs/ARCHITECTURE_PLAN.md for details.
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#include <Arduino.h>
#include <M5Unified.h>
#include <LittleFS.h>
#include <esp_sleep.h>

// Application modules
#include "config.h"
#include "ui.h"
#include "timer.h"
#include "session_manager.h"
#include "network.h"
#include "sound.h"
#include "dialog.h"
#include "persistence.h"
#include "api_client.h"
#include "polling_manager.h"

// New architecture modules
#include "screen_manager.h"
#include "app_state.h"
#include "screens/main_screen.h"
#include "screens/login_screen.h"
#include "screens/select_child_screen.h"
#include "screens/sync_screen.h"
#include "screens/system_info_screen.h"
#include "screens/settings_screen.h"
#include "screens/brightness_screen.h"
#include "screens/parent_screen.h"

// ============================================================================
// RTC Memory - Persists across deep sleep
// ============================================================================

// Sleep state stored in RTC memory (survives deep sleep)
// Uses consumed-time model: track time used, not expiration
RTC_DATA_ATTR bool rtcWasTimerRunning = false;
RTC_DATA_ATTR time_t rtcSessionStartTime = 0;   // Unix timestamp when session started (only valid if running)
RTC_DATA_ATTR uint32_t rtcConsumedTodaySeconds = 0; // Time consumed in previous sessions today
RTC_DATA_ATTR bool rtcHasValidState = false;    // Flag to indicate valid sleep data
RTC_DATA_ATTR TimerState rtcTimerState = TimerState::STOPPED;

// Phase 7: Screen state and day tracking for wake from sleep
RTC_DATA_ATTR int8_t rtcScreenType = -1;        // ScreenType at time of sleep (-1 = NONE)
RTC_DATA_ATTR uint8_t rtcWeekday = 0xFF;        // Weekday at time of sleep (0-6, 0xFF = not set)
RTC_DATA_ATTR bool rtcWasLoggedIn = false;      // Whether user was logged in when going to sleep

// ============================================================================
// Global Objects
// ============================================================================

// Core application objects
UI* ui = nullptr;
ScreenTimer screenTimer;
SessionManager* sessionManager = nullptr;
NetworkManager networkManager;
SyncManager* syncManager = nullptr;

// API and Polling (Phase 5)
ApiClient apiClient;
PollingManager pollingManager;

// New architecture - Screen Manager and Screens
ScreenManager* screenManager = nullptr;
MainScreen* mainScreen = nullptr;
LoginScreen* loginScreen = nullptr;
SelectChildScreen* selectChildScreen = nullptr;
SyncScreen* syncScreen = nullptr;
SystemInfoScreen* systemInfoScreen = nullptr;
SettingsScreen* settingsScreen = nullptr;
BrightnessScreen* brightnessScreen = nullptr;
ParentScreen* parentScreen = nullptr;

// Timing control
uint32_t lastButtonPressMs = 0;  // For auto-sleep detection
uint32_t lastBatteryUpdateMs = 0;  // For periodic battery indicator update

// Battery update interval (5 minutes in milliseconds)
constexpr uint32_t BATTERY_UPDATE_INTERVAL_MS = 5 * 60 * 1000;

// Deep sleep restore state - used to sync timer state to MainScreen after creation
bool restoreTimerRunning = false;

// Forward declaration
bool tryGoToSleep(bool userInitiated = false);

// ============================================================================
// Startup Network Sync
// ============================================================================

/**
 * Perform initial time sync at startup
 * Connects to WiFi, syncs NTP time, sets RTC, then disconnects.
 * @return true if sync was successful
 */
bool performStartupTimeSync() {
    Serial.println("[App] Starting initial time sync...");
    
    // Show connecting status
    ui->updateNetworkStatus(NetworkStatus::CONNECTING);
    
    // Use the elegant withConnection pattern
    bool success = networkManager.withConnection([]() {
        // Update UI to show connected
        ui->updateNetworkStatus(NetworkStatus::CONNECTED);
        
        // Sync time and set RTC
        return networkManager.syncTimeAndSetRTC();
    });
    
    // Update UI to show disconnected
    ui->updateNetworkStatus(NetworkStatus::DISCONNECTED);
    
    if (success) {
        Serial.println("[App] Time sync completed successfully");
    } else {
        Serial.println("[App] Time sync failed");
    }
    
    return success;
}

// ============================================================================
// Timer Control Functions
// ============================================================================

// ============================================================================
// Deep Sleep Functions
// ============================================================================

/**
 * Check if wake was from deep sleep and restore state if so
 * Phase 7: Also checks for day changes and session validity
 * Uses consumed-time model for timer restoration.
 * @return true if woke from deep sleep with valid state
 */
bool checkAndRestoreFromSleep() {
    esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
    
    if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED) {
        // Normal boot, not from sleep
        Serial.println("[Sleep] Normal boot (not from deep sleep)");
        return false;
    }
    
    Serial.printf("[Sleep] Woke from deep sleep, cause: %d\n", wakeupCause);
    
    // Release GPIO hold from deep sleep
    gpio_hold_dis((gpio_num_t)POWER_HOLD_GPIO_NUM);
    gpio_deep_sleep_hold_dis();
    
    // Check if we have valid saved state
    if (!rtcHasValidState) {
        Serial.println("[Sleep] No valid sleep state found");
        return false;
    }
    
    Serial.println("[Sleep] Restoring state from deep sleep...");
    Serial.printf("[Sleep] Timer was running: %d, State: %d\n",
                  rtcWasTimerRunning, (int)rtcTimerState);
    Serial.printf("[Sleep] Consumed today: %lu, Session start: %ld\n",
                  (unsigned long)rtcConsumedTodaySeconds, (long)rtcSessionStartTime);
    Serial.printf("[Sleep] Screen type: %d, Weekday: %d, Was logged in: %d\n",
                  rtcScreenType, rtcWeekday, rtcWasLoggedIn);
    
    // ========================================================================
    // Phase 7: Check for session validity (R7.5)
    // ========================================================================
    AppState& appState = AppState::getInstance();
    
    // If was logged in but persistence says we're not, session may have expired
    if (rtcWasLoggedIn && !appState.isLoggedIn()) {
        Serial.println("[Sleep] Session was logged in but no longer valid - will redirect to login");
        rtcHasValidState = false;
        return false;
    }
    
    // ========================================================================
    // Phase 7: Detect day change (R7.2, R7.3)
    // ========================================================================
    uint8_t currentWeekday = appState.getCurrentWeekday();
    bool dayChanged = (rtcWeekday != 0xFF && rtcWeekday != currentWeekday);
    
    if (dayChanged) {
        Serial.printf("[Sleep] Day has changed! Was: %d, Now: %d\n", 
                      rtcWeekday, currentWeekday);
        // Reset consumed time for new day
        rtcConsumedTodaySeconds = 0;
        rtcSessionStartTime = 0;
        rtcWasTimerRunning = false;
        rtcTimerState = TimerState::STOPPED;
    }
    
    // ========================================================================
    // Restore timer state using SessionManager snapshot
    // ========================================================================
    SessionSnapshot snapshot;
    snapshot.consumedTodaySeconds = rtcConsumedTodaySeconds;
    snapshot.sessionStartTime = rtcSessionStartTime;
    snapshot.timerState = rtcTimerState;
    snapshot.weekday = rtcWeekday;
    snapshot.isValid = true;
    
    // Note: sessionManager may not be created yet at this point
    // We restore directly to screenTimer and set restoreTimerRunning flag
    // SessionManager will be created later and will use the restored timer state
    
    screenTimer.setConsumedTodaySeconds(rtcConsumedTodaySeconds);
    
    if (rtcTimerState == TimerState::RUNNING && rtcWasTimerRunning && !dayChanged) {
        Serial.printf("[Sleep] Session was running, start time: %ld\n",
                      (long)rtcSessionStartTime);
        
        if (screenTimer.startFromTimestamp(rtcSessionStartTime)) {
            restoreTimerRunning = true;
            Serial.printf("[Sleep] Resumed running timer, remaining: %lu seconds\n",
                          (unsigned long)screenTimer.calculateRemainingSeconds());
        } else {
            restoreTimerRunning = false;
            Serial.println("[Sleep] Timer expired during sleep");
        }
    } else if (rtcTimerState == TimerState::EXPIRED || screenTimer.isExpired()) {
        Serial.println("[Sleep] Restoring EXPIRED timer state");
        restoreTimerRunning = false;
    } else {
        Serial.printf("[Sleep] Restoring paused timer, remaining: %lu seconds\n",
                      (unsigned long)screenTimer.calculateRemainingSeconds());
        restoreTimerRunning = false;
    }
    
    rtcHasValidState = false;
    return true;
}

/**
 * Attempt to enter deep sleep mode
 * @param userInitiated true if user requested sleep (show dialog on refusal), false for auto-sleep
 * @return false if sleep was refused (conditions not met), true if entering sleep (won't return)
 */
bool tryGoToSleep(bool userInitiated) {
    Serial.println("[Sleep] Attempting to enter deep sleep...");
    
    // Use SessionManager if available, fall back to screenTimer directly
    uint32_t remainingSeconds = sessionManager ? sessionManager->getRemainingSeconds() 
                                               : screenTimer.calculateRemainingSeconds();
    bool timerIsRunning = sessionManager ? sessionManager->isSessionRunning() 
                                         : screenTimer.isRunning();
    
    // Check if we should refuse sleep:
    // If timer is ACTIVE (running) AND less than 10 minutes remaining, don't sleep
    if (timerIsRunning && remainingSeconds < SLEEP_MIN_REMAINING_SECS) {
        Serial.println("[Sleep] Refusing sleep: timer active with < 10 min remaining");
        
        // Only show dialog and beep if user initiated the sleep attempt
        if (userInitiated && ui != nullptr) {
            ui->showInfoDialog(
                "Cannot Sleep",
                "Screen time is almost up! The device will stay awake until time expires or you pause the timer.",
                "OK"
            );
        }
        return false;
    }
    
    Serial.println("[Sleep] Conditions met, preparing for deep sleep");
    
    // ========================================================================
    // Phase 7: Save screen state and session info to RTC memory
    // ========================================================================
    
    // Save current screen type for restoration on wake
    if (screenManager != nullptr) {
        rtcScreenType = static_cast<int8_t>(screenManager->getCurrentScreenType());
    } else {
        rtcScreenType = static_cast<int8_t>(ScreenType::MAIN);
    }
    
    // Save current weekday for day-change detection on wake
    AppState& appState = AppState::getInstance();
    rtcWeekday = appState.getCurrentWeekday();
    
    // Save login state for session validation on wake
    rtcWasLoggedIn = appState.isLoggedIn();
    
    Serial.printf("[Sleep] Saved screen state - Screen: %d, Weekday: %d, LoggedIn: %d\n",
                  rtcScreenType, rtcWeekday, rtcWasLoggedIn);
    
    // ========================================================================
    // Save timer state using SessionManager snapshot
    // ========================================================================
    
    if (sessionManager != nullptr) {
        // Use SessionManager to create snapshot (preferred)
        SessionSnapshot snapshot = sessionManager->createSnapshot();
        rtcWasTimerRunning = (snapshot.timerState == TimerState::RUNNING);
        rtcTimerState = snapshot.timerState;
        rtcConsumedTodaySeconds = snapshot.consumedTodaySeconds;
        rtcSessionStartTime = snapshot.sessionStartTime;
        rtcHasValidState = true;
        
        // Persist to NVS via SessionManager
        sessionManager->persistToNvs();
    } else {
        // Fallback to direct timer access (during early init)
        rtcWasTimerRunning = timerIsRunning;
        rtcTimerState = screenTimer.getState();
        rtcConsumedTodaySeconds = screenTimer.getCompletedSessionsSeconds();
        rtcSessionStartTime = timerIsRunning ? screenTimer.getSessionStartTime() : 0;
        rtcHasValidState = true;
        
        PersistenceManager::getInstance().saveConsumedToday(rtcConsumedTodaySeconds, rtcWeekday);
    }
    
    Serial.printf("[Sleep] Saved timer - Running: %d, Start: %ld, Consumed: %lu\n",
                  rtcWasTimerRunning, (long)rtcSessionStartTime, 
                  (unsigned long)rtcConsumedTodaySeconds);
    
    // DO NOT stop the timer - we want session to continue during sleep
    // The session start time is preserved in RTC memory
    
    // Show brief sleep notification
    if (ui != nullptr) {
        ui->showNotification("Going to sleep...", 1000);
    }
    M5.delay(1000);
    
    // Calculate wake-up time if timer was running
    uint64_t sleepDurationUs = 0;
    bool useTimerWake = false;
    
    if (rtcWasTimerRunning && remainingSeconds > SLEEP_WAKE_BEFORE_EXPIRY_SECS) {
        // Wake up 10 minutes before expiration
        uint32_t sleepDurationSecs = remainingSeconds - SLEEP_WAKE_BEFORE_EXPIRY_SECS;
        
        // Guard: minimum sleep of 10 seconds, maximum of 24 hours
        if (sleepDurationSecs < 10) {
            sleepDurationSecs = 10;
        }
        if (sleepDurationSecs > 24 * 60 * 60) {
            sleepDurationSecs = 24 * 60 * 60;
        }
        
        sleepDurationUs = (uint64_t)sleepDurationSecs * 1000000ULL;
        useTimerWake = true;
        
        Serial.printf("[Sleep] Setting timer wake in %lu seconds\n", 
                      (unsigned long)sleepDurationSecs);
    }
    
    // Configure wake sources
    
    // 1. Button A wake (EXT0)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_A_GPIO_NUM, LOW);
    Serial.println("[Sleep] Button A wake enabled");
    
    // 2. Timer wake (if applicable)
    if (useTimerWake) {
        esp_sleep_enable_timer_wakeup(sleepDurationUs);
        Serial.println("[Sleep] Timer wake enabled");
    }
    
    // Keep power mosfet on while in deep sleep
    gpio_hold_en((gpio_num_t)POWER_HOLD_GPIO_NUM);
    gpio_deep_sleep_hold_en();
    
    Serial.println("[Sleep] Entering deep sleep NOW");
    Serial.flush();
    
    // Turn off display
    M5.Display.setBrightness(0);
    M5.Display.sleep();
    
    // Enter deep sleep - this function does not return
    esp_deep_sleep_start();
    
    // Should never reach here
    return true;
}

/**
 * Check for auto-sleep due to inactivity
 * Only runs the actual check every 5 seconds to avoid rapid repeated attempts
 */
void checkAutoSleep() {
    static uint32_t lastAutoSleepCheckMs = 0;
    const uint32_t AUTO_SLEEP_CHECK_INTERVAL_MS = 5000;  // Check every 5 seconds
    
    uint32_t now = millis();
    
    // Throttle: only check every 5 seconds
    if (now - lastAutoSleepCheckMs < AUTO_SLEEP_CHECK_INTERVAL_MS) {
        return;
    }
    lastAutoSleepCheckMs = now;
    
    // Don't auto-sleep if dialog is visible
    if (ui != nullptr && ui->isInfoDialogVisible()) {
        return;
    }
    
    // Don't auto-sleep if any overlay is active (menu or dialog)
    if (screenManager != nullptr && screenManager->hasActiveOverlay()) {
        return;
    }
    
    uint32_t inactiveMs = now - lastButtonPressMs;
    
    if (inactiveMs >= (AUTO_SLEEP_DURATION_SECS * 1000UL)) {
        Serial.printf("[Sleep] Auto-sleep triggered after %lu ms inactivity\n", 
                      (unsigned long)inactiveMs);
        tryGoToSleep();
    }
}

// ============================================================================
// Main Setup
// ============================================================================

void setup() {
    // Initialize serial for debugging
    Serial.begin(115200);
    delay(100);
    
    Serial.println("=========================================");
    Serial.println("  Screen Time Tracker - Starting...");
    Serial.println("=========================================");
    
    // Initialize M5Unified (auto-detects M5StickC Plus2)
    auto cfg = M5.config();
    cfg.internal_spk = true;  // Enable speaker for feedback
    cfg.external_rtc = true;  // Enable external RTC
    M5.begin(cfg);
    
    // Set landscape mode if needed (following example pattern)
    if (M5.Display.width() < M5.Display.height()) {
        M5.Display.setRotation(M5.Display.getRotation() ^ 1);
    }
    
    Serial.println("[App] M5Unified initialized");
    Serial.printf("[App] Board type: %d\n", M5.getBoard());
    
    // Check if this is a fresh boot (not wake from sleep)
    esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
    bool isFirstBoot = (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED);
    
    // Initialize LittleFS for avatar images
    if (!LittleFS.begin(true)) {  // true = format on fail
        Serial.println("[App] ERROR: LittleFS initialization failed");
        Serial.println("[App] Avatar images will not be available");
    } else {
        Serial.println("[App] LittleFS initialized");
        // Debug: List avatar files
        File root = LittleFS.open("/avatars");
        if (root && root.isDirectory()) {
            int avatarCount = 0;
            File file = root.openNextFile();
            while (file) {
                if (!file.isDirectory()) {
                    avatarCount++;
                }
                file = root.openNextFile();
            }
            Serial.printf("[App] Found %d avatar files in /avatars\n", avatarCount);
        } else {
            Serial.println("[App] WARNING: /avatars directory not found");
        }
    }
    
    // ========================================================================
    // Display splash screen on first boot
    // ========================================================================
    if (isFirstBoot) {
        Serial.println("[App] First boot detected - displaying splash screen");
        
        // Clear the display
        M5.Display.clear();
        
        // Open and draw the splash logo from LittleFS
        File splashFile = LittleFS.open("/logos/splash.png", "r");
        if (splashFile) {
            M5.Display.drawPng(&splashFile, 68, 22);
            splashFile.close();
        } else {
            Serial.println("[App] WARNING: Could not open splash.png");
        }
        
        // End the screen display batch
        M5.Display.display();
        
        // Wait 2 seconds
        delay(2000);
        
        Serial.println("[App] Splash screen complete");
    }
    
    // Create UI instance using M5.Display (which is M5GFX)
    ui = new UI(M5.Display);
    if (ui == nullptr) {
        Serial.println("[App] ERROR: Failed to create UI");
        while (1) M5.delay(1000);  // Halt on critical error
    }
    ui->begin();
    Serial.println("[App] UI initialized");
    
    // Initialize timer with 0 - allowance will be set from persistence or API
    screenTimer.begin(0);
    Serial.println("[App] Timer initialized (awaiting allowance from API)");
    
    // Initialize sound system
    soundBegin();
    
    // ========================================================================
    // Initialize Persistence (Phase 3)
    // ========================================================================
    
    // Initialize persistence manager
    PersistenceManager& persistence = PersistenceManager::getInstance();
    if (!persistence.begin()) {
        Serial.println("[App] WARNING: Persistence initialization failed");
        // Continue anyway - app will work without persistence
    } else {
        // Load stored state into AppState
        AppState& appState = AppState::getInstance();
        bool hadSession = appState.loadFromPersistence();
        
        // Apply stored brightness level (or default if not set)
        BrightnessScreen::applyStoredBrightness();
        
        // Debug: Print stored data
        persistence.debugPrint();
        
        if (hadSession) {
            Serial.println("[App] Session restored from persistence");
            
            // Load cached allowance if available
            // Use setAllowance() instead of reset() to preserve any consumed time
            // that may be restored later from RTC memory or NVS
            uint32_t cachedAllowance = appState.getScreenTime().dailyAllowanceSeconds;
            if (cachedAllowance > 0) {
                screenTimer.setAllowance(cachedAllowance);
                Serial.printf("[App] Timer allowance set to cached value: %lu seconds\n",
                              (unsigned long)cachedAllowance);
            }
        }
    }
    
    // Check if waking from deep sleep and restore state
    bool wokeFromSleep = checkAndRestoreFromSleep();
    
    // Reset warning thresholds based on current timer state
    // This prevents re-playing warnings for thresholds already passed
    resetWarningThresholds(screenTimer.calculateRemainingSeconds());
    
    // Initialize network
    networkManager.begin();
    syncManager = new SyncManager(networkManager);
    if (syncManager != nullptr) {
        syncManager->begin("https://api.screentime.example.com");
    }
    Serial.println("[App] Network initialized");
    
    // ========================================================================
    // Initialize API Client and Polling Manager (Phase 5)
    // ========================================================================
    
    apiClient.begin(networkManager);
    
    // Restore API client state from persisted session
    {
        AppState& appState = AppState::getInstance();
        const UserSession& session = appState.getSession();
        if (session.apiKey[0] != '\0') {
            apiClient.setApiKey(session.apiKey);
            Serial.printf("[App] API key restored: %s...\n", 
                String(session.apiKey).substring(0, 8).c_str());
        }
        if (session.familyId[0] != '\0') {
            apiClient.setFamilyId(session.familyId);
            Serial.printf("[App] Family ID restored: %s\n", session.familyId);
        }
    }
    
    pollingManager.begin(apiClient, networkManager);
    
    // Configure polling intervals from config
    pollingManager.setLoginPollInterval(LOGIN_POLL_INTERVAL_MS);
    pollingManager.setLoginTimeout(LOGIN_POLL_TIMEOUT_MS);
    pollingManager.setMoreTimePollInterval(MORE_TIME_POLL_INTERVAL_MS);
    pollingManager.setMoreTimeTimeout(MORE_TIME_POLL_TIMEOUT_MS);
    
    Serial.println("[App] ApiClient and PollingManager initialized");
    
    // Initialize auto-sleep timer
    lastButtonPressMs = millis();
    
    // ========================================================================
    // Initialize New Architecture (ScreenManager + Screens)
    // ========================================================================
    
    // Create SessionManager (wraps ScreenTimer)
    sessionManager = new SessionManager(screenTimer);
    if (sessionManager == nullptr) {
        Serial.println("[App] ERROR: Failed to create SessionManager");
        while (1) M5.delay(1000);
    }
    sessionManager->setApiClient(&apiClient);
    Serial.println("[App] SessionManager initialized");
    
    // Create screen manager
    screenManager = new ScreenManager(M5.Display);
    if (screenManager == nullptr) {
        Serial.println("[App] ERROR: Failed to create ScreenManager");
        while (1) M5.delay(1000);
    }
    screenManager->begin();
    
    // Create and register MainScreen
    mainScreen = new MainScreen(M5.Display, *sessionManager, *ui);
    if (mainScreen == nullptr) {
        Serial.println("[App] ERROR: Failed to create MainScreen");
        while (1) M5.delay(1000);
    }
    mainScreen->setScreenManager(screenManager);
    mainScreen->setApiClient(&apiClient, &pollingManager);
    mainScreen->setNetworkManager(&networkManager);
    screenManager->registerScreen(ScreenType::MAIN, mainScreen);
    
    // Create and register LoginScreen
    loginScreen = new LoginScreen(M5.Display);
    if (loginScreen == nullptr) {
        Serial.println("[App] ERROR: Failed to create LoginScreen");
        while (1) M5.delay(1000);
    }
    loginScreen->setScreenManager(screenManager);
    loginScreen->setApiClient(&apiClient, &pollingManager);
    screenManager->registerScreen(ScreenType::LOGIN, loginScreen);
    
    // Create and register SelectChildScreen
    selectChildScreen = new SelectChildScreen(M5.Display);
    if (selectChildScreen == nullptr) {
        Serial.println("[App] ERROR: Failed to create SelectChildScreen");
        while (1) M5.delay(1000);
    }
    selectChildScreen->setScreenManager(screenManager);
    selectChildScreen->setApiClient(&apiClient);
    screenManager->registerScreen(ScreenType::SELECT_CHILD, selectChildScreen);
    
    // Create and register SyncScreen
    syncScreen = new SyncScreen(M5.Display);
    if (syncScreen == nullptr) {
        Serial.println("[App] ERROR: Failed to create SyncScreen");
        while (1) M5.delay(1000);
    }
    syncScreen->setScreenManager(screenManager);
    screenManager->registerScreen(ScreenType::SYNC_PROGRESS, syncScreen);
    
    // Create and register SystemInfoScreen
    systemInfoScreen = new SystemInfoScreen(M5.Display);
    if (systemInfoScreen == nullptr) {
        Serial.println("[App] ERROR: Failed to create SystemInfoScreen");
        while (1) M5.delay(1000);
    }
    systemInfoScreen->setScreenManager(screenManager);
    screenManager->registerScreen(ScreenType::SYSTEM_INFO, systemInfoScreen);
    
    // Create and register SettingsScreen
    settingsScreen = new SettingsScreen(M5.Display, *ui);
    if (settingsScreen == nullptr) {
        Serial.println("[App] ERROR: Failed to create SettingsScreen");
        while (1) M5.delay(1000);
    }
    settingsScreen->setScreenManager(screenManager);
    screenManager->registerScreen(ScreenType::SETTINGS, settingsScreen);
    
    // Create and register BrightnessScreen
    brightnessScreen = new BrightnessScreen(M5.Display);
    if (brightnessScreen == nullptr) {
        Serial.println("[App] ERROR: Failed to create BrightnessScreen");
        while (1) M5.delay(1000);
    }
    brightnessScreen->setScreenManager(screenManager);
    screenManager->registerScreen(ScreenType::BRIGHTNESS, brightnessScreen);
    
    // Create and register ParentScreen
    parentScreen = new ParentScreen(M5.Display, *ui, screenTimer);
    if (parentScreen == nullptr) {
        Serial.println("[App] ERROR: Failed to create ParentScreen");
        while (1) M5.delay(1000);
    }
    parentScreen->setScreenManager(screenManager);
    parentScreen->setNetworkManager(&networkManager);
    screenManager->registerScreen(ScreenType::PARENT, parentScreen);
    
    // Sync restored timer running state to MainScreen (from deep sleep)
    if (restoreTimerRunning && mainScreen != nullptr) {
        mainScreen->startTimer();
    } 
    Serial.println("[App] ScreenManager and all screens initialized");
    
    // ========================================================================
    // Initial Screen Selection (Phase 4 + Phase 7)
    // ========================================================================
    
    // Determine which screen to show based on AppState
    AppState& appState = AppState::getInstance();
    ScreenType initialScreen = appState.determineInitialScreen();
    
    // Navigate to the appropriate screen
    if (wokeFromSleep && initialScreen == ScreenType::MAIN) {
        // Woke from sleep with valid session - go directly to main screen
        Serial.println("[App] Woke from sleep - navigating to main screen");
        screenManager->navigateTo(ScreenType::MAIN);
        
        // Check if timer expired during sleep and show dialog
        if (screenTimer.isExpired()) {
            Serial.println("[App] Timer is expired after wake - showing dialog");
            playErrorBeep();
            // MainScreen will handle showing the dialog via onEnter or we show it here
            if (mainScreen != nullptr) {
                mainScreen->showTimeUpDialog();
            }
        }
    } else if (initialScreen == ScreenType::LOGIN) {
        // Not logged in - go to login screen
        Serial.println("[App] Not logged in - navigating to login screen");
        screenManager->navigateTo(ScreenType::LOGIN);
    } else if (initialScreen == ScreenType::SELECT_CHILD) {
        // Logged in but no child selected
        Serial.println("[App] No child selected - navigating to select child screen");
        screenManager->navigateTo(ScreenType::SELECT_CHILD);
    } else {
        // Fresh boot, logged in with child - go to main screen (will sync on entry)
        Serial.println("[App] Fresh boot with session - navigating to main screen");
        screenManager->navigateTo(ScreenType::MAIN);
        
        // Perform startup time sync
        bool timeSyncSuccess = performStartupTimeSync();
        
        // Redraw after sync
        if (mainScreen != nullptr) {
            mainScreen->draw();
        }
        
        // Show error dialog if sync failed
        if (!timeSyncSuccess) {
            screenManager->showInfoDialog("Something went wrong", 
                              "Could not connect to WiFi or sync the time. "
                              "The clock may not be accurate.",
                              "OK");
        }
    }
    
    // Play startup tone (quieter beep if woke from sleep)
    if (wokeFromSleep) {
        M5.Speaker.tone(1100, 50);  // Brief chirp on wake
    } else {
        M5.Speaker.tone(880, 100);
        M5.delay(100);
        M5.Speaker.tone(1100, 100);
    }
    
    Serial.println("[App] Setup complete - entering main loop");
    Serial.println("-----------------------------------------");
    Serial.println("Controls:");
    Serial.println("  Button A (front) click: Toggle timer / dismiss dialog");
    Serial.println("  Button B (side) click:  Open menu / cycle selection");
    Serial.println("  Power button click:     Close menu (back)");
    Serial.println("  Power button hold:      Power off (battery only)");
    Serial.printf("  Auto-sleep after %lu seconds of inactivity\n", 
                  (unsigned long)AUTO_SLEEP_DURATION_SECS);
    Serial.println("-----------------------------------------");
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
    // IMPORTANT: Must call update() every loop for button detection
    M5.update();
    
    // ========================================================================
    // Input Handling - All buttons routed through ScreenManager
    // ========================================================================
    
    if (M5.BtnA.wasClicked()) {
        lastButtonPressMs = millis();
        screenManager->handleButtonA();
    }
    if (M5.BtnB.wasClicked()) {
        lastButtonPressMs = millis();
        screenManager->handleButtonB();
    }
    if (M5.BtnPWR.wasClicked()) {
        lastButtonPressMs = millis();
        screenManager->handleButtonPower();
    }
    if (M5.BtnPWR.wasHold()) {
        lastButtonPressMs = millis();
        screenManager->handleButtonPowerHold();
    }
    
    // ========================================================================
    // Screen Updates - ScreenManager handles all screen updates
    // ========================================================================
    screenManager->update();
    
    // ========================================================================
    // Screen Drawing - ScreenManager handles dialog vs screen drawing
    // ========================================================================
    screenManager->draw();
    
    // ========================================================================
    // Network Manager Update (Phase 6)
    // ========================================================================
    networkManager.update();
    
    // ========================================================================
    // Polling Manager Update (Phase 5)
    // ========================================================================
    pollingManager.update();
    
    // ========================================================================
    // Periodic Battery Indicator Update (every 5 minutes)
    // ========================================================================
    uint32_t now = millis();
    if (now - lastBatteryUpdateMs >= BATTERY_UPDATE_INTERVAL_MS) {
        lastBatteryUpdateMs = now;
        if (ui != nullptr) {
            ui->updateBatteryIndicator();
        }
    }
    
    // ========================================================================
    // Auto-sleep check
    // ========================================================================
    checkAutoSleep();
    
    // Small delay to prevent tight loop
    M5.delay(10);
}