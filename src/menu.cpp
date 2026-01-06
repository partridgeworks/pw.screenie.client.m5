/**
 * menu.cpp - Dropdown Menu implementation
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#include "menu.h"
#include <Arduino.h>
#include <string.h>

// ============================================================================
// Constructor and Initialization
// ============================================================================

DropdownMenu::DropdownMenu()
    : _itemCount(0)
    , _selectedIndex(0)
    , _visible(false)
{
    clear();
}

void DropdownMenu::clear() {
    _itemCount = 0;
    _selectedIndex = 0;
    _visible = false;
    
    // Initialize all items to empty state
    for (int i = 0; i < MENU_MAX_ITEMS; i++) {
        _items[i].label[0] = '\0';
        _items[i].callback = nullptr;
        _items[i].userData = nullptr;
        _items[i].enabled = false;
    }
}

// ============================================================================
// Menu Item Management
// ============================================================================

int DropdownMenu::addItem(const char* label, 
                          MenuActionCallback callback,
                          void* userData,
                          bool enabled) {
    // Check if menu is full
    if (_itemCount >= MENU_MAX_ITEMS) {
        Serial.println("[Menu] Warning: Menu is full, cannot add item");
        return -1;
    }
    
    // Validate label
    if (label == nullptr || label[0] == '\0') {
        Serial.println("[Menu] Warning: Invalid label, cannot add item");
        return -1;
    }
    
    // Add the item
    int index = _itemCount;
    MenuItem& item = _items[index];
    
    strncpy(item.label, label, sizeof(item.label) - 1);
    item.label[sizeof(item.label) - 1] = '\0';
    item.callback = callback;
    item.userData = userData;
    item.enabled = enabled;
    
    _itemCount++;
    
    Serial.printf("[Menu] Added item %d: '%s'\n", index, label);
    return index;
}

// ============================================================================
// Visibility Control
// ============================================================================

void DropdownMenu::show() {
    if (!_visible) {
        _visible = true;
        _selectedIndex = 0;  // Reset selection when shown
        
        // Find first enabled item
        if (_itemCount > 0 && !_items[0].enabled) {
            int next = findNextEnabled(0);
            if (next >= 0) {
                _selectedIndex = next;
            }
        }
        
        Serial.println("[Menu] Shown");
    }
}

void DropdownMenu::hide() {
    if (_visible) {
        _visible = false;
        Serial.println("[Menu] Hidden");
    }
}

bool DropdownMenu::toggle() {
    if (_visible) {
        hide();
    } else {
        show();
    }
    return _visible;
}

bool DropdownMenu::isVisible() const {
    return _visible;
}

// ============================================================================
// Navigation
// ============================================================================

void DropdownMenu::selectNext() {
    if (_itemCount == 0) return;
    
    int next = findNextEnabled(_selectedIndex);
    if (next >= 0) {
        _selectedIndex = next;
        Serial.printf("[Menu] Selected: %d ('%s')\n", 
                      _selectedIndex, _items[_selectedIndex].label);
    }
}

void DropdownMenu::selectPrevious() {
    if (_itemCount == 0) return;
    
    int prev = findPrevEnabled(_selectedIndex);
    if (prev >= 0) {
        _selectedIndex = prev;
        Serial.printf("[Menu] Selected: %d ('%s')\n", 
                      _selectedIndex, _items[_selectedIndex].label);
    }
}

bool DropdownMenu::activateSelected() {
    if (_itemCount == 0 || _selectedIndex < 0 || _selectedIndex >= _itemCount) {
        return false;
    }
    
    MenuItem& item = _items[_selectedIndex];
    
    if (!item.enabled) {
        Serial.println("[Menu] Selected item is disabled");
        return false;
    }
    
    Serial.printf("[Menu] Activating: '%s'\n", item.label);
    
    if (item.callback != nullptr) {
        item.callback(_selectedIndex, item.userData);
    }
    
    return true;
}

// ============================================================================
// Query Methods
// ============================================================================

int DropdownMenu::getSelectedIndex() const {
    return _selectedIndex;
}

int DropdownMenu::getItemCount() const {
    return _itemCount;
}

const char* DropdownMenu::getItemLabel(int index) const {
    if (index < 0 || index >= _itemCount) {
        return "";
    }
    return _items[index].label;
}

bool DropdownMenu::isItemEnabled(int index) const {
    if (index < 0 || index >= _itemCount) {
        return false;
    }
    return _items[index].enabled;
}

void DropdownMenu::setSelectedIndex(int index) {
    if (index >= 0 && index < _itemCount) {
        _selectedIndex = index;
    }
}

// ============================================================================
// Helper Methods
// ============================================================================

int DropdownMenu::findNextEnabled(int fromIndex) const {
    if (_itemCount == 0) return -1;
    
    // Start from next item
    int index = (fromIndex + 1) % _itemCount;
    int checked = 0;
    
    while (checked < _itemCount) {
        if (_items[index].enabled) {
            return index;
        }
        index = (index + 1) % _itemCount;
        checked++;
    }
    
    // No enabled items found, return current if it's valid
    return (fromIndex >= 0 && fromIndex < _itemCount) ? fromIndex : -1;
}

int DropdownMenu::findPrevEnabled(int fromIndex) const {
    if (_itemCount == 0) return -1;
    
    // Start from previous item
    int index = (fromIndex - 1 + _itemCount) % _itemCount;
    int checked = 0;
    
    while (checked < _itemCount) {
        if (_items[index].enabled) {
            return index;
        }
        index = (index - 1 + _itemCount) % _itemCount;
        checked++;
    }
    
    // No enabled items found, return current if it's valid
    return (fromIndex >= 0 && fromIndex < _itemCount) ? fromIndex : -1;
}
