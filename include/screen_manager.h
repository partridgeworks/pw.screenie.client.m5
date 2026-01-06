/**
 * screen_manager.h - Screen Manager for Screen Time Tracker
 * 
 * Orchestrates which screen is active, manages screen transitions,
 * handles overlays (dialogs, menus), and routes button inputs.
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include <M5GFX.h>
#include "screen.h"
#include "dialog.h"

// Maximum number of screens in the navigation history stack
constexpr int MAX_HISTORY_DEPTH = 8;

/**
 * ScreenType - Enumeration of all available screens
 */
enum class ScreenType {
    NONE = -1,      // No screen (initial state)
    MAIN,           // Main timer display
    LOGIN,          // QR code login
    SELECT_CHILD,   // Family member selection
    SYNC_PROGRESS,  // Full-screen sync overlay
    SYSTEM_INFO,    // System information display
    SETTINGS,       // Settings menu screen
    BRIGHTNESS,     // Brightness adjustment screen
    PARENT,         // Parent/admin access screen
    // Add more as needed
    COUNT           // Number of screen types (for array sizing)
};

/**
 * ScreenManager - Manages screen lifecycle and navigation
 * 
 * Responsibilities:
 * - Track current active screen
 * - Handle screen transitions (navigate, back)
 * - Route button inputs to active screen/overlay
 * - Manage overlay stack (dialogs, menus)
 */
class ScreenManager {
public:
    /**
     * Constructor
     * @param display Reference to the M5GFX display
     */
    explicit ScreenManager(M5GFX& display);
    
    /**
     * Destructor - cleans up screen instances
     */
    ~ScreenManager();
    
    /**
     * Initialize the screen manager
     * Must be called once during setup()
     */
    void begin();
    
    // ========================================================================
    // Screen Registration
    // ========================================================================
    
    /**
     * Register a screen instance for a given screen type
     * ScreenManager does NOT take ownership - caller manages lifetime
     * @param type The screen type identifier
     * @param screen Pointer to the screen instance
     */
    void registerScreen(ScreenType type, Screen* screen);
    
    // ========================================================================
    // Navigation
    // ========================================================================
    
    /**
     * Navigate to a new screen
     * Calls onExit on current screen, onEnter on new screen
     * @param type Screen type to navigate to
     */
    void navigateTo(ScreenType type);
    
    /**
     * Navigate back to the previous screen in history
     * @return true if navigated back, false if no history
     */
    bool navigateBack();
    
    /**
     * Get the current active screen type
     * @return Current screen type
     */
    ScreenType getCurrentScreenType() const;
    
    /**
     * Get a pointer to the current screen instance
     * @return Pointer to current screen, or nullptr if none
     */
    Screen* getCurrentScreen() const;
    
    // ========================================================================
    // Per-Frame Updates
    // ========================================================================
    
    /**
     * Update the current screen
     * Should be called every loop iteration
     */
    void update();
    
    /**
     * Draw the current screen
     * Should be called when display refresh is needed
     */
    void draw();
    
    // ========================================================================
    // Input Routing
    // ========================================================================
    
    /**
     * Handle Button A click - routes to current screen
     */
    void handleButtonA();
    
    /**
     * Handle Button B click - routes to current screen
     */
    void handleButtonB();
    
    /**
     * Handle Power button click - routes to current screen
     */
    void handleButtonPower();
    
    /**
     * Handle Power button hold - routes to current screen
     */
    void handleButtonPowerHold();
    
    // ========================================================================
    // Overlay Management (Dialogs)
    // ========================================================================
    
    /**
     * Get the shared dialog instance
     * Use this to show dialogs via the screen manager
     * @return Reference to the dialog
     */
    Dialog& getDialog();
    
    /**
     * Show an info dialog (1 button)
     * @param title Dialog title
     * @param message Dialog message
     * @param buttonLabel Button label (default "OK")
     * @param callback Optional callback when dismissed
     * @param userData Optional user data for callback
     */
    void showInfoDialog(const char* title,
                        const char* message,
                        const char* buttonLabel = "OK",
                        DialogCallback callback = nullptr,
                        void* userData = nullptr);
    
    /**
     * Show a confirm dialog (2 buttons)
     * @param title Dialog title
     * @param message Dialog message
     * @param button1Label First button label (left, typically Cancel)
     * @param button2Label Second button label (right, typically OK)
     * @param callback Callback when a button is pressed
     * @param userData Optional user data for callback
     */
    void showConfirmDialog(const char* title,
                           const char* message,
                           const char* button1Label,
                           const char* button2Label,
                           DialogCallback callback = nullptr,
                           void* userData = nullptr);
    
    /**
     * Dismiss the current dialog
     * Calls onResume() on the current screen
     */
    void dismissDialog();
    
    /**
     * Check if there's an active overlay (dialog or menu)
     * @return true if an overlay is active
     */
    bool hasActiveOverlay() const;
    
    /**
     * Check if a dialog is currently visible
     * @return true if dialog is showing
     */
    bool isDialogVisible() const;

private:
    M5GFX& _display;
    
    // Screen registry - pointers to screen instances (not owned)
    Screen* _screens[static_cast<int>(ScreenType::COUNT)];
    
    // Current active screen
    ScreenType _currentScreenType;
    
    // Navigation history stack
    ScreenType _history[MAX_HISTORY_DEPTH];
    int _historyIndex;
    
    // Overlay state
    Dialog _dialog;      // Shared dialog instance
    
    /**
     * Push current screen to history stack
     */
    void pushHistory(ScreenType type);
    
    /**
     * Pop and return the top of the history stack
     * @return Previous screen type, or NONE if empty
     */
    ScreenType popHistory();
};

#endif // SCREEN_MANAGER_H
