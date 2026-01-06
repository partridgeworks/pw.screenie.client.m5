/**
 * api_client.h - REST API Client for Screen Time Tracker
 * 
 * Provides API communication for:
 * - Device-code authentication (login)
 * - Family member listing
 * - Screen time allowance fetching
 * - Usage recording
 * - More-time request submission
 * 
 * Uses real HTTP calls to the Screenie API with ArduinoJson for parsing.
 * 
 * @author Screen Time Tracker
 * @version 2.0
 */

#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <stdint.h>
#include <stddef.h>
#include <cstring>
#include <time.h>
#include "config.h"

// Forward declarations
class NetworkManager;
class WiFiClientSecure;

// ============================================================================
// API Endpoint Constants
// ============================================================================

// Device pairing endpoints
constexpr const char* API_ENDPOINT_DEVICE_CODE = "pairing/devicecode";

// Family endpoints  
constexpr const char* API_ENDPOINT_FAMILY = "family/default";

// Screen time endpoint template: family/{familyId}/child/{childId}/screentime/on-date/{date}
constexpr const char* API_ENDPOINT_SCREENTIME_TEMPLATE = "family/%s/child/%s/screentime/on-date/%s";

// Session endpoint template: family/{familyId}/child/{childId}/session
constexpr const char* API_ENDPOINT_SESSION_TEMPLATE = "family/%s/child/%s/session";

// Grant request endpoint template: family/{familyId}/child/{childId}/grant
constexpr const char* API_ENDPOINT_GRANT_TEMPLATE = "family/%s/child/%s/grant";

// Grant status endpoint template: grant/{grantId}
constexpr const char* API_ENDPOINT_GRANT_STATUS_TEMPLATE = "grant/%s";

// ============================================================================
// API Response Structures
// ============================================================================

/**
 * DeviceCodeResponse - Response from initiating device-code login
 * 
 * Contains pairing code for QR code generation and polling.
 */
struct DeviceCodeResponse {
    bool success;                    // API call succeeded
    char pairingCode[32];            // Code for QR and polling (e.g., "ABC123")
    char deviceCode[32];             // Alias for pairingCode (for compatibility)
    char userCode[16];               // Alias for pairingCode (for compatibility)
    char qrCodeUrl[128];             // Full URL to encode as QR code
    uint32_t expiresInSeconds;       // How long code is valid
    uint32_t pollIntervalSeconds;    // Poll interval (fixed at 5s)
    char errorMessage[64];           // Error message if !success
    
    DeviceCodeResponse()
        : success(false)
        , expiresInSeconds(300)
        , pollIntervalSeconds(5)
    {
        pairingCode[0] = '\0';
        deviceCode[0] = '\0';
        userCode[0] = '\0';
        qrCodeUrl[0] = '\0';
        errorMessage[0] = '\0';
    }
};

/**
 * LoginPollResult - Result from polling for login completion
 */
struct LoginPollResult {
    bool success;                    // Polling call succeeded
    bool pending;                    // true = still waiting ("paired" status)
    bool expired;                    // Device code has expired
    bool linked;                     // Device is linked (login complete)
    char apiKey[64];                 // API key (populated on linked)
    char username[32];               // Username (for compatibility)
    char errorMessage[64];           // Error message if failed
    
    LoginPollResult()
        : success(false)
        , pending(true)
        , expired(false)
        , linked(false)
    {
        apiKey[0] = '\0';
        username[0] = '\0';
        errorMessage[0] = '\0';
    }
};

/**
 * FamilyMember - A family member returned from API
 */
struct FamilyMember {
    char id[32];                     // Unique user ID (userId from API)
    char name[32];                   // Display name
    char initial;                    // First initial for avatar
    char avatarName[32];             // Avatar filename with extension (e.g., "1F3B1_color.png")
    char position[16];               // "parent" or "child"
    
