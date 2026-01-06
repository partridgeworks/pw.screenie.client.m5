/**
 * api_client.cpp - REST API Client implementation
 * 
 * Uses HTTPClient for HTTP requests and ArduinoJson for JSON parsing.
 * Implements real API calls to the Screenie backend.
 * 
 * @author Screen Time Tracker
 * @version 2.0
 */

#include "api_client.h"
#include "network.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <cstring>
#include <time.h>
#include <M5Unified.h>

// Initialize static counter for unique mock device codes
uint32_t ApiClient::_mockDeviceCodeCounter = 1000;

// HTTP timeout in milliseconds
constexpr uint32_t HTTP_TIMEOUT_MS = 10000;

// Response buffer size (increased to handle larger API responses like screentime)
constexpr size_t RESPONSE_BUFFER_SIZE = 8192;

// ============================================================================
// Constructor / Initialization
// ============================================================================

ApiClient::ApiClient()
    : _network(nullptr)
    , _mockMode(false)  // Default to real API mode
    , _secureClient(nullptr)
    , _responseBuffer(nullptr)
    , _mockLoginStartMs(0)
    , _mockLoginDelayMs(8000)
    , _mockMoreTimeGranted(true)
    , _mockMoreTimeMinutes(30)
    , _mockMoreTimeStartMs(0)
{
    _baseUrl[0] = '\0';
    _apiKey[0] = '\0';
    _familyId[0] = '\0';
    
    // Allocate on heap to avoid stack overflow
    // WiFiClientSecure uses ~16KB for SSL buffers
    _secureClient = new WiFiClientSecure();
    _secureClient->setInsecure();  // Skip certificate verification (for development)
    
    // Allocate response buffer on heap
    _responseBuffer = new char[RESPONSE_BUFFER_SIZE];
}

void ApiClient::begin(NetworkManager& network, const char* baseUrl) {
    _network = &network;
    strncpy(_baseUrl, baseUrl, sizeof(_baseUrl) - 1);
    _baseUrl[sizeof(_baseUrl) - 1] = '\0';
    
    Serial.printf("[ApiClient] Initialized with base URL: %s\n", _baseUrl);
    Serial.printf("[ApiClient] Mock mode: %s\n", _mockMode ? "ENABLED" : "DISABLED");
}

void ApiClient::setApiKey(const char* apiKey) {
    strncpy(_apiKey, apiKey, sizeof(_apiKey) - 1);
    _apiKey[sizeof(_apiKey) - 1] = '\0';
    Serial.printf("[ApiClient] API key set: %s...%s\n", 
                  strlen(_apiKey) > 8 ? "***" : _apiKey,
                  strlen(_apiKey) > 8 ? (_apiKey + strlen(_apiKey) - 4) : "");
}

const char* ApiClient::getApiKey() const {
    return _apiKey;
}

bool ApiClient::hasApiKey() const {
    return _apiKey[0] != '\0';
}

void ApiClient::setFamilyId(const char* familyId) {
    strncpy(_familyId, familyId, sizeof(_familyId) - 1);
    _familyId[sizeof(_familyId) - 1] = '\0';
    Serial.printf("[ApiClient] Family ID set: %s\n", _familyId);
}

const char* ApiClient::getFamilyId() const {
    return _familyId;
}

bool ApiClient::hasFamilyId() const {
    return _familyId[0] != '\0';
}

// ============================================================================
// HTTP Helper Methods
// ============================================================================

bool ApiClient::ensureConnected() {
    if (_network == nullptr) {
        Serial.println("[ApiClient] ERROR: NetworkManager not set");
        return false;
    }
    
    return _network->ensureConnected();
}

int ApiClient::httpGet(const char* endpoint, char* responseBuffer, size_t bufferSize, bool authenticated) {
    if (!ensureConnected()) {
        Serial.println("[ApiClient] Cannot make GET request - not connected");
        return -1;
    }
    
    // Build full URL
    char url[256];
    snprintf(url, sizeof(url), "%s%s", _baseUrl, endpoint);
    
    Serial.printf("[ApiClient] GET %s\n", url);
    
    // Use heap-allocated WiFiClientSecure to avoid stack overflow
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    
    if (!http.begin(*_secureClient, url)) {
        Serial.println("[ApiClient] Failed to begin HTTP connection");
        return -2;
    }
    
    // Add headers
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept", "application/json");
    
    if (authenticated && hasApiKey()) {
        http.addHeader("X-API-Key", _apiKey);
    }
    
    // Make request
    int httpCode = http.GET();
    
    if (httpCode > 0) {
        // Got a response
        String payload = http.getString();
        size_t len = payload.length();
        if (len >= bufferSize) {
            len = bufferSize - 1;
        }
        memcpy(responseBuffer, payload.c_str(), len);
        responseBuffer[len] = '\0';
        
        Serial.printf("[ApiClient] Response (%d): %s\n", httpCode, 
                      len > 200 ? "(truncated)" : responseBuffer);
    } else {
        Serial.printf("[ApiClient] GET failed, error: %s\n", http.errorToString(httpCode).c_str());
        responseBuffer[0] = '\0';
    }
    
    http.end();
    return httpCode;
}

