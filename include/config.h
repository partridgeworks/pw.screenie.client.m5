/**
 * config.h - Configuration constants for Screen Time Tracker
 * 
 * Contains all configurable values, color definitions, layout constants,
 * and default settings for the application.
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <cstring>  // For strcmp/strlen in isWifiConfigured()

// ============================================================================
// DISPLAY CONFIGURATION
// ============================================================================

#define RGB565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

// M5StickC Plus2 display dimensions (landscape mode)
constexpr int SCREEN_WIDTH = 240;
constexpr int SCREEN_HEIGHT = 135;
constexpr int DISPLAY_ROTATION = 1;  // Landscape mode
constexpr int DISPLAY_BRIGHTNESS = 60;  // 0-255

// ============================================================================
// COLOR PALETTE - Professional dark theme
// ============================================================================

// Primary colors (RGB565 format)
constexpr uint16_t COLOR_BACKGROUND     = 0x1082;  // Dark blue-gray (#10202A)
constexpr uint16_t COLOR_HEADER_BG      = 0xC86E;  // Plum header (RGB 205, 11, 111)
constexpr uint16_t COLOR_BORDER         = 0x4A69;  // Medium gray border
constexpr uint16_t COLOR_TEXT_PRIMARY   = 0xFFFF;  // White
constexpr uint16_t COLOR_TEXT_SECONDARY = RGB565(215, 215, 215);  // Light gray
constexpr uint16_t COLOR_TEXT_MUTED     = RGB565(180, 180, 180);  // Muted gray

// Accent colors
constexpr uint16_t COLOR_ACCENT_PRIMARY = RGB565(205, 11, 111);  // Pink
constexpr uint16_t COLOR_ACCENT_SUCCESS = 0x07E0;  // Green for running
constexpr uint16_t COLOR_ACCENT_WARNING = 0xFD20;  // Orange for low time
constexpr uint16_t COLOR_ACCENT_DANGER  = 0xF800;  // Red for expired

// Progress bar colors
constexpr uint16_t COLOR_PROGRESS_BG    = RGB565(70, 70, 70);  // Dark background for progress
constexpr uint16_t COLOR_PROGRESS_FILL  = RGB565(180, 180, 180);  // Teal fill (#009688)

// Avatar colors
constexpr uint16_t COLOR_AVATAR_PRIMARY = 0xF2D8;  // Pink avatar background (#f05bc1 / RGB 240, 91, 193)
constexpr uint16_t COLOR_AVATAR_BG      = 0x4A49;  // Avatar circle background (fallback)
constexpr uint16_t COLOR_AVATAR_BORDER  = 0x5D1F;  // Avatar border

// Menu colors
constexpr uint16_t COLOR_MENU_BG        = 0x2945;  // Menu background
constexpr uint16_t COLOR_MENU_SELECTED  = 0x5D1F;  // Selected item highlight
constexpr uint16_t COLOR_MENU_FLASH     = 0x07E0;  // Flash on activation

// ============================================================================
// LAYOUT CONSTANTS
// ============================================================================

// Grid unit for consistent spacing (6px)
constexpr int GRID_UNIT = 6;

// Standard padding used throughout the UI
constexpr int UI_PADDING = GRID_UNIT;

// Header bar
constexpr int HEADER_HEIGHT = 24;
constexpr int HEADER_Y = 0;

// Date display
constexpr int DATE_HEIGHT = 25;

// Avatar configuration
constexpr int AVATAR_RADIUS = 25;  // 50px diameter
constexpr int AVATAR_X = SCREEN_WIDTH - UI_PADDING - AVATAR_RADIUS - 8;
// Avatar Y is calculated dynamically in ui.cpp to center between header and bottom of screen
constexpr int AVATAR_STATUS_RING_OFFSET = 0;  // No gap - ring directly around avatar

// Timer display area - centered vertically between header and progress bar
constexpr int TIMER_Y = HEADER_HEIGHT + 8;
constexpr int TIMER_X = UI_PADDING;

// Progress bar - positioned 4px below timer display, aligned with timer/date
constexpr int PROGRESS_BAR_HEIGHT = 4;
constexpr int PROGRESS_BAR_WIDTH = 150;
constexpr int PROGRESS_BAR_X = UI_PADDING;  // Left-aligned with timer and date
// Note: PROGRESS_BAR_Y is now calculated dynamically in ui.cpp based on timer position

// Status indicator (ring around avatar) - 2px stroke directly around avatar
constexpr int STATUS_RING_THICKNESS = 2;
constexpr int STATUS_RING_RADIUS = AVATAR_RADIUS;  // Ring starts at avatar edge

// ============================================================================
// MENU CONFIGURATION
// ============================================================================

constexpr int MENU_MAX_ITEMS = 10;
constexpr int MENU_ITEM_HEIGHT = 24;           // Height of each menu item row
constexpr int MENU_PADDING = 12;               // Padding around menu content area
constexpr int MENU_CHEVRON_AREA_HEIGHT = 16;   // Extra space below for chevron
constexpr int MENU_VISIBLE_ITEMS = 3;          // Show prev, selected, next
constexpr int MENU_Y = HEADER_HEIGHT;          // Start just below header
constexpr int MENU_HEIGHT = SCREEN_HEIGHT - MENU_Y;  // Fill rest of screen

// Menu colors for new design
constexpr uint16_t COLOR_MENU_ITEM_GRAY = 0x7BEF;  // Gray for non-selected items

// ============================================================================
// TIMER CONFIGURATION
// ============================================================================

// Timer update interval (ms)
constexpr uint32_t TIMER_UPDATE_INTERVAL_MS = 1000;

// Minimum session duration feature (R10)
// When enabled, stopping a session shorter than the minimum still counts
// as the minimum duration for consumed time calculation.
constexpr bool MINIMUM_SESSION_ENABLED = true;
constexpr uint32_t MINIMUM_SESSION_DURATION_SECONDS = 5 * 60;  // 5 minutes

// Mistaken session threshold (seconds)
// If a session is stopped within this time, it's considered a mistake
// and the session is forgotten (no time deducted from allowance)
constexpr uint32_t MISTAKEN_SESSION_DURATION_SECS = 5;

// ============================================================================
// API CONFIGURATION
// ============================================================================

// Base URL for the pairing page (QR code links here with pairing code appended)
constexpr const char* API_PAIRING_BASE_URL = "https://screenie.org/home/pair/";

// Base URL for API endpoints
constexpr const char* API_BASE_URL = "https://screenie.org/api/";

// ============================================================================
// POLLING CONFIGURATION
// ============================================================================

// Login polling (R1.3)
constexpr uint32_t LOGIN_POLL_INTERVAL_MS = 5000;     // Poll every 5 seconds
constexpr uint32_t LOGIN_POLL_TIMEOUT_MS = 300000;    // 5 minute timeout

// More-time request polling (R4.4, R4.5)
constexpr uint32_t MORE_TIME_POLL_INTERVAL_MS = 10000;  // Poll every 10 seconds
constexpr uint32_t MORE_TIME_POLL_TIMEOUT_MS = 300000;  // 5 minute timeout

// Screen time usage sync interval (R6.5) - how often to record usage to API
constexpr uint32_t USAGE_SYNC_INTERVAL_MS = 180000;   // Every 3 minutes

// ============================================================================
// NETWORK CONFIGURATION
// ============================================================================

// WiFi credentials - loaded from credentials.h (gitignored)
// If credentials.h doesn't exist, we check if __has_include is available
#if defined(__has_include)
  #if __has_include("credentials.h")
    #include "credentials.h"
    #define WIFI_CONFIGURED 1
  #endif
#endif

// Fallback if credentials.h doesn't exist or couldn't be detected
#ifndef WIFI_SSID
  #define WIFI_SSID     "YOUR_WIFI_SSID"
  #define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
  #define WIFI_CONFIGURED 0
#endif

#ifndef WIFI_CONFIGURED
  #define WIFI_CONFIGURED 1
#endif

// Helper to check if WiFi is properly configured at runtime
inline bool isWifiConfigured() {
    return WIFI_CONFIGURED && 
           strcmp(WIFI_SSID, "YOUR_WIFI_SSID") != 0 &&
           strlen(WIFI_SSID) > 0;
}

// NTP configuration
#define NTP_TIMEZONE  "GMT0"              // UTC timezone
#define NTP_SERVER1   "0.pool.ntp.org"
#define NTP_SERVER2   "1.pool.ntp.org"
#define NTP_SERVER3   "2.pool.ntp.org"

// Connection timeouts
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 10000;  // 10 seconds
constexpr uint32_t NTP_SYNC_TIMEOUT_MS = 8000;       // 8 seconds

// WiFi Lifecycle Management (R5)
constexpr uint32_t WIFI_KEEPALIVE_MS = 120000;     // Keep WiFi alive for 120 seconds (2 mins) after last operation (R5.2)

// NTP Sync Frequency Control (Phase 6.7)
// Only sync with NTP if this many hours have passed since last sync
constexpr uint8_t NTP_SYNC_INTERVAL_HOURS = 72;



// ============================================================================
// USER CONFIGURATION
// ============================================================================

// Maximum name length
constexpr int MAX_NAME_LENGTH = 20;

// Default user
constexpr const char* DEFAULT_USER_NAME = "Child";
constexpr char DEFAULT_USER_INITIAL = 'S';

// ============================================================================
// APP INFO
// ============================================================================

constexpr const char* APP_NAME = "Screenie";
constexpr const char* APP_VERSION = "1.0.2";

// ============================================================================
// ANIMATION TIMING
// ============================================================================

// Menu activation flash duration (ms)
constexpr uint32_t MENU_FLASH_DURATION_MS = 150;

// Screen refresh rate limiting (ms)
constexpr uint32_t MIN_REFRESH_INTERVAL_MS = 100;

// ============================================================================
// SLEEP CONFIGURATION
// ============================================================================

// Auto-sleep after inactivity (seconds)
constexpr uint32_t AUTO_SLEEP_DURATION_SECS = 90; // 1.5mins

// Minimum remaining seconds before expiration to allow sleep
// (Don't sleep if less than 10 minutes remaining and timer is active)
constexpr uint32_t SLEEP_MIN_REMAINING_SECS = 10 * 60;  // 10 minutes

// Wake-up buffer before expiration (seconds)
// Wake up this many seconds before timer expires
constexpr uint32_t SLEEP_WAKE_BEFORE_EXPIRY_SECS = 10 * 60;  // 10 minutes

// Button A GPIO for wake from deep sleep (M5StickC Plus2)
constexpr int BUTTON_A_GPIO_NUM = 37;

// Power hold GPIO (keep power mosfet on during deep sleep)
constexpr int POWER_HOLD_GPIO_NUM = 4;

#endif // CONFIG_H
