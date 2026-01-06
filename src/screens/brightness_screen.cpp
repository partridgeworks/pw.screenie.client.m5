/**
 * brightness_screen.cpp - Brightness Adjustment Screen implementation
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#include <M5Unified.h>
#include "screens/brightness_screen.h"
#include "screen_manager.h"
#include "persistence.h"
#include "config.h"
#include "sound.h"
#include <Arduino.h>

// ============================================================================
// Constructor
// ============================================================================

BrightnessScreen::BrightnessScreen(M5GFX& display)
    : _display(display)
    , _screenManager(nullptr)
    , _currentLevel(DEFAULT_BRIGHTNESS_LEVEL - 1)  // Convert to 0-indexed
{
}

void BrightnessScreen::setScreenManager(ScreenManager* manager) {
    _screenManager = manager;
}

// ============================================================================
// Screen Lifecycle
// ============================================================================

void BrightnessScreen::onEnter() {
    Serial.println("[BrightnessScreen] onEnter");
    
    // Load current brightness level from persistence
    _currentLevel = PersistenceManager::getInstance().loadBrightnessLevel();
    if (_currentLevel == 0 || _currentLevel > BRIGHTNESS_LEVEL_COUNT) {
        _currentLevel = DEFAULT_BRIGHTNESS_LEVEL;
    }
    // Convert to 0-indexed
    _currentLevel--;
    
    Serial.printf("[BrightnessScreen] Current level: %d (0-indexed)\n", _currentLevel);
    
    // Draw the screen
    draw();
}

void BrightnessScreen::onExit() {
    Serial.println("[BrightnessScreen] onExit");
    
    // Ensure brightness is saved when exiting
    saveBrightnessLevel();
}

void BrightnessScreen::onResume() {
    Serial.println("[BrightnessScreen] onResume");
    draw();
}

void BrightnessScreen::update() {
    // No animation or updates needed
}

void BrightnessScreen::draw() {
    _display.waitDisplay();
    _display.startWrite();
    
    drawBackground();
    drawTitle();
    drawChevrons();
    drawBrightnessIndicator();
    drawInstructions();
    
    _display.endWrite();
    _display.display();
}

// ============================================================================
// Input Handling
// ============================================================================

void BrightnessScreen::onButtonA() {
    Serial.println("[BrightnessScreen] Button A - exiting");
    exitScreen();
}

void BrightnessScreen::onButtonB() {
    Serial.println("[BrightnessScreen] Button B - cycling brightness");
    playButtonBeep();
    cycleBrightness();
}

void BrightnessScreen::onButtonPower() {
    Serial.println("[BrightnessScreen] Power - exiting");
    exitScreen();
}

void BrightnessScreen::onButtonPowerHold() {
    Serial.println("[BrightnessScreen] Power hold - power off");
    M5.Power.powerOff();
}

// ============================================================================
// Drawing Helpers
// ============================================================================

void BrightnessScreen::drawBackground() {
    _display.fillScreen(COLOR_BACKGROUND);
}

void BrightnessScreen::drawTitle() {
    // Draw title centered at top using FreeSansBold12pt7b
    _display.setTextColor(COLOR_TEXT_PRIMARY);
    _display.setTextSize(1);
    _display.setFont(&fonts::FreeSansBold12pt7b);
    
    const char* title = "Brightness";
    int textWidth = _display.textWidth(title);
    int textX = (SCREEN_WIDTH - textWidth) / 2;
    int textY = 10;  // Padding from top
    
    _display.setCursor(textX, textY + 18);  // FreeSansBold12pt7b has baseline offset
    _display.print(title);
}

void BrightnessScreen::drawChevrons() {
    // Draw left and right chevron-style arrows
    // Position: vertically centered, at left and right edges of screen
    
    int centerY = SCREEN_HEIGHT / 2;
    int chevronHeight = 24;
    int chevronWidth = 12;
    int strokeWidth = 2;
    
    uint16_t chevronColor = COLOR_ACCENT_PRIMARY;
    
    // Left chevron - pointing left (at left side of screen)
    int leftX = UI_PADDING + 8;
    // Draw two lines forming a < shape
    for (int i = 0; i < strokeWidth; i++) {
        _display.drawLine(leftX + chevronWidth + i, centerY - chevronHeight/2, 
                          leftX + i, centerY, chevronColor);
        _display.drawLine(leftX + i, centerY, 
                          leftX + chevronWidth + i, centerY + chevronHeight/2, chevronColor);
    }
    
    // Right chevron - pointing right (at right side of screen)
    int rightX = SCREEN_WIDTH - UI_PADDING - 8 - chevronWidth;
    // Draw two lines forming a > shape
    for (int i = 0; i < strokeWidth; i++) {
        _display.drawLine(rightX + i, centerY - chevronHeight/2, 
                          rightX + chevronWidth + i, centerY, chevronColor);
        _display.drawLine(rightX + chevronWidth + i, centerY, 
                          rightX + i, centerY + chevronHeight/2, chevronColor);
    }
}

void BrightnessScreen::drawBrightnessIndicator() {
    // Draw 4 horizontally stacked rectangles in the center
    // "On" levels are white, "Off" levels are dark gray
    
    int rectWidth = 28;
    int rectHeight = 28;
    int rectGap = 8;
    int totalWidth = (BRIGHTNESS_LEVEL_COUNT * rectWidth) + ((BRIGHTNESS_LEVEL_COUNT - 1) * rectGap);
    
    int startX = (SCREEN_WIDTH - totalWidth) / 2;
    int centerY = SCREEN_HEIGHT / 2;
    int rectY = centerY - rectHeight / 2;
    
    uint16_t colorOn = COLOR_TEXT_PRIMARY;  // White
    uint16_t colorOff = 0x3186;  // Dark gray (RGB: 48, 48, 48)
    
    for (int i = 0; i < BRIGHTNESS_LEVEL_COUNT; i++) {
        int rectX = startX + i * (rectWidth + rectGap);
        
        // Determine if this level is "on" (i.e., <= current level)
        bool isOn = (i <= _currentLevel);
        uint16_t fillColor = isOn ? colorOn : colorOff;
        
        _display.fillRoundRect(rectX, rectY, rectWidth, rectHeight, 4, fillColor);
    }
}

void BrightnessScreen::drawInstructions() {
    // Draw instruction text at bottom of screen
    int hintY = SCREEN_HEIGHT - UI_PADDING - 14;
    
    _display.setTextColor(COLOR_TEXT_MUTED);
    _display.setTextSize(1);
    _display.setFont(&fonts::Font2);
    
    const char* hint = "Press B to change brightness";
    int textWidth = _display.textWidth(hint);
    int textX = (SCREEN_WIDTH - textWidth) / 2;
    
    _display.setCursor(textX, hintY);
    _display.print(hint);
}

// ============================================================================
// Brightness Control
// ============================================================================

void BrightnessScreen::cycleBrightness() {
    // Cycle to next level (wraps around)
    _currentLevel = (_currentLevel + 1) % BRIGHTNESS_LEVEL_COUNT;
    
    Serial.printf("[BrightnessScreen] New level: %d\n", _currentLevel + 1);
    
    // Apply new brightness immediately
    applyBrightness();
    
    // Save to persistence
    saveBrightnessLevel();
    
    // Redraw indicator
    draw();
}

void BrightnessScreen::applyBrightness() {
    uint8_t brightnessValue = BRIGHTNESS_VALUES[_currentLevel];
    M5.Display.setBrightness(brightnessValue);
    
    Serial.printf("[BrightnessScreen] Applied brightness: %d (level %d)\n", 
                  brightnessValue, _currentLevel + 1);
}

void BrightnessScreen::saveBrightnessLevel() {
    // Save 1-indexed level to persistence
    uint8_t levelToSave = _currentLevel + 1;
    PersistenceManager::getInstance().saveBrightnessLevel(levelToSave);
    
    Serial.printf("[BrightnessScreen] Saved brightness level: %d\n", levelToSave);
}

// ============================================================================
// Static Methods
// ============================================================================

void BrightnessScreen::applyStoredBrightness() {
    uint8_t level = PersistenceManager::getInstance().loadBrightnessLevel();
    if (level == 0 || level > BRIGHTNESS_LEVEL_COUNT) {
        level = DEFAULT_BRIGHTNESS_LEVEL;
    }
    
    // Convert to 0-indexed for array access
    uint8_t brightnessValue = BRIGHTNESS_VALUES[level - 1];
    M5.Display.setBrightness(brightnessValue);
    
    Serial.printf("[BrightnessScreen] Applied stored brightness: %d (level %d)\n", 
                  brightnessValue, level);
}

uint8_t BrightnessScreen::getCurrentLevel() {
    uint8_t level = PersistenceManager::getInstance().loadBrightnessLevel();
    if (level == 0 || level > BRIGHTNESS_LEVEL_COUNT) {
        level = DEFAULT_BRIGHTNESS_LEVEL;
    }
    return level;
}

// ============================================================================
// Navigation
// ============================================================================

void BrightnessScreen::exitScreen() {
    if (_screenManager) {
        // Use navigateBack to return to the previous screen in history
        bool navigated = _screenManager->navigateBack();
        if (!navigated) {
            // Fallback: if no history, go to main screen
            Serial.println("[BrightnessScreen] No history, going to main screen");
            _screenManager->navigateTo(ScreenType::MAIN);
        }
    }
}