int ApiClient::httpPost(const char* endpoint, const char* body, char* responseBuffer, size_t bufferSize, bool authenticated) {
    if (!ensureConnected()) {
        Serial.println("[ApiClient] Cannot make POST request - not connected");
        return -1;
    }
    
    // Build full URL
    char url[256];
    snprintf(url, sizeof(url), "%s%s", _baseUrl, endpoint);
    
    Serial.printf("[ApiClient] POST %s\n", url);
    if (body && body[0]) {
        Serial.printf("[ApiClient] Body: %s\n", body);
    }
    
    // Use heap-allocated WiFiClientSecure to avoid stack overflow
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    
    if (!http.begin(*_secureClient, url)) {
        Serial.println("[ApiClient] Failed to begin HTTP connection");
        return -2;
    }
    
    // Add headers
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept", "application/json");
    
    if (authenticated && hasApiKey()) {
        http.addHeader("X-API-Key", _apiKey);
    }
    
    // Make request
    int httpCode;
    if (body && body[0]) {
        httpCode = http.POST(body);
    } else {
        httpCode = http.POST("");  // Empty body
    }
    
    if (httpCode > 0) {
        // Got a response
        String payload = http.getString();
        size_t len = payload.length();
        if (len >= bufferSize) {
            len = bufferSize - 1;
        }
        memcpy(responseBuffer, payload.c_str(), len);
        responseBuffer[len] = '\0';
        
        Serial.printf("[ApiClient] Response (%d): %s\n", httpCode,
                      len > 200 ? "(truncated)" : responseBuffer);
    } else {
        Serial.printf("[ApiClient] POST failed, error: %s\n", http.errorToString(httpCode).c_str());
        responseBuffer[0] = '\0';
    }
    
    http.end();
    return httpCode;
}

void ApiClient::getTodayDateString(char* buffer, size_t bufferSize) {
    // Get current time from RTC
    auto dt = M5.Rtc.getDateTime();
    snprintf(buffer, bufferSize, "%04d-%02d-%02d", 
             dt.date.year, dt.date.month, dt.date.date);
}

void ApiClient::parseTimeString(const char* timeStr, uint8_t& outHour, uint8_t& outMinute) {
    // Parse "HH:MM" format
    outHour = 0;
    outMinute = 0;
    
    if (timeStr && strlen(timeStr) >= 5) {
        char hourStr[3] = {timeStr[0], timeStr[1], '\0'};
        char minStr[3] = {timeStr[3], timeStr[4], '\0'};
        outHour = (uint8_t)atoi(hourStr);
        outMinute = (uint8_t)atoi(minStr);
    }
}

// ============================================================================
// Authentication
// ============================================================================

DeviceCodeResponse ApiClient::initiateLogin() {
    Serial.println("[ApiClient] Initiating login...");
    
    if (_mockMode) {
        return mockInitiateLogin();
    }
    
    DeviceCodeResponse response;
    
    // GET /api/pairing/devicecode (use heap-allocated buffer)
    int httpCode = httpGet(API_ENDPOINT_DEVICE_CODE, _responseBuffer, RESPONSE_BUFFER_SIZE, false);
    
    if (httpCode != 200) {
        response.success = false;
        snprintf(response.errorMessage, sizeof(response.errorMessage), 
                 "HTTP error: %d", httpCode);
        return response;
    }
    
    // Parse JSON response
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, _responseBuffer);
    
    if (error) {
        response.success = false;
        snprintf(response.errorMessage, sizeof(response.errorMessage),
                 "JSON parse error: %s", error.c_str());
        return response;
    }
    
    // Extract pairing code
    const char* pairingCode = doc["pairingCode"] | "";
    if (strlen(pairingCode) == 0) {
        response.success = false;
        strncpy(response.errorMessage, "No pairing code in response", sizeof(response.errorMessage) - 1);
        return response;
    }
    
    // Populate response
    response.success = true;
    strncpy(response.pairingCode, pairingCode, sizeof(response.pairingCode) - 1);
    response.pairingCode[sizeof(response.pairingCode) - 1] = '\0';
    
    // Copy to compatibility fields
    strncpy(response.deviceCode, response.pairingCode, sizeof(response.deviceCode) - 1);
    strncpy(response.userCode, response.pairingCode, sizeof(response.userCode) - 1);
    
    // Build QR code URL
    snprintf(response.qrCodeUrl, sizeof(response.qrCodeUrl), 
             "%s%s", API_PAIRING_BASE_URL, response.pairingCode);
    
    // Parse expiration if present
    // expiresAt is ISO date string, we'll just use default 300 seconds
    response.expiresInSeconds = 300;
    response.pollIntervalSeconds = 5;
    
    Serial.printf("[ApiClient] Login initiated, pairing code: %s\n", response.pairingCode);
    Serial.printf("[ApiClient] QR URL: %s\n", response.qrCodeUrl);
    
    return response;
}

