/**
 * menu.h - Dropdown Menu system for Screen Time Tracker
 * 
 * Provides a dropdown menu overlay system with:
 * - Configurable menu items
 * - Selection navigation
 * - Activation callbacks
 * - Visual state management
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include "config.h"

/**
 * MenuAction - Callback function type for menu item activation
 */
typedef void (*MenuActionCallback)(int itemIndex, void* userData);

/**
 * MenuItem - Represents a single menu entry
 */
struct MenuItem {
    char label[24];                 // Display text
    MenuActionCallback callback;    // Action callback (can be nullptr)
    void* userData;                 // User data passed to callback
    bool enabled;                   // Whether item is selectable
};

/**
 * DropdownMenu class - Manages a dropdown menu overlay
 * 
 * Supports up to MENU_MAX_ITEMS items with navigation
 * and activation via button presses.
 */
class DropdownMenu {
public:
    /**
     * Constructor - Initialize empty menu
     */
    DropdownMenu();

    /**
     * Clear all menu items
     */
    void clear();

    /**
     * Add an item to the menu
     * @param label Display text for the item
     * @param callback Function to call when activated (optional)
     * @param userData User data passed to callback (optional)
     * @param enabled Whether item is selectable (default true)
     * @return Index of added item, or -1 if menu is full
     */
    int addItem(const char* label, 
                MenuActionCallback callback = nullptr,
                void* userData = nullptr,
                bool enabled = true);

    /**
     * Show the menu overlay
     */
    void show();

    /**
     * Hide the menu overlay
     */
    void hide();

    /**
     * Toggle menu visibility
     * @return true if menu is now visible
     */
    bool toggle();

    /**
     * Check if menu is currently visible
     * @return true if visible
     */
    bool isVisible() const;

    /**
     * Move selection to next item
     * Wraps around to first item after last
     */
    void selectNext();

    /**
     * Move selection to previous item
     * Wraps around to last item before first
     */
    void selectPrevious();

    /**
     * Activate the currently selected item
     * Calls the item's callback if set
     * @return true if item was activated
     */
    bool activateSelected();

    /**
     * Get index of currently selected item
     * @return Selected index (0-based)
     */
    int getSelectedIndex() const;

    /**
     * Get number of items in menu
     * @return Item count
     */
    int getItemCount() const;

    /**
     * Get label of item at index
     * @param index Item index
     * @return Item label, or empty string if invalid index
     */
    const char* getItemLabel(int index) const;

    /**
     * Check if item at index is enabled
     * @param index Item index
     * @return true if enabled
     */
    bool isItemEnabled(int index) const;

    /**
     * Set selected index directly
     * @param index New selected index
     */
    void setSelectedIndex(int index);

private:
    MenuItem _items[MENU_MAX_ITEMS];
    int _itemCount;
    int _selectedIndex;
    bool _visible;

    // Find next/previous enabled item
    int findNextEnabled(int fromIndex) const;
    int findPrevEnabled(int fromIndex) const;
};

#endif // MENU_H
