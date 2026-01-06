/**
 * network.cpp - Network and Sync implementations
 * 
 * Real WiFi connection, NTP sync, and RTC setting.
 * 
 * Phase 6 Enhancements:
 * - Automatic WiFi lifecycle with keep-alive timer (R5.2-R5.4)
 * - Polling mode for login/more-time requests (R5.5)
 * - NTP sync frequency control (only sync if interval exceeded)
 * 
 * @author Screen Time Tracker
 * @version 2.0
 */

#include "network.h"
#include "persistence.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <string.h>
#include <time.h>

// SNTP header compatibility
#if __has_include(<esp_sntp.h>)
  #include <esp_sntp.h>
  #define SNTP_ENABLED 1
#elif __has_include(<sntp.h>)
  #include <sntp.h>
  #define SNTP_ENABLED 1
#else
  #define SNTP_ENABLED 0
#endif

// ============================================================================
// NetworkManager Implementation
// ============================================================================

NetworkManager::NetworkManager()
    : _status(NetworkStatus::DISCONNECTED)
    , _lastActivityMs(0)
    , _keepAliveDurationMs(WIFI_KEEPALIVE_MS)
    , _pollingMode(false)
{
}

bool NetworkManager::begin() {
    Serial.println("[Network] Network subsystem initialized");
    WiFi.mode(WIFI_STA);
    _lastActivityMs = 0;  // No activity yet
    _pollingMode = false;
    return true;
}

// ============================================================================
// Keep-Alive Management (R5.2-R5.4)
// ============================================================================

void NetworkManager::update() {
    // Skip auto-disconnect if not connected or in polling mode
    if (!isConnected() || _pollingMode) {
        return;
    }
    
    // Check if keep-alive has expired
    uint32_t now = millis();
    if (_lastActivityMs > 0 && (now - _lastActivityMs) > _keepAliveDurationMs) {
        Serial.println("[Network] Keep-alive expired, auto-disconnecting");
        disconnect();
    }
}

void NetworkManager::resetKeepAliveTimer() {
    _lastActivityMs = millis();
}

void NetworkManager::extendKeepAlive() {
    if (isConnected()) {
        resetKeepAliveTimer();
        Serial.println("[Network] Keep-alive timer extended");
    }
}

// ============================================================================
// Auto-connect (R5.1)
// ============================================================================

bool NetworkManager::ensureConnected() {
    // If already connected, just reset keep-alive timer
    if (isConnected()) {
        resetKeepAliveTimer();
        return true;
    }
    
    // Not connected - try to connect using default credentials
    Serial.println("[Network] Auto-connecting via ensureConnected()...");
    bool success = connect();
    
    if (success) {
        resetKeepAliveTimer();
    }
    
    return success;
}

// ============================================================================
// Polling Mode (R5.5)
// ============================================================================

void NetworkManager::beginPollingMode() {
    _pollingMode = true;
    Serial.println("[Network] Polling mode ENABLED - WiFi will stay connected");
    
    // Ensure we're connected when entering polling mode
    if (!isConnected()) {
        ensureConnected();
    }
}

void NetworkManager::endPollingMode() {
    _pollingMode = false;
    Serial.println("[Network] Polling mode DISABLED - normal keep-alive behavior");
    
    // Reset keep-alive timer so we don't immediately disconnect
    resetKeepAliveTimer();
}

bool NetworkManager::isInPollingMode() const {
    return _pollingMode;
}

bool NetworkManager::connect(const char* ssid, const char* password, uint32_t timeoutMs) {
    // Check if WiFi is properly configured
    if (!isWifiConfigured()) {
        Serial.println("[Network] ERROR: WiFi not configured!");
        Serial.println("[Network] Copy credentials.example.h to credentials.h and add your WiFi credentials");
        _status = NetworkStatus::ERROR;
        _wifiNotConfigured = true;
        return false;
    }
    _wifiNotConfigured = false;
    
    Serial.printf("[Network] Connecting to WiFi '%s'...\n", ssid);
    
    _status = NetworkStatus::CONNECTING;
    
    WiFi.begin(ssid, password);
    
    uint32_t startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > timeoutMs) {
            Serial.println("[Network] Connection timeout");
            _status = NetworkStatus::ERROR;
            WiFi.disconnect();
            return false;
        }
        M5.delay(100);
        Serial.print(".");
    }
    
    Serial.println();
    Serial.printf("[Network] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    _status = NetworkStatus::CONNECTED;
    
    // Reset keep-alive timer on successful connection
    resetKeepAliveTimer();
    
    return true;
}

