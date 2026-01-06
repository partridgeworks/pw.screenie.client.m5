/**
 * dialog.cpp - Dialog System implementation
 * 
 * Implements full-screen dialog overlays with button navigation.
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#include "dialog.h"
#include "config.h"
#include <Arduino.h>
#include <cstring>

// ============================================================================
// Dialog Layout Constants
// ============================================================================

// Dialog margins and padding
static constexpr int DIALOG_MARGIN = 12;
static constexpr int DIALOG_PADDING = 8;
static constexpr int DIALOG_CORNER_RADIUS = 6;

// Title bar
static constexpr int DIALOG_TITLE_HEIGHT = 28;

// Buttons
static constexpr int DIALOG_BUTTON_HEIGHT = 24;
static constexpr int DIALOG_BUTTON_MARGIN = 6;
static constexpr int DIALOG_BUTTON_SPACING = 8;
static constexpr int DIALOG_BUTTON_MIN_WIDTH = 50;
static constexpr int DIALOG_BUTTON_CORNER_RADIUS = 4;

// Progress bar
static constexpr int DIALOG_PROGRESS_HEIGHT = 10;
static constexpr int DIALOG_PROGRESS_MARGIN = 20;

// Spinner animation
static constexpr int DIALOG_SPINNER_SIZE = 16;
static constexpr uint32_t DIALOG_SPINNER_INTERVAL_MS = 100;

// Colors (using existing palette from config.h)
static constexpr uint16_t DIALOG_BG_COLOR = COLOR_HEADER_BG;
static constexpr uint16_t DIALOG_BORDER_COLOR = COLOR_BORDER;
static constexpr uint16_t DIALOG_TITLE_BG_COLOR = COLOR_ACCENT_DANGER;  // Default red for alerts
static constexpr uint16_t DIALOG_TEXT_COLOR = TFT_WHITE;
static constexpr uint16_t DIALOG_TEXT_SECONDARY_COLOR = TFT_WHITE;
static constexpr uint16_t DIALOG_BUTTON_BG_COLOR = COLOR_ACCENT_PRIMARY;
static constexpr uint16_t DIALOG_BUTTON_SELECTED_COLOR = COLOR_ACCENT_SUCCESS;
static constexpr uint16_t DIALOG_BUTTON_TEXT_COLOR = COLOR_TEXT_PRIMARY;
static constexpr uint16_t DIALOG_PROGRESS_BG_COLOR = COLOR_PROGRESS_BG;
static constexpr uint16_t DIALOG_PROGRESS_FILL_COLOR = COLOR_ACCENT_PRIMARY;
static constexpr uint16_t DIALOG_HINT_COLOR = COLOR_TEXT_MUTED;

// ============================================================================
// Constructor
// ============================================================================

Dialog::Dialog(M5GFX& display)
    : _display(display)
    , _buttonCount(0)
    , _type(DialogType::INFO)
    , _visible(false)
    , _dismissed(false)
    , _selectedButton(0)
    , _result(DialogResult::NONE)
    , _callback(nullptr)
    , _userData(nullptr)
    , _progress(-1.0f)
    , _spinnerFrame(0)
    , _lastSpinnerUpdateMs(0)
    , _needsRedraw(false)
    , _hasPendingCallback(false)
    , _pendingResult(DialogResult::NONE)
{
    _title[0] = '\0';
    _message[0] = '\0';
    for (int i = 0; i < DIALOG_MAX_BUTTONS; i++) {
        _buttons[i][0] = '\0';
    }
}

// ============================================================================
// Factory Methods
// ============================================================================

void Dialog::showInfo(const char* title,
                      const char* message,
                      const char* buttonLabel,
                      DialogCallback callback,
                      void* userData) {
    // Store content
    strncpy(_title, title, DIALOG_MAX_TITLE_LEN - 1);
    _title[DIALOG_MAX_TITLE_LEN - 1] = '\0';
    
    strncpy(_message, message, DIALOG_MAX_MESSAGE_LEN - 1);
    _message[DIALOG_MAX_MESSAGE_LEN - 1] = '\0';
    
    strncpy(_buttons[0], buttonLabel, DIALOG_MAX_BUTTON_LABEL_LEN - 1);
    _buttons[0][DIALOG_MAX_BUTTON_LABEL_LEN - 1] = '\0';
    
    _buttonCount = 1;
    _type = DialogType::INFO;
    _visible = true;
    _dismissed = false;
    _selectedButton = 0;
    _result = DialogResult::NONE;
    _callback = callback;
    _userData = userData;
    _needsRedraw = true;
    
    Serial.printf("[Dialog] showInfo: '%s'\n", title);
}

void Dialog::showConfirm(const char* title,
                         const char* message,
                         const char* button1Label,
                         const char* button2Label,
                         DialogCallback callback,
                         void* userData) {
    // Store content
    strncpy(_title, title, DIALOG_MAX_TITLE_LEN - 1);
    _title[DIALOG_MAX_TITLE_LEN - 1] = '\0';
    
    strncpy(_message, message, DIALOG_MAX_MESSAGE_LEN - 1);
    _message[DIALOG_MAX_MESSAGE_LEN - 1] = '\0';
    
    strncpy(_buttons[0], button1Label, DIALOG_MAX_BUTTON_LABEL_LEN - 1);
    _buttons[0][DIALOG_MAX_BUTTON_LABEL_LEN - 1] = '\0';
    
    strncpy(_buttons[1], button2Label, DIALOG_MAX_BUTTON_LABEL_LEN - 1);
    _buttons[1][DIALOG_MAX_BUTTON_LABEL_LEN - 1] = '\0';
    
    _buttonCount = 2;
    _type = DialogType::CONFIRM;
    _visible = true;
    _dismissed = false;
    _selectedButton = 1;  // Default to second button (usually the action)
    _result = DialogResult::NONE;
    _callback = callback;
    _userData = userData;
    _needsRedraw = true;
    
    Serial.printf("[Dialog] showConfirm: '%s' [%s] [%s]\n", title, button1Label, button2Label);
}

void Dialog::showProgress(const char* title, const char* message) {
    strncpy(_title, title, DIALOG_MAX_TITLE_LEN - 1);
    _title[DIALOG_MAX_TITLE_LEN - 1] = '\0';
    
    strncpy(_message, message, DIALOG_MAX_MESSAGE_LEN - 1);
    _message[DIALOG_MAX_MESSAGE_LEN - 1] = '\0';
    
    _buttonCount = 0;
    _type = DialogType::PROGRESS;
    _visible = true;
    _dismissed = false;
    _selectedButton = 0;
    _result = DialogResult::NONE;
    _callback = nullptr;
    _userData = nullptr;
    _progress = -1.0f;  // Indeterminate
    _spinnerFrame = 0;
    _lastSpinnerUpdateMs = millis();
    _needsRedraw = true;
    
    Serial.printf("[Dialog] showProgress: '%s'\n", title);
}

// ============================================================================
// State Updates
// ============================================================================

void Dialog::setProgress(float progress) {
    if (_type != DialogType::PROGRESS) return;
    
    _progress = progress;
    _needsRedraw = true;
}

void Dialog::setMessage(const char* message) {
    strncpy(_message, message, DIALOG_MAX_MESSAGE_LEN - 1);
    _message[DIALOG_MAX_MESSAGE_LEN - 1] = '\0';
    _needsRedraw = true;
}

void Dialog::completeProgress(const char* message, const char* buttonLabel) {
    if (_type != DialogType::PROGRESS) return;
    
    strncpy(_message, message, DIALOG_MAX_MESSAGE_LEN - 1);
    _message[DIALOG_MAX_MESSAGE_LEN - 1] = '\0';
    
    strncpy(_buttons[0], buttonLabel, DIALOG_MAX_BUTTON_LABEL_LEN - 1);
    _buttons[0][DIALOG_MAX_BUTTON_LABEL_LEN - 1] = '\0';
    
    _buttonCount = 1;
    _progress = 1.0f;  // Complete
    _needsRedraw = true;
    
    draw();
}

// ============================================================================
// Input Handling
// ============================================================================

void Dialog::handleButtonA() {
    if (!_visible || _dismissed) return;
    
    // For progress dialogs without buttons, ignore
    if (_buttonCount == 0) return;
    
    Serial.printf("[Dialog] Button A - activating button %d\n", _selectedButton);
    
    // Determine result based on selected button
    DialogResult result = (_selectedButton == 0) ? DialogResult::BUTTON_1 : DialogResult::BUTTON_2;
    
    finishWithResult(result);
}

void Dialog::handleButtonB() {
    if (!_visible || _dismissed) return;
    
    // Only cycle if there are multiple buttons
    if (_buttonCount <= 1) return;
    
    // Cycle to next button
    _selectedButton = (_selectedButton + 1) % _buttonCount;
    _needsRedraw = true;
    
    Serial.printf("[Dialog] Button B - selected button %d\n", _selectedButton);
    
    // Redraw to show new selection
    draw();
}

// ============================================================================
// Drawing
// ============================================================================

void Dialog::draw() {
    if (!_visible) return;
    
    _display.waitDisplay();
    _display.startWrite();
    
    // Clear screen with background
    _display.fillScreen(COLOR_BACKGROUND);
    
    drawBackground();
    drawTitleBar();
    drawMessage();
    
    if (_type == DialogType::PROGRESS && _buttonCount == 0) {
        if (_progress < 0) {
            drawSpinner();
        } else {
            drawProgressBar();
        }
    } else if (_buttonCount > 0) {
        drawButtons();
    }
    
    _display.endWrite();
    _display.display();
    
    _needsRedraw = false;
}

bool Dialog::needsRedraw() const {
    if (_type == DialogType::PROGRESS && _progress < 0 && _visible) {
        // Spinner needs periodic updates
        return (millis() - _lastSpinnerUpdateMs >= DIALOG_SPINNER_INTERVAL_MS);
    }
    return _needsRedraw;
}

void Dialog::drawBackground() {
    // Calculate dialog bounds
    int dialogX = DIALOG_MARGIN;
    int dialogY = DIALOG_MARGIN;
    int dialogWidth = SCREEN_WIDTH - (DIALOG_MARGIN * 2);
    int dialogHeight = SCREEN_HEIGHT - (DIALOG_MARGIN * 2);
    
    // Draw border
    _display.fillRoundRect(dialogX - 2, dialogY - 2,
                          dialogWidth + 4, dialogHeight + 4,
                          DIALOG_CORNER_RADIUS + 1, DIALOG_BORDER_COLOR);
    
    // Draw dialog background
    _display.fillRoundRect(dialogX, dialogY,
                          dialogWidth, dialogHeight,
                          DIALOG_CORNER_RADIUS, DIALOG_BG_COLOR);
}

void Dialog::drawTitleBar() {
    int dialogX = DIALOG_MARGIN;
    int dialogY = DIALOG_MARGIN;
    int dialogWidth = SCREEN_WIDTH - (DIALOG_MARGIN * 2);
    
    // Draw title bar background
    _display.fillRoundRect(dialogX, dialogY,
                          dialogWidth, DIALOG_TITLE_HEIGHT,
                          DIALOG_CORNER_RADIUS, DIALOG_TITLE_BG_COLOR);
    
    // Square off bottom corners of title bar
    _display.fillRect(dialogX, dialogY + DIALOG_TITLE_HEIGHT - DIALOG_CORNER_RADIUS,
                     dialogWidth, DIALOG_CORNER_RADIUS, DIALOG_TITLE_BG_COLOR);
    
    // Draw title text (centered)
    _display.setTextColor(DIALOG_TEXT_COLOR);
    _display.setFont(&fonts::Font2);
    _display.setTextSize(1);
    
    int titleWidth = _display.textWidth(_title);
    int titleX = dialogX + (dialogWidth - titleWidth) / 2;
    int titleY = dialogY + (DIALOG_TITLE_HEIGHT - 14) / 2;
    
    _display.setCursor(titleX, titleY);
    _display.print(_title);
}

void Dialog::drawMessage() {
    int dialogX = DIALOG_MARGIN;
    int dialogY = DIALOG_MARGIN;
    int dialogWidth = SCREEN_WIDTH - (DIALOG_MARGIN * 2);
    
    // Message area starts below title
    int messageX = dialogX + DIALOG_PADDING;
    int messageY = dialogY + DIALOG_TITLE_HEIGHT + DIALOG_PADDING;
    int messageWidth = dialogWidth - (DIALOG_PADDING * 2);
    
    _display.setTextColor(DIALOG_TEXT_SECONDARY_COLOR);
    _display.setFont(&fonts::Font0);
    _display.setTextSize(1);
    
    // Simple word-wrap text drawing
    int currentX = messageX;
    int currentY = messageY;
    int lineHeight = 12;
    
    for (size_t i = 0; i < strlen(_message); i++) {
        char c = _message[i];
        
        if (c == '\n') {
            currentY += lineHeight;
            currentX = messageX;
            continue;
        }
        
        // Get character width
        char charStr[2] = {c, '\0'};
        int charWidth = _display.textWidth(charStr);
        
        // Check if we need to wrap
        if (currentX + charWidth > messageX + messageWidth) {
            currentY += lineHeight;
            currentX = messageX;
        }
        
        _display.setCursor(currentX, currentY);
        _display.print(c);
        currentX += charWidth;
    }
}

void Dialog::drawButtons() {
    int dialogX = DIALOG_MARGIN;
    int dialogY = DIALOG_MARGIN;
    int dialogWidth = SCREEN_WIDTH - (DIALOG_MARGIN * 2);
    int dialogHeight = SCREEN_HEIGHT - (DIALOG_MARGIN * 2);
    
    // Calculate button area (6px from bottom edge)
    int buttonAreaY = dialogY + dialogHeight - DIALOG_BUTTON_HEIGHT - 6;
    
    if (_buttonCount == 1) {
        // Single centered button
        int buttonWidth = 60;
        int buttonX = dialogX + (dialogWidth - buttonWidth) / 2;
        
        // Draw button background
        uint16_t bgColor = (_selectedButton == 0) ? DIALOG_BUTTON_SELECTED_COLOR : DIALOG_BUTTON_BG_COLOR;
        _display.fillRoundRect(buttonX, buttonAreaY, buttonWidth, DIALOG_BUTTON_HEIGHT,
                              DIALOG_BUTTON_CORNER_RADIUS, bgColor);
        
        // Draw button label
        _display.setTextColor(DIALOG_BUTTON_TEXT_COLOR);
        _display.setFont(&fonts::Font0);
        _display.setTextSize(1);
        
        int labelWidth = _display.textWidth(_buttons[0]);
        int labelX = buttonX + (buttonWidth - labelWidth) / 2;
        int labelY = buttonAreaY + (DIALOG_BUTTON_HEIGHT - 8) / 2;
        
        _display.setCursor(labelX, labelY);
        _display.print(_buttons[0]);
        
    } else if (_buttonCount == 2) {
        // Two buttons side by side
        int totalButtonWidth = dialogWidth - (DIALOG_PADDING * 2) - DIALOG_BUTTON_SPACING;
        int buttonWidth = totalButtonWidth / 2;
        int button1X = dialogX + DIALOG_PADDING;
        int button2X = button1X + buttonWidth + DIALOG_BUTTON_SPACING;
        
        // Draw button 1 (left)
        uint16_t bg1Color = (_selectedButton == 0) ? DIALOG_BUTTON_SELECTED_COLOR : DIALOG_BUTTON_BG_COLOR;
        _display.fillRoundRect(button1X, buttonAreaY, buttonWidth, DIALOG_BUTTON_HEIGHT,
                              DIALOG_BUTTON_CORNER_RADIUS, bg1Color);
        
        _display.setTextColor(DIALOG_BUTTON_TEXT_COLOR);
        _display.setFont(&fonts::Font0);
        _display.setTextSize(1);
        
        int label1Width = _display.textWidth(_buttons[0]);
        int label1X = button1X + (buttonWidth - label1Width) / 2;
        int labelY = buttonAreaY + (DIALOG_BUTTON_HEIGHT - 8) / 2;
        
        _display.setCursor(label1X, labelY);
        _display.print(_buttons[0]);
        
        // Draw button 2 (right)
        uint16_t bg2Color = (_selectedButton == 1) ? DIALOG_BUTTON_SELECTED_COLOR : DIALOG_BUTTON_BG_COLOR;
        _display.fillRoundRect(button2X, buttonAreaY, buttonWidth, DIALOG_BUTTON_HEIGHT,
                              DIALOG_BUTTON_CORNER_RADIUS, bg2Color);
        
        int label2Width = _display.textWidth(_buttons[1]);
        int label2X = button2X + (buttonWidth - label2Width) / 2;
        
        _display.setCursor(label2X, labelY);
        _display.print(_buttons[1]);
    }
}

void Dialog::drawProgressBar() {
    int dialogX = DIALOG_MARGIN;
    int dialogY = DIALOG_MARGIN;
    int dialogWidth = SCREEN_WIDTH - (DIALOG_MARGIN * 2);
    int dialogHeight = SCREEN_HEIGHT - (DIALOG_MARGIN * 2);
    
    // Progress bar position
    int progressX = dialogX + DIALOG_PROGRESS_MARGIN;
    int progressY = dialogY + dialogHeight - DIALOG_PROGRESS_HEIGHT - DIALOG_PROGRESS_MARGIN;
    int progressWidth = dialogWidth - (DIALOG_PROGRESS_MARGIN * 2);
    
    // Background
    _display.fillRoundRect(progressX, progressY, progressWidth, DIALOG_PROGRESS_HEIGHT,
                          3, DIALOG_PROGRESS_BG_COLOR);
    
    // Fill based on progress (0.0 to 1.0)
    if (_progress > 0) {
        int fillWidth = (int)(progressWidth * _progress);
        if (fillWidth > 0) {
            _display.fillRoundRect(progressX, progressY, fillWidth, DIALOG_PROGRESS_HEIGHT,
                                  3, DIALOG_PROGRESS_FILL_COLOR);
        }
    }
}

void Dialog::drawSpinner() {
    int dialogX = DIALOG_MARGIN;
    int dialogY = DIALOG_MARGIN;
    int dialogWidth = SCREEN_WIDTH - (DIALOG_MARGIN * 2);
    int dialogHeight = SCREEN_HEIGHT - (DIALOG_MARGIN * 2);
    
    // Spinner position (centered horizontally, below message)
    int spinnerX = dialogX + dialogWidth / 2;
    int spinnerY = dialogY + dialogHeight - DIALOG_SPINNER_SIZE - 20;
    
    // Simple rotating dots animation
    int dotCount = 8;
    int radius = DIALOG_SPINNER_SIZE / 2;
    int dotRadius = 2;
    
    for (int i = 0; i < dotCount; i++) {
        float angle = (2.0f * PI * i / dotCount) + (_spinnerFrame * 0.5f);
        int dotX = spinnerX + (int)(radius * cos(angle));
        int dotY = spinnerY + (int)(radius * sin(angle));
        
        // Fade dots based on position relative to current frame
        uint16_t color = (i == (_spinnerFrame % dotCount)) ? 
                         DIALOG_TEXT_COLOR : DIALOG_TEXT_SECONDARY_COLOR;
        
        _display.fillCircle(dotX, dotY, dotRadius, color);
    }
    
    // Update spinner frame
    uint32_t now = millis();
    if (now - _lastSpinnerUpdateMs >= DIALOG_SPINNER_INTERVAL_MS) {
        _spinnerFrame = (_spinnerFrame + 1) % dotCount;
        _lastSpinnerUpdateMs = now;
    }
}

void Dialog::drawHint() {
    int dialogX = DIALOG_MARGIN;
    int dialogY = DIALOG_MARGIN;
    int dialogWidth = SCREEN_WIDTH - (DIALOG_MARGIN * 2);
    int dialogHeight = SCREEN_HEIGHT - (DIALOG_MARGIN * 2);
    
    const char* hint;
    if (_buttonCount == 1) {
        hint = "Press front button";
    } else {
        hint = "Side: cycle | Front: select";
    }
    
    _display.setTextColor(DIALOG_HINT_COLOR);
    _display.setFont(&fonts::Font0);
    _display.setTextSize(1);
    
    int hintWidth = _display.textWidth(hint);
    int hintX = dialogX + (dialogWidth - hintWidth) / 2;
    int hintY = dialogY + dialogHeight - 12;
    
    _display.setCursor(hintX, hintY);
    _display.print(hint);
}

// ============================================================================
// State Queries
// ============================================================================

bool Dialog::isVisible() const {
    return _visible;
}

bool Dialog::isDismissed() const {
    return _dismissed;
}

DialogResult Dialog::getResult() const {
    return _result;
}

int Dialog::getSelectedButton() const {
    return _selectedButton;
}

void Dialog::dismiss() {
    _visible = false;
    _dismissed = true;
    _result = DialogResult::DISMISSED;
    _hasPendingCallback = false;  // Clear any pending callback
    Serial.println("[Dialog] Dismissed without callback");
}

void Dialog::reset() {
    _visible = false;
    _dismissed = false;
    _result = DialogResult::NONE;
    _buttonCount = 0;
    _selectedButton = 0;
    _progress = -1.0f;
    _callback = nullptr;
    _userData = nullptr;
    _title[0] = '\0';
    _message[0] = '\0';
    _needsRedraw = false;
    _hasPendingCallback = false;
    _pendingResult = DialogResult::NONE;
}

bool Dialog::hasPendingCallback() const {
    return _hasPendingCallback;
}

void Dialog::invokePendingCallback() {
    if (!_hasPendingCallback || _callback == nullptr) {
        return;
    }
    
    // Clear pending state before invoking (in case callback shows new dialog)
    _hasPendingCallback = false;
    DialogCallback callback = _callback;
    void* userData = _userData;
    DialogResult result = _pendingResult;
    
    Serial.printf("[Dialog] Invoking deferred callback with result %d\n", static_cast<int>(result));
    
    callback(result, userData);
}

// ============================================================================
// Private Helpers
// ============================================================================

void Dialog::finishWithResult(DialogResult result) {
    _result = result;
    _dismissed = true;
    _visible = false;
    
    Serial.printf("[Dialog] Finished with result %d\n", static_cast<int>(result));
    
    // Defer callback invocation - ScreenManager will call it after screen redraws
    // This prevents UI freeze when callback does blocking work
    if (_callback != nullptr) {
        _hasPendingCallback = true;
        _pendingResult = result;
    }
}
