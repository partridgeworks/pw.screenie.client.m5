/**
 * persistence.cpp - Data Persistence Implementation
 * 
 * Uses ESP32 NVS (Non-Volatile Storage) via Preferences library
 * for storing session data and application state.
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#include "persistence.h"
#include "app_state.h"
#include "config.h"
#include <Arduino.h>

// ============================================================================
// Singleton Instance
// ============================================================================

PersistenceManager& PersistenceManager::getInstance() {
    static PersistenceManager instance;
    return instance;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

PersistenceManager::PersistenceManager()
    : _initialized(false)
    , _namespaceOpen(false)
{
}

PersistenceManager::~PersistenceManager() {
    if (_namespaceOpen) {
        _prefs.end();
    }
}

// ============================================================================
// Initialization
// ============================================================================

bool PersistenceManager::begin() {
    if (_initialized) {
        Serial.println("[Persistence] Already initialized");
        return true;
    }
    
    Serial.println("[Persistence] Initializing NVS storage...");
    
    // Try to open namespace to verify NVS is working
    // Note: First open as read-write to ensure namespace is created
    bool opened = _prefs.begin(NVS_NAMESPACE, false);  // false = read-write mode
    if (!opened) {
        Serial.printf("[Persistence] ERROR: Failed to open namespace '%s'\n", NVS_NAMESPACE);
        return false;
    }
    
    _namespaceOpen = true;
    Serial.printf("[Persistence] Namespace '%s' opened successfully\n", NVS_NAMESPACE);
    
    // Check and migrate data if needed
    migrateDataIfNeeded();
    
    // Close namespace after init check
    _prefs.end();
    _namespaceOpen = false;
    
    _initialized = true;
    Serial.println("[Persistence] NVS storage initialized successfully");
    
    return true;
}

bool PersistenceManager::isInitialized() const {
    return _initialized;
}

bool PersistenceManager::openNamespace(bool readOnly) {
    if (_namespaceOpen) {
        return true;
    }
    
    if (!_prefs.begin(NVS_NAMESPACE, readOnly)) {
        Serial.printf("[Persistence] Failed to open namespace '%s'\n", NVS_NAMESPACE);
        return false;
    }
    
    _namespaceOpen = true;
    return true;
}

void PersistenceManager::closeNamespace() {
    if (_namespaceOpen) {
        _prefs.end();
        _namespaceOpen = false;
    }
}

void PersistenceManager::migrateDataIfNeeded() {
    uint8_t storedVersion = _prefs.getUChar(KEY_DATA_VERSION, 0);
    
    if (storedVersion == 0) {
        // First run or pre-versioned data - nothing to migrate
        Serial.println("[Persistence] No stored data version, setting to current");
        // Will be set on first save
    } else if (storedVersion < PERSISTENCE_DATA_VERSION) {
        Serial.printf("[Persistence] Migrating data from v%d to v%d\n", 
                      storedVersion, PERSISTENCE_DATA_VERSION);
        
        // Add migration logic here for future versions
        // Example:
        // if (storedVersion < 2) {
        //     // Migrate from v1 to v2
        // }
    }
}

// ============================================================================
// Session Persistence (R8.1)
// ============================================================================

bool PersistenceManager::saveSession(const UserSession& session) {
    if (!_initialized) {
        Serial.println("[Persistence] ERROR: Not initialized");
        return false;
    }
    
    if (!openNamespace(false)) {  // Read-write mode
        return false;
    }
    
    Serial.println("[Persistence] Saving session...");
    
    bool success = true;
    
    // Save login status
    success &= (_prefs.putBool(KEY_IS_LOGGED_IN, session.isLoggedIn) > 0 || 
                session.isLoggedIn == false);  // putBool returns 0 for false value but still succeeds
    
    // Save API key
    if (session.apiKey[0] != '\0') {
        success &= (_prefs.putString(KEY_API_KEY, session.apiKey) > 0);
    } else {
        _prefs.remove(KEY_API_KEY);
    }
    
    // Save family ID
    if (session.familyId[0] != '\0') {
        success &= (_prefs.putString(KEY_FAMILY_ID, session.familyId) > 0);
    } else {
        _prefs.remove(KEY_FAMILY_ID);
    }
    
    // Save username
    if (session.username[0] != '\0') {
        success &= (_prefs.putString(KEY_USERNAME, session.username) > 0);
    } else {
        _prefs.remove(KEY_USERNAME);
    }
    
    // Save selected child info
    if (session.selectedChildId[0] != '\0') {
        success &= (_prefs.putString(KEY_CHILD_ID, session.selectedChildId) > 0);
    } else {
        _prefs.remove(KEY_CHILD_ID);
    }
    
    if (session.selectedChildName[0] != '\0') {
        success &= (_prefs.putString(KEY_CHILD_NAME, session.selectedChildName) > 0);
    } else {
        _prefs.remove(KEY_CHILD_NAME);
    }
    
    _prefs.putChar(KEY_CHILD_INITIAL, session.selectedChildInitial);
    
    // Save selected child avatar
    if (session.selectedChildAvatarName[0] != '\0') {
        success &= (_prefs.putString(KEY_CHILD_AVATAR, session.selectedChildAvatarName) > 0);
    } else {
        _prefs.remove(KEY_CHILD_AVATAR);
    }
    
    // Save data version
    _prefs.putUChar(KEY_DATA_VERSION, PERSISTENCE_DATA_VERSION);
    
    closeNamespace();
    
    if (success) {
        Serial.printf("[Persistence] Session saved (logged in: %s, child: %s)\n",
                      session.isLoggedIn ? "yes" : "no",
                      session.selectedChildName[0] ? session.selectedChildName : "none");
    } else {
        Serial.println("[Persistence] ERROR: Failed to save some session data");
    }
    
    return success;
}

bool PersistenceManager::loadSession(UserSession& session) {
    if (!_initialized) {
        Serial.println("[Persistence] ERROR: Not initialized");
        return false;
    }
    
    if (!openNamespace(true)) {  // Read-only mode
        return false;
    }
    
    Serial.println("[Persistence] Loading session...");
    
    // Check if logged in
    bool isLoggedIn = _prefs.getBool(KEY_IS_LOGGED_IN, false);
    
    if (!isLoggedIn) {
        closeNamespace();
        Serial.println("[Persistence] No stored session (not logged in)");
        return false;
    }
    
    // Load session data
    session.isLoggedIn = true;
    
    // Load API key
    String apiKey = _prefs.getString(KEY_API_KEY, "");
    if (apiKey.length() > 0) {
        strncpy(session.apiKey, apiKey.c_str(), sizeof(session.apiKey) - 1);
        session.apiKey[sizeof(session.apiKey) - 1] = '\0';
    } else {
        session.apiKey[0] = '\0';
    }
    
    // Load family ID
    String familyId = _prefs.getString(KEY_FAMILY_ID, "");
    if (familyId.length() > 0) {
        strncpy(session.familyId, familyId.c_str(), sizeof(session.familyId) - 1);
        session.familyId[sizeof(session.familyId) - 1] = '\0';
    } else {
        session.familyId[0] = '\0';
    }
    
    // Load username
    String username = _prefs.getString(KEY_USERNAME, "");
    if (username.length() > 0) {
        strncpy(session.username, username.c_str(), sizeof(session.username) - 1);
        session.username[sizeof(session.username) - 1] = '\0';
    } else {
        strncpy(session.username, DEFAULT_USER_NAME, sizeof(session.username) - 1);
        session.username[sizeof(session.username) - 1] = '\0';
    }
    
    // Load selected child info
    String childId = _prefs.getString(KEY_CHILD_ID, "");
    if (childId.length() > 0) {
        strncpy(session.selectedChildId, childId.c_str(), sizeof(session.selectedChildId) - 1);
        session.selectedChildId[sizeof(session.selectedChildId) - 1] = '\0';
    } else {
        session.selectedChildId[0] = '\0';
    }
    
    String childName = _prefs.getString(KEY_CHILD_NAME, "");
    if (childName.length() > 0) {
        strncpy(session.selectedChildName, childName.c_str(), sizeof(session.selectedChildName) - 1);
        session.selectedChildName[sizeof(session.selectedChildName) - 1] = '\0';
    } else {
        session.selectedChildName[0] = '\0';
    }
    
    session.selectedChildInitial = _prefs.getChar(KEY_CHILD_INITIAL, DEFAULT_USER_INITIAL);
    
    // Load selected child avatar
    String avatarName = _prefs.getString(KEY_CHILD_AVATAR, "");
    if (avatarName.length() > 0) {
        strncpy(session.selectedChildAvatarName, avatarName.c_str(), sizeof(session.selectedChildAvatarName) - 1);
        session.selectedChildAvatarName[sizeof(session.selectedChildAvatarName) - 1] = '\0';
    } else {
        session.selectedChildAvatarName[0] = '\0';
    }
    
    closeNamespace();
    
    Serial.printf("[Persistence] Session loaded (user: %s, child: %s)\n",
                  session.username,
                  session.selectedChildName[0] ? session.selectedChildName : "none");
    
    return true;
}

bool PersistenceManager::clearSession() {
    if (!_initialized) {
        Serial.println("[Persistence] ERROR: Not initialized");
        return false;
    }
    
    if (!openNamespace(false)) {  // Read-write mode
        return false;
    }
    
    Serial.println("[Persistence] Clearing session...");
    
    _prefs.remove(KEY_IS_LOGGED_IN);
    _prefs.remove(KEY_API_KEY);
    _prefs.remove(KEY_USERNAME);
    _prefs.remove(KEY_CHILD_ID);
    _prefs.remove(KEY_CHILD_NAME);
    _prefs.remove(KEY_CHILD_INITIAL);
    
    closeNamespace();
    
    Serial.println("[Persistence] Session cleared");
    return true;
}

bool PersistenceManager::hasStoredSession() {
    if (!_initialized) {
        return false;
    }
    
    if (!openNamespace(true)) {
        return false;
    }
    
    bool hasSession = _prefs.getBool(KEY_IS_LOGGED_IN, false);
    closeNamespace();
    
    return hasSession;
}

// ============================================================================
// Weekday Persistence (R8.2)
// ============================================================================

bool PersistenceManager::saveLastActiveWeekday(uint8_t weekday) {
    if (!_initialized) {
        Serial.println("[Persistence] ERROR: Not initialized");
        return false;
    }
    
    if (weekday > 6) {
        Serial.printf("[Persistence] ERROR: Invalid weekday %d\n", weekday);
        return false;
    }
    
    if (!openNamespace(false)) {
        return false;
    }
    
    size_t written = _prefs.putUChar(KEY_LAST_WEEKDAY, weekday);
    closeNamespace();
    
    if (written > 0) {
        Serial.printf("[Persistence] Saved last active weekday: %d\n", weekday);
        return true;
    }
    
    Serial.println("[Persistence] ERROR: Failed to save weekday");
    return false;
}

uint8_t PersistenceManager::loadLastActiveWeekday() {
    if (!_initialized) {
        return 0xFF;
    }
    
    if (!openNamespace(true)) {
        return 0xFF;
    }
    
    uint8_t weekday = _prefs.getUChar(KEY_LAST_WEEKDAY, 0xFF);
    closeNamespace();
    
    if (weekday <= 6) {
        Serial.printf("[Persistence] Loaded last active weekday: %d\n", weekday);
    } else {
        Serial.println("[Persistence] No stored weekday (first run or cleared)");
    }
    
    return weekday;
}

// ============================================================================
// Screen Time Cache (R8.3)
// ============================================================================

bool PersistenceManager::saveDailyAllowance(uint32_t dailyAllowanceSeconds) {
    if (!_initialized) {
        return false;
    }
    
    if (!openNamespace(false)) {
        return false;
    }
    
    size_t written = _prefs.putULong(KEY_DAILY_ALLOWANCE, dailyAllowanceSeconds);
    closeNamespace();
    
    if (written > 0) {
        Serial.printf("[Persistence] Saved daily allowance: %lu seconds\n", 
                      (unsigned long)dailyAllowanceSeconds);
        return true;
    }
    
    return false;
}

uint32_t PersistenceManager::loadDailyAllowance() {
    if (!_initialized) {
        return 0;
    }
    
    if (!openNamespace(true)) {
        return 0;
    }
    
    uint32_t allowance = _prefs.getULong(KEY_DAILY_ALLOWANCE, 0);
    closeNamespace();
    
    if (allowance > 0) {
        Serial.printf("[Persistence] Loaded daily allowance: %lu seconds\n", 
                      (unsigned long)allowance);
    }
    
    return allowance;
}

bool PersistenceManager::saveUnlimitedAllowance(bool hasUnlimitedAllowance) {
    if (!_initialized) {
        return false;
    }
    
    if (!openNamespace(false)) {
        return false;
    }
    
    size_t written = _prefs.putBool(KEY_UNLIMITED_ALLOW, hasUnlimitedAllowance);
    closeNamespace();
    
    if (written > 0 || !hasUnlimitedAllowance) {  // putBool returns 0 for false but still succeeds
        Serial.printf("[Persistence] Saved unlimited allowance flag: %d\n", hasUnlimitedAllowance);
        return true;
    }
    
    return false;
}

bool PersistenceManager::loadUnlimitedAllowance() {
    if (!_initialized) {
        return false;
    }
    
    if (!openNamespace(true)) {
        return false;
    }
    
    bool hasUnlimited = _prefs.getBool(KEY_UNLIMITED_ALLOW, false);
    closeNamespace();
    
    if (hasUnlimited) {
        Serial.println("[Persistence] Loaded unlimited allowance flag: true");
    }
    
    return hasUnlimited;
}

bool PersistenceManager::saveLastSyncTime(int64_t timestamp) {
    if (!_initialized) {
        return false;
    }
    
    if (!openNamespace(false)) {
        return false;
    }
    
    // Store as two 32-bit values since NVS doesn't have native 64-bit support
    uint32_t low = (uint32_t)(timestamp & 0xFFFFFFFF);
    uint32_t high = (uint32_t)((timestamp >> 32) & 0xFFFFFFFF);
    
    bool success = true;
    success &= (_prefs.putULong("lastSyncLo", low) > 0);
    success &= (_prefs.putULong("lastSyncHi", high) > 0);
    
    closeNamespace();
    
    if (success) {
        Serial.printf("[Persistence] Saved last sync time: %lld\n", (long long)timestamp);
    }
    
    return success;
}

int64_t PersistenceManager::getLastSyncTime() {
    if (!_initialized) {
        return 0;
    }
    
    if (!openNamespace(true)) {
        return 0;
    }
    
    uint32_t low = _prefs.getULong("lastSyncLo", 0);
    uint32_t high = _prefs.getULong("lastSyncHi", 0);
    
    closeNamespace();
    
    int64_t timestamp = ((int64_t)high << 32) | low;
    
    if (timestamp > 0) {
        Serial.printf("[Persistence] Loaded last sync time: %lld\n", (long long)timestamp);
    }
    
    return timestamp;
}

// ============================================================================
// NTP Sync Time Persistence (Phase 6.7)
// ============================================================================

bool PersistenceManager::saveLastNtpSyncTime(int64_t timestamp) {
    if (!_initialized) {
        return false;
    }
    
    if (!openNamespace(false)) {
        return false;
    }
    
    // Store as two 32-bit values since NVS doesn't have native 64-bit support
    uint32_t low = (uint32_t)(timestamp & 0xFFFFFFFF);
    uint32_t high = (uint32_t)((timestamp >> 32) & 0xFFFFFFFF);
    
    bool success = true;
    success &= (_prefs.putULong("ntpSyncLo", low) > 0);
    success &= (_prefs.putULong("ntpSyncHi", high) > 0);
    
    closeNamespace();
    
    if (success) {
        Serial.printf("[Persistence] Saved last NTP sync time: %lld\n", (long long)timestamp);
    }
    
    return success;
}

int64_t PersistenceManager::getLastNtpSyncTime() {
    if (!_initialized) {
        return 0;
    }
    
    if (!openNamespace(true)) {
        return 0;
    }
    
    uint32_t low = _prefs.getULong("ntpSyncLo", 0);
    uint32_t high = _prefs.getULong("ntpSyncHi", 0);
    
    closeNamespace();
    
    int64_t timestamp = ((int64_t)high << 32) | low;
    
    if (timestamp > 0) {
        Serial.printf("[Persistence] Loaded last NTP sync time: %lld\n", (long long)timestamp);
    }
    
    return timestamp;
}

// ============================================================================
// Consumed Time Persistence
// ============================================================================

bool PersistenceManager::saveConsumedToday(uint32_t consumedSeconds, uint8_t weekday) {
    if (!_initialized) {
        Serial.println("[Persistence] ERROR: Not initialized");
        return false;
    }
    
    if (weekday > 6) {
        Serial.printf("[Persistence] ERROR: Invalid weekday %d\n", weekday);
        return false;
    }
    
    if (!openNamespace(false)) {
        return false;
    }
    
    bool success = true;
    success &= (_prefs.putULong(KEY_CONSUMED_TODAY, consumedSeconds) > 0);
    success &= (_prefs.putUChar(KEY_CONSUMED_WEEKDAY, weekday) > 0);
    
    closeNamespace();
    
    if (success) {
        Serial.printf("[Persistence] Saved consumed time: %lu sec (weekday %d)\n", 
                      (unsigned long)consumedSeconds, weekday);
    } else {
        Serial.println("[Persistence] ERROR: Failed to save consumed time");
    }
    
    return success;
}

uint32_t PersistenceManager::loadConsumedToday(uint8_t currentWeekday) {
    if (!_initialized) {
        return 0;
    }
    
    if (!openNamespace(true)) {
        return 0;
    }
    
    uint8_t savedWeekday = _prefs.getUChar(KEY_CONSUMED_WEEKDAY, 0xFF);
    uint32_t consumedSeconds = _prefs.getULong(KEY_CONSUMED_TODAY, 0);
    
    closeNamespace();
    
    // Check if it's the same day
    if (savedWeekday != currentWeekday) {
        Serial.printf("[Persistence] Consumed time is from different day (saved=%d, current=%d), returning 0\n",
                      savedWeekday, currentWeekday);
        return 0;
    }
    
    Serial.printf("[Persistence] Loaded consumed time: %lu sec (weekday %d)\n", 
                  (unsigned long)consumedSeconds, currentWeekday);
    
    return consumedSeconds;
}

bool PersistenceManager::clearConsumedToday() {
    if (!_initialized) {
        return false;
    }
    
    if (!openNamespace(false)) {
        return false;
    }
    
    _prefs.remove(KEY_CONSUMED_TODAY);
    _prefs.remove(KEY_CONSUMED_WEEKDAY);
    
    closeNamespace();
    
    Serial.println("[Persistence] Cleared consumed time");
    return true;
}

// ============================================================================
// Brightness Persistence
// ============================================================================

bool PersistenceManager::saveBrightnessLevel(uint8_t level) {
    if (!_initialized) {
        Serial.println("[Persistence] ERROR: Not initialized");
        return false;
    }
    
    if (!openNamespace(false)) {  // Read-write mode
        return false;
    }
    
    bool success = (_prefs.putUChar(KEY_BRIGHTNESS_LEVEL, level) > 0);
    
    closeNamespace();
    
    if (success) {
        Serial.printf("[Persistence] Saved brightness level: %d\n", level);
    } else {
        Serial.println("[Persistence] ERROR: Failed to save brightness level");
    }
    
    return success;
}

uint8_t PersistenceManager::loadBrightnessLevel() {
    if (!_initialized) {
        return 0;
    }
    
    if (!openNamespace(true)) {  // Read-only mode
        return 0;
    }
    
    uint8_t level = _prefs.getUChar(KEY_BRIGHTNESS_LEVEL, 0);
    
    closeNamespace();
    
    Serial.printf("[Persistence] Loaded brightness level: %d\n", level);
    
    return level;
}

// ============================================================================
// Utility
// ============================================================================

bool PersistenceManager::clearAll() {
    if (!_initialized) {
        return false;
    }
    
    if (!openNamespace(false)) {
        return false;
    }
    
    Serial.println("[Persistence] Clearing all stored data...");
    
    bool success = _prefs.clear();
    closeNamespace();
    
    if (success) {
        Serial.println("[Persistence] All data cleared");
    } else {
        Serial.println("[Persistence] ERROR: Failed to clear data");
    }
    
    return success;
}

uint8_t PersistenceManager::getDataVersion() {
    if (!_initialized) {
        return 0;
    }
    
    if (!openNamespace(true)) {
        return 0;
    }
    
    uint8_t version = _prefs.getUChar(KEY_DATA_VERSION, 0);
    closeNamespace();
    
    return version;
}

void PersistenceManager::debugPrint() {
    if (!_initialized) {
        Serial.println("[Persistence] Not initialized");
        return;
    }
    
    if (!openNamespace(true)) {
        return;
    }
    
    Serial.println("=== Persistence Debug Info ===");
    Serial.printf("  Data Version: %d\n", _prefs.getUChar(KEY_DATA_VERSION, 0));
    Serial.printf("  Is Logged In: %s\n", _prefs.getBool(KEY_IS_LOGGED_IN, false) ? "yes" : "no");
    Serial.printf("  API Key: %s\n", _prefs.isKey(KEY_API_KEY) ? "(set)" : "(not set)");
    Serial.printf("  Username: %s\n", _prefs.getString(KEY_USERNAME, "(not set)").c_str());
    Serial.printf("  Child ID: %s\n", _prefs.getString(KEY_CHILD_ID, "(not set)").c_str());
    Serial.printf("  Child Name: %s\n", _prefs.getString(KEY_CHILD_NAME, "(not set)").c_str());
    Serial.printf("  Child Initial: %c\n", _prefs.getChar(KEY_CHILD_INITIAL, '?'));
    Serial.printf("  Last Weekday: %d\n", _prefs.getUChar(KEY_LAST_WEEKDAY, 0xFF));
    Serial.printf("  Daily Allowance: %lu\n", _prefs.getULong(KEY_DAILY_ALLOWANCE, 0));
    Serial.printf("  Free Entries: %d\n", _prefs.freeEntries());
    Serial.println("==============================");
    
    closeNamespace();
}
