/**
 * select_child_screen.cpp - Select Child Screen implementation
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#include <M5Unified.h>
#include <LittleFS.h>
#include "screens/select_child_screen.h"
#include "screen_manager.h"
#include "app_state.h"
#include "sound.h"
#include "config.h"
#include <Arduino.h>

// ============================================================================
// Constructor
// ============================================================================

SelectChildScreen::SelectChildScreen(M5GFX& display)
    : _display(display)
    , _screenManager(nullptr)
    , _apiClient(nullptr)
    , _memberCount(0)
    , _currentIndex(0)
    , _loading(false)
{
    // Clear members array
    for (int i = 0; i < MAX_MEMBERS; i++) {
        _members[i] = FamilyMember();
    }
}

void SelectChildScreen::setScreenManager(ScreenManager* manager) {
    _screenManager = manager;
}

void SelectChildScreen::setApiClient(ApiClient* api) {
    _apiClient = api;
}

// ============================================================================
// Screen Lifecycle
// ============================================================================

void SelectChildScreen::onEnter() {
    Serial.println("[SelectChildScreen] onEnter");
    
    // Reset state
    _currentIndex = 0;
    _loading = true;
    
    // Draw loading state
    draw();
    
    // Try to load members from API, fall back to mock if not available
    if (_apiClient != nullptr) {
        if (!loadMembersFromApi()) {
            Serial.println("[SelectChildScreen] API load failed, using mock data");
            loadMockMembers();
        }
    } else {
        Serial.println("[SelectChildScreen] No API client, using mock data");
        loadMockMembers();
    }
    _loading = false;
    
    // Redraw with members
    draw();
}

void SelectChildScreen::onExit() {
    Serial.println("[SelectChildScreen] onExit");
}

void SelectChildScreen::onResume() {
    Serial.println("[SelectChildScreen] onResume");
    draw();
}

void SelectChildScreen::update() {
    // No animation needed for this screen
}

void SelectChildScreen::draw() {
    _display.waitDisplay();
    _display.startWrite();
    
    drawBackground();
    drawTitle();
    
    if (_loading) {
        drawLoadingState();
    } else if (_memberCount > 0) {
        drawCurrentChild();
        if (_memberCount > 1) {
            drawChevrons();
        }
    } else {
        // No members - show error
        _display.setTextColor(COLOR_TEXT_SECONDARY);
        _display.setFont(&fonts::Font2);
        _display.setCursor(SCREEN_WIDTH / 2 - 50, SCREEN_HEIGHT / 2);
        _display.print("No children found");
    }
    
    _display.endWrite();
    _display.display();
}

// ============================================================================
// Input Handling
// ============================================================================

void SelectChildScreen::onButtonA() {
    if (_memberCount > 0 && !_loading) {
        // Select current child
        Serial.printf("[SelectChildScreen] Selected: %s\n", _members[_currentIndex].name);
        playButtonBeep();
        confirmSelection();
    }
}

void SelectChildScreen::onButtonB() {
    if (_memberCount > 1 && !_loading) {
        // Next child
        selectNext();
        draw();
    }
}

void SelectChildScreen::onButtonPower() {
    if (_memberCount > 1 && !_loading) {
        // Previous child
        selectPrevious();
        draw();
    }
}

void SelectChildScreen::onButtonPowerHold() {
    Serial.println("[SelectChildScreen] Power hold - powering off");
    M5.Power.powerOff();
}

// ============================================================================
// Mock Data
// ============================================================================

void SelectChildScreen::loadMockMembers() {
    // Create mock family members for testing
    _memberCount = 3;
    
    // Child 1
    strncpy(_members[0].id, "child-001", sizeof(_members[0].id));
    strncpy(_members[0].name, "Sophie", sizeof(_members[0].name));
    strncpy(_members[0].position, "child", sizeof(_members[0].position));
    _members[0].initial = 'S';
    
    // Child 2
    strncpy(_members[1].id, "child-002", sizeof(_members[1].id));
    strncpy(_members[1].name, "Oliver", sizeof(_members[1].name));
    strncpy(_members[1].position, "child", sizeof(_members[1].position));
    _members[1].initial = 'O';
    
    // Child 3
    strncpy(_members[2].id, "child-003", sizeof(_members[2].id));
    strncpy(_members[2].name, "Emma", sizeof(_members[2].name));
    strncpy(_members[2].position, "child", sizeof(_members[2].position));
    _members[2].initial = 'E';
    
    Serial.printf("[SelectChildScreen] Loaded %d mock members\n", _memberCount);
}

bool SelectChildScreen::loadMembersFromApi() {
    Serial.println("[SelectChildScreen] Loading members from API...");
    
    if (_apiClient == nullptr) {
        Serial.println("[SelectChildScreen] No API client available");
        return false;
    }
    
    // Fetch family members (children only) from API
    bool success = _apiClient->getFamilyMembers(_members, MAX_MEMBERS, _memberCount);
    
    if (!success || _memberCount == 0) {
        Serial.println("[SelectChildScreen] Failed to load members from API");
        _memberCount = 0;
        return false;
    }
    
    // Copy family ID from ApiClient to session so it gets persisted
    if (_apiClient->hasFamilyId()) {
        AppState& appState = AppState::getInstance();
        UserSession& session = appState.getSession();
        strncpy(session.familyId, _apiClient->getFamilyId(), sizeof(session.familyId) - 1);
        session.familyId[sizeof(session.familyId) - 1] = '\0';
        Serial.printf("[SelectChildScreen] Family ID copied to session: %s\n", session.familyId);
    }
    
    Serial.printf("[SelectChildScreen] Loaded %d children from API\n", _memberCount);
    return true;
}

// ============================================================================
// Drawing Methods
// ============================================================================

void SelectChildScreen::drawBackground() {
    _display.fillScreen(COLOR_BACKGROUND);
}

void SelectChildScreen::drawTitle() {
    _display.setTextColor(COLOR_TEXT_PRIMARY);
    _display.setFont(&fonts::Font2);
    _display.setTextSize(1);
    
    const char* title = getTitle();
    int textWidth = _display.textWidth(title);
    _display.setCursor((SCREEN_WIDTH - textWidth) / 2, TITLE_Y);
    _display.print(title);
}

void SelectChildScreen::drawCurrentChild() {
    if (_currentIndex < 0 || _currentIndex >= _memberCount) {
        return;
    }
    
    const FamilyMember& member = _members[_currentIndex];
    
    // NOTE: Draw name and dots BEFORE avatar because drawPng() affects
    // subsequent drawing operations. This order ensures all elements render.
    
    // Draw name below avatar area
    drawName(member.name);
    
    // Draw page indicator if multiple members
    if (_memberCount > 1) {
        int dotY = SCREEN_HEIGHT - 12;
        int totalWidth = _memberCount * 10 - 4;  // 6px dot + 4px spacing
        int startX = (SCREEN_WIDTH - totalWidth) / 2;
        
        for (int i = 0; i < _memberCount; i++) {
            uint16_t color = (i == _currentIndex) ? COLOR_ACCENT_PRIMARY : COLOR_TEXT_MUTED;
            int dotX = startX + i * 10;
            _display.fillCircle(dotX, dotY, 3, color);
        }
    }
    
    // Draw large avatar in center (last due to PNG rendering side effects)
    drawAvatar(member.initial, member.avatarName, SCREEN_WIDTH / 2, AVATAR_CENTER_Y, AVATAR_LARGE_RADIUS);
}

void SelectChildScreen::drawAvatar(char initial, const char* avatarName, int centerX, int centerY, int radius) {
    // Try to load and draw PNG avatar if avatarName is provided
    bool pngDrawn = false;
    
    if (avatarName != nullptr && avatarName[0] != '\0') {
        // Construct full path: /avatars/[avatarName]
        // Handle both cases: avatarName with or without .png extension
        char avatarPath[64];
        
        // Check if avatarName already ends with .png
        size_t len = strlen(avatarName);
        bool hasPngExtension = (len > 4 && strcmp(&avatarName[len - 4], ".png") == 0);
        
        if (hasPngExtension) {
            // Already has extension, use as-is
            snprintf(avatarPath, sizeof(avatarPath), "/avatars/%s", avatarName);
        } else {
            // No extension, append .png
            snprintf(avatarPath, sizeof(avatarPath), "/avatars/%s.png", avatarName);
        }
        
        // Try to load PNG from LittleFS
        if (LittleFS.exists(avatarPath)) {
            // Draw background circle first (in case PNG has transparency)
            _display.fillCircle(centerX, centerY, radius, COLOR_AVATAR_PRIMARY);
            
            // Load and draw PNG centered in the circle
            File avatarFile = LittleFS.open(avatarPath, "r");
            if (avatarFile) {
                // Calculate top-left position to center the image in the circle
                // PNG is 50x50, avatar diameter is 50 (radius 25), so no scaling needed
                int imgX = centerX - radius;
                int imgY = centerY - radius;
                
                // Draw PNG at position
                _display.drawPng(&avatarFile, imgX, imgY);
                
                avatarFile.close();
                pngDrawn = true;
                
                // Draw border after PNG
                _display.drawCircle(centerX, centerY, radius, COLOR_AVATAR_BORDER);
                _display.drawCircle(centerX, centerY, radius + 1, COLOR_AVATAR_BORDER);
            }
        }
    }
    
    // Fallback: Draw initial letter if PNG not available
    if (!pngDrawn) {
        // Draw circular avatar with primary color
        _display.fillCircle(centerX, centerY, radius, COLOR_AVATAR_PRIMARY);
        
        // Draw border
        _display.drawCircle(centerX, centerY, radius, COLOR_AVATAR_BORDER);
        _display.drawCircle(centerX, centerY, radius + 1, COLOR_AVATAR_BORDER);
        
        // Draw initial centered in circle
        _display.setTextColor(COLOR_TEXT_PRIMARY);
        _display.setFont(&fonts::Font4);  // Larger font for bigger avatar
        _display.setTextSize(1);
        
        char initialStr[2] = {initial, '\0'};
        int textWidth = _display.textWidth(initialStr);
        int textX = centerX - textWidth / 2;
        int textY = centerY - 12;  // Approximate vertical center for Font4
        
        _display.setCursor(textX, textY);
        _display.print(initialStr);
    }
}

void SelectChildScreen::drawName(const char* name) {
    // Set text properties
    _display.setTextDatum(textdatum_t::top_center);
    _display.setTextColor(COLOR_TEXT_PRIMARY, COLOR_BACKGROUND);
    _display.setFont(&fonts::Font2);
    _display.setTextSize(1);
    
    // Draw name centered
    _display.drawString(name, SCREEN_WIDTH / 2, NAME_Y);
}

void SelectChildScreen::drawChevrons() {
    // Left chevron (previous)
    drawChevronLeft(CHEVRON_LEFT_X, CHEVRON_Y);
    
    // Right chevron (next)
    drawChevronRight(CHEVRON_RIGHT_X, CHEVRON_Y);
}

void SelectChildScreen::drawChevronLeft(int x, int y) {
    // Draw < shape
    int size = 10;
    uint16_t color = COLOR_TEXT_SECONDARY;
    
    // Draw lines for chevron
    _display.drawLine(x + size, y - size, x, y, color);
    _display.drawLine(x, y, x + size, y + size, color);
    
    // Make it thicker
    _display.drawLine(x + size, y - size + 1, x, y + 1, color);
    _display.drawLine(x, y + 1, x + size, y + size + 1, color);
}

void SelectChildScreen::drawChevronRight(int x, int y) {
    // Draw > shape
    int size = 10;
    uint16_t color = COLOR_TEXT_SECONDARY;
    
    // Draw lines for chevron
    _display.drawLine(x - size, y - size, x, y, color);
    _display.drawLine(x, y, x - size, y + size, color);
    
    // Make it thicker
    _display.drawLine(x - size, y - size + 1, x, y + 1, color);
    _display.drawLine(x, y + 1, x - size, y + size + 1, color);
}

void SelectChildScreen::drawLoadingState() {
    _display.setTextColor(COLOR_TEXT_SECONDARY);
    _display.setFont(&fonts::Font2);
    _display.setTextSize(1);
    
    const char* msg = "Loading...";
    int textWidth = _display.textWidth(msg);
    _display.setCursor((SCREEN_WIDTH - textWidth) / 2, SCREEN_HEIGHT / 2 - 8);
    _display.print(msg);
}

// ============================================================================
// Navigation
// ============================================================================

void SelectChildScreen::selectNext() {
    if (_memberCount <= 1) return;
    
    _currentIndex = (_currentIndex + 1) % _memberCount;
    Serial.printf("[SelectChildScreen] Next: %d (%s)\n", 
                  _currentIndex, _members[_currentIndex].name);
}

void SelectChildScreen::selectPrevious() {
    if (_memberCount <= 1) return;
    
    _currentIndex = (_currentIndex - 1 + _memberCount) % _memberCount;
    Serial.printf("[SelectChildScreen] Previous: %d (%s)\n", 
                  _currentIndex, _members[_currentIndex].name);
}

void SelectChildScreen::confirmSelection() {
    if (_currentIndex < 0 || _currentIndex >= _memberCount) {
        return;
    }
    
    const FamilyMember& member = _members[_currentIndex];
    
    // Update AppState with selected child
    AppState& appState = AppState::getInstance();
    UserSession& session = appState.getSession();
    
    strncpy(session.selectedChildId, member.id, sizeof(session.selectedChildId) - 1);
    strncpy(session.selectedChildName, member.name, sizeof(session.selectedChildName) - 1);
    session.selectedChildInitial = member.initial;
    strncpy(session.selectedChildAvatarName, member.avatarName, sizeof(session.selectedChildAvatarName) - 1);
    
    // Ensure family ID is in session (copy from ApiClient if available)
    if (_apiClient != nullptr && _apiClient->hasFamilyId() && session.familyId[0] == '\0') {
        strncpy(session.familyId, _apiClient->getFamilyId(), sizeof(session.familyId) - 1);
        session.familyId[sizeof(session.familyId) - 1] = '\0';
        Serial.printf("[SelectChildScreen] Family ID copied to session: %s\n", session.familyId);
    }
    
    // Save to persistence
    appState.saveSessionToPersistence();
    
    Serial.printf("[SelectChildScreen] Confirmed selection: %s (%s)\n", 
                  member.name, member.id);
    
    // Navigate to main screen
    if (_screenManager != nullptr) {
        _screenManager->navigateTo(ScreenType::MAIN);
    }
}
