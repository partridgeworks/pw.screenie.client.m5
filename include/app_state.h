/**
 * app_state.h - Application State for Screen Time Tracker
 * 
 * Centralized state management using singleton pattern.
 * Holds user session, screen time data, and network status.
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#ifndef APP_STATE_H
#define APP_STATE_H

#include <stdint.h>
#include "config.h"
#include "network.h"
#include "screen_manager.h"

// Forward declaration
class PersistenceManager;

// ============================================================================
// Data Structures
// ============================================================================

/**
 * UserSession - Logged-in user information
 */
struct UserSession {
    bool isLoggedIn;                     // Whether user is authenticated
    char apiKey[64];                     // API key from authentication
    char familyId[32];                   // Family group ID (for API calls)
    char username[MAX_NAME_LENGTH];      // Display name
    char selectedChildId[32];            // ID of selected child/family member
    char selectedChildName[MAX_NAME_LENGTH]; // Name of selected child
    char selectedChildInitial;           // Initial letter for avatar
    char selectedChildAvatarName[32];    // Avatar filename with extension (e.g., "1F3B1_color.png")
    
    UserSession() 
        : isLoggedIn(false)
        , selectedChildInitial(DEFAULT_USER_INITIAL)
    {
        apiKey[0] = '\0';
        familyId[0] = '\0';
        username[0] = '\0';
        selectedChildId[0] = '\0';
        selectedChildName[0] = '\0';
        selectedChildAvatarName[0] = '\0';
    }
};

/**
 * ScreenTimeData - Screen time tracking data
 */
struct ScreenTimeData {
    uint32_t dailyAllowanceSeconds;      // Today's total allowed screen time
    uint32_t usedTodaySeconds;           // Time used today (for syncing)
    uint32_t remainingSeconds;           // Time remaining
    bool isActive;                       // Timer currently running
    uint8_t lastActiveWeekday;           // 0-6 (Sun-Sat) for day-change detection
    int64_t lastSyncTimestamp;           // Unix timestamp of last sync
    bool hasUnlimitedAllowance;          // true = no time restriction for today (null from API)
    
    ScreenTimeData()
        : dailyAllowanceSeconds(0)       // 0 = not yet fetched from API
        , usedTodaySeconds(0)
        , remainingSeconds(0)
        , isActive(false)
        , lastActiveWeekday(0xFF)        // 0xFF = not set
        , lastSyncTimestamp(0)
        , hasUnlimitedAllowance(false)
    {}
};

/**
 * DaySchedule - Wake/bedtime schedule (future use)
 * Returned by API but currently ignored
 */
struct DaySchedule {
    uint8_t wakeUpHour;
    uint8_t wakeUpMinute;
    uint8_t bedTimeHour;
    uint8_t bedTimeMinute;
    
    DaySchedule()
        : wakeUpHour(7)
        , wakeUpMinute(0)
        , bedTimeHour(21)
        , bedTimeMinute(0)
    {}
};

// ============================================================================
// AppState Singleton
// ============================================================================

/**
 * AppState - Centralized application state (singleton)
 * 
 * Access via AppState::getInstance()
 */
class AppState {
public:
    /**
     * Get the singleton instance
     * @return Reference to the AppState instance
     */
    static AppState& getInstance();
    
    // Prevent copying
    AppState(const AppState&) = delete;
    AppState& operator=(const AppState&) = delete;
    
    // ========================================================================
    // User Session
    // ========================================================================
    
    /**
     * Get the current user session
     * @return Reference to user session data
     */
    UserSession& getSession();
    const UserSession& getSession() const;
    
    /**
     * Check if user is logged in
     * @return true if logged in with valid API key
     */
    bool isLoggedIn() const;
    
    /**
     * Check if a child/family member is selected
     * @return true if a child is selected
     */
    bool hasSelectedChild() const;
    
    /**
     * Get the display initial for the avatar
     * Uses selected child initial, or default if none selected
     * @return Character to display in avatar
     */
    char getAvatarInitial() const;
    
    /**
     * Get the display name
     * Uses selected child name, or default if none selected
     * @return Name string
     */
    const char* getDisplayName() const;
    
    // ========================================================================
    // Screen Time
    // ========================================================================
    
    /**
     * Get screen time data
     * @return Reference to screen time data
     */
    ScreenTimeData& getScreenTime();
    const ScreenTimeData& getScreenTime() const;
    
    // ========================================================================
    // Network Status
    // ========================================================================
    
    /**
     * Get current network status
     * @return Current network status
     */
    NetworkStatus getNetworkStatus() const;
    
    /**
     * Update network status
     * @param status New status
     */
    void setNetworkStatus(NetworkStatus status);
    
    // ========================================================================
    // Day Change Detection
    // ========================================================================
    
    /**
     * Check if the weekday has changed since last recorded
     * Compares current RTC weekday to lastActiveWeekday
     * @return true if weekday has changed (new day)
     */
    bool hasWeekdayChanged() const;
    
    /**
     * Update the last active weekday to current day
     * Call this after syncing on a new day
     */
    void updateLastActiveWeekday();
    
    /**
     * Get current weekday from RTC
     * @return 0-6 (Sunday-Saturday)
     */
    uint8_t getCurrentWeekday() const;
    
    // ========================================================================
    // Screen Flow
    // ========================================================================
    
    /**
     * Determine which screen should be shown initially
     * Based on login state and child selection
     * @return Initial screen type
     */
    ScreenType determineInitialScreen() const;
    
    // ========================================================================
    // Persistence (R8)
    // ========================================================================
    
    /**
     * Load state from persistent storage
     * Call this once during app startup, after PersistenceManager::begin()
     * @return true if session was restored (user was logged in)
     */
    bool loadFromPersistence();
    
    /**
     * Save session to persistent storage
     * Call this after login or child selection changes
     * @return true if save successful
     */
    bool saveSessionToPersistence();
    
    /**
     * Save weekday to persistent storage
     * Call this after day change detection and sync
     * @return true if save successful
     */
    bool saveWeekdayToPersistence();
    
    /**
     * Save screen time allowance to persistent storage
     * Call this after fetching new allowance from API
     * @return true if save successful
     */
    bool saveAllowanceToPersistence();
    
    /**
     * Clear all persistent data (logout)
     * Clears session and resets state to defaults
     * @return true if clear successful
     */
    bool clearPersistence();

private:
    /**
     * Private constructor for singleton
     */
    AppState();
    
    UserSession _session;
    ScreenTimeData _screenTime;
    DaySchedule _schedule;
    NetworkStatus _networkStatus;
};

#endif // APP_STATE_H
