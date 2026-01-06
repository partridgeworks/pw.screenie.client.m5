/**
 * system_info_screen.cpp - System Info Screen implementation
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#include <M5Unified.h>
#include "screens/system_info_screen.h"
#include "screen_manager.h"
#include "config.h"
#include <Arduino.h>

// ============================================================================
// Constructor
// ============================================================================

SystemInfoScreen::SystemInfoScreen(M5GFX& display)
    : _display(display)
    , _screenManager(nullptr)
    , _batteryLevel(0)
    , _batteryVoltage(0)
{
}

void SystemInfoScreen::setScreenManager(ScreenManager* manager) {
    _screenManager = manager;
}

// ============================================================================
// Screen Lifecycle
// ============================================================================

void SystemInfoScreen::onEnter() {
    Serial.println("[SystemInfoScreen] onEnter");
    
    // Read battery info on enter
    _batteryLevel = M5.Power.getBatteryLevel();
    _batteryVoltage = M5.Power.getBatteryVoltage();
    
    Serial.printf("[SystemInfoScreen] Battery: %d%%, %dmV\n", 
                  _batteryLevel, _batteryVoltage);
    
    // Draw the screen
    draw();
}

void SystemInfoScreen::onExit() {
    Serial.println("[SystemInfoScreen] onExit");
}

void SystemInfoScreen::onResume() {
    Serial.println("[SystemInfoScreen] onResume");
    draw();
}

void SystemInfoScreen::update() {
    // No animation or updates needed
}

void SystemInfoScreen::draw() {
    _display.waitDisplay();
    _display.startWrite();
    
    drawBackground();
    drawTitle();
    drawBatteryInfo();
    drawVersionInfo();
    drawExitHint();
    
    _display.endWrite();
    _display.display();
}

// ============================================================================
// Input Handling - Any button exits
// ============================================================================

void SystemInfoScreen::onButtonA() {
    Serial.println("[SystemInfoScreen] Button A - exiting");
    exitScreen();
}

void SystemInfoScreen::onButtonB() {
    Serial.println("[SystemInfoScreen] Button B - exiting");
    exitScreen();
}

void SystemInfoScreen::onButtonPower() {
    Serial.println("[SystemInfoScreen] Power - exiting");
    exitScreen();
}

void SystemInfoScreen::onButtonPowerHold() {
    Serial.println("[SystemInfoScreen] Power hold - power off");
    M5.Power.powerOff();
}

// ============================================================================
// Drawing Helpers
// ============================================================================

void SystemInfoScreen::drawBackground() {
    _display.fillScreen(COLOR_BACKGROUND);
}

void SystemInfoScreen::drawTitle() {
    // Draw header bar with title
    _display.fillRect(0, HEADER_Y, SCREEN_WIDTH, HEADER_HEIGHT, COLOR_HEADER_BG);
    _display.fillRect(0, HEADER_HEIGHT, SCREEN_WIDTH, 1, 0xCE59);  // Light gray line
    
    // Title text centered
    _display.setTextColor(COLOR_TEXT_PRIMARY);
    _display.setTextSize(1);
    _display.setFont(&fonts::Font2);
    
    const char* title = "System Info";
    int textWidth = _display.textWidth(title);
    int textX = (SCREEN_WIDTH - textWidth) / 2;
    int textY = HEADER_Y + (HEADER_HEIGHT - 12) / 2 - 1;
    
    _display.setCursor(textX, textY);
    _display.print(title);
}

void SystemInfoScreen::drawBatteryInfo() {
    // Content area starts below header
    int contentY = HEADER_HEIGHT + 12;
    int leftMargin = UI_PADDING + 4;
    int valueX = 140;  // Right-align values
    
    _display.setTextColor(COLOR_TEXT_SECONDARY);
    _display.setTextSize(1);
    _display.setFont(&fonts::Font2);
    
    // Battery Voltage label
    _display.setCursor(leftMargin, contentY);
    _display.print("Battery Voltage:");
    
    // Battery Voltage value
    _display.setTextColor(COLOR_TEXT_PRIMARY);
    char voltageStr[16];
    float voltageV = _batteryVoltage / 1000.0f;
    snprintf(voltageStr, sizeof(voltageStr), "%.2f V", voltageV);
    _display.setCursor(valueX, contentY);
    _display.print(voltageStr);
    
    // Battery Percentage label
    contentY += 20;
    _display.setTextColor(COLOR_TEXT_SECONDARY);
    _display.setCursor(leftMargin, contentY);
    _display.print("Battery Level:");
    
    // Battery Percentage value
    _display.setTextColor(COLOR_TEXT_PRIMARY);
    char levelStr[16];
    if (_batteryLevel >= 0) {
        snprintf(levelStr, sizeof(levelStr), "%d%%", _batteryLevel);
    } else {
        snprintf(levelStr, sizeof(levelStr), "N/A");
    }
    _display.setCursor(valueX, contentY);
    _display.print(levelStr);
}

void SystemInfoScreen::drawVersionInfo() {
    // Position below battery info
    int contentY = HEADER_HEIGHT + 12 + 20 + 20;
    int leftMargin = UI_PADDING + 4;
    int valueX = 140;
    
    // App Version label
    _display.setTextColor(COLOR_TEXT_SECONDARY);
    _display.setFont(&fonts::Font2);
    _display.setCursor(leftMargin, contentY);
    _display.print("App Version:");
    
    // App Version value
    _display.setTextColor(COLOR_TEXT_PRIMARY);
    _display.setCursor(valueX, contentY);
    _display.print(APP_VERSION);
}

void SystemInfoScreen::drawExitHint() {
    // Draw hint at bottom of screen
    int hintY = SCREEN_HEIGHT - UI_PADDING - 12;
    
    _display.setTextColor(COLOR_TEXT_MUTED);
    _display.setTextSize(1);
    _display.setFont(&fonts::Font0);
    
    const char* hint = "Press any button to exit";
    int textWidth = _display.textWidth(hint);
    int textX = (SCREEN_WIDTH - textWidth) / 2;
    
    _display.setCursor(textX, hintY);
    _display.print(hint);
}

// ============================================================================
// Navigation
// ============================================================================

void SystemInfoScreen::exitScreen() {
    if (_screenManager) {
        // Use navigateBack to return to the previous screen in history
        bool navigated = _screenManager->navigateBack();
        if (!navigated) {
            // Fallback: if no history, go to main screen
            Serial.println("[SystemInfoScreen] No history, going to main screen");
            _screenManager->navigateTo(ScreenType::MAIN);
        }
    }
}
