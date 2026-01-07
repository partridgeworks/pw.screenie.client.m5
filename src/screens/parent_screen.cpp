/**
 * parent_screen.cpp - Parent Screen implementation
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#include <M5Unified.h>
#include "screens/parent_screen.h"
#include "screen_manager.h"
#include "persistence.h"
#include "app_state.h"
#include "network.h"
#include "config.h"
#include "sound.h"
#include "timer.h"
#include <Arduino.h>

// Define the static constexpr array
constexpr uint8_t ParentScreen::EXPECTED_SEQUENCE[SEQUENCE_LENGTH];

// ============================================================================
// Constructor
// ============================================================================

ParentScreen::ParentScreen(M5GFX& display, UI& ui, ScreenTimer& timer)
    : _display(display)
    , _ui(ui)
    , _timer(timer)
    , _screenManager(nullptr)
    , _networkManager(nullptr)
    , _isUnlocked(false)
    , _sequenceIndex(0)
{
}

void ParentScreen::setScreenManager(ScreenManager* manager) {
    _screenManager = manager;
}

void ParentScreen::setNetworkManager(NetworkManager* network) {
    _networkManager = network;
}

// ============================================================================
// Screen Lifecycle
// ============================================================================

void ParentScreen::onEnter() {
    Serial.println("[ParentScreen] onEnter");
    
    // Reset state
    _isUnlocked = false;
    _sequenceIndex = 0;
    
    // Clear any existing menu
    destroyMenu();
    
    // Draw the screen
    draw();
}

void ParentScreen::onExit() {
    Serial.println("[ParentScreen] onExit");
    
    // Clean up menu
    _menu.hide();
    destroyMenu();
}

void ParentScreen::onResume() {
    Serial.println("[ParentScreen] onResume");
    
    // Redraw the screen
    draw();
}

void ParentScreen::update() {
    // No animation or updates needed
}

void ParentScreen::draw() {
    _display.waitDisplay();
    _display.startWrite();
    
    drawBackground();
    drawHeader();
    drawTitle();
    drawInstruction();
    drawHint();
    
    _display.endWrite();
    _display.display();
    
    // Draw menu on top if visible
    if (_menu.isVisible()) {
        drawMenu();
    }
}

// ============================================================================
// Input Handling
// ============================================================================

void ParentScreen::onButtonA() {
    if (_menu.isVisible()) {
        // Menu visible - activate selected item
        Serial.println("[ParentScreen] Button A - activating menu item");
        
        // Flash the selected item
        _ui.flashMenuItem(_menu, _menu.getSelectedIndex());
        M5.delay(MENU_FLASH_DURATION_MS);
        
        // Activate the item
        _menu.activateSelected();
        
        // Redraw menu
        drawMenu();
    } else if (!_isUnlocked) {
        // Not unlocked - process as sequence input
        processSequenceButton(0);  // 0 = A
    }
}

void ParentScreen::onButtonB() {
    if (_menu.isVisible()) {
        // Menu visible - cycle to next item
        _menu.selectNext();
        drawMenu();
        Serial.printf("[ParentScreen] Button B - selected item %d\n", 
                      _menu.getSelectedIndex());
    } else if (_isUnlocked) {
        // Unlocked - open the menu
        _menu.show();
        drawMenu();
        Serial.println("[ParentScreen] Button B - opening menu");
    } else {
        // Not unlocked - process as sequence input
        processSequenceButton(1);  // 1 = B
    }
}

void ParentScreen::onButtonPower() {
    Serial.println("[ParentScreen] Power - going back");
    
    // Close menu if open
    if (_menu.isVisible()) {
        _menu.hide();
        draw();
        return;
    }
    
    // Always go back on power button
    exitScreen();
}

void ParentScreen::onButtonPowerHold() {
    Serial.println("[ParentScreen] Power hold - power off");
    M5.Power.powerOff();
}

// ============================================================================
// Sequence Handling
// ============================================================================

void ParentScreen::processSequenceButton(uint8_t button) {
    Serial.printf("[ParentScreen] Sequence input: %c (index %d)\n", 
                  button == 0 ? 'A' : 'B', _sequenceIndex);
    
    // Check if this button matches the expected sequence
    if (button == EXPECTED_SEQUENCE[_sequenceIndex]) {
        // Correct button - advance
        _sequenceIndex++;
        
        Serial.printf("[ParentScreen] Correct! Progress: %d/%d\n", 
                      _sequenceIndex, SEQUENCE_LENGTH);
        
        // Check if sequence is complete
        if (_sequenceIndex >= SEQUENCE_LENGTH) {
            onUnlocked();
        }
    } else {
        // Wrong button - reset sequence
        Serial.println("[ParentScreen] Wrong button - resetting sequence");
        _sequenceIndex = 0;
        
        // Check if this button starts a new valid sequence
        if (button == EXPECTED_SEQUENCE[0]) {
            _sequenceIndex = 1;
        }
    }
}

void ParentScreen::onUnlocked() {
    Serial.println("[ParentScreen] UNLOCKED!");
    
    _isUnlocked = true;
    _sequenceIndex = 0;  // Reset for safety
    
    // Play success sound
    playButtonBeep();
    
    // Set up the parent menu
    setupMenu();
    
    // Redraw screen with new state
    draw();
}

// ============================================================================
// Menu Setup
// ============================================================================

void ParentScreen::setupMenu() {
    _menu.clear();
    
    // Add parent menu items
    _menu.addItem("Reset time", onResetTimeSelected, this, true);
    
    // Only show Change child and Logout if logged in
    AppState& appState = AppState::getInstance();
    if (appState.isLoggedIn()) {
        _menu.addItem("Change child", onChangeChildSelected, this, true);
        _menu.addItem("Wipe & Reset", onLogoutSelected, this, true);
    }
    
    Serial.printf("[ParentScreen] Menu initialized with %d items\n", _menu.getItemCount());
}

void ParentScreen::destroyMenu() {
    _menu.clear();
    Serial.println("[ParentScreen] Menu cleared");
}

// ============================================================================
// Drawing Helpers
// ============================================================================

void ParentScreen::drawBackground() {
    _display.fillScreen(COLOR_BACKGROUND);
}

void ParentScreen::drawHeader() {
    // Use the shared header drawing from UI
    NetworkStatus networkStatus = AppState::getInstance().getNetworkStatus();
    _ui.drawStandardHeader("Parent", networkStatus);
}

void ParentScreen::drawTitle() {
    // Title text centered below header
    int titleY = HEADER_HEIGHT + 20;
    
    _display.setTextColor(COLOR_TEXT_PRIMARY);
    _display.setTextSize(1);
    _display.setFont(&fonts::Font2);
    
    const char* title = "Parent screen";
    int textWidth = _display.textWidth(title);
    int textX = (SCREEN_WIDTH - textWidth) / 2;
    
    _display.setCursor(textX, titleY);
    _display.print(title);
}

void ParentScreen::drawInstruction() {
    // Instruction centered in middle of content area
    int instructionY = HEADER_HEIGHT + 50;
    
    // Clear the instruction area first (for updates after unlock)
    _display.fillRect(0, instructionY - 2, SCREEN_WIDTH, 20, COLOR_BACKGROUND);
    
    _display.setTextColor(COLOR_TEXT_SECONDARY);
    _display.setTextSize(1);
    _display.setFont(&fonts::Font2);
    
    const char* instruction = _isUnlocked ? "Menu Unlocked" : "Password to unlock.";
    int textWidth = _display.textWidth(instruction);
    int textX = (SCREEN_WIDTH - textWidth) / 2;
    
    _display.setCursor(textX, instructionY);
    _display.print(instruction);
}

void ParentScreen::drawHint() {
    // Hint at bottom of screen (smaller font)
    int hintY = SCREEN_HEIGHT - UI_PADDING - 12;
    
    _display.setTextColor(COLOR_TEXT_MUTED);
    _display.setTextSize(1);
    _display.setFont(&fonts::Font0);
    
    const char* hint = "Button C to go back.";
    int textWidth = _display.textWidth(hint);
    int textX = (SCREEN_WIDTH - textWidth) / 2;
    
    _display.setCursor(textX, hintY);
    _display.print(hint);
}

void ParentScreen::drawMenu() {
    _ui.drawMenu(_menu);
}

// ============================================================================
// Menu Callbacks (static)
// ============================================================================

void ParentScreen::onResetTimeSelected(int itemIndex, void* userData) {
    ParentScreen* self = static_cast<ParentScreen*>(userData);
    if (self) self->handleResetTime();
}

void ParentScreen::onChangeChildSelected(int itemIndex, void* userData) {
    ParentScreen* self = static_cast<ParentScreen*>(userData);
    if (self) self->navigateToChangeChild();
}

void ParentScreen::onLogoutSelected(int itemIndex, void* userData) {
    ParentScreen* self = static_cast<ParentScreen*>(userData);
    if (self) self->handleLogout();
}

// ============================================================================
// Menu Actions
// ============================================================================

void ParentScreen::handleResetTime() {
    Serial.println("[ParentScreen] Reset time activated");
    
    // Reset consumed time in timer
    _timer.setConsumedTodaySeconds(0);
    
    // Save to persistence
    uint8_t weekday = AppState::getInstance().getCurrentWeekday();
    PersistenceManager::getInstance().saveConsumedToday(0, weekday);
    
    Serial.println("[ParentScreen] Consumed time reset to 0");
    
    // Show confirmation notification
    _ui.showNotification("Time reset", 1500);
    
    // Hide the menu
    _menu.hide();
    
    // Wait for notification and redraw
    M5.delay(1500);
    draw();
}

void ParentScreen::navigateToChangeChild() {
    Serial.println("[ParentScreen] Navigating to Select Child screen");
    
    if (_screenManager) {
        _screenManager->navigateTo(ScreenType::SELECT_CHILD);
    }
}

void ParentScreen::handleLogout() {
    Serial.println("[ParentScreen] Logout activated");
    
    // Clear persistence and reset state
    AppState& appState = AppState::getInstance();
    appState.clearPersistence();

    _ui.showNotification("Logging out", 0);
    
    // Clear NTP Sync Time and Resync
    PersistenceManager::getInstance().saveLastNtpSyncTime(0);
    Serial.println("[ParentScreen] Cleared NTP sync time");
    
    if (_networkManager) {
        if (_networkManager->ensureConnected()) {
            Serial.println("[ParentScreen] Re-syncing NTP time...");
            _networkManager->syncTimeAndSetRTC(true);  // Force sync
        } else {
            Serial.println("[ParentScreen] Could not connect for NTP resync");
        }
    }
    
    // Debug print to verify cleared
    PersistenceManager::getInstance().debugPrint();
    
    _ui.showNotification("Logged out", 1000);
    M5.delay(1000);
    
    // Navigate to login screen
    if (_screenManager) {
        _screenManager->navigateTo(ScreenType::LOGIN);
    }
}

// ============================================================================
// Navigation
// ============================================================================

void ParentScreen::exitScreen() {
    if (_screenManager) {
        // Use navigateBack to return to the previous screen
        bool navigated = _screenManager->navigateBack();
        if (!navigated) {
            // Fallback: go to main screen
            Serial.println("[ParentScreen] No history, going to main screen");
            _screenManager->navigateTo(ScreenType::MAIN);
        }
    }
}