    FamilyMember()
        : initial('?')
    {
        id[0] = '\0';
        name[0] = '\0';
        avatarName[0] = '\0';
        position[0] = '\0';
    }
    
    bool isChild() const {
        return strcmp(position, "child") == 0;
    }
};

/**
 * FamilyGroupResult - Response from fetching family group
 */
struct FamilyGroupResult {
    bool success;                    // API call succeeded
    char familyId[32];               // Family group ID (needed for other API calls)
    char familyName[48];             // Family display name
    FamilyMember members[10];        // Array of family members
    int memberCount;                 // Number of members returned
    char errorMessage[64];
    
    FamilyGroupResult()
        : success(false)
        , memberCount(0)
    {
        familyId[0] = '\0';
        familyName[0] = '\0';
        errorMessage[0] = '\0';
    }
};

/**
 * AllowanceResult - Response from fetching today's screen time allowance
 */
struct AllowanceResult {
    bool success;                    // API call succeeded
    uint32_t dailyAllowanceMinutes;  // Effective allowed minutes for today
    uint32_t usedTodayMinutes;       // Minutes already used (for compatibility)
    uint32_t totalBonusMinutes;      // Any bonus minutes already granted
    bool hasUnlimitedAllowance;      // true = effectiveAllowedMinutes was null (no restriction)
    // Future use - currently ignored by app
    uint8_t wakeUpHour;
    uint8_t wakeUpMinute;
    uint8_t bedTimeHour;
    uint8_t bedTimeMinute;
    char errorMessage[64];
    
    AllowanceResult()
        : success(false)
        , dailyAllowanceMinutes(0)
        , usedTodayMinutes(0)
        , totalBonusMinutes(0)
        , hasUnlimitedAllowance(false)
        , wakeUpHour(7)
        , wakeUpMinute(0)
        , bedTimeHour(21)
        , bedTimeMinute(0)
    {
        errorMessage[0] = '\0';
    }
};

/**
 * ConsumedTimeResult - Response from pushing consumed screen time
 */
struct ConsumedTimeResult {
    bool success;                    // API call succeeded
    char errorMessage[64];           // Error message if !success
    
    ConsumedTimeResult()
        : success(false)
    {
        errorMessage[0] = '\0';
    }
};

/**
 * MoreTimeRequestResult - Response from submitting more-time request
 */
struct MoreTimeRequestResult {
    bool success;                    // Request submitted successfully
    char requestId[32];              // Grant ID to poll for approval status
    char status[16];                 // Initial status ("requested")
    char errorMessage[64];
    
    MoreTimeRequestResult()
        : success(false)
    {
        requestId[0] = '\0';
        status[0] = '\0';
        errorMessage[0] = '\0';
    }
};

/**
 * MoreTimePollResult - Result from polling for more-time approval
 */
struct MoreTimePollResult {
    bool success;                    // Poll call succeeded
    bool granted;                    // Request was approved ("granted" status)
    bool pending;                    // Still waiting ("requested" status)
    bool denied;                     // Explicitly denied ("rejected" status)
    bool expired;                    // Request expired (timeout)
    uint32_t additionalMinutes;      // Bonus minutes if granted
    char errorMessage[64];
    
    MoreTimePollResult()
        : success(false)
        , granted(false)
        , pending(true)
        , denied(false)
        , expired(false)
        , additionalMinutes(0)
    {
        errorMessage[0] = '\0';
    }
};

// ============================================================================
// API Client Class
// ============================================================================

/**
 * ApiClient - REST API client for server communication
 * 
 * Manages all HTTP communication with the Screenie API.
 * Uses HTTPClient for requests and ArduinoJson for parsing.
 * 
 * Usage:
 *   ApiClient api;
 *   api.begin(networkManager);
 *   
 *   // Initiate login
 *   DeviceCodeResponse dcr = api.initiateLogin();
 *   if (dcr.success) {
 *       // Display QR code and pairing code
 *       // Poll for completion
 *   }
 */
