/**
 * network.h - Network and Sync for Screen Time Tracker
 * 
 * Provides network functionality:
 * - WiFi connection management with automatic lifecycle (R5)
 * - Server synchronization
 * - Time sync (NTP) with frequency control
 * - Remote configuration updates
 * 
 * Phase 6 Enhancements:
 * - ensureConnected() for auto-connect
 * - Keep-alive timer for automatic WiFi disconnect (R5.2-R5.4)
 * - Polling mode to keep WiFi active during login/more-time (R5.5)
 * - NTP sync frequency control (only sync if interval exceeded)
 * 
 * @author Screen Time Tracker
 * @version 2.0
 */

#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <stddef.h>
#include <functional>
#include "config.h"

/**
 * NetworkStatus - Connection status enumeration
 */
enum class NetworkStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR
};

/**
 * SyncStatus - Synchronization status
 */
enum class SyncStatus {
    IDLE,
    SYNCING,
    SUCCESS,
    FAILED
};

/**
 * NetworkManager class - Handles all network operations
 * 
 * Provides real WiFi connectivity with automatic lifecycle management:
 * - Auto-connect with ensureConnected()
 * - Keep-alive timer for automatic WiFi disconnect (R5.2-R5.4)
 * - Polling mode to keep WiFi active during login/more-time (R5.5)
 * - NTP sync frequency control
 */
class NetworkManager {
public:
    /**
     * Callback type for network operations
     * Return true if operation succeeded, false otherwise
     */
    using NetworkCallback = std::function<bool()>;

    /**
     * Constructor
     */
    NetworkManager();

    /**
     * Initialize network subsystem
     * @return true on success
     */
    bool begin();

    // ========================================================================
    // Auto-managed connection (R5.1-R5.4)
    // ========================================================================
    
    /**
     * Update network state - MUST be called every loop iteration
     * Manages keep-alive timer and auto-disconnect.
     */
    void update();

    /**
     * Ensure WiFi is connected, auto-connecting if needed (R5.1)
     * Resets the keep-alive timer on each call (R5.3).
     * @return true if connected (or now connected)
     */
    bool ensureConnected();

    /**
     * Extend keep-alive timer without new connection
     * Use when performing operations that should keep WiFi alive.
     */
    void extendKeepAlive();

    // ========================================================================
    // Polling mode (R5.5)
    // ========================================================================
    
    /**
     * Enter polling mode - WiFi won't auto-disconnect
     * Use during login polling or more-time request polling.
     */
    void beginPollingMode();

    /**
     * Exit polling mode - resume normal keep-alive behavior
     */
    void endPollingMode();

    /**
     * Check if in polling mode
     * @return true if polling mode is active
     */
    bool isInPollingMode() const;

    // ========================================================================
    // Manual control (for edge cases)
    // ========================================================================
    
    /**
     * Connect to WiFi network (manual control)
     * Prefer using ensureConnected() for automatic management.
     * @param ssid Network SSID
     * @param password Network password
     * @param timeoutMs Connection timeout in milliseconds
     * @return true if connection successful
     */
    bool connect(const char* ssid = WIFI_SSID, 
                 const char* password = WIFI_PASSWORD,
                 uint32_t timeoutMs = WIFI_CONNECT_TIMEOUT_MS);

    /**
     * Force disconnect from WiFi
     * Normally not needed - WiFi disconnects automatically after keep-alive.
     */
    void disconnect();

    /**
     * Force disconnect, ignoring polling mode
     * Use for power off or forced cleanup.
     */
    void forceDisconnect();

    // ========================================================================
    // Status
    // ========================================================================
    
    /**
     * Get current connection status
     * @return Current NetworkStatus
     */
    NetworkStatus getStatus() const;

    /**
     * Check if connected to WiFi
     * @return true if connected
     */
    bool isConnected() const;

    /**
     * Get WiFi signal strength
     * @return RSSI value
     */
    int getSignalStrength() const;

    // ========================================================================
    // Legacy patterns (still supported)
    // ========================================================================
    
