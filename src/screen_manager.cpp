/**
 * screen_manager.cpp - Screen Manager implementation
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#include "screen_manager.h"
#include "dialog.h"
#include <Arduino.h>

// ============================================================================
// Constructor / Destructor
// ============================================================================

ScreenManager::ScreenManager(M5GFX& display)
    : _display(display)
    , _currentScreenType(ScreenType::NONE)
    , _historyIndex(-1)
    , _dialog(display)
{
    // Initialize all screen pointers to nullptr
    for (int i = 0; i < static_cast<int>(ScreenType::COUNT); i++) {
        _screens[i] = nullptr;
    }
    
    // Initialize history stack
    for (int i = 0; i < MAX_HISTORY_DEPTH; i++) {
        _history[i] = ScreenType::NONE;
    }
}

ScreenManager::~ScreenManager() {
    // Note: We don't own the screens, so we don't delete them
    // Just clear pointers for safety
    for (int i = 0; i < static_cast<int>(ScreenType::COUNT); i++) {
        _screens[i] = nullptr;
    }
}

// ============================================================================
// Initialization
// ============================================================================

void ScreenManager::begin() {
    Serial.println("[ScreenMgr] Initialized");
}

// ============================================================================
// Screen Registration
// ============================================================================

void ScreenManager::registerScreen(ScreenType type, Screen* screen) {
    if (type == ScreenType::NONE || type == ScreenType::COUNT) {
        Serial.println("[ScreenMgr] ERROR: Invalid screen type for registration");
        return;
    }
    
    int index = static_cast<int>(type);
    if (index >= 0 && index < static_cast<int>(ScreenType::COUNT)) {
        _screens[index] = screen;
        Serial.printf("[ScreenMgr] Registered screen type %d\n", index);
    }
}

// ============================================================================
// Navigation
// ============================================================================

void ScreenManager::navigateTo(ScreenType type) {
    if (type == ScreenType::NONE || type == ScreenType::COUNT) {
        Serial.println("[ScreenMgr] ERROR: Cannot navigate to NONE/COUNT");
        return;
    }
    
    int index = static_cast<int>(type);
    if (index < 0 || index >= static_cast<int>(ScreenType::COUNT)) {
        Serial.printf("[ScreenMgr] ERROR: Invalid screen type %d\n", index);
        return;
    }
    
    Screen* newScreen = _screens[index];
    if (newScreen == nullptr) {
        Serial.printf("[ScreenMgr] ERROR: Screen type %d not registered\n", index);
        return;
    }
    
    // Exit current screen if there is one
    Screen* currentScreen = getCurrentScreen();
    if (currentScreen != nullptr) {
        Serial.printf("[ScreenMgr] Exiting screen type %d\n", 
                      static_cast<int>(_currentScreenType));
        currentScreen->onExit();
        
        // Push current screen to history for back navigation
        pushHistory(_currentScreenType);
    }
    
    // Update current screen type
    _currentScreenType = type;
    Serial.printf("[ScreenMgr] Entering screen type %d\n", index);
    
    // Enter new screen
    newScreen->onEnter();
    
    // Draw the new screen
    newScreen->draw();
}

bool ScreenManager::navigateBack() {
    ScreenType previousType = popHistory();
    
    if (previousType == ScreenType::NONE) {
        Serial.println("[ScreenMgr] Cannot go back - history empty");
        return false;
    }
    
    int index = static_cast<int>(previousType);
    Screen* previousScreen = _screens[index];
    if (previousScreen == nullptr) {
        Serial.printf("[ScreenMgr] ERROR: Previous screen %d not registered\n", index);
        return false;
    }
    
    // Exit current screen
    Screen* currentScreen = getCurrentScreen();
    if (currentScreen != nullptr) {
        currentScreen->onExit();
    }
    
    // Update current screen type (don't push to history - we're going back)
    _currentScreenType = previousType;
    Serial.printf("[ScreenMgr] Navigated back to screen type %d\n", index);
    
    // Resume the previous screen
    previousScreen->onResume();
    
    // Draw the screen
    previousScreen->draw();
    
    return true;
}

ScreenType ScreenManager::getCurrentScreenType() const {
    return _currentScreenType;
}

Screen* ScreenManager::getCurrentScreen() const {
    if (_currentScreenType == ScreenType::NONE || 
        _currentScreenType == ScreenType::COUNT) {
        return nullptr;
    }
    
    int index = static_cast<int>(_currentScreenType);
    if (index >= 0 && index < static_cast<int>(ScreenType::COUNT)) {
        return _screens[index];
    }
    
    return nullptr;
}

// ============================================================================
// Per-Frame Updates
// ============================================================================

void ScreenManager::update() {
    // Don't update screen if a dialog is visible - dialog takes over
    if (_dialog.isVisible()) {
        return;
    }
    
    Screen* screen = getCurrentScreen();
    if (screen != nullptr) {
        screen->update();
    }
}

void ScreenManager::draw() {
    // Only draw if there's a dialog visible that needs redraw
    if (_dialog.isVisible() && _dialog.needsRedraw()) {
        _dialog.draw();
    }
    // Note: When no dialog, screens draw themselves incrementally via update()
}

// ============================================================================
// Input Routing
// ============================================================================

void ScreenManager::handleButtonA() {
    // Route to dialog first if visible
    if (_dialog.isVisible()) {
        _dialog.handleButtonA();
        
        // If dialog was dismissed, trigger onResume for current screen
        // and redraw BEFORE invoking callback (deferred callback pattern)
        if (_dialog.isDismissed()) {
            Screen* screen = getCurrentScreen();
            if (screen != nullptr) {
                screen->onResume();
                screen->draw();
            }
            
            // Now invoke the callback after screen has been redrawn
            // This prevents UI freeze when callback does blocking work
            if (_dialog.hasPendingCallback()) {
                _dialog.invokePendingCallback();
            }
        }
        return;
    }
    
    // Then route to current screen
    Screen* screen = getCurrentScreen();
    if (screen != nullptr) {
        screen->onButtonA();
    }
}

void ScreenManager::handleButtonB() {
    // Route to dialog first if visible
    if (_dialog.isVisible()) {
        _dialog.handleButtonB();
        return;
    }
    
    // Then route to current screen
    Screen* screen = getCurrentScreen();
    if (screen != nullptr) {
        screen->onButtonB();
    }
}

void ScreenManager::handleButtonPower() {
    // Dialogs don't handle power button - pass through
    Screen* screen = getCurrentScreen();
    if (screen != nullptr) {
        screen->onButtonPower();
    }
}

void ScreenManager::handleButtonPowerHold() {
    Screen* screen = getCurrentScreen();
    if (screen != nullptr) {
        screen->onButtonPowerHold();
    }
}

// ============================================================================
// Overlay Management (Dialogs)
// ============================================================================

Dialog& ScreenManager::getDialog() {
    return _dialog;
}

void ScreenManager::showInfoDialog(const char* title,
                                    const char* message,
                                    const char* buttonLabel,
                                    DialogCallback callback,
                                    void* userData) {
    _dialog.showInfo(title, message, buttonLabel, callback, userData);
    Serial.println("[ScreenMgr] Showing info dialog");
}

void ScreenManager::showConfirmDialog(const char* title,
                                       const char* message,
                                       const char* button1Label,
                                       const char* button2Label,
                                       DialogCallback callback,
                                       void* userData) {
    _dialog.showConfirm(title, message, button1Label, button2Label, callback, userData);
    Serial.println("[ScreenMgr] Showing confirm dialog");
}

void ScreenManager::dismissDialog() {
    if (_dialog.isVisible()) {
        _dialog.dismiss();
        
        // Trigger onResume for current screen
        Screen* screen = getCurrentScreen();
        if (screen != nullptr) {
            screen->onResume();
            screen->draw();
        }
        
        Serial.println("[ScreenMgr] Dialog dismissed");
    }
}

bool ScreenManager::hasActiveOverlay() const {
    // Check dialog and ask current screen about its menu
    if (_dialog.isVisible()) {
        return true;
    }
    
    Screen* screen = getCurrentScreen();
    if (screen != nullptr && screen->isMenuVisible()) {
        return true;
    }
    
    return false;
}

bool ScreenManager::isDialogVisible() const {
    return _dialog.isVisible();
}

// ============================================================================
// History Stack Management
// ============================================================================

void ScreenManager::pushHistory(ScreenType type) {
    if (type == ScreenType::NONE) {
        return;
    }
    
    if (_historyIndex < MAX_HISTORY_DEPTH - 1) {
        _historyIndex++;
        _history[_historyIndex] = type;
        Serial.printf("[ScreenMgr] Pushed to history, depth now %d\n", _historyIndex + 1);
    } else {
        // History full - shift everything down and add at end
        for (int i = 0; i < MAX_HISTORY_DEPTH - 1; i++) {
            _history[i] = _history[i + 1];
        }
        _history[MAX_HISTORY_DEPTH - 1] = type;
        Serial.println("[ScreenMgr] History full, shifted");
    }
}

ScreenType ScreenManager::popHistory() {
    if (_historyIndex < 0) {
        return ScreenType::NONE;
    }
    
    ScreenType type = _history[_historyIndex];
    _history[_historyIndex] = ScreenType::NONE;
    _historyIndex--;
    Serial.printf("[ScreenMgr] Popped from history, depth now %d\n", _historyIndex + 1);
    
    return type;
}