void NetworkManager::disconnect() {
    // Don't disconnect if in polling mode (R5.5)
    if (_pollingMode) {
        Serial.println("[Network] Disconnect requested but in polling mode - ignoring");
        return;
    }
    
    forceDisconnect();
}

void NetworkManager::forceDisconnect() {
    Serial.println("[Network] Force disconnecting from WiFi");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    _status = NetworkStatus::DISCONNECTED;
    _lastActivityMs = 0;  // Clear keep-alive timer
}

NetworkStatus NetworkManager::getStatus() const {
    return _status;
}

bool NetworkManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

int NetworkManager::getSignalStrength() const {
    if (isConnected()) {
        return WiFi.RSSI();
    }
    return 0;
}

bool NetworkManager::withConnection(NetworkCallback callback) {
    // Connect to WiFi using default credentials
    if (!connect()) {
        return false;
    }
    
    // Execute the callback
    bool result = callback();
    
    // Always disconnect after (legacy pattern)
    forceDisconnect();  // Force disconnect to bypass polling mode check
    
    return result;
}

// ============================================================================
// NTP Sync with Frequency Control (Phase 6.7)
// ============================================================================

bool NetworkManager::isNtpSyncNeeded() const {
    int64_t lastNtpSync = PersistenceManager::getInstance().getLastNtpSyncTime();
    
    if (lastNtpSync == 0) {
        Serial.println("[Network] NTP sync needed - never synced before");
        return true;
    }
    
    // Get current time from RTC
    time_t currentTime = time(nullptr);
    
    // Calculate hours since last sync
    int64_t secondsSinceSync = (int64_t)currentTime - lastNtpSync;
    int hoursSinceSync = (int)(secondsSinceSync / 3600);
    
    if (hoursSinceSync >= NTP_SYNC_INTERVAL_HOURS) {
        Serial.printf("[Network] NTP sync needed - %d hours since last sync\n", hoursSinceSync);
        return true;
    }
    
    Serial.printf("[Network] NTP sync not needed - only %d hours since last sync (interval: %d hours)\n", 
                  hoursSinceSync, NTP_SYNC_INTERVAL_HOURS);
    return false;
}

bool NetworkManager::syncTimeAndSetRTC(bool force) {
    if (!isConnected()) {
        Serial.println("[Network] Cannot sync time - not connected");
        return false;
    }
    
    // Check if sync is needed (unless forced)
    if (!force && !isNtpSyncNeeded()) {
        Serial.println("[Network] Skipping NTP sync - recent sync exists");
        return true;  // Return success since we have valid time
    }
    
    // Check if RTC is available
    if (!M5.Rtc.isEnabled()) {
        Serial.println("[Network] RTC not found");
        return false;
    }
    
    Serial.println("[Network] Syncing time with NTP...");
    
    // Configure timezone and NTP servers
    configTzTime(NTP_TIMEZONE, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
    
    // Wait for NTP sync
    uint32_t startTime = millis();
    
#if SNTP_ENABLED
    while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
        if (millis() - startTime > NTP_SYNC_TIMEOUT_MS) {
            Serial.println("[Network] NTP sync timeout");
            return false;
        }
        M5.delay(100);
        Serial.print(".");
    }
#else
    M5.delay(1600);  // Initial delay for older implementations
    struct tm timeInfo;
    while (!getLocalTime(&timeInfo, 1000)) {
        if (millis() - startTime > NTP_SYNC_TIMEOUT_MS) {
            Serial.println("[Network] NTP sync timeout");
            return false;
        }
        Serial.print(".");
    }
#endif
    
    Serial.println();
    Serial.println("[Network] NTP sync complete");
    
    // Wait for the next second boundary for precise sync
    time_t t = time(nullptr) + 1;
    while (t > time(nullptr)) {
        M5.delay(10);
    }
    
    // Set the RTC to UTC time
    M5.Rtc.setDateTime(gmtime(&t));
    
    // Save the NTP sync timestamp to persistence
    PersistenceManager::getInstance().saveLastNtpSyncTime((int64_t)t);
    
    // Verify RTC was set
    auto dt = M5.Rtc.getDateTime();
    Serial.printf("[Network] RTC set to: %04d/%02d/%02d %02d:%02d:%02d UTC\n",
                  dt.date.year, dt.date.month, dt.date.date,
                  dt.time.hours, dt.time.minutes, dt.time.seconds);
    
    return true;
}