LoginPollResult ApiClient::pollLoginStatus(const char* pairingCode) {
    Serial.printf("[ApiClient] Polling login status for: %s\n", pairingCode);
    
    if (_mockMode) {
        return mockPollLoginStatus(pairingCode);
    }
    
    LoginPollResult result;
    
    // Build endpoint: POST /api/pairing/devicecode/{pairingCode}
    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), "%s/%s", API_ENDPOINT_DEVICE_CODE, pairingCode);
    
    int httpCode = httpPost(endpoint, nullptr, _responseBuffer, RESPONSE_BUFFER_SIZE, false);
    
    if (httpCode != 200) {
        result.success = false;
        result.pending = false;
        snprintf(result.errorMessage, sizeof(result.errorMessage),
                 "HTTP error: %d", httpCode);
        return result;
    }
    
    // Parse JSON response
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, _responseBuffer);
    
    if (error) {
        result.success = false;
        result.pending = false;
        snprintf(result.errorMessage, sizeof(result.errorMessage),
                 "JSON parse error: %s", error.c_str());
        return result;
    }
    
    // Extract status
    const char* status = doc["status"] | "";
    Serial.printf("[ApiClient] Poll status: %s\n", status);
    
    result.success = true;
    
    // Pairing lifecycle: issued -> linked -> paired
    // - "issued": waiting for user to scan QR code
    // - "linked": user scanned, API key will be issued on next poll  
    // - "paired": API key received, pairing complete
    // - "expired": code expired, need to request new one
    
    if (strcmp(status, "paired") == 0) {
        // Login complete - device is paired and we have the API key
        result.pending = false;
        result.linked = true;
        
        // Extract API key
        const char* apiKey = doc["apiKey"] | "";
        strncpy(result.apiKey, apiKey, sizeof(result.apiKey) - 1);
        result.apiKey[sizeof(result.apiKey) - 1] = '\0';
        
        // Extract username if present
        const char* userName = doc["userName"] | "User";
        strncpy(result.username, userName, sizeof(result.username) - 1);
        result.username[sizeof(result.username) - 1] = '\0';
        
        Serial.printf("[ApiClient] Login complete! API key received.\n");
    } else if (strcmp(status, "expired") == 0) {
        // Pairing code expired - need to request a new one
        result.pending = false;
        result.expired = true;
        strncpy(result.errorMessage, "Pairing code expired. Please try again.", sizeof(result.errorMessage) - 1);
    } else if (strcmp(status, "issued") == 0 || strcmp(status, "linked") == 0) {
        // Still in progress:
        // - "issued": waiting for user to scan QR
        // - "linked": user scanned, waiting for next poll to get API key
        result.pending = true;
        Serial.printf("[ApiClient] Pairing in progress (status=%s), continue polling...\n", status);
    } else {
        // Unknown status - treat as pending to be safe
        result.pending = true;
        Serial.printf("[ApiClient] Unknown pairing status '%s', continue polling...\n", status);
    }
    
    return result;
}

void ApiClient::logout() {
    Serial.println("[ApiClient] Logging out...");
    
    // Clear local state
    _apiKey[0] = '\0';
    _familyId[0] = '\0';
    
    Serial.println("[ApiClient] Logout complete");
}

// ============================================================================
// Family Members
// ============================================================================

