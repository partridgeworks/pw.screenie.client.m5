/**
 * timer.h - Screen Timer management for Screen Time Tracker
 * 
 * Handles screen time tracking using a consumed-time model:
 * - Tracks total consumed time today (previous sessions + current)
 * - Starting/stopping sessions
 * - Progress calculation
 * - Time expiry detection
 * 
 * Uses consumed time (not remaining) for easier API sync and deep sleep.
 * 
 * @author Screen Time Tracker
 * @version 2.0
 */

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <time.h>
#include "config.h"

/**
 * TimerState - Possible states of the screen timer
 */
enum class TimerState {
    STOPPED,    // Timer not running, preserving remaining time
    RUNNING,    // Timer actively counting down
    EXPIRED     // Timer reached zero
};

/**
 * ScreenTimer class - Manages screen time tracking
 * 
 * Uses a consumed-time model: tracks how much time has been used today.
 * Remaining time = allowance - consumed.
 * This simplifies deep sleep (just save session start time) and API sync.
 */
class ScreenTimer {
public:
    /**
     * Constructor - Initialize with no allowance (must call begin())
     */
    ScreenTimer();

    /**
     * Initialize the timer with a specific allowance
     * @param allowanceSeconds Daily screen time allowance in seconds
     * @param consumedSeconds Time already consumed today (optional, for restore)
     */
    void begin(uint32_t allowanceSeconds, uint32_t consumedSeconds = 0);

    /**
     * Reset timer for a new day (clears consumed time)
     * @param allowanceSeconds New allowance (optional, uses current if 0)
     */
    void reset(uint32_t allowanceSeconds = 0);

    /**
     * Start a new session (begin tracking consumed time)
     * @return true if started successfully, false if already expired
     */
    bool start();

    /**
     * Start a session that was already running (for deep sleep restore)
     * @param sessionStartTime Unix timestamp when session originally started
     * @return true if started successfully
     */
    bool startFromTimestamp(time_t sessionStartTime);

    /**
     * Stop/pause the countdown timer.
     * @param minimumDuration If > 0, enforce this as minimum session duration
     * @return Actual session duration in seconds (before any minimum enforcement)
     */
    uint32_t stop(uint32_t minimumDuration = 0);

    /**
     * Abort/cancel the current session without committing time.
     * Used for "mistaken" sessions that should be forgotten.
     * Session time is NOT added to consumed total.
     */
    void abortSession();

    /**
     * Toggle between running and stopped states
     * @return true if timer is now running
     */
    bool toggle();

    /**
     * Update timer state - must be called regularly in loop()
     * Decrements remaining time if running
     */
    void update();

    /**
     * Get remaining time in seconds
     * @return Remaining seconds
     */
    uint32_t calculateRemainingSeconds() const;

    /**
     * Get progress as fraction (1.0 = full, 0.0 = expired)
     * @return Progress value between 0.0 and 1.0
     */
    float getProgress() const;

    /**
     * Get current timer state
     * @return Current TimerState
     */
    TimerState getState() const;

    /**
     * Check if timer is currently running
     * @return true if running
     */
    bool isRunning() const;

    /**
     * Check if timer has expired
     * @return true if remaining time is zero
     */
    bool isExpired() const;

    /**
     * Get the total daily allowance
     * @return Total allowance in seconds
     */
    uint32_t getTotalAllowance() const;

    /**
     * Get total consumed time today (completed sessions + current session if running)
     * This is a computed value, not the raw _consumedTodaySeconds variable.
     * @return Total consumed seconds
     */
    uint32_t getTotalConsumedSeconds() const;

    /**
     * Get consumed time from completed sessions only (NOT including current session).
     * Use this for deep sleep persistence to avoid double-counting.
     * @return Completed sessions consumed seconds
     */
    uint32_t getCompletedSessionsSeconds() const;

    /**
     * Get current session duration (0 if not running)
     * @return Current session elapsed seconds
     */
    uint32_t getCurrentSessionSeconds() const;

    /**
     * Get session start timestamp (for deep sleep persistence)
     * @return Unix timestamp when current session started, 0 if not running
     */
    time_t getSessionStartTime() const;

    /**
     * Set consumed time directly (for sync/restore purposes)
     * @param seconds Consumed time in seconds
     */
    void setConsumedTodaySeconds(uint32_t seconds);


    /**
     * Set the daily allowance without clearing consumed time.
     * Use this when restoring state or updating allowance mid-day.
     * @param allowanceSeconds New total allowance in seconds
     */
    void setAllowance(uint32_t allowanceSeconds);

    /**
     * Add additional time to the daily allowance (e.g., more-time granted).
     * @param additionalSeconds Extra seconds to add to allowance
     */
    void addAllowance(uint32_t additionalSeconds);

private:
    TimerState _state;
    uint32_t _totalAllowanceSeconds;
    uint32_t _consumedTodaySeconds;  // Time from COMPLETED sessions only (not current)
    time_t _sessionStartTime;        // Unix timestamp when current session started (0 if stopped)
    
    /**
     * Calculate current session elapsed time
     * @return Elapsed seconds in current session, or 0 if not running
     */
    uint32_t calculateCurrentSessionSeconds() const;
};

#endif // TIMER_H
