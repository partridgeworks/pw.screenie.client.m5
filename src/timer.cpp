/**
 * timer.cpp - Screen Timer implementation for Screen Time Tracker
 * 
 * Implements consumed-time tracking model using Unix timestamps.
 * Remaining time is computed as: allowance - (consumedToday + currentSession)
 * 
 * @author Screen Time Tracker
 * @version 2.0
 */

#include "timer.h"
#include <Arduino.h>

// ============================================================================
// Constructor and Initialization
// ============================================================================

ScreenTimer::ScreenTimer()
    : _state(TimerState::STOPPED)
    , _totalAllowanceSeconds(0)  // Must call begin() or reset() to set allowance
    , _consumedTodaySeconds(0)
    , _sessionStartTime(0)
{
}

void ScreenTimer::begin(uint32_t allowanceSeconds, uint32_t consumedSeconds) {
    _totalAllowanceSeconds = allowanceSeconds;
    _consumedTodaySeconds = consumedSeconds;
    _sessionStartTime = 0;
    
    // Check if already expired based on consumed time
    if (_consumedTodaySeconds >= _totalAllowanceSeconds) {
        _state = TimerState::EXPIRED;
    } else {
        _state = TimerState::STOPPED;
    }
    
    Serial.printf("[Timer] Initialized: allowance=%lu, consumed=%lu, remaining=%lu\n", 
                  (unsigned long)allowanceSeconds,
                  (unsigned long)consumedSeconds,
                  (unsigned long)calculateRemainingSeconds());
}

// ============================================================================
// Timer Control Methods
// ============================================================================

void ScreenTimer::reset(uint32_t allowanceSeconds) {
    if (allowanceSeconds > 0) {
        _totalAllowanceSeconds = allowanceSeconds;
    }
    
    // Reset consumed time for new day
    _consumedTodaySeconds = 0;
    _sessionStartTime = 0;
    _state = TimerState::STOPPED;
    
    Serial.printf("[Timer] Reset for new day: allowance=%lu seconds\n", 
                  (unsigned long)_totalAllowanceSeconds);
}

bool ScreenTimer::start() {
    // Check if already expired
    uint32_t remaining = calculateRemainingSeconds();
    if (remaining == 0) {
        _state = TimerState::EXPIRED;
        Serial.println("[Timer] Cannot start - time expired");
        return false;
    }
    
    if (_state != TimerState::RUNNING) {
        // Record session start time (Unix timestamp)
        time(&_sessionStartTime);
        _state = TimerState::RUNNING;
        Serial.printf("[Timer] Session started at timestamp %ld, %lu seconds remaining\n",
                      (long)_sessionStartTime,
                      (unsigned long)remaining);
    }
    
    return true;
}

bool ScreenTimer::startFromTimestamp(time_t sessionStartTime) {
    // Validate the timestamp is reasonable (not in future, not too far in past)
    time_t now;
    time(&now);
    
    if (sessionStartTime > now) {
        Serial.println("[Timer] startFromTimestamp: timestamp in future, using now");
        sessionStartTime = now;
    }
    
    // Check if this would result in expiry
    uint32_t sessionElapsed = (uint32_t)(now - sessionStartTime);
    uint32_t totalConsumed = _consumedTodaySeconds + sessionElapsed;
    
    if (totalConsumed >= _totalAllowanceSeconds) {
        _state = TimerState::EXPIRED;
        _consumedTodaySeconds = _totalAllowanceSeconds;
        _sessionStartTime = 0;
        Serial.println("[Timer] startFromTimestamp: session has expired during sleep");
        return false;
    }
    
    _sessionStartTime = sessionStartTime;
    _state = TimerState::RUNNING;
    Serial.printf("[Timer] Restored session from timestamp %ld (elapsed: %lu sec)\n",
                  (long)_sessionStartTime,
                  (unsigned long)sessionElapsed);
    return true;
}

uint32_t ScreenTimer::stop(uint32_t minimumDuration) {
    if (_state != TimerState::RUNNING) {
        return 0;
    }
    
    // Get actual session duration
    uint32_t actualSeconds = calculateCurrentSessionSeconds();
    
    // Determine effective duration (enforce minimum if specified)
    uint32_t effectiveSeconds = actualSeconds;
    if (minimumDuration > 0 && actualSeconds < minimumDuration) {
        effectiveSeconds = minimumDuration;
    }
    
    // Commit to consumed time
    _consumedTodaySeconds += effectiveSeconds;
    _sessionStartTime = 0;
    _state = TimerState::STOPPED;
    
    if (minimumDuration > 0 && actualSeconds < minimumDuration) {
        Serial.printf("[Timer] Session stopped (minimum enforced): actual=%lu sec, committed=%lu sec, total consumed=%lu\n",
                      (unsigned long)actualSeconds,
                      (unsigned long)effectiveSeconds,
                      (unsigned long)_consumedTodaySeconds);
    } else {
        Serial.printf("[Timer] Session stopped: +%lu sec, total consumed=%lu, remaining=%lu\n",
                      (unsigned long)actualSeconds,
                      (unsigned long)_consumedTodaySeconds,
                      (unsigned long)calculateRemainingSeconds());
    }
    
    return actualSeconds;
}

