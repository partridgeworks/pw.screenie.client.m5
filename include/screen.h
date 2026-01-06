/**
 * screen.h - Screen base class for Screen Time Tracker
 * 
 * Abstract base class for all screens in the application.
 * Each screen owns its own lifecycle, input handling, and rendering.
 * 
 * Screens may optionally own a menu. If a screen has a menu, it creates
 * the menu in onEnter() and destroys it in onExit(). The screen handles
 * all menu-related button routing internally.
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#ifndef SCREEN_H
#define SCREEN_H

#include <M5GFX.h>

// Forward declaration
class DropdownMenu;

/**
 * Screen - Abstract base class for all screens
 * 
 * Defines the interface that all screens must implement.
 * Screens are managed by ScreenManager which calls these methods
 * at appropriate times.
 */
class Screen {
public:
    /**
     * Virtual destructor for proper cleanup of derived classes
     */
    virtual ~Screen() = default;
    
    // ========================================================================
    // Lifecycle Methods
    // ========================================================================
    
    /**
     * Called when this screen becomes the active screen
     * Use for initialization, starting network requests, etc.
     */
    virtual void onEnter() = 0;
    
    /**
     * Called when this screen is about to be replaced by another
     * Use for cleanup, stopping timers, saving state, etc.
     */
    virtual void onExit() = 0;
    
    /**
     * Called when returning to this screen from an overlay (dialog/menu)
     * Default implementation does nothing.
     */
    virtual void onResume() {}
    
    // ========================================================================
    // Per-Frame Methods
    // ========================================================================
    
    /**
     * Called every loop iteration to update screen state
     * Handle timers, animations, polling, etc.
     */
    virtual void update() = 0;
    
    /**
     * Called to render the screen
     * Should draw all visible elements
     */
    virtual void draw() = 0;
    
    // ========================================================================
    // Input Handling
    // ========================================================================
    
    /**
     * Called when Button A (front button) is clicked
     * Default implementation does nothing.
     */
    virtual void onButtonA() {}
    
    /**
     * Called when Button B (side button) is clicked
     * Default implementation does nothing.
     */
    virtual void onButtonB() {}
    
    /**
     * Called when Power button is clicked
     * Default implementation does nothing.
     */
    virtual void onButtonPower() {}
    
    /**
     * Called when Power button is held
     * Default implementation does nothing.
     */
    virtual void onButtonPowerHold() {}
    
    // ========================================================================
    // Screen Metadata
    // ========================================================================
    
    /**
     * Get the title of this screen (displayed in header)
     * @return Title string
     */
    virtual const char* getTitle() const = 0;
    
    /**
     * Whether this screen should show the standard header
     * Override to return false for full-screen layouts (like login)
     * @return true if header should be shown (default)
     */
    virtual bool showsHeader() const { return true; }
    
    /**
     * Whether this screen needs frequent updates (e.g., has animations)
     * ScreenManager may use this to optimize update frequency
     * @return true if screen needs frequent updates
     */
    virtual bool needsFrequentUpdates() const { return false; }
    
    // ========================================================================
    // Menu Support
    // ========================================================================
    
    /**
     * Check if this screen has an active menu
     * Override in screens that have menus
     * @return true if screen has a menu
     */
    virtual bool hasMenu() const { return false; }
    
    /**
     * Check if the menu is currently visible
     * Override in screens that have menus
     * @return true if menu is visible
     */
    virtual bool isMenuVisible() const { return false; }
};

#endif // SCREEN_H
