/**
 * polling_manager.cpp - Polling Manager implementation
 * 
 * Manages non-blocking polling for login and more-time requests.
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#include "polling_manager.h"
#include "api_client.h"
#include "network.h"
#include <Arduino.h>
#include <cstring>

// ============================================================================
// Default Configuration
// ============================================================================

// Login polling defaults
constexpr uint32_t DEFAULT_LOGIN_POLL_INTERVAL_MS = 5000;      // 5 seconds
constexpr uint32_t DEFAULT_LOGIN_TIMEOUT_MS = 300000;          // 5 minutes

// More-time polling defaults
constexpr uint32_t DEFAULT_MORE_TIME_POLL_INTERVAL_MS = 10000; // 10 seconds  
constexpr uint32_t DEFAULT_MORE_TIME_TIMEOUT_MS = 300000;      // 5 minutes

// ============================================================================
// Constructor / Initialization
// ============================================================================

PollingManager::PollingManager()
    : _api(nullptr)
    , _network(nullptr)
    , _type(PollingType::NONE)
    , _status(PollingStatus::IDLE)
    , _callback(nullptr)
    , _userData(nullptr)
    , _startTimeMs(0)
    , _lastPollMs(0)
    , _loginPollIntervalMs(DEFAULT_LOGIN_POLL_INTERVAL_MS)
    , _loginTimeoutMs(DEFAULT_LOGIN_TIMEOUT_MS)
    , _moreTimePollIntervalMs(DEFAULT_MORE_TIME_POLL_INTERVAL_MS)
    , _moreTimeTimeoutMs(DEFAULT_MORE_TIME_TIMEOUT_MS)
{
    _pollId[0] = '\0';
}

void PollingManager::begin(ApiClient& api, NetworkManager& network) {
    _api = &api;
    _network = &network;
    
    Serial.println("[PollingManager] Initialized");
    Serial.printf("  Login poll interval: %lu ms\n", (unsigned long)_loginPollIntervalMs);
    Serial.printf("  Login timeout: %lu ms\n", (unsigned long)_loginTimeoutMs);
    Serial.printf("  More-time poll interval: %lu ms\n", (unsigned long)_moreTimePollIntervalMs);
    Serial.printf("  More-time timeout: %lu ms\n", (unsigned long)_moreTimeTimeoutMs);
}

// ============================================================================
// Update Loop
// ============================================================================

void PollingManager::update() {
    // Nothing to do if not polling
    if (_type == PollingType::NONE || _status != PollingStatus::POLLING) {
        return;
    }
    
    uint32_t now = millis();
    uint32_t elapsed = now - _startTimeMs;
    uint32_t timeout = getCurrentTimeout();
    uint32_t interval = getCurrentInterval();
    
    // Check for timeout
    if (elapsed >= timeout) {
        Serial.printf("[PollingManager] Polling timed out after %lu ms\n", elapsed);
        
        PollingResult result;
        result.success = false;
        result.timedOut = true;
        strncpy(result.message, "Request timed out", sizeof(result.message) - 1);
        
        completePolling(result);
        return;
    }
    
    // Check if it's time to poll
    if (now - _lastPollMs >= interval) {
        _lastPollMs = now;
        
        switch (_type) {
            case PollingType::LOGIN:
                pollLogin();
                break;
                
            case PollingType::MORE_TIME:
                pollMoreTime();
                break;
                
            default:
                break;
        }
    }
}

// ============================================================================
// Polling Control
// ============================================================================

void PollingManager::startLoginPolling(const char* deviceCode, 
                                        PollingCallback callback, 
                                        void* userData) {
    if (_api == nullptr) {
        Serial.println("[PollingManager] ERROR: ApiClient not set");
        return;
    }
    
    // Stop any existing polling
    if (_type != PollingType::NONE) {
        stopPolling();
    }
    
    // Store polling parameters
    strncpy(_pollId, deviceCode, sizeof(_pollId) - 1);
    _pollId[sizeof(_pollId) - 1] = '\0';
    _callback = callback;
    _userData = userData;
    
    // Start polling
    _type = PollingType::LOGIN;
    _status = PollingStatus::POLLING;
    _startTimeMs = millis();
    _lastPollMs = 0;  // Poll immediately on first update
    
    Serial.printf("[PollingManager] Started login polling for device: %s\n", _pollId);
    Serial.printf("  Interval: %lu ms, Timeout: %lu ms\n", 
                  (unsigned long)_loginPollIntervalMs, 
                  (unsigned long)_loginTimeoutMs);
    
    // Enable polling mode - keeps WiFi connected (Phase 6)
    if (_network) {
        _network->beginPollingMode();
    }
}

void PollingManager::startMoreTimePolling(const char* requestId, 
                                           PollingCallback callback, 
                                           void* userData) {
    if (_api == nullptr) {
        Serial.println("[PollingManager] ERROR: ApiClient not set");
        return;
    }
    
    // Stop any existing polling
    if (_type != PollingType::NONE) {
        stopPolling();
    }
    
    // Store polling parameters
    strncpy(_pollId, requestId, sizeof(_pollId) - 1);
    _pollId[sizeof(_pollId) - 1] = '\0';
    _callback = callback;
    _userData = userData;
    
    // Start polling
    _type = PollingType::MORE_TIME;
    _status = PollingStatus::POLLING;
    _startTimeMs = millis();
    _lastPollMs = 0;  // Poll immediately on first update
    
    Serial.printf("[PollingManager] Started more-time polling for request: %s\n", _pollId);
    Serial.printf("  Interval: %lu ms, Timeout: %lu ms\n", 
                  (unsigned long)_moreTimePollIntervalMs, 
                  (unsigned long)_moreTimeTimeoutMs);
    
    // Enable polling mode - keeps WiFi connected (Phase 6)
    if (_network) {
        _network->beginPollingMode();
    }
}

void PollingManager::stopPolling() {
    if (_type == PollingType::NONE) {
        return;
    }
    
    Serial.printf("[PollingManager] Stopping %s polling\n",
                  _type == PollingType::LOGIN ? "login" : "more-time");
    
    // End network polling mode (Phase 6)
    if (_network) {
        _network->endPollingMode();
    }
    
    _type = PollingType::NONE;
    _status = PollingStatus::IDLE;
    _pollId[0] = '\0';
    _callback = nullptr;
    _userData = nullptr;
    _startTimeMs = 0;
    _lastPollMs = 0;
}

// ============================================================================
// Status
// ============================================================================

bool PollingManager::isPolling() const {
    return _type != PollingType::NONE && _status == PollingStatus::POLLING;
}

PollingType PollingManager::getPollingType() const {
    return _type;
}

PollingStatus PollingManager::getStatus() const {
    return _status;
}

uint32_t PollingManager::getRemainingTimeoutSeconds() const {
    if (!isPolling()) {
        return 0;
    }
    
    uint32_t elapsed = millis() - _startTimeMs;
    uint32_t timeout = getCurrentTimeout();
    
    if (elapsed >= timeout) {
        return 0;
    }
    
    return (timeout - elapsed) / 1000;
}

uint32_t PollingManager::getElapsedSeconds() const {
    if (_type == PollingType::NONE) {
        return 0;
    }
    
    return (millis() - _startTimeMs) / 1000;
}

// ============================================================================
// Configuration
// ============================================================================

void PollingManager::setLoginPollInterval(uint32_t intervalMs) {
    _loginPollIntervalMs = intervalMs;
}

void PollingManager::setLoginTimeout(uint32_t timeoutMs) {
    _loginTimeoutMs = timeoutMs;
}

void PollingManager::setMoreTimePollInterval(uint32_t intervalMs) {
    _moreTimePollIntervalMs = intervalMs;
}

void PollingManager::setMoreTimeTimeout(uint32_t timeoutMs) {
    _moreTimeTimeoutMs = timeoutMs;
}

// ============================================================================
// Internal Methods
// ============================================================================

void PollingManager::pollLogin() {
    Serial.printf("[PollingManager] Polling login status (%lu s elapsed)...\n",
                  getElapsedSeconds());
    
    LoginPollResult apiResult = _api->pollLoginStatus(_pollId);
    
    if (!apiResult.success) {
        // API call failed
        PollingResult result;
        result.success = false;
        strncpy(result.message, apiResult.errorMessage, sizeof(result.message) - 1);
        completePolling(result);
        return;
    }
    
    if (apiResult.expired) {
        // Device code expired - user needs to get a new code
        PollingResult result;
        result.success = false;
        result.timedOut = true;
        // Use the error message from API if available, otherwise default
        if (apiResult.errorMessage[0] != '\0') {
            strncpy(result.message, apiResult.errorMessage, sizeof(result.message) - 1);
        } else {
            strncpy(result.message, "Pairing code expired", sizeof(result.message) - 1);
        }
        completePolling(result);
        return;
    }
    
    if (!apiResult.pending) {
        // Login complete!
        PollingResult result;
        result.success = true;
        strncpy(result.message, "Login successful", sizeof(result.message) - 1);
        strncpy(result.apiKey, apiResult.apiKey, sizeof(result.apiKey) - 1);
        strncpy(result.username, apiResult.username, sizeof(result.username) - 1);
        completePolling(result);
    }
    // If still pending, just wait for next poll interval
}

void PollingManager::pollMoreTime() {
    Serial.printf("[PollingManager] Polling more-time status (%lu s elapsed)...\n",
                  getElapsedSeconds());
    
    MoreTimePollResult apiResult = _api->pollMoreTimeStatus(_pollId);
    
    if (!apiResult.success) {
        // API call failed
        PollingResult result;
        result.success = false;
        strncpy(result.message, apiResult.errorMessage, sizeof(result.message) - 1);
        completePolling(result);
        return;
    }
    
    if (apiResult.expired) {
        // Request expired
        PollingResult result;
        result.success = false;
        result.timedOut = true;
        strncpy(result.message, "Request expired", sizeof(result.message) - 1);
        completePolling(result);
        return;
    }
    
    if (!apiResult.pending) {
        // Decision made
        PollingResult result;
        result.success = true;
        result.granted = apiResult.granted;
        result.denied = apiResult.denied;
        result.additionalMinutes = apiResult.additionalMinutes;
        
        if (apiResult.granted) {
            snprintf(result.message, sizeof(result.message), 
                     "Granted %lu extra minutes!", 
                     (unsigned long)apiResult.additionalMinutes);
        } else {
            strncpy(result.message, "Request was denied", sizeof(result.message) - 1);
        }
        
        completePolling(result);
    }
    // If still pending, just wait for next poll interval
}

void PollingManager::completePolling(const PollingResult& result) {
    Serial.printf("[PollingManager] Polling complete: %s\n", result.message);
    
    // Update status
    _status = result.success ? PollingStatus::SUCCESS : 
              result.timedOut ? PollingStatus::TIMEOUT : PollingStatus::ERROR;
    
    // Store callback before clearing state
    PollingCallback callback = _callback;
    void* userData = _userData;
    
    // Clear polling state
    _type = PollingType::NONE;
    _pollId[0] = '\0';
    _callback = nullptr;
    _userData = nullptr;
    
    // End network polling mode (Phase 6 will implement this)
    // if (_network) _network->endPollingMode();
    
    // Invoke callback
    if (callback) {
        callback(result, userData);
    }
}

uint32_t PollingManager::getCurrentInterval() const {
    switch (_type) {
        case PollingType::LOGIN:
            return _loginPollIntervalMs;
        case PollingType::MORE_TIME:
            return _moreTimePollIntervalMs;
        default:
            return 0;
    }
}

uint32_t PollingManager::getCurrentTimeout() const {
    switch (_type) {
        case PollingType::LOGIN:
            return _loginTimeoutMs;
        case PollingType::MORE_TIME:
            return _moreTimeTimeoutMs;
        default:
            return 0;
    }
}