void ScreenTimer::abortSession() {
    if (_state == TimerState::RUNNING) {
        // Abort session WITHOUT adding time to consumed total
        uint32_t sessionSeconds = calculateCurrentSessionSeconds();
        _sessionStartTime = 0;
        _state = TimerState::STOPPED;
        
        Serial.printf("[Timer] Session aborted (forgotten): %lu sec NOT counted, remaining=%lu\n",
                      (unsigned long)sessionSeconds,
                      (unsigned long)calculateRemainingSeconds());
    }
}

bool ScreenTimer::toggle() {
    if (_state == TimerState::RUNNING) {
        stop();
        return false;
    } else {
        return start();
    }
}

// ============================================================================
// Timer Update
// ============================================================================

void ScreenTimer::update() {
    // Only update if running
    if (_state != TimerState::RUNNING) {
        return;
    }
    
    // Calculate current remaining time
    uint32_t remaining = calculateRemainingSeconds();
    
    // Debug output every 10 seconds (when crossing a 10-second boundary)
    static uint32_t lastLoggedRemaining = 0xFFFFFFFF;
    if (remaining != lastLoggedRemaining && remaining % 10 == 0 && remaining > 0) {
        Serial.printf("[Timer] %lu seconds remaining\n", 
                      (unsigned long)remaining);
        lastLoggedRemaining = remaining;
    }
    
    // Check for expiry
    if (remaining == 0) {
        // Finalize consumed time to match allowance
        _consumedTodaySeconds = _totalAllowanceSeconds;
        _sessionStartTime = 0;
        _state = TimerState::EXPIRED;
        Serial.println("[Timer] EXPIRED - Screen time used up!");
    }
}

// ============================================================================
// Helper Methods
// ============================================================================

uint32_t ScreenTimer::calculateCurrentSessionSeconds() const {
    if (_state != TimerState::RUNNING || _sessionStartTime == 0) {
        return 0;
    }
    
    time_t now;
    time(&now);
    
    // Guard against clock issues (session start in future)
    if (now <= _sessionStartTime) {
        return 0;
    }
    
    return (uint32_t)(now - _sessionStartTime);
}

// ============================================================================
// Query Methods
// ============================================================================

uint32_t ScreenTimer::calculateRemainingSeconds() const {
    uint32_t totalConsumed = getTotalConsumedSeconds();
    
    if (totalConsumed >= _totalAllowanceSeconds) {
        return 0;
    }
    
    return _totalAllowanceSeconds - totalConsumed;
}

float ScreenTimer::getProgress() const {
    if (_totalAllowanceSeconds == 0) {
        return 0.0f;
    }
    
    uint32_t remaining = calculateRemainingSeconds();
    return (float)remaining / (float)_totalAllowanceSeconds;
}

TimerState ScreenTimer::getState() const {
    return _state;
}

bool ScreenTimer::isRunning() const {
    return _state == TimerState::RUNNING;
}

bool ScreenTimer::isExpired() const {
    // Only check state - update() is responsible for transitioning to EXPIRED
    // when remaining time reaches 0. This avoids masking state sync bugs.
    return _state == TimerState::EXPIRED;
}

uint32_t ScreenTimer::getTotalAllowance() const {
    return _totalAllowanceSeconds;
}

uint32_t ScreenTimer::getTotalConsumedSeconds() const {
    return _consumedTodaySeconds + calculateCurrentSessionSeconds();
}

uint32_t ScreenTimer::getCompletedSessionsSeconds() const {
    return _consumedTodaySeconds;
}

uint32_t ScreenTimer::getCurrentSessionSeconds() const {
    return calculateCurrentSessionSeconds();
}

time_t ScreenTimer::getSessionStartTime() const {
    return _sessionStartTime;
}

void ScreenTimer::setConsumedTodaySeconds(uint32_t seconds) {
    _consumedTodaySeconds = seconds;
    
    // Update state based on new value
    if (_consumedTodaySeconds >= _totalAllowanceSeconds) {
        _state = TimerState::EXPIRED;
        _sessionStartTime = 0;
    } else if (_state == TimerState::EXPIRED) {
        _state = TimerState::STOPPED;
    }
    
    Serial.printf("[Timer] Consumed time set to %lu seconds, remaining=%lu\n", 
                  (unsigned long)seconds,
                  (unsigned long)calculateRemainingSeconds());
}



void ScreenTimer::setAllowance(uint32_t allowanceSeconds) {
    if (allowanceSeconds == 0) {
        return;
    }
    
    _totalAllowanceSeconds = allowanceSeconds;
    
    // Update state based on whether consumed exceeds new allowance
    if (_consumedTodaySeconds >= _totalAllowanceSeconds) {
        _state = TimerState::EXPIRED;
        _sessionStartTime = 0;
    } else if (_state == TimerState::EXPIRED) {
        // Un-expire if we now have remaining time
        _state = TimerState::STOPPED;
    }
    
    Serial.printf("[Timer] Allowance set to %lu seconds, remaining=%lu\n", 
                  (unsigned long)allowanceSeconds,
                  (unsigned long)calculateRemainingSeconds());
}

void ScreenTimer::addAllowance(uint32_t additionalSeconds) {
    _totalAllowanceSeconds += additionalSeconds;
    
    // If timer was expired, it might not be anymore
    if (_state == TimerState::EXPIRED && calculateRemainingSeconds() > 0) {
        _state = TimerState::STOPPED;
    }
    
    Serial.printf("[Timer] Added %lu seconds, new allowance=%lu, remaining=%lu\n", 
                  (unsigned long)additionalSeconds,
                  (unsigned long)_totalAllowanceSeconds,
                  (unsigned long)calculateRemainingSeconds());
}
