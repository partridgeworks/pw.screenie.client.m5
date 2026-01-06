/**
 * sync_screen.cpp - Sync Progress Screen implementation
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#include <M5Unified.h>
#include "screens/sync_screen.h"
#include "screen_manager.h"
#include "config.h"
#include <Arduino.h>
#include <cmath>

// ============================================================================
// Constructor
// ============================================================================

SyncScreen::SyncScreen(M5GFX& display)
    : _display(display)
    , _screenManager(nullptr)
    , _progress(0.0f)
    , _showSpinner(true)
    , _showProgress(false)
    , _lastAnimationMs(0)
    , _spinnerFrame(0)
{
    strncpy(_message, "Loading...", sizeof(_message));
}

void SyncScreen::setScreenManager(ScreenManager* manager) {
    _screenManager = manager;
}

// ============================================================================
// Screen Lifecycle
// ============================================================================

void SyncScreen::onEnter() {
    Serial.println("[SyncScreen] onEnter");
    
    // Reset to default state
    reset();
    
    draw();
}

void SyncScreen::onExit() {
    Serial.println("[SyncScreen] onExit");
}

void SyncScreen::onResume() {
    Serial.println("[SyncScreen] onResume");
    draw();
}

void SyncScreen::update() {
    uint32_t now = millis();
    
    // Update spinner animation
    if (_showSpinner && (now - _lastAnimationMs >= SPINNER_INTERVAL_MS)) {
        _lastAnimationMs = now;
        _spinnerFrame = (_spinnerFrame + 1) % SPINNER_FRAMES;
        
        // Redraw just the spinner area
        _display.startWrite();
        drawSpinner();
        _display.endWrite();
        _display.display();
    }
}

void SyncScreen::draw() {
    _display.waitDisplay();
    _display.startWrite();
    
    drawBackground();
    
    if (_showSpinner && !_showProgress) {
        drawSpinner();
    }
    
    if (_showProgress) {
        drawProgressBar();
    }
    
    drawMessage();
    
    _display.endWrite();
    _display.display();
}

// ============================================================================
// Input Handling (mostly non-interactive)
// ============================================================================

void SyncScreen::onButtonA() {
    // No action - sync screen is non-interactive
}

void SyncScreen::onButtonB() {
    // No action
}

void SyncScreen::onButtonPower() {
    // No action
}

void SyncScreen::onButtonPowerHold() {
    // Allow power off even during sync
    Serial.println("[SyncScreen] Power hold - powering off");
    M5.Power.powerOff();
}

// ============================================================================
// State Management
// ============================================================================

void SyncScreen::setMessage(const char* message) {
    strncpy(_message, message, sizeof(_message) - 1);
    _message[sizeof(_message) - 1] = '\0';
    
    // Redraw message area
    _display.startWrite();
    // Clear message area
    _display.fillRect(0, MESSAGE_Y - 5, SCREEN_WIDTH, 30, COLOR_BACKGROUND);
    drawMessage();
    _display.endWrite();
    _display.display();
}

void SyncScreen::setProgress(float progress) {
    _progress = progress;
    if (_progress < 0.0f) _progress = 0.0f;
    if (_progress > 1.0f) _progress = 1.0f;
    
    _showProgress = true;
    _showSpinner = false;
    
    // Redraw progress bar
    _display.startWrite();
    drawProgressBar();
    _display.endWrite();
    _display.display();
}

void SyncScreen::showSpinner(bool show) {
    _showSpinner = show;
    if (show) {
        _showProgress = false;
    }
    draw();
}

void SyncScreen::reset() {
    strncpy(_message, "Loading...", sizeof(_message));
    _progress = 0.0f;
    _showSpinner = true;
    _showProgress = false;
    _spinnerFrame = 0;
    _lastAnimationMs = millis();
}

// ============================================================================
// Drawing Methods
// ============================================================================

void SyncScreen::drawBackground() {
    _display.fillScreen(COLOR_BACKGROUND);
}

void SyncScreen::drawSpinner() {
    // Draw a rotating arc spinner
    int centerX = SCREEN_WIDTH / 2;
    int centerY = SPINNER_CENTER_Y;
    
    // Clear spinner area
    _display.fillCircle(centerX, centerY, SPINNER_RADIUS + 5, COLOR_BACKGROUND);
    
    // Draw spinner track (faint circle)
    _display.drawCircle(centerX, centerY, SPINNER_RADIUS, COLOR_BORDER);
    
    // Draw active arc segment
    // Calculate arc position based on frame
    float startAngle = (_spinnerFrame * 360.0f / SPINNER_FRAMES) * DEG_TO_RAD;
    float arcLength = 90.0f * DEG_TO_RAD;  // 90 degree arc
    
    // Draw thick arc by drawing multiple circles at different positions
    for (int i = 0; i < 6; i++) {
        float angle = startAngle + (arcLength * i / 5);
        int x = centerX + (int)(SPINNER_RADIUS * cos(angle));
        int y = centerY + (int)(SPINNER_RADIUS * sin(angle));
        
        // Gradient from bright to dim
        uint16_t color = (i < 3) ? COLOR_ACCENT_PRIMARY : COLOR_ACCENT_PRIMARY;
        int radius = (i == 0 || i == 1) ? 4 : 3;
        
        _display.fillCircle(x, y, radius, color);
    }
}

void SyncScreen::drawProgressBar() {
    int barX = (SCREEN_WIDTH - PROGRESS_WIDTH) / 2;
    int barY = PROGRESS_Y;
    
    // Draw background
    _display.fillRoundRect(barX, barY, PROGRESS_WIDTH, PROGRESS_HEIGHT, 
                           PROGRESS_HEIGHT / 2, COLOR_PROGRESS_BG);
    
    // Draw fill
    int fillWidth = (int)(PROGRESS_WIDTH * _progress);
    if (fillWidth > 0) {
        _display.fillRoundRect(barX, barY, fillWidth, PROGRESS_HEIGHT,
                               PROGRESS_HEIGHT / 2, COLOR_PROGRESS_FILL);
    }
    
    // Draw border
    _display.drawRoundRect(barX, barY, PROGRESS_WIDTH, PROGRESS_HEIGHT,
                           PROGRESS_HEIGHT / 2, COLOR_BORDER);
}

void SyncScreen::drawMessage() {
    _display.setTextColor(COLOR_TEXT_PRIMARY);
    _display.setFont(&fonts::Font2);
    _display.setTextSize(1);
    
    int textWidth = _display.textWidth(_message);
    int textX = (SCREEN_WIDTH - textWidth) / 2;
    
    _display.setCursor(textX, MESSAGE_Y);
    _display.print(_message);
}