FamilyGroupResult ApiClient::getFamilyGroup() {
    Serial.println("[ApiClient] Getting family group...");
    
    if (_mockMode) {
        return mockGetFamilyGroup();
    }
    
    FamilyGroupResult result;
    
    // GET /api/family
    int httpCode = httpGet(API_ENDPOINT_FAMILY, _responseBuffer, RESPONSE_BUFFER_SIZE, true);
    
    if (httpCode != 200) {
        result.success = false;
        snprintf(result.errorMessage, sizeof(result.errorMessage),
                 "HTTP error: %d", httpCode);
        return result;
    }
    
    // Parse JSON response
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, _responseBuffer);
    
    if (error) {
        result.success = false;
        snprintf(result.errorMessage, sizeof(result.errorMessage),
                 "JSON parse error: %s", error.c_str());
        return result;
    }
    
    result.success = true;
    
    // Extract family group info
    JsonObject familyGroup = doc["familyGroup"];
    const char* familyId = familyGroup["_id"] | "";
    const char* familyName = familyGroup["name"] | "";
    
    strncpy(result.familyId, familyId, sizeof(result.familyId) - 1);
    result.familyId[sizeof(result.familyId) - 1] = '\0';
    
    strncpy(result.familyName, familyName, sizeof(result.familyName) - 1);
    result.familyName[sizeof(result.familyName) - 1] = '\0';
    
    // Store family ID for later use
    setFamilyId(result.familyId);
    
    // Extract members
    JsonArray members = doc["members"];
    result.memberCount = 0;
    
    for (JsonObject member : members) {
        if (result.memberCount >= 10) break;
        
        FamilyMember& fm = result.members[result.memberCount];
        
        const char* userId = member["userId"] | "";
        const char* name = member["name"] | "";
        const char* avatarName = member["avatarName"] | "";
        const char* position = member["position"] | "";
        
        strncpy(fm.id, userId, sizeof(fm.id) - 1);
        fm.id[sizeof(fm.id) - 1] = '\0';
        
        strncpy(fm.name, name, sizeof(fm.name) - 1);
        fm.name[sizeof(fm.name) - 1] = '\0';
        
        strncpy(fm.avatarName, avatarName, sizeof(fm.avatarName) - 1);
        fm.avatarName[sizeof(fm.avatarName) - 1] = '\0';
        
        strncpy(fm.position, position, sizeof(fm.position) - 1);
        fm.position[sizeof(fm.position) - 1] = '\0';
        
        // Extract initial from name
        fm.initial = (name && name[0]) ? name[0] : '?';
        
        result.memberCount++;
    }
    
    Serial.printf("[ApiClient] Family group loaded: %s (%s), %d members\n",
                  result.familyName, result.familyId, result.memberCount);
    
    return result;
}

bool ApiClient::getFamilyMembers(FamilyMember* members, int maxCount, int& outCount) {
    Serial.println("[ApiClient] Getting family members...");
    
    if (_mockMode) {
        return mockGetFamilyMembers(members, maxCount, outCount);
    }
    
    // Allocate on heap to avoid stack overflow (FamilyGroupResult is ~1KB)
    FamilyGroupResult* groupResult = new FamilyGroupResult();
    if (groupResult == nullptr) {
        Serial.println("[ApiClient] Failed to allocate FamilyGroupResult");
        outCount = 0;
        return false;
    }
    
    // Get full family group
    *groupResult = getFamilyGroup();
    
    if (!groupResult->success) {
        outCount = 0;
        delete groupResult;
        return false;
    }
    
    // Filter to children only
    outCount = 0;
    for (int i = 0; i < groupResult->memberCount && outCount < maxCount; i++) {
        if (groupResult->members[i].isChild()) {
            members[outCount] = groupResult->members[i];
            outCount++;
        }
    }
    
    Serial.printf("[ApiClient] Returned %d children\n", outCount);
    delete groupResult;
    return true;
}

// ============================================================================
// Screen Time
// ============================================================================

