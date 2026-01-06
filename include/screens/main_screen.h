/**
 * main_screen.h - Main Screen for Screen Time Tracker
 * 
 * The primary screen displaying the countdown timer, progress bar,
 * and avatar. Handles timer control and menu interaction.
 * 
 * This screen owns its dropdown menu and defines all menu actions.
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#ifndef MAIN_SCREEN_H
#define MAIN_SCREEN_H

#include <time.h>
#include "../screen.h"
#include "../timer.h"
#include "../session_manager.h"
#include "../menu.h"
#include "../ui.h"
#include "../dialog.h"
#include "../polling_manager.h"

// Forward declarations
class ScreenManager;
class ApiClient;
class PollingManager;
class NetworkManager;

/**
 * MainScreen - Primary screen time display
 * 
 * Shows:
 * - Header with date and network status
 * - Large countdown timer
 * - Avatar with status ring
 * - Progress bar
 * 
 * Handles:
 * - Timer start/stop via Button A
 * - Menu access via Button B
 * - Menu navigation and activation
 * 
 * Menu Items:
 * - Settings
 * - Refresh
 * - More time
 * - Sleep now
 */
class MainScreen : public Screen {
public:
    /**
     * Constructor
     * @param display Reference to M5GFX display
     * @param sessionManager Reference to the session manager
     * @param ui Reference to the UI class (for existing drawing methods)
     */
    MainScreen(M5GFX& display, SessionManager& sessionManager, UI& ui);
    
    /**
     * Set the screen manager (for navigation callbacks)
     * @param manager Pointer to screen manager
     */
    void setScreenManager(ScreenManager* manager);
    
    /**
     * Set the API client and polling manager
     * @param api Pointer to ApiClient
     * @param polling Pointer to PollingManager
     */
    void setApiClient(ApiClient* api, PollingManager* polling);
    
    /**
     * Set the network manager (for sync operations)
     * @param network Pointer to NetworkManager
     */
    void setNetworkManager(NetworkManager* network);
    
    // ========================================================================
    // Screen Lifecycle
    // ========================================================================
    
    void onEnter() override;
    void onExit() override;
    void onResume() override;
    void update() override;
    void draw() override;
    
    // ========================================================================
    // Input Handling
    // ========================================================================
    
    void onButtonA() override;
    void onButtonB() override;
    void onButtonPower() override;
    void onButtonPowerHold() override;
    
    // ========================================================================
    // Screen Metadata
    // ========================================================================
    
    const char* getTitle() const override { return "Screen Time"; }
    bool showsHeader() const override { return true; }
    bool needsFrequentUpdates() const override { return true; }
    bool hasMenu() const override { return true; }
    bool isMenuVisible() const override;
    
    // ========================================================================
    // Timer Control (exposed for external use)
    // ========================================================================
    
    /**
     * Start the timer
     * @return true if started successfully
     */
    bool startTimer();
    
    /**
     * Stop the timer
     * @param minimumDuration If > 0, enforce this as minimum session duration
     */
    void stopTimer(uint32_t minimumDuration = 0);
    
    /**
     * Toggle timer running state
     */
    void toggleTimer();
    
    /**
     * Check if timer is currently running
     * @return true if running
     */
    bool isTimerRunning() const;
    
    /**
     * Get reference to the timer
     * @return Reference to ScreenTimer
     */
    ScreenTimer& getTimer();

    /**
     * Request more screen time
     * Submits request to API and starts polling for approval.
     */
    void requestMoreTime();
    
    /**
     * Check if currently polling for more time
     * @return true if more-time polling is active
     */
    bool isPollingForMoreTime() const;
    
    /**
     * Show the "time is up" dialog
     * Used when timer expires, including on wake from sleep
     */
    void showTimeUpDialog();

private:
    M5GFX& _display;
    SessionManager& _sessionManager;
    ScreenTimer& _timer;  // Convenience reference (from SessionManager)
    UI& _ui;  // Reference to existing UI for drawing methods
    ScreenManager* _screenManager;
    ApiClient* _apiClient;
    PollingManager* _pollingManager;
    NetworkManager* _networkManager;
    
    // Owned menu - created in onEnter, destroyed in onExit
    DropdownMenu _menu;
    
    bool _isPollingForMoreTime;  // Visual indicator in UI
    uint32_t _lastDisplayUpdateMs;
    
    // Menu setup and actions
    void setupMenu();
    void destroyMenu();
    
    // Menu action callbacks (static for use as function pointers)
    static void onMenuRefreshSync(int itemIndex, void* userData);
    static void onMenuRequestMoreTime(int itemIndex, void* userData);
    static void onMenuSleepNow(int itemIndex, void* userData);
    static void onMenuSettings(int itemIndex, void* userData);
    static void onMenuParent(int itemIndex, void* userData);
    
    // Menu action implementations
    void doRefreshSync();
    void doSleepNow();
    void doSettings();
    void doParent();
    
    // API integration
    
    /**
     * Fetch today's allowance from the API
     * @return true if successfully fetched, false on error
     */
    bool fetchAllowanceFromApi();
    
    /**
     * Show a "Try Again" dialog when allowance fetch fails
     * Called on first boot or new day when allowance is required.
     */
    void showAllowanceFetchFailedDialog();
    
    /**
     * Dialog callback for allowance fetch failure
     */
    static void onAllowanceFetchFailedDialogResult(DialogResult result, void* userData);
    
    // Drawing methods
    void drawFullScreen();
    void updateDynamicElements();
    void drawWifiWarning();
    
    // Timer expired handling
    void onTimerExpired(uint32_t sessionDurationSeconds, time_t sessionStartTime);
    
    // Dialog callback (static to be usable as function pointer)
    static void onTimeUpDialogResult(DialogResult result, void* userData);
    
    // Minimum session dialog callback
    static void onMinimumSessionDialogResult(DialogResult result, void* userData);
    
    // More-time polling callback
    static void onMoreTimePollResult(const PollingResult& result, void* userData);
    void handleMoreTimeResult(const PollingResult& result);
};

#endif // MAIN_SCREEN_H