class ApiClient {
public:
    /**
     * Constructor
     */
    ApiClient();
    
    /**
     * Initialize the API client
     * @param network Reference to NetworkManager for connectivity
     * @param baseUrl Base URL for API (optional, uses config default)
     */
    void begin(NetworkManager& network, const char* baseUrl = API_BASE_URL);
    
    /**
     * Set the API key for authenticated requests
     * @param apiKey The API key from authentication
     */
    void setApiKey(const char* apiKey);
    
    /**
     * Get the current API key
     * @return Current API key (empty string if not set)
     */
    const char* getApiKey() const;
    
    /**
     * Check if API client has a valid API key set
     * @return true if API key is set
     */
    bool hasApiKey() const;
    
    /**
     * Set the family ID for API calls that require it
     * @param familyId The family group ID
     */
    void setFamilyId(const char* familyId);
    
    /**
     * Get the current family ID
     * @return Current family ID (empty string if not set)
     */
    const char* getFamilyId() const;
    
    /**
     * Check if family ID is set
     * @return true if family ID is set
     */
    bool hasFamilyId() const;
    
    // ========================================================================
    // Authentication (Device-code Flow)
    // ========================================================================
    
    /**
     * Initiate device-code login
     * Gets a pairing code for QR code generation.
     * User scans QR or enters code on another device.
     * 
     * @return DeviceCodeResponse with pairing code
     */
    DeviceCodeResponse initiateLogin();
    
    /**
     * Poll for login completion
     * Call this periodically after initiateLogin() until complete.
     * 
     * @param pairingCode The pairing code from initiateLogin()
     * @return LoginPollResult with status and API key if successful
     */
    LoginPollResult pollLoginStatus(const char* pairingCode);
    
    /**
     * Logout - invalidate the current session
     * Clears the API key locally.
     */
    void logout();
    
    // ========================================================================
    // Family Members
    // ========================================================================
    
    /**
     * Get family group with all members
     * Also stores the familyId for subsequent calls.
     * 
     * @return FamilyGroupResult with family info and members
     */
    FamilyGroupResult getFamilyGroup();
    
    /**
     * Get list of children only (convenience wrapper)
     * 
     * @param members Output array for family members (children only)
     * @param maxCount Maximum number of members to return
     * @param outCount Output: actual number of members returned
     * @return true if successful
     */
    bool getFamilyMembers(FamilyMember* members, int maxCount, int& outCount);
    
    // ========================================================================
    // Screen Time
    // ========================================================================
    
    /**
     * Get today's screen time allowance for a child
     * 
     * @param childId The child's user ID
     * @return AllowanceResult with allowance and schedule info
     */
    AllowanceResult getTodayAllowance(const char* childId);
    
    /**
     * Record screen time usage
     * Call this periodically while timer is running.
     * 
     * @param childId The child's ID
     * @param minutesUsed Minutes of screen time used since last report
     * @return true if recording succeeded
     */
    bool recordScreenTimeUsed(const char* childId, uint32_t minutesUsed);
    
    /**
     * Push a completed screen time session to the API
     * Call this when a session ends (timer stopped manually or time expires).
     * Posts the session duration and start time.
     * 
     * @param childId The child's ID
     * @param sessionDurationMinutes Duration of this session in minutes
     * @param sessionStartTime Unix timestamp when the session started
     * @return ConsumedTimeResult with success/error status
     */
    ConsumedTimeResult pushConsumedTime(const char* childId, uint32_t sessionDurationMinutes, time_t sessionStartTime);
    
    // ========================================================================
    // Request More Time
    // ========================================================================
    
    /**
     * Submit a request for additional screen time
     * Parent will be notified and can approve/deny.
     * 
     * @param childId The child's ID
     * @param childName The child's name (for notification message, optional)
     * @param bonusMinutes Minutes of extra time to request (default 15)
     * @return MoreTimeRequestResult with grant ID for polling
     */
    MoreTimeRequestResult requestAdditionalTime(const char* childId, 
                                                 const char* childName = nullptr,
                                                 uint32_t bonusMinutes = 15);
    