AllowanceResult ApiClient::getTodayAllowance(const char* childId) {
    Serial.printf("[ApiClient] Getting today's allowance for child: %s\n", childId);
    
    // Log the API key being used (masked for security)
    if (hasApiKey()) {
        size_t keyLen = strlen(_apiKey);
        if (keyLen > 8) {
            Serial.printf("[ApiClient] Using API key: %.4s...%s (len=%zu)\n", 
                          _apiKey, _apiKey + keyLen - 4, keyLen);
        } else {
            Serial.printf("[ApiClient] Using API key: %s (len=%zu)\n", _apiKey, keyLen);
        }
    } else {
        Serial.println("[ApiClient] WARNING: No API key set!");
    }
    
    if (_mockMode) {
        return mockGetTodayAllowance(childId);
    }
    
    AllowanceResult result;
    
    // Check we have family ID
    if (!hasFamilyId()) {
        result.success = false;
        strncpy(result.errorMessage, "Family ID not set", sizeof(result.errorMessage) - 1);
        return result;
    }
    
    // Get today's date
    char dateStr[16];
    getTodayDateString(dateStr, sizeof(dateStr));
    
    // Build endpoint: family/{familyId}/child/{childId}/screentime/on-date/{date}
    char endpoint[256];
    snprintf(endpoint, sizeof(endpoint), API_ENDPOINT_SCREENTIME_TEMPLATE,
             _familyId, childId, dateStr);
    
    Serial.printf("[ApiClient] Requesting: %s%s\n", _baseUrl, endpoint);
    
    int httpCode = httpGet(endpoint, _responseBuffer, RESPONSE_BUFFER_SIZE, true);
    
    if (httpCode != 200) {
        result.success = false;
        snprintf(result.errorMessage, sizeof(result.errorMessage),
                 "HTTP error: %d", httpCode);
        return result;
    }
    
    // Log raw response for debugging
    Serial.println("[ApiClient] === RAW ALLOWANCE RESPONSE ===");
    Serial.println(_responseBuffer);
    Serial.println("[ApiClient] === END RAW RESPONSE ===");
    
    // Parse JSON response
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, _responseBuffer);
    
    if (error) {
        result.success = false;
        snprintf(result.errorMessage, sizeof(result.errorMessage),
                 "JSON parse error: %s", error.c_str());
        return result;
    }
    
    result.success = true;
    
    // Extract effective allowance
    JsonObject effectiveAllowance = doc["effectiveAllowance"];
    
    // Check if effectiveAllowedMinutes is null (unlimited time)
    if (effectiveAllowance["effectiveAllowedMinutes"].isNull()) {
        result.hasUnlimitedAllowance = true;
        result.dailyAllowanceMinutes = 0;  // Set to 0 when unlimited
        Serial.println("[ApiClient] Unlimited allowance detected (effectiveAllowedMinutes is null)");
    } else {
        result.hasUnlimitedAllowance = false;
        result.dailyAllowanceMinutes = effectiveAllowance["effectiveAllowedMinutes"] | 0;
    }
    
    result.totalBonusMinutes = effectiveAllowance["totalBonusMinutes"] | 0;
    
    // Parse wake/bed times (handle null values with defaults)
    const char* wakeUp = effectiveAllowance["effectiveWakeUpTime"] | "07:00";
    const char* bedTime = effectiveAllowance["effectiveBedTime"] | "21:00";
    parseTimeString(wakeUp, result.wakeUpHour, result.wakeUpMinute);
    parseTimeString(bedTime, result.bedTimeHour, result.bedTimeMinute);
    
    // usedTodayMinutes is not in this API response, default to 0
    result.usedTodayMinutes = 0;
    
    Serial.printf("[ApiClient] Allowance: %lu min (unlimited=%d), bonus: %lu min, wake: %02d:%02d, bed: %02d:%02d\n",
                  (unsigned long)result.dailyAllowanceMinutes,
                  result.hasUnlimitedAllowance,
                  (unsigned long)result.totalBonusMinutes,
                  result.wakeUpHour, result.wakeUpMinute,
                  result.bedTimeHour, result.bedTimeMinute);
    
    return result;
}

bool ApiClient::recordScreenTimeUsed(const char* childId, uint32_t minutesUsed) {
    Serial.printf("[ApiClient] Recording %lu minutes used for child: %s\n", 
                  (unsigned long)minutesUsed, childId);
    
    // TODO: Implement screen time usage recording when API endpoint is available
    // For now, just log and return success
    Serial.println("[ApiClient] Usage recording not yet implemented");
    return true;
}

ConsumedTimeResult ApiClient::pushConsumedTime(const char* childId, uint32_t sessionDurationMinutes, time_t sessionStartTime) {
    Serial.printf("[ApiClient] Pushing session: %lu minutes, started at %ld for child: %s\n",
                  (unsigned long)sessionDurationMinutes, (long)sessionStartTime, childId);
    
    ConsumedTimeResult result;
    
    // Check we have family ID
    if (!hasFamilyId()) {
        result.success = false;
        strncpy(result.errorMessage, "Family ID not set", sizeof(result.errorMessage) - 1);
        Serial.println("[ApiClient] pushConsumedTime failed: no family ID");
        return result;
    }
    
    // Check for valid child ID
    if (childId == nullptr || childId[0] == '\0') {
        result.success = false;
        strncpy(result.errorMessage, "Child ID not set", sizeof(result.errorMessage) - 1);
        Serial.println("[ApiClient] pushConsumedTime failed: no child ID");
        return result;
    }
    
    // Check for valid session start time
    if (sessionStartTime == 0) {
        result.success = false;
        strncpy(result.errorMessage, "Session start time not set", sizeof(result.errorMessage) - 1);
        Serial.println("[ApiClient] pushConsumedTime failed: no session start time");
        return result;
    }
    
    // Mock mode returns success without making HTTP call
    if (_mockMode) {
        Serial.printf("[ApiClient] Mock: pushed session %lu minutes\n",
                      (unsigned long)sessionDurationMinutes);
        result.success = true;
        return result;
    }
    
    // Format sessionStartTime as ISO 8601 string: "YYYY-MM-DD HH:MM:SSZ"
    struct tm* timeInfo = gmtime(&sessionStartTime);
    char startedAtStr[32];
    snprintf(startedAtStr, sizeof(startedAtStr), "%04d-%02d-%02d %02d:%02d:%02dZ",
             timeInfo->tm_year + 1900,
             timeInfo->tm_mon + 1,
             timeInfo->tm_mday,
             timeInfo->tm_hour,
             timeInfo->tm_min,
             timeInfo->tm_sec);
    
    // Build endpoint: family/{familyId}/child/{childId}/session/
    char endpoint[256];
    snprintf(endpoint, sizeof(endpoint), API_ENDPOINT_SESSION_TEMPLATE,
             _familyId, childId);
    
    // Build request body: { "duration": N, "startedAt": "YYYY-MM-DD HH:MM:SSZ" }
    char requestBody[128];
    snprintf(requestBody, sizeof(requestBody), "{\"duration\":%lu,\"startedAt\":\"%s\"}", 
             (unsigned long)sessionDurationMinutes, startedAtStr);
    
    // Make the POST request
    int httpCode = httpPost(endpoint, requestBody, _responseBuffer, RESPONSE_BUFFER_SIZE, true);
    
    if (httpCode >= 200 && httpCode < 300) {
        result.success = true;
        Serial.printf("[ApiClient] Session pushed successfully (HTTP %d)\n", httpCode);
    } else {
        result.success = false;
        snprintf(result.errorMessage, sizeof(result.errorMessage),
                 "HTTP error: %d", httpCode);
        Serial.printf("[ApiClient] pushConsumedTime failed: HTTP %d\n", httpCode);
    }
    
    return result;
}

