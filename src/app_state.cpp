/**
 * app_state.cpp - Application State implementation
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#include "app_state.h"
#include "persistence.h"
#include <time.h>
#include <Arduino.h>

// ============================================================================
// Singleton Instance
// ============================================================================

AppState& AppState::getInstance() {
    static AppState instance;
    return instance;
}

// ============================================================================
// Constructor
// ============================================================================

AppState::AppState()
    : _session()
    , _screenTime()
    , _schedule()
    , _networkStatus(NetworkStatus::DISCONNECTED)
{
    // Initialize with default user name
    strncpy(_session.username, DEFAULT_USER_NAME, MAX_NAME_LENGTH - 1);
    _session.username[MAX_NAME_LENGTH - 1] = '\0';
    _session.selectedChildInitial = DEFAULT_USER_INITIAL;
}

// ============================================================================
// User Session
// ============================================================================

UserSession& AppState::getSession() {
    return _session;
}

const UserSession& AppState::getSession() const {
    return _session;
}

bool AppState::isLoggedIn() const {
    return _session.isLoggedIn && _session.apiKey[0] != '\0';
}

bool AppState::hasSelectedChild() const {
    return _session.selectedChildId[0] != '\0';
}

char AppState::getAvatarInitial() const {
    if (hasSelectedChild() && _session.selectedChildInitial != '\0') {
        return _session.selectedChildInitial;
    }
    return DEFAULT_USER_INITIAL;
}

const char* AppState::getDisplayName() const {
    if (hasSelectedChild() && _session.selectedChildName[0] != '\0') {
        return _session.selectedChildName;
    }
    if (_session.username[0] != '\0') {
        return _session.username;
    }
    return DEFAULT_USER_NAME;
}

// ============================================================================
// Screen Time
// ============================================================================

ScreenTimeData& AppState::getScreenTime() {
    return _screenTime;
}

const ScreenTimeData& AppState::getScreenTime() const {
    return _screenTime;
}

// ============================================================================
// Network Status
// ============================================================================

NetworkStatus AppState::getNetworkStatus() const {
    return _networkStatus;
}

void AppState::setNetworkStatus(NetworkStatus status) {
    _networkStatus = status;
}

// ============================================================================
// Day Change Detection
// ============================================================================

bool AppState::hasWeekdayChanged() const {
    // If never set, consider it changed (need initial sync)
    if (_screenTime.lastActiveWeekday == 0xFF) {
        return true;
    }
    
    uint8_t currentDay = getCurrentWeekday();
    return currentDay != _screenTime.lastActiveWeekday;
}

void AppState::updateLastActiveWeekday() {
    _screenTime.lastActiveWeekday = getCurrentWeekday();
    Serial.printf("[AppState] Updated last active weekday to %d\n", 
                  _screenTime.lastActiveWeekday);
}

uint8_t AppState::getCurrentWeekday() const {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    return static_cast<uint8_t>(timeinfo.tm_wday);  // 0 = Sunday
}

// ============================================================================
// Screen Flow
// ============================================================================

ScreenType AppState::determineInitialScreen() const {
    // Check login state and navigate accordingly (per Phase 4)
    if (!isLoggedIn()) {
        Serial.println("[AppState] Not logged in -> LOGIN screen");
        return ScreenType::LOGIN;
    }
    
    if (!hasSelectedChild()) {
        Serial.println("[AppState] No child selected -> SELECT_CHILD screen");
        return ScreenType::SELECT_CHILD;
    }
    
    Serial.println("[AppState] Logged in with child -> MAIN screen");
    return ScreenType::MAIN;
}

// ============================================================================
// Persistence (R8)
// ============================================================================

bool AppState::loadFromPersistence() {
    PersistenceManager& persistence = PersistenceManager::getInstance();
    
    Serial.println("[AppState] Loading state from persistence...");
    
    // Load session data
    bool hadSession = persistence.loadSession(_session);
    
    // Load weekday for day-change detection
    uint8_t storedWeekday = persistence.loadLastActiveWeekday();
    if (storedWeekday <= 6) {
        _screenTime.lastActiveWeekday = storedWeekday;
    }
    
    // Load cached daily allowance (for offline display)
    uint32_t cachedAllowance = persistence.loadDailyAllowance();
    if (cachedAllowance > 0) {
        _screenTime.dailyAllowanceSeconds = cachedAllowance;
        // Only update remaining if not currently running (avoid overwriting RTC state)
        if (!_screenTime.isActive) {
            _screenTime.remainingSeconds = cachedAllowance;
        }
    }
    
    // Load unlimited allowance flag
    _screenTime.hasUnlimitedAllowance = persistence.loadUnlimitedAllowance();
    
    // Load last sync timestamp
    _screenTime.lastSyncTimestamp = persistence.getLastSyncTime();
    
    if (hadSession) {
        Serial.printf("[AppState] Session restored (user: %s, child: %s)\n",
                      _session.username,
                      _session.selectedChildName[0] ? _session.selectedChildName : "none");
    } else {
        Serial.println("[AppState] No stored session (first run or logged out)");
    }
    
    return hadSession;
}

bool AppState::saveSessionToPersistence() {
    PersistenceManager& persistence = PersistenceManager::getInstance();
    
    bool success = persistence.saveSession(_session);
    
    if (success) {
        Serial.println("[AppState] Session saved to persistence");
    } else {
        Serial.println("[AppState] ERROR: Failed to save session");
    }
    
    return success;
}

bool AppState::saveWeekdayToPersistence() {
    PersistenceManager& persistence = PersistenceManager::getInstance();
    
    bool success = persistence.saveLastActiveWeekday(_screenTime.lastActiveWeekday);
    
    if (success) {
        Serial.printf("[AppState] Weekday %d saved to persistence\n", 
                      _screenTime.lastActiveWeekday);
    } else {
        Serial.println("[AppState] ERROR: Failed to save weekday");
    }
    
    return success;
}

bool AppState::saveAllowanceToPersistence() {
    PersistenceManager& persistence = PersistenceManager::getInstance();
    
    bool success = true;
    success &= persistence.saveDailyAllowance(_screenTime.dailyAllowanceSeconds);
    success &= persistence.saveUnlimitedAllowance(_screenTime.hasUnlimitedAllowance);
    
    // Also update sync timestamp
    time_t now;
    time(&now);
    _screenTime.lastSyncTimestamp = (int64_t)now;
    success &= persistence.saveLastSyncTime(_screenTime.lastSyncTimestamp);
    
    if (success) {
        Serial.printf("[AppState] Allowance %lu seconds (unlimited=%d) saved to persistence\n",
                      (unsigned long)_screenTime.dailyAllowanceSeconds,
                      _screenTime.hasUnlimitedAllowance);
    } else {
        Serial.println("[AppState] ERROR: Failed to save allowance");
    }
    
    return success;
}

bool AppState::clearPersistence() {
    PersistenceManager& persistence = PersistenceManager::getInstance();
    
    Serial.println("[AppState] Clearing persistence and resetting state...");
    
    // Clear persistent storage
    bool success = persistence.clearSession();
    
    // Reset session to defaults
    _session = UserSession();
    strncpy(_session.username, DEFAULT_USER_NAME, MAX_NAME_LENGTH - 1);
    _session.username[MAX_NAME_LENGTH - 1] = '\0';
    _session.selectedChildInitial = DEFAULT_USER_INITIAL;
    
    // Reset screen time to defaults (keep allowance but reset running state)
    _screenTime.isActive = false;
    _screenTime.usedTodaySeconds = 0;
    _screenTime.remainingSeconds = _screenTime.dailyAllowanceSeconds;
    
    if (success) {
        Serial.println("[AppState] Persistence cleared and state reset");
    } else {
        Serial.println("[AppState] ERROR: Failed to clear persistence");
    }
    
    return success;
}
