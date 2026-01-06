/**
 * settings_screen.h - Settings Screen for Screen Time Tracker
 * 
 * Container screen that immediately displays a settings menu.
 * The screen itself is mostly transparent - its purpose is to host the menu.
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#ifndef SETTINGS_SCREEN_H
#define SETTINGS_SCREEN_H

#include "../screen.h"
#include "../menu.h"
#include "../ui.h"
#include "../dialog.h"
#include <M5GFX.h>

// Forward declarations
class ScreenManager;

/**
 * SettingsScreen - Settings menu host screen
 * 
 * Shows:
 * - Very dim "SETTINGS" placeholder text (should never be visible)
 * - Settings menu overlay immediately on enter
 * 
 * Menu Items:
 * - Brightness
 * - System Info
 * - Change Child
 * - Logout
 * 
 * Button Mapping:
 * - Button A: Activate selected menu item
 * - Button B: Cycle menu items
 * - Power: Go back (close menu and return to previous screen)
 */
class SettingsScreen : public Screen {
public:
    /**
     * Constructor
     * @param display Reference to M5GFX display
     * @param ui Reference to UI for menu drawing
     */
    SettingsScreen(M5GFX& display, UI& ui);
    
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
    
    const char* getTitle() const override { return "Settings"; }
    bool showsHeader() const override { return true; }
    bool needsFrequentUpdates() const override { return false; }
    bool hasMenu() const override { return true; }
    bool isMenuVisible() const override { return _menu.isVisible(); }

private:
    M5GFX& _display;
    UI& _ui;
    ScreenManager* _screenManager;
    DropdownMenu _menu;
    
    // Menu setup and teardown
    void setupMenu();
    void destroyMenu();
    
    // Drawing helpers
    void drawBackground();
    void drawHeader();
    void drawPlaceholderText();
    void drawMenu();
    
    // Menu action callbacks (static for use as function pointers)
    static void onBrightnessSelected(int itemIndex, void* userData);
    static void onSystemInfoSelected(int itemIndex, void* userData);
    static void onPowerOffSelected(int itemIndex, void* userData);
    
    // Dialog result callbacks (static)
    static void onPowerOffDialogResult(DialogResult result, void* userData);
    
    // Instance methods for menu actions
    void navigateToBrightness();
    void navigateToSystemInfo();
    void handlePowerOff();
    
    // Helper to go back to previous screen
    void exitScreen();
};

#endif // SETTINGS_SCREEN_H
