/**
 * sound.h - Sound/Beep management for Screen Time Tracker
 * 
 * Provides centralized functions for audio feedback:
 * - Button press beeps (short, stopwatch-style)
 * - Warning beeps at time thresholds (different frequency)
 * - Expiry alarm beeps (long, urgent)
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

// ============================================================================
// SOUND CONFIGURATION
// ============================================================================

// Master speaker volume (0-255)
constexpr uint8_t SPEAKER_VOLUME = 200;

// Button beep (short, stopwatch-style)
constexpr uint16_t BEEP_BUTTON_FREQ_HZ = 1800;       // Higher pitch
constexpr uint32_t BEEP_BUTTON_DURATION_MS = 100;     // Short duration

// Warning beep (slightly longer, different frequency)
constexpr uint16_t BEEP_WARNING_FREQ_HZ = 1200;      // Medium pitch
constexpr uint32_t BEEP_WARNING_DURATION_MS = 200;   // Slightly longer
constexpr uint32_t BEEP_WARNING_GAP_MS = 150;        // Gap between beeps

// Expiry beep (long, urgent)
constexpr uint16_t BEEP_EXPIRY_FREQ_HZ = 800;        // Lower pitch (more urgent)
constexpr uint32_t BEEP_EXPIRY_DURATION_MS = 500;    // Long duration
constexpr uint32_t BEEP_EXPIRY_GAP_MS = 300;         // Gap between beeps

// Error beep (distinct low-high pattern for blocked action)
constexpr uint16_t BEEP_ERROR_FREQ_LOW_HZ = 400;     // Low pitch
constexpr uint16_t BEEP_ERROR_FREQ_HIGH_HZ = 600;    // Slightly higher
constexpr uint32_t BEEP_ERROR_DURATION_MS = 200;     // Medium duration

// ============================================================================
// TIME THRESHOLDS (in seconds)
// ============================================================================

constexpr uint32_t WARNING_THRESHOLD_10MIN = 10 * 60;  // 600 seconds
constexpr uint32_t WARNING_THRESHOLD_5MIN  = 5 * 60;   // 300 seconds
constexpr uint32_t WARNING_THRESHOLD_2MIN  = 2 * 60;   // 120 seconds
constexpr uint32_t WARNING_THRESHOLD_1MIN  = 1 * 60;   // 60 seconds

// ============================================================================
// SOUND FUNCTIONS
// ============================================================================

/**
 * Initialize the sound system
 * Call this in setup() after M5.begin()
 */
void soundBegin();

/**
 * Play a short button press beep
 * Used for timer start/stop feedback
 */
void playButtonBeep();

/**
 * Play warning beeps (multiple beeps)
 * @param count Number of beeps to play (1-5)
 */
void playWarningBeeps(uint8_t count);

/**
 * Play expiry alarm (5 long beeps)
 * Called when timer reaches zero
 */
void playExpiryAlarm();

/**
 * Play error beep (blocked action)
 * Used when an action is not allowed (e.g., starting expired timer)
 */
void playErrorBeep();

/**
 * Check remaining time and play appropriate warning beeps
 * Tracks which thresholds have been triggered to avoid repeating
 * @param remainingSeconds Current remaining time in seconds
 * @param isRunning Whether the timer is currently running
 */
void checkAndPlayWarningBeeps(uint32_t remainingSeconds, bool isRunning);

/**
 * Reset warning thresholds (call when timer is reset or restored from sleep)
 * @param remainingSeconds Current remaining time to determine which thresholds are already passed
 */
void resetWarningThresholds(uint32_t remainingSeconds);

#endif // SOUND_H
