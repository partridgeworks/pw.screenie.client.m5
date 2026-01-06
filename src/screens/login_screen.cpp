/**
 * login_screen.cpp - Login Screen implementation
 * 
 * Uses ApiClient for device-code login initiation and
 * PollingManager for checking login completion.
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#include <M5Unified.h>
#include "screens/login_screen.h"
#include "screen_manager.h"
#include "api_client.h"
#include "polling_manager.h"
#include "app_state.h"
#include "sound.h"
#include "config.h"
#include <Arduino.h>

// Include QR code library
#include "qrcode.h"

// Forward declaration from main.cpp for sleep functionality
extern bool tryGoToSleep(bool userInitiated);

// ============================================================================
// Constructor
// ============================================================================

LoginScreen::LoginScreen(M5GFX& display)
    : _display(display)
    , _screenManager(nullptr)
    , _apiClient(nullptr)
    , _pollingManager(nullptr)
    , _state(LoginState::INITIALIZING)
    , _qrSize(0)
    , _qrGenerated(false)
    , _lastAnimationMs(0)
    , _animationFrame(0)
{
    _pairingCode[0] = '\0';
    _deviceCode[0] = '\0';
    _errorMessage[0] = '\0';
    memset(_qrData, 0, sizeof(_qrData));
}

void LoginScreen::setScreenManager(ScreenManager* manager) {
    _screenManager = manager;
}

void LoginScreen::setApiClient(ApiClient* api, PollingManager* polling) {
    _apiClient = api;
    _pollingManager = polling;
}

// ============================================================================
// Screen Lifecycle
// ============================================================================

void LoginScreen::onEnter() {
    Serial.println("[LoginScreen] onEnter");
    
    // Set up the menu for this screen
    setupMenu();
    
    // Reset state
    _state = LoginState::INITIALIZING;
    _pairingCode[0] = '\0';
    _deviceCode[0] = '\0';
    _errorMessage[0] = '\0';
    _qrGenerated = false;
    _animationFrame = 0;
    _lastAnimationMs = millis();
    
    // Clear the display first to ensure clean slate
    _display.fillScreen(COLOR_BACKGROUND);
    _display.display();
    
    // Initiate login via API (or mock if API not available)
    initiateLogin();
    
    // Draw initial screen
    draw();
}

void LoginScreen::onExit() {
    Serial.println("[LoginScreen] onExit");
    
    // Ensure menu is hidden
    _menu.hide();
    
    // Clean up menu
    destroyMenu();
    
    // Stop polling if active
    if (_pollingManager && _pollingManager->isPolling()) {
        _pollingManager->stopPolling();
    }
}

void LoginScreen::onResume() {
    Serial.println("[LoginScreen] onResume");
    draw();
}

void LoginScreen::update() {
    uint32_t now = millis();
    
    // Update animation
    if (now - _lastAnimationMs >= ANIMATION_INTERVAL_MS) {
        _lastAnimationMs = now;
        _animationFrame = (_animationFrame + 1) % 4;
        
        // Only redraw the polling indicator, not the whole screen
        if (_state == LoginState::DISPLAYING_CODE) {
            _display.startWrite();
            drawPollingIndicator();
            _display.endWrite();
            _display.display();
        }
    }
    
    // Note: Polling is handled by PollingManager.update() in main.cpp
    // We don't need to check for poll completion here; the callback handles it
}

void LoginScreen::draw() {
    _display.waitDisplay();
    _display.startWrite();
    
    drawBackground();
    drawTitle();
    
    switch (_state) {
        case LoginState::INITIALIZING:
            // Show loading indicator
            drawPollingIndicator();
            break;
            
        case LoginState::DISPLAYING_CODE:
            drawQRCode();
            drawPairingCode();
            drawPollingIndicator();
            drawInstructions();
            break;
            
        case LoginState::SUCCESS:
            // Brief success state - typically transitions immediately
            _display.setTextColor(COLOR_ACCENT_SUCCESS);
            _display.setFont(&fonts::Font2);
            _display.setTextSize(1);
            _display.setCursor(SCREEN_WIDTH / 2 - 30, SCREEN_HEIGHT / 2);
            _display.print("Success!");
            break;
            
        case LoginState::ERROR:
            drawErrorState();
            break;
    }
    
    _display.endWrite();
    _display.display();
}

// ============================================================================
// Input Handling
// ============================================================================

void LoginScreen::onButtonA() {
    // Check if menu is visible first
    if (_menu.isVisible()) {
        // Menu visible - activate selected item
        Serial.println("[LoginScreen] Button A - activating menu item");
        _menu.activateSelected();
        _menu.hide();
        draw();
        return;
    }
    
    if (_state == LoginState::ERROR) {
        // Retry login
        Serial.println("[LoginScreen] Retrying login...");
        playButtonBeep();
        initiateLogin();
        draw();
    } else if (_state == LoginState::DISPLAYING_CODE) {
        // For testing: skip wait and simulate immediate success
        Serial.println("[LoginScreen] Button A - skipping to success (test mode)");
        playButtonBeep();
        
        // Stop polling and trigger success manually
        if (_pollingManager) {
            _pollingManager->stopPolling();
        }
        setLoginSuccess();
    }
}

void LoginScreen::onButtonB() {
    // Check if menu is visible first
    if (_menu.isVisible()) {
        // Menu visible - cycle to next item
        _menu.selectNext();
        drawMenu();
        return;
    }
    
    // Menu not visible - open it
    Serial.println("[LoginScreen] Button B - opening menu");
    _menu.show();
    drawMenu();
}

void LoginScreen::onButtonPower() {
    // Check if menu is visible first
    if (_menu.isVisible()) {
        // Menu visible - close it
        Serial.println("[LoginScreen] Power - closing menu");
        _menu.hide();
        draw();
        return;
    }
    
    // No other action on login screen when menu closed
    Serial.println("[LoginScreen] Power button - (no action on login screen)");
}

void LoginScreen::onButtonPowerHold() {
    Serial.println("[LoginScreen] Power hold - powering off");
    M5.Power.powerOff();
}

// ============================================================================
// Login State Management
// ============================================================================

void LoginScreen::initiateLogin() {
    Serial.println("[LoginScreen] Initiating login...");
    _state = LoginState::INITIALIZING;
    
    // Use ApiClient if available
    if (_apiClient != nullptr) {
        DeviceCodeResponse response = _apiClient->initiateLogin();
        
        if (response.success) {
            // Store codes
            strncpy(_pairingCode, response.userCode, sizeof(_pairingCode) - 1);
            _pairingCode[sizeof(_pairingCode) - 1] = '\0';
            strncpy(_deviceCode, response.deviceCode, sizeof(_deviceCode) - 1);
            _deviceCode[sizeof(_deviceCode) - 1] = '\0';
            
            // Generate QR code
            if (generateQRCode(response.qrCodeUrl)) {
                _state = LoginState::DISPLAYING_CODE;
                
                // Start polling for login completion
                if (_pollingManager != nullptr) {
                    _pollingManager->startLoginPolling(
                        _deviceCode,
                        LoginScreen::onLoginPollResult,
                        this  // Pass this pointer for callback
                    );
                }
                
                Serial.printf("[LoginScreen] Login initiated, code: %s\n", _pairingCode);
            } else {
                setError("QR code generation failed");
            }
        } else {
            setError(response.errorMessage);
        }
    } else {
        // Fallback to mock login if no API client
        Serial.println("[LoginScreen] No ApiClient, using mock login");
        simulateMockLogin();
    }
}

void LoginScreen::setPairingCode(const char* pairingCode) {
    strncpy(_pairingCode, pairingCode, sizeof(_pairingCode) - 1);
    _pairingCode[sizeof(_pairingCode) - 1] = '\0';
    
    // Generate QR code for the pairing URL
    char url[128];
    snprintf(url, sizeof(url), "%s%s", API_PAIRING_BASE_URL, _pairingCode);
    
    if (generateQRCode(url)) {
        _state = LoginState::DISPLAYING_CODE;
        Serial.printf("[LoginScreen] Pairing code set: %s\n", _pairingCode);
    } else {
        setError("QR code generation failed");
    }
}

void LoginScreen::setError(const char* errorMessage) {
    strncpy(_errorMessage, errorMessage, sizeof(_errorMessage) - 1);
    _errorMessage[sizeof(_errorMessage) - 1] = '\0';
    _state = LoginState::ERROR;
    
    // Stop polling if active
    if (_pollingManager && _pollingManager->isPolling()) {
        _pollingManager->stopPolling();
    }
    
    Serial.printf("[LoginScreen] Error: %s\n", errorMessage);
}

void LoginScreen::setLoginSuccess() {
    _state = LoginState::SUCCESS;
    
    // Note: API key and username should already be set by the polling callback
    // If called directly for testing, set mock values
    AppState& appState = AppState::getInstance();
    if (!appState.isLoggedIn()) {
        UserSession& session = appState.getSession();
        session.isLoggedIn = true;
        strncpy(session.apiKey, "mock-api-key-login", sizeof(session.apiKey) - 1);
        strncpy(session.username, "Parent", sizeof(session.username) - 1);
        
        // Update ApiClient with the new key
        if (_apiClient) {
            _apiClient->setApiKey(session.apiKey);
        }
    }
    
    // Save to persistence
    appState.saveSessionToPersistence();
    
    // Brief delay to show success, then transition
    draw();
    M5.delay(500);
    playButtonBeep();
    
    transitionToSelectChild();
}

void LoginScreen::simulateMockLogin() {
    // Generate a mock pairing code
    snprintf(_pairingCode, sizeof(_pairingCode), "%06lu", 
             (unsigned long)(micros() % 1000000));
    snprintf(_deviceCode, sizeof(_deviceCode), "mock-device-%lu",
             (unsigned long)(millis() % 100000));
    
    // Generate QR code
    char url[128];
    snprintf(url, sizeof(url), "%s%s", API_PAIRING_BASE_URL, _pairingCode);
    
    if (generateQRCode(url)) {
        _state = LoginState::DISPLAYING_CODE;
        
        // If we have a polling manager and API client, use them
        // The ApiClient is in mock mode by default so it will simulate login
        if (_pollingManager != nullptr && _apiClient != nullptr) {
            _pollingManager->startLoginPolling(
                _deviceCode,
                LoginScreen::onLoginPollResult,
                this
            );
        }
        
        Serial.printf("[LoginScreen] Mock login started, code: %s\n", _pairingCode);
    } else {
        setError("Failed to generate QR code");
    }
}

// ============================================================================
// Polling Callback
// ============================================================================

void LoginScreen::onLoginPollResult(const PollingResult& result, void* userData) {
    LoginScreen* self = static_cast<LoginScreen*>(userData);
    if (self != nullptr) {
        self->handleLoginResult(result);
    }
}

void LoginScreen::handleLoginResult(const PollingResult& result) {
    Serial.printf("[LoginScreen] Poll result: success=%d, timedOut=%d, msg=%s\n",
                  result.success, result.timedOut, result.message);
    
    if (result.success) {
        // Login successful - store credentials
        AppState& appState = AppState::getInstance();
        UserSession& session = appState.getSession();
        session.isLoggedIn = true;
        strncpy(session.apiKey, result.apiKey, sizeof(session.apiKey) - 1);
        strncpy(session.username, result.username, sizeof(session.username) - 1);
        
        // Update ApiClient with the new key
        if (_apiClient) {
            _apiClient->setApiKey(result.apiKey);
        }
        
        setLoginSuccess();
    } else {
        // Login failed - show the error message from the result
        // This handles both timeout (code expired) and other errors
        setError(result.message[0] != '\0' ? result.message : "Login failed");
    }
}

// ============================================================================
// Drawing Methods
// ============================================================================

void LoginScreen::drawBackground() {
    _display.fillScreen(COLOR_BACKGROUND);
}

void LoginScreen::drawTitle() {
    _display.setTextColor(COLOR_TEXT_PRIMARY);
    _display.setFont(&fonts::Font2);
    _display.setTextSize(1);
    
    const char* title = "Link Device";
    int textWidth = _display.textWidth(title);
    _display.setCursor((SCREEN_WIDTH - textWidth) / 2, 6);
    _display.print(title);
    
    // Underline
    int lineY = 22;
    _display.drawFastHLine(20, lineY, SCREEN_WIDTH - 40, COLOR_BORDER);
}

void LoginScreen::drawQRCode() {
    if (!_qrGenerated || _qrSize == 0) {
        // Show placeholder
        int boxSize = 60;
        int boxX = 20;
        int boxY = 35;
        _display.drawRect(boxX, boxY, boxSize, boxSize, COLOR_BORDER);
        _display.setTextColor(COLOR_TEXT_MUTED);
        _display.setFont(&fonts::Font0);
        _display.setCursor(boxX + 15, boxY + 25);
        _display.print("QR");
        return;
    }
    
    // Calculate QR code display size
    // QR should fit in left portion of screen
    int maxSize = 70;  // Max pixel size for QR
    int moduleSize = maxSize / _qrSize;
    if (moduleSize < 1) moduleSize = 1;
    
    int qrPixelSize = moduleSize * _qrSize;
    int qrX = 15;  // Left aligned with some margin
    int qrY = 30;  // Below title
    
    // Draw white background for QR code (with quiet zone)
    int quietZone = moduleSize * 2;
    _display.fillRect(qrX - quietZone, qrY - quietZone, 
                      qrPixelSize + quietZone * 2, qrPixelSize + quietZone * 2, 
                      TFT_WHITE);
    
    // Draw QR code modules
    for (int y = 0; y < _qrSize; y++) {
        for (int x = 0; x < _qrSize; x++) {
            if (getQRModule(x, y)) {
                _display.fillRect(qrX + x * moduleSize, qrY + y * moduleSize,
                                  moduleSize, moduleSize, TFT_BLACK);
            }
        }
    }
}

void LoginScreen::drawPairingCode() {
    // Position to the right of QR code
    int codeX = 98;
    int codeY = 45;
    
    // Label
    _display.setTextColor(COLOR_TEXT_SECONDARY);
    _display.setFont(&fonts::Font0);
    _display.setTextSize(1);
    _display.setCursor(codeX, codeY);
    _display.print("or enter code:");
    
    // Large pairing code
    _display.setTextColor(COLOR_ACCENT_PRIMARY);
    _display.setFont(&fonts::Font4);
    _display.setTextSize(1);
    _display.setCursor(codeX, codeY + 18);
    _display.print(_pairingCode);
}

void LoginScreen::drawPollingIndicator() {
    // Animated dots at bottom of screen
    int dotY = 118;
    int dotStartX = SCREEN_WIDTH / 2 - 20;
    int dotSpacing = 12;
    int dotRadius = 3;
    
    // Clear the dot area first
    _display.fillRect(dotStartX - 5, dotY - 5, 50, 15, COLOR_BACKGROUND);
    
    // Draw dots with animation
    for (int i = 0; i < 4; i++) {
        uint16_t color = (i == _animationFrame) ? COLOR_ACCENT_PRIMARY : COLOR_TEXT_MUTED;
        _display.fillCircle(dotStartX + i * dotSpacing, dotY, dotRadius, color);
    }
}

void LoginScreen::drawErrorState() {
    // Error icon (simple X)
    int iconX = SCREEN_WIDTH / 2;
    int iconY = 50;
    int iconSize = 20;
    
    _display.drawLine(iconX - iconSize/2, iconY - iconSize/2, 
                      iconX + iconSize/2, iconY + iconSize/2, COLOR_ACCENT_DANGER);
    _display.drawLine(iconX - iconSize/2, iconY + iconSize/2, 
                      iconX + iconSize/2, iconY - iconSize/2, COLOR_ACCENT_DANGER);
    
    // Error message
    _display.setTextColor(COLOR_TEXT_PRIMARY);
    _display.setFont(&fonts::Font2);
    _display.setTextSize(1);
    
    int textWidth = _display.textWidth(_errorMessage);
    _display.setCursor((SCREEN_WIDTH - textWidth) / 2, 80);
    _display.print(_errorMessage);
    
    // Retry hint
    _display.setTextColor(COLOR_TEXT_SECONDARY);
    _display.setFont(&fonts::Font0);
    _display.setCursor(SCREEN_WIDTH / 2 - 40, 110);
    _display.print("Press A to retry");
}

void LoginScreen::drawInstructions() {
    // Instructions at bottom
    int instrY = 105;
    int instrX = 105;
    
    _display.setTextColor(COLOR_TEXT_MUTED);
    _display.setFont(&fonts::Font0);
    _display.setTextSize(1);
    _display.setCursor(instrX, instrY);
    _display.print("Scan QR or enter");
    _display.setCursor(instrX, instrY + 10);
    _display.print("code on phone");
}

// ============================================================================
// QR Code Generation
// ============================================================================

bool LoginScreen::generateQRCode(const char* url) {
    // Use the qrcode library to generate QR code
    QRCode qrcode;
    
    // Calculate buffer size for version 4 (33x33)
    uint8_t qrcodeData[qrcode_getBufferSize(QR_MAX_VERSION)];
    
    // Generate QR code with error correction level M (medium, 15% recovery)
    int result = qrcode_initText(&qrcode, qrcodeData, QR_MAX_VERSION, ECC_MEDIUM, url);
    
    if (result != 0) {
        Serial.printf("[LoginScreen] QR code generation failed: %d\n", result);
        _qrGenerated = false;
        return false;
    }
    
    _qrSize = qrcode.size;
    
    // Copy QR data to our buffer
    // The qrcode library uses qrcode_getModule(qrcode, x, y) to read modules
    // We store the qrcode struct data for later use
    memcpy(_qrData, qrcodeData, sizeof(_qrData));
    
    _qrGenerated = true;
    Serial.printf("[LoginScreen] QR code generated, size: %dx%d\n", _qrSize, _qrSize);
    
    return true;
}

bool LoginScreen::getQRModule(int x, int y) const {
    if (!_qrGenerated || x < 0 || y < 0 || x >= _qrSize || y >= _qrSize) {
        return false;
    }
    
    // Recreate QRCode struct to use qrcode_getModule
    // This is a bit inefficient but the qrcode library doesn't expose internals
    QRCode qrcode;
    qrcode.size = _qrSize;
    qrcode.modules = (uint8_t*)_qrData;
    
    return qrcode_getModule(&qrcode, x, y);
}

// ============================================================================
// Navigation
// ============================================================================

void LoginScreen::transitionToSelectChild() {
    if (_screenManager != nullptr) {
        _screenManager->navigateTo(ScreenType::SELECT_CHILD);
    }
}

// ============================================================================
// Menu Support
// ============================================================================

bool LoginScreen::isMenuVisible() const {
    return _menu.isVisible();
}

void LoginScreen::setupMenu() {
    _menu.clear();
    
    // Login screen menu with Sleep, Settings, and Parent Menu options
    _menu.addItem("Sleep", onMenuSleepNow, this, true);
    _menu.addItem("Settings", onMenuSettings, this, true);
    _menu.addItem("Parent Menu", onMenuParent, this, true);
    
    Serial.printf("[LoginScreen] Menu initialized with %d items\n", _menu.getItemCount());
}

void LoginScreen::destroyMenu() {
    _menu.clear();
    Serial.println("[LoginScreen] Menu cleared");
}

void LoginScreen::drawMenu() {
    // Draw menu overlay on the login screen using the new full-screen design
    
    if (!_menu.isVisible()) return;
    
    _display.waitDisplay();
    _display.startWrite();
    
    int itemCount = _menu.getItemCount();
    int selectedIndex = _menu.getSelectedIndex();
    
    // Fill entire area below header with dark gray background
    _display.fillRect(0, MENU_Y, SCREEN_WIDTH, MENU_HEIGHT, COLOR_MENU_BG);
    
    // Calculate content area (with padding, leaving space for chevron)
    // Move content up by 24px from original position
    int contentY = MENU_Y + MENU_PADDING - 24;
    int contentHeight = MENU_HEIGHT - (2 * MENU_PADDING) - MENU_CHEVRON_AREA_HEIGHT;
    int itemAreaHeight = contentHeight / MENU_VISIBLE_ITEMS;
    int itemGap = 8;  // 8px gap between items
    
    // Draw the 3 visible items: previous, selected, next (with wrapping)
    for (int slot = 0; slot < MENU_VISIBLE_ITEMS && itemCount > 0; slot++) {
        // Calculate which item index to show in this slot
        int displayIndex;
        if (slot == 0) {
            // Previous item (wraps to end if at start)
            displayIndex = (selectedIndex - 1 + itemCount) % itemCount;
        } else if (slot == 1) {
            // Current selected item
            displayIndex = selectedIndex;
        } else {
            // Next item (wraps to start if at end)
            displayIndex = (selectedIndex + 1) % itemCount;
        }
        
        // Add gap spacing: slot 0 at base, slot 1 adds gap, slot 2 adds 2*gap
        int itemY = contentY + (slot * itemAreaHeight) + (slot * itemGap);
        
        // Set color based on slot (middle slot = selected = white, others = gray)
        if (slot == 1) {
            _display.setTextColor(COLOR_TEXT_PRIMARY);  // White for selected
        } else {
            _display.setTextColor(COLOR_MENU_ITEM_GRAY);  // Gray for prev/next
        }
        
        // Use FreeSansBold12pt7b for ~16px bold look
        _display.setFont(&fonts::FreeSansBold12pt7b);
        _display.setTextSize(1);
        
        // Get label and truncate to 18 characters max
        const char* rawLabel = _menu.getItemLabel(displayIndex);
        char label[19];  // 18 chars + null terminator
        strncpy(label, rawLabel, 18);
        label[18] = '\0';
        
        // Center the text horizontally
        int textWidth = _display.textWidth(label);
        int textX = (SCREEN_WIDTH - textWidth) / 2;
        int textY = itemY + (itemAreaHeight + 12) / 2;  // Center vertically (baseline adjustment for GFX font)
        
        _display.setCursor(textX, textY);
        _display.print(label);
    }
    
    // Draw downward chevron at bottom center
    int chevronY = MENU_Y + MENU_HEIGHT - MENU_CHEVRON_AREA_HEIGHT + 2;
    int chevronX = SCREEN_WIDTH / 2;
    int chevronSize = 6;
    
    _display.setColor(COLOR_TEXT_PRIMARY);
    // Draw simple V chevron
    _display.drawLine(chevronX - chevronSize, chevronY, chevronX, chevronY + chevronSize, COLOR_TEXT_PRIMARY);
    _display.drawLine(chevronX, chevronY + chevronSize, chevronX + chevronSize, chevronY, COLOR_TEXT_PRIMARY);
    // Make it slightly thicker
    _display.drawLine(chevronX - chevronSize, chevronY + 1, chevronX, chevronY + chevronSize + 1, COLOR_TEXT_PRIMARY);
    _display.drawLine(chevronX, chevronY + chevronSize + 1, chevronX + chevronSize, chevronY + 1, COLOR_TEXT_PRIMARY);
    
    _display.endWrite();
    _display.display();
}

void LoginScreen::onMenuSleepNow(int itemIndex, void* userData) {
    LoginScreen* self = static_cast<LoginScreen*>(userData);
    if (self) self->doSleepNow();
}

void LoginScreen::onMenuSettings(int itemIndex, void* userData) {
    LoginScreen* self = static_cast<LoginScreen*>(userData);
    if (self) self->doSettings();
}

void LoginScreen::onMenuParent(int itemIndex, void* userData) {
    LoginScreen* self = static_cast<LoginScreen*>(userData);
    if (self) self->doParent();
}

void LoginScreen::doSleepNow() {
    Serial.println("[LoginScreen] Sleep Now activated");
    
    // Try to go to sleep - user initiated
    tryGoToSleep(true);
}

void LoginScreen::doSettings() {
    Serial.println("[LoginScreen] Settings activated");
    
    if (_screenManager) {
        _screenManager->navigateTo(ScreenType::SETTINGS);
    }
}

void LoginScreen::doParent() {
    Serial.println("[LoginScreen] Parent Menu activated");
    
    if (_screenManager) {
        _screenManager->navigateTo(ScreenType::PARENT);
    }
}
