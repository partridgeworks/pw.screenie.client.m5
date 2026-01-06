/**
 * settings_screen.cpp - Settings Screen implementation
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#include <M5Unified.h>
#include "screens/settings_screen.h"
#include "screen_manager.h"
#include "persistence.h"
#include "app_state.h"
#include "config.h"
#include "sound.h"
#include <Arduino.h>

// ============================================================================
// Constructor
// ============================================================================

SettingsScreen::SettingsScreen(M5GFX& display, UI& ui)
    : _display(display)
    , _ui(ui)
    , _screenManager(nullptr)
{
}

void SettingsScreen::setScreenManager(ScreenManager* manager) {
    _screenManager = manager;
}

// ============================================================================
// Screen Lifecycle
// ============================================================================

void SettingsScreen::onEnter() {
    Serial.println("[SettingsScreen] onEnter");
    
    // Set up the menu
    setupMenu();
    
    // Draw full screen with header
    drawBackground();
    drawHeader();
    drawPlaceholderText();
    
    // Show the menu immediately
    _menu.show();
    drawMenu();
}

void SettingsScreen::onExit() {
    Serial.println("[SettingsScreen] onExit");
    
    // Ensure menu is hidden
    _menu.hide();
    
    // Clean up menu
    destroyMenu();
}

void SettingsScreen::onResume() {
    Serial.println("[SettingsScreen] onResume");
    
    // Re-setup the menu if coming back
    if (_menu.getItemCount() == 0) {
        setupMenu();
    }
    
    // Show full screen with header and menu again
    drawBackground();
    drawHeader();
    drawPlaceholderText();
    _menu.show();
    drawMenu();
}

void SettingsScreen::update() {
    // No animation or updates needed
}

void SettingsScreen::draw() {
    drawBackground();
    drawHeader();
    drawPlaceholderText();
    
    if (_menu.isVisible()) {
        drawMenu();
    }
}

// ============================================================================
// Input Handling
// ============================================================================

void SettingsScreen::onButtonA() {
    if (_menu.isVisible()) {
        // Menu visible - activate selected item
        Serial.println("[SettingsScreen] Button A - activating menu item");
        
        // Flash the selected item
        _ui.flashMenuItem(_menu, _menu.getSelectedIndex());
        M5.delay(MENU_FLASH_DURATION_MS);
        
        // Activate the item - this may navigate away
        _menu.activateSelected();
        
        // Check if we're still the active screen
        if (_screenManager && 
            _screenManager->getCurrentScreenType() != ScreenType::SETTINGS) {
            // Screen changed - don't redraw
            return;
        }
        
        // Still on this screen - redraw menu
        drawMenu();
    }
}

void SettingsScreen::onButtonB() {
    if (_menu.isVisible()) {
        // Menu visible - cycle to next item
        _menu.selectNext();
        drawMenu();
        Serial.printf("[SettingsScreen] Button B - selected item %d\n", 
                      _menu.getSelectedIndex());
    } else {
        // Menu not visible (shouldn't happen) - show it
        _menu.show();
        drawMenu();
    }
}

void SettingsScreen::onButtonPower() {
    Serial.println("[SettingsScreen] Power - going back");
    exitScreen();
}

void SettingsScreen::onButtonPowerHold() {
    Serial.println("[SettingsScreen] Power hold - power off");
    M5.Power.powerOff();
}

// ============================================================================
// Menu Setup
// ============================================================================

void SettingsScreen::setupMenu() {
    _menu.clear();
    
    // Add settings menu items - always available
    _menu.addItem("Brightness", onBrightnessSelected, this, true);
    _menu.addItem("System Info", onSystemInfoSelected, this, true);
    
    // Power off is always available
    _menu.addItem("Power off", onPowerOffSelected, this, true);
    
    Serial.printf("[SettingsScreen] Menu initialized with %d items\n", _menu.getItemCount());
}

void SettingsScreen::destroyMenu() {
    _menu.clear();
    Serial.println("[SettingsScreen] Menu cleared");
}

// ============================================================================
// Drawing Helpers
// ============================================================================

void SettingsScreen::drawBackground() {
    _display.waitDisplay();
    _display.startWrite();
    _display.fillScreen(COLOR_BACKGROUND);
    _display.endWrite();
}

void SettingsScreen::drawHeader() {
    // Use the shared header drawing from UI
    NetworkStatus networkStatus = AppState::getInstance().getNetworkStatus();
    _ui.drawStandardHeader("Settings", networkStatus);
}

void SettingsScreen::drawPlaceholderText() {
    // Draw very dim placeholder text in center
    // This should never be visible in normal use
    _display.startWrite();
    
    _display.setTextColor(0x2104);  // Very dark gray, barely visible
    _display.setTextSize(1);
    _display.setFont(&fonts::Font2);
    
    const char* text = "SETTINGS";
    int textWidth = _display.textWidth(text);
    int textX = (SCREEN_WIDTH - textWidth) / 2;
    int textY = (SCREEN_HEIGHT - 12) / 2;
    
    _display.setCursor(textX, textY);
    _display.print(text);
    
    _display.endWrite();
    _display.display();
}

void SettingsScreen::drawMenu() {
    _ui.drawMenu(_menu);
}

// ============================================================================
// Menu Callbacks (static)
// ============================================================================

void SettingsScreen::onBrightnessSelected(int itemIndex, void* userData) {
    SettingsScreen* self = static_cast<SettingsScreen*>(userData);
    if (self) self->navigateToBrightness();
}

void SettingsScreen::onSystemInfoSelected(int itemIndex, void* userData) {
    SettingsScreen* self = static_cast<SettingsScreen*>(userData);
    if (self) self->navigateToSystemInfo();
}

void SettingsScreen::onPowerOffSelected(int itemIndex, void* userData) {
    SettingsScreen* self = static_cast<SettingsScreen*>(userData);
    if (self) self->handlePowerOff();
}

void SettingsScreen::onPowerOffDialogResult(DialogResult result, void* userData) {
    if (result == DialogResult::BUTTON_2) {
        // User selected "Power off"
        Serial.println("[SettingsScreen] Power off confirmed");
        M5.Power.powerOff();
    }
    // BUTTON_1 (Cancel) - do nothing, dialog closes automatically
}

// ============================================================================
// Menu Actions
// ============================================================================

void SettingsScreen::navigateToBrightness() {
    Serial.println("[SettingsScreen] Navigating to Brightness screen");
    
    if (_screenManager) {
        _screenManager->navigateTo(ScreenType::BRIGHTNESS);
    }
}

void SettingsScreen::navigateToSystemInfo() {
    Serial.println("[SettingsScreen] Navigating to System Info screen");
    
    if (_screenManager) {
        _screenManager->navigateTo(ScreenType::SYSTEM_INFO);
    }
}

void SettingsScreen::handlePowerOff() {
    Serial.println("[SettingsScreen] Power off requested - showing confirmation");
    
    if (_screenManager) {
        _screenManager->showConfirmDialog(
            "Are you sure?",
            "This app does not need to be powered off, except for long-term storage.",
            "Cancel",      // Button 1 (default)
            "Power off",   // Button 2
            SettingsScreen::onPowerOffDialogResult,
            this
        );
    }
}

// ============================================================================
// Navigation
// ============================================================================

void SettingsScreen::exitScreen() {
    if (_screenManager) {
        // Use navigateBack to return to the previous screen
        bool navigated = _screenManager->navigateBack();
        if (!navigated) {
            // Fallback: go to main screen
            Serial.println("[SettingsScreen] No history, going to main screen");
            _screenManager->navigateTo(ScreenType::MAIN);
        }
    }
}
