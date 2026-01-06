/**
 * session_manager.cpp - Session Lifecycle Management Implementation
 * 
 * Consolidates session management logic for screen time tracking.
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#include "session_manager.h"
#include "timer.h"
#include "api_client.h"
#include "persistence.h"
#include "app_state.h"
#include "config.h"
#include <Arduino.h>

// ============================================================================
// Constructor
// ============================================================================

SessionManager::SessionManager(ScreenTimer& timer)
    : _timer(timer)
    , _apiClient(nullptr)
{
}

void SessionManager::setApiClient(ApiClient* api) {
    _apiClient = api;
}

// ============================================================================
// Session Control
// ============================================================================

bool SessionManager::startSession() {
    // Check for unlimited allowance - don't allow timer to start
    AppState& appState = AppState::getInstance();
    if (appState.getScreenTime().hasUnlimitedAllowance) {
        Serial.println("[SessionManager] Cannot start - unlimited allowance");
        return false;
    }
    
    // Delegate to timer for actual start logic
    if (!_timer.start()) {
        Serial.println("[SessionManager] Cannot start - timer refused (expired?)");
        return false;
    }
    
    Serial.printf("[SessionManager] Session started, %lu seconds remaining\n",
                  (unsigned long)_timer.calculateRemainingSeconds());
    
    return true;
}

uint32_t SessionManager::stopSession(uint32_t minimumDuration) {
    if (!_timer.isRunning()) {
        Serial.println("[SessionManager] stopSession called but timer not running");
        return 0;
    }
    
    // Capture session info BEFORE stopping (stop clears sessionStartTime)
    time_t sessionStartTime = _timer.getSessionStartTime();
    
    // Stop timer - returns actual duration, commits effective duration internally
    uint32_t actualDuration = _timer.stop(minimumDuration);
    
    // Calculate effective duration for API (with minimum enforcement)
    uint32_t effectiveDuration = actualDuration;
    if (minimumDuration > 0 && actualDuration < minimumDuration) {
        effectiveDuration = minimumDuration;
        Serial.printf("[SessionManager] Session stopped with minimum enforcement (%lu -> %lu sec)\n",
                      (unsigned long)actualDuration, (unsigned long)minimumDuration);
    } else {
        Serial.printf("[SessionManager] Session stopped: %lu sec\n",
                      (unsigned long)actualDuration);
    }
    
    // Persist consumed time to NVS (crash recovery)
    persistToNvs();
    
    // Push completed session to API (fire-and-forget)
    pushSessionToApi(effectiveDuration, sessionStartTime);
    
    return actualDuration;
}

void SessionManager::abortSession() {
    if (!_timer.isRunning()) {
        return;
    }
    
    uint32_t sessionSeconds = _timer.getCurrentSessionSeconds();
    _timer.abortSession();
    
    Serial.printf("[SessionManager] Session aborted: %lu sec NOT counted\n",
                  (unsigned long)sessionSeconds);
}

void SessionManager::onSessionExpired(uint32_t sessionDuration, time_t sessionStartTime) {
    Serial.printf("[SessionManager] Session expired: %lu sec\n",
                  (unsigned long)sessionDuration);
    
    // Persist consumed time to NVS (crash recovery)
    // This was missing before - the timer has already committed the time internally
    persistToNvs();
    
    // Push completed session to API
    pushSessionToApi(sessionDuration, sessionStartTime);
}

// ============================================================================
// Deep Sleep Support
// ============================================================================

SessionSnapshot SessionManager::createSnapshot() const {
    SessionSnapshot snapshot;
    
    // Save consumed time from COMPLETED sessions only
    // Current session will be recalculated on restore using sessionStartTime
    snapshot.consumedTodaySeconds = _timer.getCompletedSessionsSeconds();
    snapshot.sessionStartTime = _timer.getSessionStartTime();
    snapshot.timerState = _timer.getState();
    snapshot.weekday = AppState::getInstance().getCurrentWeekday();
    snapshot.isValid = true;
    
    Serial.printf("[SessionManager] Created snapshot: consumed=%lu, startTime=%ld, state=%d, weekday=%d\n",
                  (unsigned long)snapshot.consumedTodaySeconds,
                  (long)snapshot.sessionStartTime,
                  (int)snapshot.timerState,
                  snapshot.weekday);
    
    return snapshot;
}

bool SessionManager::restoreFromSnapshot(const SessionSnapshot& snapshot, uint8_t currentWeekday) {
    if (!snapshot.isValid) {
        Serial.println("[SessionManager] Cannot restore - invalid snapshot");
        return false;
    }
    
    // Check for day change
    if (snapshot.weekday != 0xFF && snapshot.weekday != currentWeekday) {
        Serial.printf("[SessionManager] Day changed (was=%d, now=%d) - not restoring\n",
                      snapshot.weekday, currentWeekday);
        return false;
    }
    
    // Restore consumed time from completed sessions
    _timer.setConsumedTodaySeconds(snapshot.consumedTodaySeconds);
    
    // If session was running, restore it from timestamp
    if (snapshot.timerState == TimerState::RUNNING && snapshot.sessionStartTime > 0) {
        if (_timer.startFromTimestamp(snapshot.sessionStartTime)) {
            Serial.printf("[SessionManager] Restored running session, remaining=%lu sec\n",
                          (unsigned long)_timer.calculateRemainingSeconds());
        } else {
            Serial.println("[SessionManager] Session expired during sleep");
            return false;  // Timer is now EXPIRED
        }
    } else if (snapshot.timerState == TimerState::EXPIRED) {
        Serial.println("[SessionManager] Restored EXPIRED state");
    } else {
        Serial.printf("[SessionManager] Restored paused state, remaining=%lu sec\n",
                      (unsigned long)_timer.calculateRemainingSeconds());
    }
    
    return true;
}

void SessionManager::persistToNvs() {
    // Only persist COMPLETED sessions - current session is tracked via sessionStartTime
    // in RTC memory, so persisting total would cause double-counting on restore
    uint32_t consumedSeconds = _timer.getCompletedSessionsSeconds();
    uint8_t weekday = AppState::getInstance().getCurrentWeekday();
    
    PersistenceManager::getInstance().saveConsumedToday(consumedSeconds, weekday);
    
    Serial.printf("[SessionManager] Persisted to NVS: %lu sec (weekday %d)\n",
                  (unsigned long)consumedSeconds, weekday);
}

uint32_t SessionManager::loadFromNvs(uint8_t currentWeekday) {
    return PersistenceManager::getInstance().loadConsumedToday(currentWeekday);
}

void SessionManager::clearNvsConsumedTime() {
    PersistenceManager::getInstance().clearConsumedToday();
    Serial.println("[SessionManager] Cleared NVS consumed time");
}

// ============================================================================
// Day Change Support
// ============================================================================

void SessionManager::resetForNewDay(uint32_t newAllowanceSeconds) {
    Serial.println("[SessionManager] Resetting for new day");
    
    // Clear NVS consumed time
    clearNvsConsumedTime();
    
    // Reset timer
    _timer.reset(newAllowanceSeconds);
}

// ============================================================================
// State Queries
// ============================================================================

bool SessionManager::isSessionRunning() const {
    return _timer.isRunning();
}

bool SessionManager::isExpired() const {
    return _timer.isExpired();
}

uint32_t SessionManager::getRemainingSeconds() const {
    return _timer.calculateRemainingSeconds();
}

uint32_t SessionManager::getTotalConsumedSeconds() const {
    return _timer.getTotalConsumedSeconds();
}

uint32_t SessionManager::getCurrentSessionSeconds() const {
    return _timer.getCurrentSessionSeconds();
}

time_t SessionManager::getSessionStartTime() const {
    return _timer.getSessionStartTime();
}

ScreenTimer& SessionManager::getTimer() {
    return _timer;
}

// ============================================================================
// Private Methods
// ============================================================================

void SessionManager::pushSessionToApi(uint32_t durationSeconds, time_t startTime) {
    if (_apiClient == nullptr) {
        Serial.println("[SessionManager] No API client - skipping session push");
        return;
    }
    
    AppState& appState = AppState::getInstance();
    const char* childId = appState.getSession().selectedChildId;
    
    if (childId[0] == '\0') {
        Serial.println("[SessionManager] No child selected - skipping session push");
        return;
    }
    
    if (startTime == 0) {
        Serial.println("[SessionManager] No session start time - skipping session push");
        return;
    }
    
    // Convert seconds to minutes (rounded up)
    uint32_t durationMinutes = (durationSeconds + 59) / 60;
    
    Serial.printf("[SessionManager] Pushing session to API: %lu minutes, started at %ld\n",
                  (unsigned long)durationMinutes, (long)startTime);
    
    // This is a non-critical operation - we log but don't block on failure
    ConsumedTimeResult result = _apiClient->pushConsumedTime(childId, durationMinutes, startTime);
    
    if (result.success) {
        Serial.println("[SessionManager] Session pushed successfully");
    } else {
        Serial.printf("[SessionManager] Failed to push session: %s\n",
                      result.errorMessage);
    }
}
