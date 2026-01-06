/**
 * main_screen.cpp - Main Screen implementation
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#include <M5Unified.h>
#include "screens/main_screen.h"
#include "screen_manager.h"
#include "session_manager.h"
#include "api_client.h"
#include "polling_manager.h"
#include "network.h"
#include "app_state.h"
#include "persistence.h"
#include "sound.h"
#include "config.h"
#include "dialog.h"
#include <Arduino.h>

// Forward declaration from main.cpp for sleep functionality
extern bool tryGoToSleep(bool userInitiated);

// ============================================================================
// Constructor
// ============================================================================

MainScreen::MainScreen(M5GFX& display, SessionManager& sessionManager, UI& ui)
    : _display(display)
    , _sessionManager(sessionManager)
    , _timer(sessionManager.getTimer())
    , _ui(ui)
    , _screenManager(nullptr)
    , _apiClient(nullptr)
    , _pollingManager(nullptr)
    , _networkManager(nullptr)
    , _isPollingForMoreTime(false)
    , _lastDisplayUpdateMs(0)
{
}

void MainScreen::setScreenManager(ScreenManager* manager) {
    _screenManager = manager;
}

void MainScreen::setApiClient(ApiClient* api, PollingManager* polling) {
    _apiClient = api;
    _pollingManager = polling;
}

void MainScreen::setNetworkManager(NetworkManager* network) {
    _networkManager = network;
}

// ============================================================================
// Screen Lifecycle
// ============================================================================

void MainScreen::onEnter() {
    Serial.println("[MainScreen] onEnter");
    
    // Set up the menu for this screen
    setupMenu();
    
    // Check for day change and sync if needed (R7)
    AppState& state = AppState::getInstance();
    if (state.hasWeekdayChanged()) {
        Serial.println("[MainScreen] Day has changed - syncing new allowance");
        
        // Clear persisted consumed time for new day via SessionManager
        _sessionManager.clearNvsConsumedTime();
        
        drawFullScreen();
        
        // Fetch new daily allowance from API
        bool success = fetchAllowanceFromApi();
        
        // Update weekday tracking
        state.updateLastActiveWeekday();
        state.saveWeekdayToPersistence();
        
        if (!success) {
            // Show retry dialog for new day sync failure
            showAllowanceFetchFailedDialog();
        } else {
            // Show brief notification that it's a new day
            _ui.showNotification("New day!", 1000);
        }
    } else {
        // Not a new day - load consumed time from persistence (crash recovery)
        uint8_t currentWeekday = state.getCurrentWeekday();
        uint32_t consumedSeconds = _sessionManager.loadFromNvs(currentWeekday);
        if (consumedSeconds > 0 && !_sessionManager.isSessionRunning()) {
            // Only restore if timer is not already running (avoid overwriting live state)
            _timer.setConsumedTodaySeconds(consumedSeconds);
            Serial.printf("[MainScreen] Restored consumed time from NVS: %lu sec\n",
                          (unsigned long)consumedSeconds);
        }
        
        // Only fetch allowance from API if we don't have a cached value for today.
        // The daily allowance is persisted to NVS and restored via loadFromPersistence(),
        // so after wake from sleep we can use the cached value instead of re-fetching.
        // User can manually refresh using the 'Refresh' menu item if needed.
        uint32_t cachedAllowance = state.getScreenTime().dailyAllowanceSeconds;
        bool hasUnlimited = state.getScreenTime().hasUnlimitedAllowance;
        
        // If unlimited or no cached allowance, fetch from API
        if (hasUnlimited || cachedAllowance == 0) {
            // Need to fetch from API - either unlimited or no valid cached allowance
            Serial.println(hasUnlimited ? 
                "[MainScreen] Has unlimited flag - refreshing from API" :
                "[MainScreen] No cached allowance - fetching from API");
            drawFullScreen();
            
            bool success = fetchAllowanceFromApi();
            
            if (!success) {
                // Show retry dialog for first boot / no cached allowance
                showAllowanceFetchFailedDialog();
            }
        } else {
            // Have valid cached allowance - use it
            Serial.printf("[MainScreen] Using cached allowance: %lu seconds\n",
                          (unsigned long)cachedAllowance);
            // Use setAllowance() to preserve consumed time already loaded
            // (reset() would clear consumed time, breaking the progress bar)
            if (!_timer.isRunning()) {
                _timer.setAllowance(cachedAllowance);
            }
        }
    }
    
    // Ensure menu is hidden initially
    _menu.hide();
    
    // Draw the full screen
    drawFullScreen();
}

void MainScreen::onExit() {
    Serial.println("[MainScreen] onExit");
    
    // Ensure menu is hidden when leaving
    _menu.hide();
    
    // Clean up menu (for next entry)
    destroyMenu();
}

void MainScreen::onResume() {
    Serial.println("[MainScreen] onResume");
    
    // Re-setup the menu (it was destroyed in onExit)
    setupMenu();
    
    // Ensure menu is hidden initially
    _menu.hide();
    
    // Returning from a dialog or overlay - redraw everything
    drawFullScreen();
}

void MainScreen::update() {
    // Update timer if running
    if (_sessionManager.isSessionRunning()) {
        // Capture session info BEFORE update (in case timer expires during update)
        uint32_t preUpdateSessionDuration = _sessionManager.getCurrentSessionSeconds();
        time_t preUpdateSessionStartTime = _sessionManager.getSessionStartTime();
        
        _timer.update();
        
        // Check for warning thresholds and play beeps
        checkAndPlayWarningBeeps(_sessionManager.getRemainingSeconds(), _sessionManager.isSessionRunning());
        
        // Check if timer just expired
        if (_sessionManager.isExpired()) {
            onTimerExpired(preUpdateSessionDuration, preUpdateSessionStartTime);
        }
    }
    
    // Update display periodically
    uint32_t now = millis();
    if (!_menu.isVisible() && (now - _lastDisplayUpdateMs >= TIMER_UPDATE_INTERVAL_MS)) {
        // Check if a full redraw is needed (e.g., after notification cleared)
        if (_ui.needsFullRedraw()) {
            drawFullScreen();
        } else {
            updateDynamicElements();
        }
        _lastDisplayUpdateMs = now;
    }
    
    // Update polling state based on PollingManager
    if (_pollingManager) {
        bool wasPolling = _isPollingForMoreTime;
        _isPollingForMoreTime = (_pollingManager->getPollingType() == PollingType::MORE_TIME);
        
        // Redraw if state changed
        if (wasPolling != _isPollingForMoreTime) {
            if (!_menu.isVisible()) {
                drawFullScreen();
            }
        }
    }
}

void MainScreen::draw() {
    drawFullScreen();
}

// ============================================================================
// Input Handling
// ============================================================================

void MainScreen::onButtonA() {
    // Note: Dialog input is now handled by ScreenManager before reaching here
    // So if we get here, there's no dialog visible
    
    if (_menu.isVisible()) {
        // Menu visible - activate selected item
        Serial.println("[MainScreen] Button A - activating menu item");
        
        // Flash the selected item
        _ui.flashMenuItem(_menu, _menu.getSelectedIndex());
        M5.delay(MENU_FLASH_DURATION_MS);
        
        // Activate the item - note this may trigger a screen transition
        // via navigateTo(), so we must check if we're still the active screen
        _menu.activateSelected();
        
        // Check if we're still the active screen - menu activation may have
        // navigated away, in which case we should NOT redraw this screen
        if (_screenManager && 
            _screenManager->getCurrentScreenType() != ScreenType::MAIN) {
            // Screen changed - don't redraw, just return
            return;
        }
        
        // Still on this screen - hide menu and redraw
        _menu.hide();
        
        // Redraw without menu
        drawFullScreen();
    } else {
        // Menu hidden - toggle timer
        Serial.println("[MainScreen] Button A - toggling timer");
        playButtonBeep();
        toggleTimer();
        updateDynamicElements();
    }
}

void MainScreen::onButtonB() {
    if (_menu.isVisible()) {
        // Menu visible - cycle to next item
        _menu.selectNext();
        _ui.drawMenu(_menu);
        Serial.printf("[MainScreen] Button B - selected item %d\n", 
                      _menu.getSelectedIndex());
    } else {
        // Menu hidden - open it
        Serial.println("[MainScreen] Button B - opening menu");
        _menu.show();
        _ui.drawMenu(_menu);
    }
}

void MainScreen::onButtonPower() {
    if (_menu.isVisible()) {
        // Menu visible - close it (back action)
        Serial.println("[MainScreen] Power - closing menu");
        _menu.hide();
        
        // Redraw without menu
        drawFullScreen();
    }
    // Menu hidden - no action on main screen
}

void MainScreen::onButtonPowerHold() {
    Serial.println("[MainScreen] Power hold - powering off");
    
    // Show brief notification
    _ui.showNotification("Powering off...", 500);
    M5.delay(500);
    
    // Power off (only works on battery power)
    M5.Power.powerOff();
}

// ============================================================================
// Timer Control
// ============================================================================

bool MainScreen::startTimer() {
    // Check if expired first (show dialog)
    if (_sessionManager.isExpired()) {
        Serial.println("[MainScreen] Cannot start - time expired");
        playErrorBeep();
        showTimeUpDialog();
        return false;
    }
    
    // Delegate to SessionManager
    if (_sessionManager.startSession()) {
        Serial.println("[MainScreen] Timer started");
        return true;
    }
    
    // SessionManager refused (expired or unlimited)
    playErrorBeep();
    return false;
}

void MainScreen::stopTimer(uint32_t minimumDuration) {
    // Show notification immediately (non-blocking with duration 0)
    _ui.showNotification("Storing...", 0);
    
    // Delegate to SessionManager - it handles persistence and API push
    _sessionManager.stopSession(minimumDuration);
    
    // Clear notification and redraw to show stopped state
    drawFullScreen();
}

void MainScreen::toggleTimer() {
    if (_sessionManager.isSessionRunning()) {
        uint32_t currentSessionSecs = _sessionManager.getCurrentSessionSeconds();
        
        // Check for mistaken session first (very short, accidental start)
        if (currentSessionSecs < MISTAKEN_SESSION_DURATION_SECS) {
            // Abort the session without counting any time
            _sessionManager.abortSession();
            
            Serial.printf("[MainScreen] Mistaken session aborted (%lu sec < %lu sec threshold)\n",
                          (unsigned long)currentSessionSecs,
                          (unsigned long)MISTAKEN_SESSION_DURATION_SECS);
            
            // Show brief feedback
            _ui.showNotification("Cancelled", 100);
            return;
        }
        
        // Check if minimum session warning should be shown
        if (MINIMUM_SESSION_ENABLED) {
            if (currentSessionSecs < MINIMUM_SESSION_DURATION_SECONDS) {
                // Show warning dialog
                uint32_t minMinutes = MINIMUM_SESSION_DURATION_SECONDS / 60;
                char message[128];
                snprintf(message, sizeof(message),
                    "If you stop now you will still lose %lu minutes screen time (the minimum) - OK to carry on?",
                    (unsigned long)minMinutes);
                
                if (_screenManager) {
                    _screenManager->showConfirmDialog(
                        "Warning",
                        message,
                        "Don't pause",  // Button 1 - cancel (keep running)
                        "OK",           // Button 2 - proceed with stop (default)
                        MainScreen::onMinimumSessionDialogResult,
                        this
                    );
                }
                return;  // Don't stop yet - wait for dialog result
            }
        }
        stopTimer();
    } else {
        startTimer();
    }
}

bool MainScreen::isTimerRunning() const {
    return _sessionManager.isSessionRunning();
}

ScreenTimer& MainScreen::getTimer() {
    return _timer;
}

// ============================================================================
// Menu State
// ============================================================================

bool MainScreen::isMenuVisible() const {
    return _menu.isVisible();
}

// ============================================================================
// Drawing
// ============================================================================

void MainScreen::drawFullScreen() {
    // Use the existing UI class for drawing the main screen
    // This maintains current visual appearance while allowing gradual refactoring
    AppState& state = AppState::getInstance();
    const UserSession& session = state.getSession();
    
    _ui.drawMainScreen(
        _timer,
        state.getDisplayName(),
        state.getAvatarInitial(),
        session.selectedChildAvatarName,
        _timer.isRunning(),
        state.getNetworkStatus()
    );
    
    // Show WiFi warning if credentials not configured
    if (_networkManager && _networkManager->isWifiNotConfigured()) {
        drawWifiWarning();
    }
}

void MainScreen::drawWifiWarning() {
    // Draw a warning banner at the bottom of the screen
    _display.startWrite();
    
    int bannerY = SCREEN_HEIGHT - 20;
    int bannerHeight = 20;
    
    // Draw semi-transparent warning background
    _display.fillRect(0, bannerY, SCREEN_WIDTH, bannerHeight, COLOR_ACCENT_WARNING);
    
    // Draw warning text
    _display.setTextColor(TFT_BLACK);
    _display.setTextDatum(middle_center);
    _display.setFont(&fonts::Font0);
    _display.drawString("WiFi not configured - see credentials.h", 
                        SCREEN_WIDTH / 2, bannerY + bannerHeight / 2);
    
    _display.endWrite();
    _display.display();
}

void MainScreen::updateDynamicElements() {
    _ui.updateDynamicElements(_timer, _timer.isRunning());
}

// ============================================================================
// Timer Expired Handling
// ============================================================================

void MainScreen::showTimeUpDialog() {
    AppState& appState = AppState::getInstance();
    
    // Don't show dialog if unlimited allowance
    if (appState.getScreenTime().hasUnlimitedAllowance) {
        Serial.println("[MainScreen] Skipping time up dialog - unlimited allowance");
        return;
    }
    
    if (_screenManager) {
        // Show two-button dialog: OK and Request More Time (per R9.5)
        _screenManager->showConfirmDialog(
            "Time's Up!",
            "You're out of time for today. Take a break!",
            "OK",
            "More Time",
            MainScreen::onTimeUpDialogResult,
            this  // Pass this pointer for callback
        );
    }
}



void MainScreen::onTimeUpDialogResult(DialogResult result, void* userData) {
    MainScreen* self = static_cast<MainScreen*>(userData);
    if (self == nullptr) return;
    
    if (result == DialogResult::BUTTON_2) {
        // "Request More Time" selected
        Serial.println("[MainScreen] User requested more time from dialog");
        self->requestMoreTime();
    } else {
        // "OK" selected or dialog dismissed
        Serial.println("[MainScreen] Time's up dialog acknowledged");
    }
}

void MainScreen::onMinimumSessionDialogResult(DialogResult result, void* userData) {
    MainScreen* self = static_cast<MainScreen*>(userData);
    if (self == nullptr) return;
    
    if (result == DialogResult::BUTTON_2) {
        // "OK" selected - proceed with stop, applying minimum session
        Serial.println("[MainScreen] User confirmed stop with minimum session");
        self->stopTimer(MINIMUM_SESSION_DURATION_SECONDS);
    } else {
        // "Don't pause" selected - keep timer running
        Serial.println("[MainScreen] User cancelled stop, timer continues");
    }
}

void MainScreen::onTimerExpired(uint32_t sessionDurationSeconds, time_t sessionStartTime) {
    Serial.println("[MainScreen] Screen time expired!");
    
    // Show notification immediately (non-blocking with duration 0)
    _ui.showNotification("Storing...", 0);
    
    // Delegate to SessionManager - handles persistence and API push
    _sessionManager.onSessionExpired(sessionDurationSeconds, sessionStartTime);
    
    // Play expiry alarm
    playExpiryAlarm();
    
    // Clear notification and show expired state
    drawFullScreen();
}

// ============================================================================
// Request More Time
// ============================================================================

void MainScreen::requestMoreTime() {
    Serial.println("[MainScreen] Requesting more time...");
    
    // Check if unlimited allowance
    AppState& appState = AppState::getInstance();
    if (appState.getScreenTime().hasUnlimitedAllowance) {
        Serial.println("[MainScreen] Unlimited allowance - cannot request more time");
        
        if (_screenManager) {
            _screenManager->showInfoDialog(
                "Unlimited Time",
                "You already have unlimited screen time today. No need to request more!",
                "OK"
            );
        }
        return;
    }
    
    // Check if we have the necessary components
    if (_apiClient == nullptr || _pollingManager == nullptr) {
        Serial.println("[MainScreen] API client or polling manager not available");
        _ui.showNotification("Not available", 1500);
        return;
    }
    
    // Check if already polling
    if (_pollingManager->isPolling()) {
        Serial.println("[MainScreen] Already polling for more time");
        _ui.showNotification("Request pending...", 1500);
        return;
    }
    
    // Get child ID and name from app state
    const char* childId = appState.getSession().selectedChildId;
    const char* childName = appState.getSession().selectedChildName;
    
    if (childId[0] == '\0') {
        Serial.println("[MainScreen] No child selected");
        _ui.showNotification("No child selected", 1500);
        return;
    }
    
    // Submit the request via API (with child name for notification message)
    MoreTimeRequestResult result = _apiClient->requestAdditionalTime(childId, childName);
    
    if (!result.success) {
        Serial.printf("[MainScreen] More time request failed: %s\n", result.errorMessage);
        _ui.showNotification("Request failed", 1500);
        return;
    }
    
    // Start polling for approval
    _pollingManager->startMoreTimePolling(
        result.requestId,
        MainScreen::onMoreTimePollResult,
        this
    );
    
    // Update UI to show polling
    _isPollingForMoreTime = true;
    
    _ui.showNotification("Request sent!", 1000);
    drawFullScreen();
}

bool MainScreen::isPollingForMoreTime() const {
    return _isPollingForMoreTime;
}

void MainScreen::onMoreTimePollResult(const PollingResult& result, void* userData) {
    MainScreen* self = static_cast<MainScreen*>(userData);
    if (self != nullptr) {
        self->handleMoreTimeResult(result);
    }
}

void MainScreen::handleMoreTimeResult(const PollingResult& result) {
    Serial.printf("[MainScreen] More time result: success=%d, granted=%d, denied=%d, minutes=%lu\n",
                  result.success, result.granted, result.denied,
                  (unsigned long)result.additionalMinutes);
    
    // Clear polling state
    _isPollingForMoreTime = false;
    
    if (result.timedOut) {
        _ui.showNotification("Request timed out", 2000);
    } else if (result.granted) {
        // Notify user first (before potential network delay from fetching allowance)
        char msg[32];
        snprintf(msg, sizeof(msg), "+%lu min!", (unsigned long)result.additionalMinutes);
        playButtonBeep();
        _ui.showNotification(msg, 2000);
        
        // Fetch the updated allowance from the API to get the authoritative value.
        // This avoids issues where the server's bonusMinutes field may contain
        // cumulative bonus rather than just the delta from this specific grant.
        bool fetchSuccess = fetchAllowanceFromApi();
        
        if (!fetchSuccess) {
            // Fallback: add locally if API fetch fails (better than nothing)
            Serial.println("[MainScreen] API fetch failed, falling back to local add");
            uint32_t additionalSeconds = result.additionalMinutes * 60;
            _timer.addAllowance(additionalSeconds);
            
            AppState& appState = AppState::getInstance();
            appState.getScreenTime().dailyAllowanceSeconds += additionalSeconds;
            appState.saveAllowanceToPersistence();
        }
        
        // Force redraw to update timer display
        _ui.forceFullRedraw();
    } else if (result.denied) {
        _ui.showNotification("Request denied", 2000);
    } else {
        _ui.showNotification(result.message, 2000);
    }
    
    drawFullScreen();
}

// ============================================================================
// Menu Setup and Actions
// ============================================================================

void MainScreen::setupMenu() {
    _menu.clear();
    
    // Add menu items (per R10.2 - simplified, settings moved to Settings screen)
    
    _menu.addItem("Ask for more time", onMenuRequestMoreTime, this, true); // Request more screen time
    _menu.addItem("Refresh", onMenuRefreshSync, this, true);       // Sync time/allowance
    _menu.addItem("Settings", onMenuSettings, this, true);         // Navigate to settings
    _menu.addItem("Parent Menu", onMenuParent, this, true);        // Navigate to parent screen
    _menu.addItem("Sleep now", onMenuSleepNow, this, true);        // Enter deep sleep
    
    Serial.printf("[MainScreen] Menu initialized with %d items\n", _menu.getItemCount());
}

void MainScreen::destroyMenu() {
    _menu.clear();
    Serial.println("[MainScreen] Menu cleared");
}

// Static menu callbacks - delegate to instance methods

void MainScreen::onMenuRefreshSync(int itemIndex, void* userData) {
    MainScreen* self = static_cast<MainScreen*>(userData);
    if (self) self->doRefreshSync();
}

void MainScreen::onMenuRequestMoreTime(int itemIndex, void* userData) {
    MainScreen* self = static_cast<MainScreen*>(userData);
    if (self) self->requestMoreTime();
}

void MainScreen::onMenuSleepNow(int itemIndex, void* userData) {
    MainScreen* self = static_cast<MainScreen*>(userData);
    if (self) self->doSleepNow();
}

void MainScreen::onMenuSettings(int itemIndex, void* userData) {
    MainScreen* self = static_cast<MainScreen*>(userData);
    if (self) self->doSettings();
}

void MainScreen::onMenuParent(int itemIndex, void* userData) {
    MainScreen* self = static_cast<MainScreen*>(userData);
    if (self) self->doParent();
}

void MainScreen::doSettings() {
    Serial.println("[MainScreen] Settings activated");
    
    if (_screenManager) {
        _screenManager->navigateTo(ScreenType::SETTINGS);
    }
}

void MainScreen::doParent() {
    Serial.println("[MainScreen] Parent Menu activated");
    
    if (_screenManager) {
        _screenManager->navigateTo(ScreenType::PARENT);
    }
}

// ============================================================================
// API Integration
// ============================================================================

bool MainScreen::fetchAllowanceFromApi() {
    if (_apiClient == nullptr) {
        Serial.println("[MainScreen] No API client - skipping allowance fetch");
        return false;
    }
    
    AppState& appState = AppState::getInstance();
    const char* childId = appState.getSession().selectedChildId;
    
    if (childId[0] == '\0') {
        Serial.println("[MainScreen] No child selected - skipping allowance fetch");
        return false;
    }
    
    Serial.println("[MainScreen] Fetching allowance from API...");
    
    AllowanceResult result = _apiClient->getTodayAllowance(childId);
    
    if (result.success) {
        uint32_t allowanceSeconds = result.dailyAllowanceMinutes * 60;
        
        // Store unlimited flag in app state
        appState.getScreenTime().hasUnlimitedAllowance = result.hasUnlimitedAllowance;
        
        // Use setAllowance() to preserve consumed time (reset() would clear it)
        // This is safe on day change since consumed time was already cleared.
        // setAllowance() is also safe to call while timer is running - it just
        // updates the allowance and the remaining time calculation will reflect
        // the new value immediately (important for "more time" grants).
        if (result.hasUnlimitedAllowance) {
            // Unlimited time - set a very large allowance to prevent expiry checks
            // Use UINT32_MAX / 2 to avoid overflow in calculations
            _timer.setAllowance(UINT32_MAX / 2);
            appState.getScreenTime().dailyAllowanceSeconds = 0;  // Store 0 as marker
            Serial.println("[MainScreen] Unlimited allowance detected - no time restriction");
        } else if (allowanceSeconds > 0) {
            _timer.setAllowance(allowanceSeconds);
            appState.getScreenTime().dailyAllowanceSeconds = allowanceSeconds;
            Serial.printf("[MainScreen] Allowance updated: %lu minutes (timer %s)\n",
                          (unsigned long)result.dailyAllowanceMinutes,
                          _timer.isRunning() ? "running" : "stopped");
        }
        
        appState.saveAllowanceToPersistence();
        return true;
    } else {
        Serial.printf("[MainScreen] Failed to fetch allowance: %s\n",
                      result.errorMessage);
        return false;
    }
}

void MainScreen::showAllowanceFetchFailedDialog() {
    if (_screenManager == nullptr) {
        Serial.println("[MainScreen] No screen manager - cannot show dialog");
        return;
    }
    
    Dialog& dialog = _screenManager->getDialog();
    dialog.showInfo(
        "Sync Failed",
        "Could not fetch screen time allowance from server.",
        "Try Again",
        onAllowanceFetchFailedDialogResult,
        this
    );
}

void MainScreen::onAllowanceFetchFailedDialogResult(DialogResult result, void* userData) {
    MainScreen* self = static_cast<MainScreen*>(userData);
    if (self == nullptr) return;
    
    Serial.println("[MainScreen] Allowance fetch failed dialog result - retrying");
    
    // Try again
    self->drawFullScreen();
    
    bool success = self->fetchAllowanceFromApi();
    
    if (!success) {
        // Still failed - show the dialog again
        self->showAllowanceFetchFailedDialog();
    } else {
        // Success - redraw the screen
        self->drawFullScreen();
    }
}

// Menu action implementations

void MainScreen::doRefreshSync() {
    Serial.println("[MainScreen] Refresh/Sync activated");
    
    _ui.showNotification("Syncing...", 500);
    
    if (_networkManager == nullptr) {
        _ui.showNotification("No network!", 1500);
        drawFullScreen();
        return;
    }
    
    // Perform time sync
    bool timeSuccess = _networkManager->withConnection([this]() {
        _ui.updateNetworkStatus(NetworkStatus::CONNECTED);
        return _networkManager->syncTimeAndSetRTC();
    });
    
    // Also fetch updated screen time allowance - reuse fetchAllowanceFromApi()
    // to avoid code duplication and ensure consistent behavior
    bool allowanceSuccess = fetchAllowanceFromApi();
    
    _ui.updateNetworkStatus(NetworkStatus::DISCONNECTED);
    
    if (timeSuccess || allowanceSuccess) {
        // Update weekday tracking after sync
        AppState::getInstance().updateLastActiveWeekday();
        AppState::getInstance().saveWeekdayToPersistence();
        
        // Note: We no longer push consumed time on refresh sync.
        // Sessions are pushed when they end (via pushSessionToApi).
        
        _ui.showNotification("Synced", 1000);
    } else {
        _ui.showNotification("Sync failed", 1500);
    }
    
    drawFullScreen();
}

void MainScreen::doSleepNow() {
    Serial.println("[MainScreen] Sleep Now activated");
    
    // Try to go to sleep (includes validation) - user initiated
    tryGoToSleep(true);
}