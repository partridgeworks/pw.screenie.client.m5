/**
 * sync_screen.h - Sync Progress Screen for Screen Time Tracker
 * 
 * Full-screen overlay showing network sync progress with spinner animation.
 * Used during NTP sync, API calls, and data loading.
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#ifndef SYNC_SCREEN_H
#define SYNC_SCREEN_H

#include "../screen.h"
#include <M5GFX.h>

// Forward declarations
class ScreenManager;

/**
 * SyncScreen - Full-screen sync/loading overlay
 * 
 * Shows:
 * - Spinner animation
 * - Status message
 * - Optional progress bar
 * 
 * No direct button handling - caller must dismiss when operation completes.
 */
class SyncScreen : public Screen {
public:
    /**
     * Constructor
     * @param display Reference to M5GFX display
     */
    explicit SyncScreen(M5GFX& display);
    
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
    // Input Handling (minimal - mostly non-interactive)
    // ========================================================================
    
    void onButtonA() override;
    void onButtonB() override;
    void onButtonPower() override;
    void onButtonPowerHold() override;
    
    // ========================================================================
    // Screen Metadata
    // ========================================================================
    
    const char* getTitle() const override { return ""; }
    bool showsHeader() const override { return false; }  // Full screen
    bool needsFrequentUpdates() const override { return true; }  // For spinner
    
    // ========================================================================
    // Sync State Management
    // ========================================================================
    
    /**
     * Set the status message
     * @param message Message to display (e.g., "Connecting to WiFi...")
     */
    void setMessage(const char* message);
    
    /**
     * Set progress (0.0 - 1.0)
     * If progress is set, shows progress bar instead of spinner
     * @param progress Progress value (0.0 = start, 1.0 = complete)
     */
    void setProgress(float progress);
    
    /**
     * Show/hide spinner
     * @param show Whether to show spinner animation
     */
    void showSpinner(bool show);
    
    /**
     * Reset to default state (spinner visible, no progress, default message)
     */
    void reset();

private:
    M5GFX& _display;
    ScreenManager* _screenManager;
    
    // State
    char _message[64];
    float _progress;
    bool _showSpinner;
    bool _showProgress;
    
    // Animation
    uint32_t _lastAnimationMs;
    uint8_t _spinnerFrame;
    static constexpr uint32_t SPINNER_INTERVAL_MS = 100;
    static constexpr int SPINNER_FRAMES = 8;
    
    // Layout
    static constexpr int SPINNER_CENTER_Y = 55;
    static constexpr int SPINNER_RADIUS = 20;
    static constexpr int MESSAGE_Y = 100;
    static constexpr int PROGRESS_Y = 90;
    static constexpr int PROGRESS_HEIGHT = 8;
    static constexpr int PROGRESS_WIDTH = 160;
    
    // Drawing methods
    void drawBackground();
    void drawSpinner();
    void drawProgressBar();
    void drawMessage();
};

#endif // SYNC_SCREEN_H
