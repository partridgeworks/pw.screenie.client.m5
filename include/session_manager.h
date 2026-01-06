/**
 * session_manager.h - Session Lifecycle Management for Screen Time Tracker
 * 
 * Consolidates session management logic that was previously scattered across
 * MainScreen, ScreenTimer, and main.cpp. Provides a single interface for:
 * - Starting/stopping screen time sessions
 * - Persisting session state (NVS for crash recovery, RTC for deep sleep)
 * - Pushing completed sessions to API
 * - Suspending/resuming sessions for deep sleep
 * 
 * The SessionManager owns the timer logic flow but delegates to ScreenTimer
 * for the core time tracking calculations.
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include <stdint.h>
#include <time.h>
#include "timer.h"

// Forward declarations
class ApiClient;
class ScreenTimer;

/**
 * SessionSnapshot - Portable state for saving/restoring sessions
 * Used for both RTC memory (deep sleep) and NVS (crash recovery)
 */
struct SessionSnapshot {
    uint32_t consumedTodaySeconds;   // Time consumed in completed sessions
    time_t sessionStartTime;         // Unix timestamp when current session started (0 if not running)
    TimerState timerState;           // Current timer state
    uint8_t weekday;                 // Weekday when snapshot was taken (0-6)
    bool isValid;                    // Whether this snapshot contains valid data
    
    SessionSnapshot()
        : consumedTodaySeconds(0)
        , sessionStartTime(0)
        , timerState(TimerState::STOPPED)
        , weekday(0xFF)
        , isValid(false)
    {}
};

/**
 * SessionManager - Centralized session lifecycle management
 * 
 * Encapsulates all logic for:
 * - Starting/stopping sessions
 * - Tracking consumed time
 * - Persisting state for crash recovery and deep sleep
 * - Pushing sessions to API
 * 
 * Usage:
 *   SessionManager sessionMgr(timer);
 *   sessionMgr.setApiClient(&apiClient);
 *   
 *   // Start a session
 *   if (sessionMgr.startSession()) { ... }
 *   
 *   // Stop a session
 *   sessionMgr.stopSession();
 *   
 *   // Before deep sleep
 *   sessionMgr.suspendForSleep();
 *   
 *   // After waking from deep sleep
 *   sessionMgr.resumeFromSleep(snapshot);
 */
class SessionManager {
public:
    /**
     * Constructor
     * @param timer Reference to the ScreenTimer for time calculations
     */
    explicit SessionManager(ScreenTimer& timer);
    
    /**
     * Set the API client for pushing sessions
     * @param api Pointer to ApiClient (can be nullptr if no API available)
     */
    void setApiClient(ApiClient* api);
    
    // ========================================================================
    // Session Control
    // ========================================================================
    
    /**
     * Start a new screen time session
     * Records the start time and transitions timer to RUNNING state.
     * @return true if session started, false if already expired or running
     */
    bool startSession();
    
    /**
     * Stop the current session normally
     * Commits consumed time, persists to NVS, and pushes to API.
     * @param minimumDuration If > 0, enforce this as minimum session duration
     * @return Actual session duration in seconds (before minimum enforcement)
     */
    uint32_t stopSession(uint32_t minimumDuration = 0);
    
    /**
     * Abort/cancel the current session without counting time
     * Used for "mistaken" sessions (very short, accidental starts).
     * Time is NOT added to consumed total.
     */
    void abortSession();
    
    /**
     * Handle timer expiry
     * Called when the timer reaches zero. Commits time, persists, and pushes to API.
     * @param sessionDuration Duration of the expired session
     * @param sessionStartTime When the session started
     */
    void onSessionExpired(uint32_t sessionDuration, time_t sessionStartTime);
    
    // ========================================================================
    // Deep Sleep Support
    // ========================================================================
    
    /**
     * Create a snapshot of current session state for saving
     * Use before deep sleep or for periodic NVS backup.
     * @return SessionSnapshot with current state
     */
    SessionSnapshot createSnapshot() const;
    
    /**
     * Restore session state from a snapshot
     * Use after waking from deep sleep.
     * @param snapshot The snapshot to restore from
     * @param currentWeekday Current weekday for day-change detection
     * @return true if restoration successful, false if day changed or invalid
     */
    bool restoreFromSnapshot(const SessionSnapshot& snapshot, uint8_t currentWeekday);
    
    /**
     * Persist current consumed time to NVS (crash recovery)
     * Called periodically and on session stop.
     */
    void persistToNvs();
    
    /**
     * Load consumed time from NVS
     * Called on startup for crash recovery (not from deep sleep).
     * @param currentWeekday Current weekday for day-change detection
     * @return Consumed seconds if same day, 0 if new day or not set
     */
    uint32_t loadFromNvs(uint8_t currentWeekday);
    
    /**
     * Clear persisted consumed time (for day reset)
     */
    void clearNvsConsumedTime();
    
    // ========================================================================
    // Day Change Support
    // ========================================================================
    
    /**
     * Reset for a new day
     * Clears consumed time and resets timer state.
     * @param newAllowanceSeconds New daily allowance (optional, uses current if 0)
     */
    void resetForNewDay(uint32_t newAllowanceSeconds = 0);
    
    // ========================================================================
    // State Queries
    // ========================================================================
    
    /**
     * Check if a session is currently running
     * @return true if session is active
     */
    bool isSessionRunning() const;
    
    /**
     * Check if time has expired
     * @return true if no time remaining
     */
    bool isExpired() const;
    
    /**
     * Get remaining seconds
     * @return Seconds remaining today
     */
    uint32_t getRemainingSeconds() const;
    
    /**
     * Get total consumed time today (completed sessions + current session if running)
     * @return Total consumed seconds
     */
    uint32_t getTotalConsumedSeconds() const;
    
    /**
     * Get current session duration
     * @return Current session elapsed seconds, 0 if not running
     */
    uint32_t getCurrentSessionSeconds() const;
    
    /**
     * Get session start time
     * @return Unix timestamp when current session started, 0 if not running
     */
    time_t getSessionStartTime() const;
    
    /**
     * Get reference to the underlying timer
     * @return Reference to ScreenTimer
     */
    ScreenTimer& getTimer();
    
private:
    ScreenTimer& _timer;
    ApiClient* _apiClient;
    
    /**
     * Push a completed session to the API
     * Fire-and-forget operation - failures are logged but don't block.
     * @param durationSeconds Session duration in seconds
     * @param startTime When the session started
     */
    void pushSessionToApi(uint32_t durationSeconds, time_t startTime);
};

#endif // SESSION_MANAGER_H