// ============================================================================
// Request More Time
// ============================================================================

MoreTimeRequestResult ApiClient::requestAdditionalTime(const char* childId, 
                                                         const char* childName,
                                                         uint32_t bonusMinutes) {
    Serial.printf("[ApiClient] Requesting %lu more minutes for child: %s\n", 
                  (unsigned long)bonusMinutes, childId);
    
    if (_mockMode) {
        return mockRequestAdditionalTime(childId);
    }
    
    MoreTimeRequestResult result;
    
    // Check we have family ID
    if (!hasFamilyId()) {
        result.success = false;
        strncpy(result.errorMessage, "Family ID not set", sizeof(result.errorMessage) - 1);
        return result;
    }
    
    // Get today's date
    char dateStr[16];
    getTodayDateString(dateStr, sizeof(dateStr));
    
    // Build request body
    JsonDocument requestDoc;
    requestDoc["applicableDate"] = dateStr;
    requestDoc["bonusMinutes"] = bonusMinutes;
    requestDoc["overrideWakeUpTime"] = nullptr;
    requestDoc["overrideBedTime"] = nullptr;
    requestDoc["status"] = "requested";
    
    // Build notes
    char notes[64];
    if (childName && childName[0]) {
        snprintf(notes, sizeof(notes), "requested by %s via screenie stick", childName);
    } else {
        strncpy(notes, "requested via screenie stick", sizeof(notes) - 1);
    }
    requestDoc["notes"] = notes;
    
    char requestBody[256];
    serializeJson(requestDoc, requestBody, sizeof(requestBody));
    
    // Build endpoint: family/{familyId}/child/{childId}/grant
    char endpoint[256];
    snprintf(endpoint, sizeof(endpoint), API_ENDPOINT_GRANT_TEMPLATE,
             _familyId, childId);
    
    int httpCode = httpPost(endpoint, requestBody, _responseBuffer, RESPONSE_BUFFER_SIZE, true);
    
    if (httpCode != 200 && httpCode != 201) {
        result.success = false;
        snprintf(result.errorMessage, sizeof(result.errorMessage),
                 "HTTP error: %d", httpCode);
        return result;
    }
    
    // Parse JSON response
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, _responseBuffer);
    
    if (error) {
        result.success = false;
        snprintf(result.errorMessage, sizeof(result.errorMessage),
                 "JSON parse error: %s", error.c_str());
        return result;
    }
    
    result.success = true;
    
    // Extract grant ID and status from response
    // Response format: { "grant": { "_id": "...", "status": "..." }, "id": "..." }
    const char* grantId = doc["grant"]["_id"] | "";
    const char* status = doc["grant"]["status"] | "requested";
    
    strncpy(result.requestId, grantId, sizeof(result.requestId) - 1);
    result.requestId[sizeof(result.requestId) - 1] = '\0';
    
    strncpy(result.status, status, sizeof(result.status) - 1);
    result.status[sizeof(result.status) - 1] = '\0';
    
    Serial.printf("[ApiClient] More time request submitted, grant ID: %s, status: %s\n",
                  result.requestId, result.status);
    
    return result;
}