    /**
     * Elegant connect-work-disconnect pattern
     * Connects to WiFi, executes the callback, then disconnects.
     * 
     * NOTE: This is the legacy pattern. For new code, prefer:
     *   if (network.ensureConnected()) { doWork(); }
     * 
     * @param callback Function to execute while connected
     * @return true if connection succeeded AND callback returned true
     */
    bool withConnection(NetworkCallback callback);

    // ========================================================================
    // NTP time sync
    // ========================================================================
    
    /**
     * Sync time with NTP server and set the RTC
     * Must be called while connected to WiFi.
     * Respects NTP_SYNC_INTERVAL_HOURS - will skip sync if not enough time passed.
     * @param force If true, sync regardless of interval
     * @return true if time sync and RTC set successful (or skipped because recent)
     */
    bool syncTimeAndSetRTC(bool force = false);

    /**
     * Check if NTP sync is needed based on interval
     * @return true if sync should be performed
     */
    bool isNtpSyncNeeded() const;

private:
    NetworkStatus _status;
    
    // Keep-alive state (R5.2-R5.4)
    uint32_t _lastActivityMs;          // Last network activity timestamp
    uint32_t _keepAliveDurationMs;     // How long to keep WiFi after last op
    
    // Polling mode (R5.5)
    bool _pollingMode;                 // If true, skip auto-disconnect
    
    // WiFi configuration status
    bool _wifiNotConfigured = false;   // True if credentials not set up
    
public:
    /**
     * Check if WiFi credentials are not configured
     * @return true if WiFi is not configured (credentials.h missing or placeholder values)
     */
    bool isWifiNotConfigured() const { return _wifiNotConfigured; }

private:
    // Internal helper
    void resetKeepAliveTimer();
};

/**
 * SyncManager class - Handles data synchronization with server
 * 
 * This is a stub implementation for future server sync.
 */
class SyncManager {
public:
    /**
     * Constructor
     * @param network Reference to NetworkManager
     */
    explicit SyncManager(NetworkManager& network);

    /**
     * Initialize sync subsystem
     * @param serverUrl Base URL of sync server
     * @return true on success
     */
    bool begin(const char* serverUrl = nullptr);

    /**
     * Sync remaining time to server
     * @param userId User identifier
     * @param remainingSeconds Current remaining time
     * @return true if sync successful
     */
    bool syncRemainingTime(const char* userId, uint32_t remainingSeconds);

    /**
     * Fetch remaining time from server
     * @param userId User identifier
     * @param outSeconds Output parameter for remaining seconds
     * @return true if fetch successful
     */
    bool fetchRemainingTime(const char* userId, uint32_t& outSeconds);

    /**
     * Sync timer start event
     * @param userId User identifier
     * @return true if sync successful
     */
    bool syncTimerStarted(const char* userId);

    /**
     * Sync timer stop event
     * @param userId User identifier  
     * @param remainingSeconds Time remaining when stopped
     * @return true if sync successful
     */
    bool syncTimerStopped(const char* userId, uint32_t remainingSeconds);

    /**
     * Fetch user configuration from server
     * @param userId User identifier
     * @param outDailyAllowance Output for daily allowance
     * @param outUserName Output buffer for user name
     * @param nameBufferSize Size of name buffer
     * @return true if fetch successful
     */
    bool fetchUserConfig(const char* userId, 
                         uint32_t& outDailyAllowance,
                         char* outUserName,
                         size_t nameBufferSize);

    /**
     * Synchronize time with NTP server
     * @return true if time sync successful
     */
    bool syncTime();

    /**
     * Get current sync status
     * @return Current SyncStatus
     */
    SyncStatus getStatus() const;

    /**
     * Get last sync timestamp
     * @return Epoch time of last successful sync (0 if never)
     */
    uint32_t getLastSyncTime() const;

private:
    NetworkManager& _network;
    SyncStatus _status;
    uint32_t _lastSyncTime;
    char _serverUrl[128];
};

#endif // NETWORK_H
