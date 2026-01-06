/**
 * system_info_screen.h - System Info Screen for Screen Time Tracker
 * 
 * Displays system information including battery status and app version.
 * Any button press exits back to the previous screen.
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#ifndef SYSTEM_INFO_SCREEN_H
#define SYSTEM_INFO_SCREEN_H

#include "../screen.h"
#include <M5GFX.h>

// Forward declarations
class ScreenManager;

/**
 * SystemInfoScreen - System information display screen
 * 
 * Shows:
 * - Heading "System Info"
 * - Battery Voltage
 * - Battery Percentage Remaining
 * - App Version
 * 
 * Button Mapping:
 * - Any button: Return to previous screen
 */
class SystemInfoScreen : public Screen {
public:
    /**
     * Constructor
     * @param display Reference to M5GFX display
     */
    explicit SystemInfoScreen(M5GFX& display);
    
    /**
     * Set the screen manager (for navigation callbacks)
     * @param manager Pointer to screen manager
     */
    void setScreenManager(ScreenManager* manager);
    
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
    
    const char* getTitle() const override { return "System Info"; }
    bool showsHeader() const override { return false; }  // Custom full-screen layout
    bool needsFrequentUpdates() const override { return false; }

private:
    M5GFX& _display;
    ScreenManager* _screenManager;
    
    // Cached battery info (read on enter)
    int _batteryLevel;      // Percentage 0-100
    int _batteryVoltage;    // Voltage in mV
    
    // Drawing helpers
    void drawBackground();
    void drawTitle();
    void drawBatteryInfo();
    void drawVersionInfo();
    void drawExitHint();
    
    // Helper to go back to previous screen
    void exitScreen();
};

#endif // SYSTEM_INFO_SCREEN_H
