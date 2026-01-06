/**
 * ui.h - User Interface declarations for Screen Time Tracker
 * 
 * Handles all display rendering including:
 * - Main screen layout with border, header, avatar
 * - Countdown timer display
 * - Progress bar
 * - Status indicators
 * - Button hints
 * - Info dialogs
 * 
 * Uses M5GFX for all graphics operations.
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#ifndef UI_H
#define UI_H

#include <M5GFX.h>
#include "config.h"
#include "network.h"
#include "timer.h"  // For TimerState enum

// Forward declarations
class ScreenTimer;
class DropdownMenu;

/**
 * UI class - Manages all display rendering
 * 
 * This class is responsible for drawing all UI elements on the M5StickC Plus2
 * display. It uses double buffering via sprite for smooth updates.
 */
class UI {
public:
    /**
     * Constructor - Initialize UI with display reference
     * @param display Reference to M5GFX display object
     */
    explicit UI(M5GFX& display);

    /**
     * Initialize the display and prepare for rendering
     * Must be called once during setup()
     */
    void begin();

    /**
     * Draw the complete main screen
     * @param timer Reference to timer for time values
     * @param userName Name of the current user
     * @param userInitial Initial letter for avatar (fallback if no avatar PNG)
     * @param avatarName Avatar PNG filename (e.g., "1F3B1_color.png" or "1F3B1_color")
     * @param isTimerRunning True if countdown is active
     * @param networkStatus Current network status for header indicator
     */
    void drawMainScreen(const ScreenTimer& timer, 
                        const char* userName, 
                        char userInitial,
                        const char* avatarName,
                        bool isTimerRunning,
                        NetworkStatus networkStatus = NetworkStatus::DISCONNECTED);

    /**
     * Update only the dynamic elements (timer, progress, status)
     * More efficient than full redraw
     * @param timer Reference to timer for time values
     * @param isTimerRunning True if countdown is active
     */
    void updateDynamicElements(const ScreenTimer& timer, bool isTimerRunning);

    /**
     * Draw the dropdown menu overlay
     * @param menu Reference to the menu to draw
     */
    void drawMenu(const DropdownMenu& menu);

    /**
     * Clear the menu area and restore underlying UI
     * @param timer Reference to timer for redraw
     * @param userName User name for redraw
     * @param userInitial Initial for avatar redraw
     * @param avatarName Avatar PNG filename (with or without .png extension)
     * @param isTimerRunning Timer state for redraw
     * @param networkStatus Network status for header
     */
    void clearMenu(const ScreenTimer& timer,
                   const char* userName,
                   char userInitial,
                   const char* avatarName,
                   bool isTimerRunning,
                   NetworkStatus networkStatus = NetworkStatus::DISCONNECTED);

    /**
     * Flash a menu item to indicate activation
     * @param menu Reference to menu
     * @param itemIndex Index of item to flash
     */
    void flashMenuItem(const DropdownMenu& menu, int itemIndex);

    /**
     * Show a brief notification/toast message
     * @param message Message to display
     * @param durationMs How long to show (0 = until cleared)
     */
    void showNotification(const char* message, uint32_t durationMs = 2000);

    /**
     * Show a full-screen info dialog
     * @param title Dialog title/header
     * @param message Main message text
     * @param buttonLabel Label for the action button (default "OK")
     */
    void showInfoDialog(const char* title, 
                        const char* message, 
                        const char* buttonLabel = "OK");

    /**
     * Check if an info dialog is currently displayed
     * @return true if dialog is showing
     */
    bool isInfoDialogVisible() const;

    /**
     * Dismiss the current info dialog
     */
    void dismissInfoDialog();

    /**
     * Update the network status indicator in the header
     * @param status Current network status
     */
    void updateNetworkStatus(NetworkStatus status);

    /**
     * Update the battery indicator in the header
     * Call periodically (e.g., every 5 minutes) to refresh battery level display
     */
    void updateBatteryIndicator();

    /**
     * Force a complete screen refresh
     */
    void forceFullRedraw();
    
    /**
     * Check if a full redraw is needed (e.g., after notification cleared)
     * @return true if full redraw is pending
     */
    bool needsFullRedraw() const;

    // ========================================================================
    // Shared Header Drawing
    // ========================================================================
    
    /**
     * Draw the standard app header bar
     * Can be used by any screen that wants the common header.
     * @param appName App/screen name to display
     */
    void drawHeader(const char* appName);
    
    /**
     * Draw the network status and battery indicators in the header
     * Call after drawHeader() to add the right-side indicators.
     * @param networkStatus Current network status
     */
    void drawNetworkStatusInHeader(NetworkStatus networkStatus);
    
    /**
     * Draw the complete standard header (header bar + network/battery indicators)
     * Convenience method combining drawHeader() and drawNetworkStatusInHeader().
     * @param title Title to display in header
     * @param networkStatus Current network status
     */
    void drawStandardHeader(const char* title, NetworkStatus networkStatus);

private:
    M5GFX& _display;
    M5Canvas _canvas;  // For double buffering if needed
    
    bool _needsFullRedraw;
    uint32_t _lastUpdateMs;
    bool _infoDialogVisible;
    NetworkStatus _currentNetworkStatus;
    
    // Header layout state
    int _headerAppNameEndX;  // X position where app name ends (for date positioning)
    
    // Cached state for change detection (prevents flicker when paused)
    uint32_t _lastDrawnSeconds;
    float _lastDrawnProgress;
    bool _lastDrawnRunning;
    
    // Private drawing methods
    void drawDateInHeader();
    void drawDateOnMainScreen();
    void drawAvatar(char initial, const char* userName, const char* avatarName, int x, int y);
    void drawChildName(const char* userName, int x, int y);
    void drawTimerDisplay(uint32_t remainingSeconds, TimerState state);
    void drawProgressBar(const ScreenTimer& timer);
    void clearActivationRing(const ScreenTimer &timer);
    void drawStatusRing(const ScreenTimer& timer);
    
    // Internal version of drawNetworkStatusInHeader that uses cached status
    void drawNetworkStatusInHeader();
    
    // Helper methods
    void formatTime(uint32_t seconds, char* buffer, size_t bufferSize);
    uint16_t getProgressColor(float progress);
    void getDayOfWeekStr(int dayOfWeek, char* buffer);
    void getMonthStr(int month, char* buffer);
    char getNetworkStatusChar(NetworkStatus status);
};

#endif // UI_H