MoreTimePollResult ApiClient::pollMoreTimeStatus(const char* requestId) {
    Serial.printf("[ApiClient] Polling more time status for: %s\n", requestId);
    
    if (_mockMode) {
        return mockPollMoreTimeStatus(requestId);
    }
    
    MoreTimePollResult result;
    
    // Build endpoint: grant/{grantId}
    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), API_ENDPOINT_GRANT_STATUS_TEMPLATE, requestId);
    
    int httpCode = httpGet(endpoint, _responseBuffer, RESPONSE_BUFFER_SIZE, true);
    
    if (httpCode != 200) {
        result.success = false;
        result.pending = false;
        snprintf(result.errorMessage, sizeof(result.errorMessage),
                 "HTTP error: %d", httpCode);
        return result;
    }
    
    // Parse JSON response
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, _responseBuffer);
    
    if (error) {
        result.success = false;
        result.pending = false;
        snprintf(result.errorMessage, sizeof(result.errorMessage),
                 "JSON parse error: %s", error.c_str());
        return result;
    }
    
    result.success = true;
    
    // Extract status and bonus minutes from grant object
    // Response format: { "grant": { "status": "...", "bonusMinutes": N, ... } }
    const char* status = doc["grant"]["status"] | "";
    result.additionalMinutes = doc["grant"]["bonusMinutes"] | 0;
    
    Serial.printf("[ApiClient] Grant status: %s, bonus: %lu min\n",
                  status, (unsigned long)result.additionalMinutes);
    
    if (strcmp(status, "granted") == 0) {
        result.pending = false;
        result.granted = true;
    } else if (strcmp(status, "rejected") == 0) {
        result.pending = false;
        result.denied = true;
    } else {
        // "requested" or other = still pending
        result.pending = true;
    }
    
    return result;
}

// ============================================================================
// Mock Control
// ============================================================================

void ApiClient::setMockMode(bool enabled) {
    _mockMode = enabled;
    Serial.printf("[ApiClient] Mock mode: %s\n", enabled ? "ENABLED" : "DISABLED");
}

bool ApiClient::isMockMode() const {
    return _mockMode;
}

void ApiClient::setMockLoginDelay(uint32_t delayMs) {
    _mockLoginDelayMs = delayMs;
}

void ApiClient::setMockMoreTimeResponse(bool granted, uint32_t additionalMinutes) {
    _mockMoreTimeGranted = granted;
    _mockMoreTimeMinutes = additionalMinutes;
}

// ============================================================================
// Mock Response Generators
// ============================================================================

DeviceCodeResponse ApiClient::mockInitiateLogin() {
    DeviceCodeResponse response;
    response.success = true;
    
    // Generate a unique device code
    snprintf(response.pairingCode, sizeof(response.pairingCode), 
             "MOCK%04lu", (unsigned long)(_mockDeviceCodeCounter++ % 10000));
    
    // Copy to compatibility fields
    strncpy(response.deviceCode, response.pairingCode, sizeof(response.deviceCode) - 1);
    strncpy(response.userCode, response.pairingCode, sizeof(response.userCode) - 1);
    
    // Build the QR code URL
    snprintf(response.qrCodeUrl, sizeof(response.qrCodeUrl), 
             "%s%s", API_PAIRING_BASE_URL, response.pairingCode);
    
    response.expiresInSeconds = 300;
    response.pollIntervalSeconds = 5;
    
    // Start the mock login timer
    _mockLoginStartMs = millis();
    
    Serial.printf("[ApiClient] Mock login initiated:\n");
    Serial.printf("  Pairing code: %s\n", response.pairingCode);
    Serial.printf("  QR URL: %s\n", response.qrCodeUrl);
    Serial.printf("  Will succeed in %lu ms\n", (unsigned long)_mockLoginDelayMs);
    
    return response;
}

LoginPollResult ApiClient::mockPollLoginStatus(const char* pairingCode) {
    LoginPollResult result;
    result.success = true;
    
    // Check if enough time has passed
    uint32_t elapsed = millis() - _mockLoginStartMs;
    
    if (elapsed >= _mockLoginDelayMs) {
        // Login complete!
        result.pending = false;
        result.linked = true;
        strncpy(result.apiKey, "mock-api-key-xyz789", sizeof(result.apiKey) - 1);
        strncpy(result.username, "MockUser", sizeof(result.username) - 1);
        Serial.printf("[ApiClient] Mock login complete after %lu ms\n", elapsed);
    } else {
        // Still pending
        result.pending = true;
        Serial.printf("[ApiClient] Mock login pending... (%lu/%lu ms)\n", 
                      elapsed, (unsigned long)_mockLoginDelayMs);
    }
    
    return result;
}