// ============================================================================
// SyncManager Implementation (Stubs)
// ============================================================================

SyncManager::SyncManager(NetworkManager& network)
    : _network(network)
    , _status(SyncStatus::IDLE)
    , _lastSyncTime(0)
{
    _serverUrl[0] = '\0';
}

bool SyncManager::begin(const char* serverUrl) {
    if (serverUrl != nullptr) {
        strncpy(_serverUrl, serverUrl, sizeof(_serverUrl) - 1);
        _serverUrl[sizeof(_serverUrl) - 1] = '\0';
    }
    
    Serial.printf("[Sync] STUB: Sync manager initialized with server: %s\n",
                  _serverUrl[0] ? _serverUrl : "(none)");
    return true;
}

bool SyncManager::syncRemainingTime(const char* userId, uint32_t remainingSeconds) {
    Serial.printf("[Sync] STUB: Syncing remaining time for user '%s': %lu seconds\n",
                  userId, (unsigned long)remainingSeconds);
    
    _status = SyncStatus::SUCCESS;
    _lastSyncTime = millis() / 1000;  // Simplified timestamp
    
    return true;
}

bool SyncManager::fetchRemainingTime(const char* userId, uint32_t& outSeconds) {
    Serial.printf("[Sync] STUB: Fetching remaining time for user '%s'\n", userId);
    
    // Return 0 - this stub is not used, real implementation uses ApiClient
    outSeconds = 0;
    
    _status = SyncStatus::SUCCESS;
    return true;
}

bool SyncManager::syncTimerStarted(const char* userId) {
    Serial.printf("[Sync] STUB: Timer started event for user '%s'\n", userId);
    
    _status = SyncStatus::SUCCESS;
    _lastSyncTime = millis() / 1000;
    
    return true;
}

bool SyncManager::syncTimerStopped(const char* userId, uint32_t remainingSeconds) {
    Serial.printf("[Sync] STUB: Timer stopped for user '%s' with %lu seconds remaining\n",
                  userId, (unsigned long)remainingSeconds);
    
    _status = SyncStatus::SUCCESS;
    _lastSyncTime = millis() / 1000;
    
    return true;
}

bool SyncManager::fetchUserConfig(const char* userId,
                                   uint32_t& outDailyAllowance,
                                   char* outUserName,
                                   size_t nameBufferSize) {
    Serial.printf("[Sync] STUB: Fetching config for user '%s'\n", userId);
    
    // Return 0 - this stub is not used, real implementation uses ApiClient
    outDailyAllowance = 0;
    
    if (outUserName != nullptr && nameBufferSize > 0) {
        strncpy(outUserName, DEFAULT_USER_NAME, nameBufferSize - 1);
        outUserName[nameBufferSize - 1] = '\0';
    }
    
    _status = SyncStatus::SUCCESS;
    return true;
}

bool SyncManager::syncTime() {
    Serial.println("[Sync] STUB: NTP time sync requested");
    
    // In real implementation, would sync with NTP server
    // For now, just mark as successful
    
    _status = SyncStatus::SUCCESS;
    return true;
}

SyncStatus SyncManager::getStatus() const {
    return _status;
}

uint32_t SyncManager::getLastSyncTime() const {
    return _lastSyncTime;
}
