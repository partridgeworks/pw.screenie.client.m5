/**
 * brightness_screen.h - Brightness Adjustment Screen for Screen Time Tracker
 * 
 * Displays brightness level indicator with left/right chevrons.
 * Button B cycles through 4 brightness levels.
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#ifndef BRIGHTNESS_SCREEN_H
#define BRIGHTNESS_SCREEN_H

#include "../screen.h"
#include <M5GFX.h>

// Forward declarations
class ScreenManager;

// Brightness levels (1-4 mapped to percentage values of 255)
constexpr uint8_t BRIGHTNESS_LEVEL_COUNT = 4;
constexpr uint8_t BRIGHTNESS_VALUES[BRIGHTNESS_LEVEL_COUNT] = {
    15,   // Level 1: 10% - Dim
    50,  // Level 2: 40% - Normal
    100,  // Level 3: 70% - Bright
    160   // Level 4: 90% - Very Bright
};

// Default brightness level (1-4, 0-indexed internally)
constexpr uint8_t DEFAULT_BRIGHTNESS_LEVEL = 2;  // Normal (40%)

/**
 * BrightnessScreen - Brightness adjustment screen
 * 
 * Shows:
 * - Header: "Brightness" centered
 * - Middle: Left/right chevrons with brightness indicator bars
 * - Footer: Instruction to use Button B
 * 
 * Button Mapping:
 * - Button A: Exit (go back)
 * - Button B: Cycle brightness level
 * - Power: Exit (go back)
 */
class BrightnessScreen : public Screen {
public:
    /**
     * Constructor
     * @param display Reference to M5GFX display
     */
    explicit BrightnessScreen(M5GFX& display);
    
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
    
    const char* getTitle() const override { return "Brightness"; }
    bool showsHeader() const override { return false; }  // Custom full-screen layout
    bool needsFrequentUpdates() const override { return false; }

    // ========================================================================
    // Static Brightness Control
    // ========================================================================
    
    /**
     * Apply brightness from stored level
     * Call on startup and wake from sleep
     */
    static void applyStoredBrightness();
    
    /**
     * Get the current brightness level (1-4)
     * @return Current level (1-4)
     */
    static uint8_t getCurrentLevel();

private:
    M5GFX& _display;
    ScreenManager* _screenManager;
    
    // Current brightness level (0-indexed internally, displayed as 1-4)
    uint8_t _currentLevel;
    
    // Drawing helpers
    void drawBackground();
    void drawTitle();
    void drawBrightnessIndicator();
    void drawChevrons();
    void drawInstructions();
    
    // Brightness control
    void cycleBrightness();
    void applyBrightness();
    void saveBrightnessLevel();
    
    // Helper to go back to previous screen
    void exitScreen();
};

#endif // BRIGHTNESS_SCREEN_H