FamilyGroupResult ApiClient::mockGetFamilyGroup() {
    FamilyGroupResult result;
    result.success = true;
    
    strncpy(result.familyId, "mock-family-001", sizeof(result.familyId) - 1);
    strncpy(result.familyName, "Mock Family", sizeof(result.familyName) - 1);
    
    // Add mock members
    const char* names[] = {"Mock Parent", "Sophie", "Oliver", "Emma"};
    const char* ids[] = {"parent-001", "child-001", "child-002", "child-003"};
    const char* positions[] = {"parent", "child", "child", "child"};
    const char* avatars[] = {"", "1F3B1_color.png", "1F680_color.png", "1F348_color.png"};
    
    result.memberCount = 4;
    for (int i = 0; i < 4; i++) {
        strncpy(result.members[i].id, ids[i], sizeof(result.members[i].id) - 1);
        strncpy(result.members[i].name, names[i], sizeof(result.members[i].name) - 1);
        strncpy(result.members[i].avatarName, avatars[i], sizeof(result.members[i].avatarName) - 1);
        strncpy(result.members[i].position, positions[i], sizeof(result.members[i].position) - 1);
        result.members[i].initial = names[i][0];
    }
    
    // Store family ID
    setFamilyId(result.familyId);
    
    Serial.printf("[ApiClient] Mock: Returned family with %d members\n", result.memberCount);
    return result;
}

bool ApiClient::mockGetFamilyMembers(FamilyMember* members, int maxCount, int& outCount) {
    FamilyGroupResult groupResult = mockGetFamilyGroup();
    
    // Filter to children only
    outCount = 0;
    for (int i = 0; i < groupResult.memberCount && outCount < maxCount; i++) {
        if (groupResult.members[i].isChild()) {
            members[outCount] = groupResult.members[i];
            outCount++;
        }
    }
    
    Serial.printf("[ApiClient] Mock: Returned %d children\n", outCount);
    return true;
}

AllowanceResult ApiClient::mockGetTodayAllowance(const char* childId) {
    AllowanceResult result;
    result.success = true;
    
    // Different allowances based on child ID for testing
    if (strcmp(childId, "child-001") == 0) {
        result.dailyAllowanceMinutes = 120;  // 2 hours
        result.usedTodayMinutes = 30;
    } else if (strcmp(childId, "child-002") == 0) {
        result.dailyAllowanceMinutes = 90;   // 1.5 hours
        result.usedTodayMinutes = 0;
    } else {
        result.dailyAllowanceMinutes = 60;   // 1 hour
        result.usedTodayMinutes = 15;
    }
    
    result.wakeUpHour = 7;
    result.wakeUpMinute = 0;
    result.bedTimeHour = 21;
    result.bedTimeMinute = 0;
    result.totalBonusMinutes = 0;
    
    Serial.printf("[ApiClient] Mock allowance: %lu min allowed, %lu min used\n",
                  (unsigned long)result.dailyAllowanceMinutes,
                  (unsigned long)result.usedTodayMinutes);
    
    return result;
}

MoreTimeRequestResult ApiClient::mockRequestAdditionalTime(const char* childId) {
    MoreTimeRequestResult result;
    result.success = true;
    
    // Generate a request ID
    snprintf(result.requestId, sizeof(result.requestId), 
             "req-%lu", (unsigned long)(millis() % 1000000));
    strncpy(result.status, "requested", sizeof(result.status) - 1);
    
    // Start the more-time polling timer
    _mockMoreTimeStartMs = millis();
    
    Serial.printf("[ApiClient] Mock more-time request created: %s\n", result.requestId);
    Serial.printf("[ApiClient] Will be %s after 10 seconds\n", 
                  _mockMoreTimeGranted ? "GRANTED" : "DENIED");
    
    return result;
}

MoreTimePollResult ApiClient::mockPollMoreTimeStatus(const char* requestId) {
    MoreTimePollResult result;
    result.success = true;
    
    // Simulate 10-second delay before decision
    const uint32_t MOCK_DECISION_DELAY_MS = 10000;
    uint32_t elapsed = millis() - _mockMoreTimeStartMs;
    
    if (elapsed >= MOCK_DECISION_DELAY_MS) {
        // Decision made
        result.pending = false;
        
        if (_mockMoreTimeGranted) {
            result.granted = true;
            result.additionalMinutes = _mockMoreTimeMinutes;
            Serial.printf("[ApiClient] Mock more-time GRANTED: +%lu minutes\n",
                          (unsigned long)_mockMoreTimeMinutes);
        } else {
            result.denied = true;
            Serial.println("[ApiClient] Mock more-time DENIED");
        }
    } else {
        // Still pending
        result.pending = true;
        Serial.printf("[ApiClient] Mock more-time pending... (%lu/%lu ms)\n",
                      elapsed, MOCK_DECISION_DELAY_MS);
    }
    
    return result;
}
