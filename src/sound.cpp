/**
 * sound.cpp - Sound/Beep management implementation
 * 
 * Implements audio feedback for the Screen Time Tracker.
 * Uses M5Unified's Speaker API for tone generation.
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#include "sound.h"
#include <M5Unified.h>

// ============================================================================
// State Tracking
// ============================================================================

// Track which warning thresholds have been triggered
// to avoid repeating beeps for the same threshold
static bool triggered10Min = false;
static bool triggered5Min = false;
static bool triggered2Min = false;
static bool triggered1Min = false;

// ============================================================================
// Initialization
// ============================================================================

void soundBegin() {
    // Speaker is already initialized by M5.begin()
    // Set volume (0-255 range)
    M5.Speaker.setVolume(SPEAKER_VOLUME);
    
    Serial.printf("[Sound] Sound system initialized (volume: %d)\n", SPEAKER_VOLUME);
    
    // Reset all threshold triggers
    triggered10Min = false;
    triggered5Min = false;
    triggered2Min = false;
    triggered1Min = false;
}

// ============================================================================
// Beep Functions
// ============================================================================

void playButtonBeep() {
    // Short, crisp beep for button presses (stopwatch style)
    M5.Speaker.tone(BEEP_BUTTON_FREQ_HZ, BEEP_BUTTON_DURATION_MS);
    
    Serial.println("[Sound] Button beep played");
}

void playWarningBeeps(uint8_t count) {
    // Clamp count to reasonable range
    if (count == 0) count = 1;
    if (count > 5) count = 5;
    
    Serial.printf("[Sound] Playing %d warning beep(s)\n", count);
    
    for (uint8_t i = 0; i < count; i++) {
        M5.Speaker.tone(BEEP_WARNING_FREQ_HZ, BEEP_WARNING_DURATION_MS);
        
        // Wait for beep to complete plus gap (except after last beep)
        if (i < count - 1) {
            M5.delay(BEEP_WARNING_DURATION_MS + BEEP_WARNING_GAP_MS);
        }
    }
}

void playExpiryAlarm() {
    Serial.println("[Sound] Playing expiry alarm (5 long beeps)");
    
    // 5 long beeps at lower frequency
    for (uint8_t i = 0; i < 5; i++) {
        M5.Speaker.tone(BEEP_EXPIRY_FREQ_HZ, BEEP_EXPIRY_DURATION_MS);
        
        // Wait for beep to complete plus gap (except after last beep)
        if (i < 4) {
            M5.delay(BEEP_EXPIRY_DURATION_MS + BEEP_EXPIRY_GAP_MS);
        }
    }
}

void playErrorBeep() {
    Serial.println("[Sound] Playing error beep");
    
    // Low-high pattern to indicate blocked action
    M5.Speaker.tone(BEEP_ERROR_FREQ_LOW_HZ, BEEP_ERROR_DURATION_MS);
    M5.delay(BEEP_ERROR_DURATION_MS + 50);
    M5.Speaker.tone(BEEP_ERROR_FREQ_HIGH_HZ, BEEP_ERROR_DURATION_MS);
}

// ============================================================================
// Warning Threshold Checking
// ============================================================================

void checkAndPlayWarningBeeps(uint32_t remainingSeconds, bool isRunning) {
    // Only play warnings when timer is running
    if (!isRunning) {
        return;
    }
    
    // Check each threshold - play beeps only when crossing the threshold
    // We check from highest to lowest to handle cases where time jumps
    
    // 10 minutes: 2 beeps
    if (remainingSeconds <= WARNING_THRESHOLD_10MIN && 
        remainingSeconds > WARNING_THRESHOLD_5MIN && 
        !triggered10Min) {
        
        triggered10Min = true;
        Serial.println("[Sound] 10 minute warning triggered");
        playWarningBeeps(2);
    }
    
    // 5 minutes: 3 beeps
    if (remainingSeconds <= WARNING_THRESHOLD_5MIN && 
        remainingSeconds > WARNING_THRESHOLD_2MIN && 
        !triggered5Min) {
        
        triggered5Min = true;
        // Also mark higher thresholds as triggered (in case we skipped them)
        triggered10Min = true;
        
        Serial.println("[Sound] 5 minute warning triggered");
        playWarningBeeps(3);
    }
    
    // 2 minutes: 4 beeps
    if (remainingSeconds <= WARNING_THRESHOLD_2MIN && 
        remainingSeconds > WARNING_THRESHOLD_1MIN && 
        !triggered2Min) {
        
        triggered2Min = true;
        triggered5Min = true;
        triggered10Min = true;
        
        Serial.println("[Sound] 2 minute warning triggered");
        playWarningBeeps(4);
    }
    
    // 1 minute: 5 beeps
    if (remainingSeconds <= WARNING_THRESHOLD_1MIN && 
        remainingSeconds > 0 && 
        !triggered1Min) {
        
        triggered1Min = true;
        triggered2Min = true;
        triggered5Min = true;
        triggered10Min = true;
        
        Serial.println("[Sound] 1 minute warning triggered");
        playWarningBeeps(5);
    }
}

void resetWarningThresholds(uint32_t remainingSeconds) {
    // Reset all thresholds that are above the current remaining time
    // This prevents re-triggering when resuming from sleep or resetting timer
    
    // Mark thresholds as already triggered if we're already past them
    triggered10Min = (remainingSeconds <= WARNING_THRESHOLD_10MIN);
    triggered5Min = (remainingSeconds <= WARNING_THRESHOLD_5MIN);
    triggered2Min = (remainingSeconds <= WARNING_THRESHOLD_2MIN);
    triggered1Min = (remainingSeconds <= WARNING_THRESHOLD_1MIN);
    
    Serial.printf("[Sound] Warning thresholds reset for %lu seconds remaining\n", 
                  (unsigned long)remainingSeconds);
    Serial.printf("[Sound] Triggered state: 10m=%d, 5m=%d, 2m=%d, 1m=%d\n",
                  triggered10Min, triggered5Min, triggered2Min, triggered1Min);
}