    /**
     * Poll for more-time request approval
     * Call this periodically after requestAdditionalTime().
     * 
     * @param requestId The grant ID from requestAdditionalTime()
     * @return MoreTimePollResult with approval status
     */
    MoreTimePollResult pollMoreTimeStatus(const char* requestId);
    
    // ========================================================================
    // Mock Control (for testing/development)
    // ========================================================================
    
    /**
     * Enable or disable mock mode
     * When enabled, all API calls return simulated responses.
     * @param enabled true to enable mock mode
     */
    void setMockMode(bool enabled);
    
    /**
     * Check if mock mode is enabled
     * @return true if using mock responses
     */
    bool isMockMode() const;
    
    /**
     * Set the mock login delay (how long until mock login succeeds)
     * @param delayMs Delay in milliseconds
     */
    void setMockLoginDelay(uint32_t delayMs);
    
    /**
     * Set mock more-time response
     * @param granted Whether to simulate approval
     * @param additionalMinutes Extra time if granted
     */
    void setMockMoreTimeResponse(bool granted, uint32_t additionalMinutes);

private:
    NetworkManager* _network;
    char _baseUrl[128];
    char _apiKey[64];
    char _familyId[32];
    bool _mockMode;
    
    // Heap-allocated HTTP client to avoid stack overflow
    // WiFiClientSecure is ~16KB on stack due to SSL buffers
    WiFiClientSecure* _secureClient;
    char* _responseBuffer;  // Heap-allocated response buffer
    
    // Mock state
    uint32_t _mockLoginStartMs;
    uint32_t _mockLoginDelayMs;
    bool _mockMoreTimeGranted;
    uint32_t _mockMoreTimeMinutes;
    uint32_t _mockMoreTimeStartMs;
    
    // Mock device code counter (for generating unique codes)
    static uint32_t _mockDeviceCodeCounter;
    
    // HTTP request helpers
    bool ensureConnected();
    
    /**
     * Perform HTTP GET request
     * @param endpoint API endpoint (relative to base URL)
     * @param responseBuffer Buffer for response body
     * @param bufferSize Size of response buffer
     * @param authenticated If true, include X-API-Key header
     * @return HTTP status code (200 = success), or negative for errors
     */
    int httpGet(const char* endpoint, char* responseBuffer, size_t bufferSize, bool authenticated = true);
    
    /**
     * Perform HTTP POST request
     * @param endpoint API endpoint (relative to base URL)
     * @param body Request body (JSON), can be nullptr for empty body
     * @param responseBuffer Buffer for response body
     * @param bufferSize Size of response buffer
     * @param authenticated If true, include X-API-Key header
     * @return HTTP status code (200/201 = success), or negative for errors
     */
    int httpPost(const char* endpoint, const char* body, char* responseBuffer, size_t bufferSize, bool authenticated = true);
    
    // Date formatting helper
    void getTodayDateString(char* buffer, size_t bufferSize);
    
    // Time parsing helper - parses "HH:MM" into hour and minute
    void parseTimeString(const char* timeStr, uint8_t& outHour, uint8_t& outMinute);
    
    // Mock response generators (for testing)
    DeviceCodeResponse mockInitiateLogin();
    LoginPollResult mockPollLoginStatus(const char* pairingCode);
    FamilyGroupResult mockGetFamilyGroup();
    bool mockGetFamilyMembers(FamilyMember* members, int maxCount, int& outCount);
    AllowanceResult mockGetTodayAllowance(const char* childId);
    MoreTimeRequestResult mockRequestAdditionalTime(const char* childId);
    MoreTimePollResult mockPollMoreTimeStatus(const char* grantId);
};

#endif // API_CLIENT_H
