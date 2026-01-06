/**
 * polling_manager.h - Polling Manager for Screen Time Tracker
 * 
 * Manages background polling for:
 * - Login completion (device-code flow)
 * - More-time request approval
 * 
 * Handles polling intervals, timeouts, and callbacks.
 * Call update() every loop iteration for non-blocking polling.
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#ifndef POLLING_MANAGER_H
#define POLLING_MANAGER_H

#include <stdint.h>
#include <functional>
#include "config.h"

// Forward declarations
class ApiClient;
class NetworkManager;

// ============================================================================
// Polling Types and Callbacks
// ============================================================================

/**
 * PollingType - What kind of polling is active
 */
enum class PollingType {
    NONE,           // No active polling
    LOGIN,          // Waiting for user to complete login on another device
    MORE_TIME       // Waiting for parent to approve more time
};

/**
 * PollingStatus - Current status of a polling operation
 */
enum class PollingStatus {
    IDLE,           // Not polling
    POLLING,        // Actively polling
    SUCCESS,        // Polling completed successfully
    TIMEOUT,        // Polling timed out
    ERROR           // Polling failed with error
};

/**
 * PollingResult - Result passed to callback when polling completes
 */
struct PollingResult {
    bool success;                    // Operation succeeded
    bool timedOut;                   // Polling timed out
    char message[64];                // Result message
    
    // Login-specific results
    char apiKey[64];                 // API key if login succeeded
    char username[32];               // Username if login succeeded
    
    // More-time-specific results
    bool granted;                    // More time was granted
    bool denied;                     // More time was denied
    uint32_t additionalMinutes;      // Extra minutes if granted
    
    PollingResult()
        : success(false)
        , timedOut(false)
        , granted(false)
        , denied(false)
        , additionalMinutes(0)
    {
        message[0] = '\0';
        apiKey[0] = '\0';
        username[0] = '\0';
    }
};

/**
 * Callback type for polling completion
 * @param result The polling result
 * @param userData User-provided context pointer
 */
using PollingCallback = std::function<void(const PollingResult& result, void* userData)>;

// ============================================================================
// Polling Manager Class
// ============================================================================

/**
 * PollingManager - Handles background polling operations
 * 
 * Features:
 * - Non-blocking polling via update() calls
 * - Configurable intervals and timeouts
 * - Automatic timeout handling
 * - Callback on completion
 * 
 * Usage:
 *   PollingManager polling;
 *   polling.begin(apiClient, networkManager);
 *   
 *   // Start login polling
 *   polling.startLoginPolling("device-code", [](const PollingResult& result, void*) {
 *       if (result.success) { ... }
 *   });
 *   
 *   // In loop()
 *   polling.update();
 */
class PollingManager {
public:
    /**
     * Constructor
     */
    PollingManager();
    
    /**
     * Initialize the polling manager
     * @param api Reference to ApiClient
     * @param network Reference to NetworkManager
     */
    void begin(ApiClient& api, NetworkManager& network);
    
    /**
     * Update polling state - call every loop iteration
     * Checks if it's time to poll and handles timeouts.
     */
    void update();
    
    // ========================================================================
    // Polling Control
    // ========================================================================
    
    /**
     * Start polling for login completion
     * 
     * @param deviceCode Device code from ApiClient::initiateLogin()
     * @param callback Function to call when polling completes
     * @param userData Optional context pointer passed to callback
     */
    void startLoginPolling(const char* deviceCode, 
                           PollingCallback callback, 
                           void* userData = nullptr);
    
    /**
     * Start polling for more-time approval
     * 
     * @param requestId Request ID from ApiClient::requestAdditionalTime()
     * @param callback Function to call when polling completes
     * @param userData Optional context pointer passed to callback
     */
    void startMoreTimePolling(const char* requestId, 
                              PollingCallback callback, 
                              void* userData = nullptr);
    
    /**
     * Stop all active polling
     */
    void stopPolling();
    
    // ========================================================================
    // Status
    // ========================================================================
    
    /**
     * Check if currently polling
     * @return true if any polling is active
     */
    bool isPolling() const;
    
    /**
     * Get the type of active polling
     * @return PollingType (NONE if not polling)
     */
    PollingType getPollingType() const;
    
    /**
     * Get current polling status
     * @return PollingStatus
     */
    PollingStatus getStatus() const;
    
    /**
     * Get remaining timeout in seconds
     * @return Seconds until timeout (0 if no timeout or not polling)
     */
    uint32_t getRemainingTimeoutSeconds() const;
    
    /**
     * Get elapsed time since polling started in seconds
     * @return Seconds since polling started (0 if not polling)
     */
    uint32_t getElapsedSeconds() const;
    
    // ========================================================================
    // Configuration
    // ========================================================================
    
    /**
     * Set login polling interval
     * @param intervalMs Milliseconds between polls
     */
    void setLoginPollInterval(uint32_t intervalMs);
    
    /**
     * Set login polling timeout
     * @param timeoutMs Milliseconds before timeout
     */
    void setLoginTimeout(uint32_t timeoutMs);
    
    /**
     * Set more-time polling interval
     * @param intervalMs Milliseconds between polls
     */
    void setMoreTimePollInterval(uint32_t intervalMs);
    
    /**
     * Set more-time polling timeout
     * @param timeoutMs Milliseconds before timeout
     */
    void setMoreTimeTimeout(uint32_t timeoutMs);

private:
    ApiClient* _api;
    NetworkManager* _network;
    
    // Current polling state
    PollingType _type;
    PollingStatus _status;
    char _pollId[32];                  // Device code or request ID
    PollingCallback _callback;
    void* _userData;
    
    // Timing
    uint32_t _startTimeMs;             // When polling started
    uint32_t _lastPollMs;              // Last poll timestamp
    
    // Configuration (milliseconds)
    uint32_t _loginPollIntervalMs;
    uint32_t _loginTimeoutMs;
    uint32_t _moreTimePollIntervalMs;
    uint32_t _moreTimeTimeoutMs;
    
    // Internal methods
    void pollLogin();
    void pollMoreTime();
    void completePolling(const PollingResult& result);
    uint32_t getCurrentInterval() const;
    uint32_t getCurrentTimeout() const;
};

#endif // POLLING_MANAGER_H
