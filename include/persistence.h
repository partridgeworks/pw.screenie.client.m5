/**
 * persistence.h - Data Persistence for Screen Time Tracker
 * 
 * Handles saving/loading of session data and application state
 * using ESP32 NVS (Non-Volatile Storage) via the Preferences library.
 * 
 * NVS is preferred over SD card for this use case because:
 * - M5StickC-Plus 2 doesn't have an SD card slot
 * - NVS is built into ESP32 flash with wear leveling
 * - Ideal for small key-value data (session tokens, settings)
 * - No external hardware required
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include <stdint.h>
#include <Preferences.h>

// Forward declarations
struct UserSession;
struct ScreenTimeData;

// ============================================================================
// NVS Namespace and Keys
// ============================================================================

// NVS namespace (max 15 chars)
constexpr const char* NVS_NAMESPACE = "screentimer";

// NVS Keys (max 15 chars each)
constexpr const char* KEY_IS_LOGGED_IN      = "isLoggedIn";
constexpr const char* KEY_API_KEY           = "apiKey";
constexpr const char* KEY_FAMILY_ID         = "familyId";
constexpr const char* KEY_USERNAME          = "username";
constexpr const char* KEY_CHILD_ID          = "childId";
constexpr const char* KEY_CHILD_NAME        = "childName";
constexpr const char* KEY_CHILD_INITIAL     = "childInitial";
constexpr const char* KEY_CHILD_AVATAR      = "childAvatar";   // Avatar filename with extension (e.g., "1F3B1_color.png")
constexpr const char* KEY_LAST_WEEKDAY      = "lastWeekday";
constexpr const char* KEY_DAILY_ALLOWANCE   = "dailyAllow";
constexpr const char* KEY_LAST_SYNC_TIME    = "lastSync";
constexpr const char* KEY_LAST_NTP_SYNC     = "lastNtpSync";  // Last NTP time sync timestamp
constexpr const char* KEY_DATA_VERSION      = "dataVersion";
constexpr const char* KEY_CONSUMED_TODAY    = "consumedToday"; // Consumed screen time today (seconds)
constexpr const char* KEY_CONSUMED_WEEKDAY  = "consumedDay";   // Weekday when consumed time was saved
constexpr const char* KEY_BRIGHTNESS_LEVEL  = "brightness";    // Brightness level (1-4)
constexpr const char* KEY_UNLIMITED_ALLOW   = "unlimitedAllow"; // true = no time restriction (null from API)

// Data version for migration support
constexpr uint8_t PERSISTENCE_DATA_VERSION = 2;  // Bumped for consumed time model

// ============================================================================
// PersistenceManager Class
// ============================================================================

/**
 * PersistenceManager - Handles saving/loading data to ESP32 NVS
 * 
 * Uses the ESP32 Preferences library for persistent key-value storage.
 * All data is stored in a single namespace "screentimer".
 * 
 * Usage:
 *   PersistenceManager& persistence = PersistenceManager::getInstance();
 *   persistence.begin();
 *   
 *   // Load session on startup
 *   UserSession session;
 *   if (persistence.loadSession(session)) { ... }
 *   
 *   // Save session after login
 *   persistence.saveSession(session);
 */
class PersistenceManager {
public:
    /**
     * Get the singleton instance
     * @return Reference to the PersistenceManager instance
     */
    static PersistenceManager& getInstance();
    
    // Prevent copying
    PersistenceManager(const PersistenceManager&) = delete;
    PersistenceManager& operator=(const PersistenceManager&) = delete;
    
    // ========================================================================
    // Initialization
    // ========================================================================
    
    /**
     * Initialize the persistence manager
     * Must be called before any other methods.
     * @return true if initialization successful
     */
    bool begin();
    
    /**
     * Check if persistence is initialized
     * @return true if begin() was called successfully
     */
    bool isInitialized() const;
    
    // ========================================================================
    // Session Persistence (R8.1)
    // ========================================================================
    
    /**
     * Save user session to NVS
     * Stores API key, username, and selected child info.
     * @param session The session data to save
     * @return true if save successful
     */
    bool saveSession(const UserSession& session);
    
    /**
     * Load user session from NVS
     * Populates session struct with stored data.
     * @param session Output parameter for loaded session
     * @return true if session was loaded (user was logged in), false if no session
     */
    bool loadSession(UserSession& session);
    
    /**
     * Clear stored session (logout)
     * Removes all session-related data from NVS.
     * @return true if clear successful
     */
    bool clearSession();
    
    /**
     * Check if a session exists in storage
     * @return true if user was previously logged in
     */
    bool hasStoredSession();
    
    // ========================================================================
    // Weekday Persistence (R8.2)
    // ========================================================================
    
    /**
     * Save last active weekday for day-change detection
     * @param weekday 0-6 (Sunday-Saturday)
     * @return true if save successful
     */
    bool saveLastActiveWeekday(uint8_t weekday);
    
    /**
     * Load last active weekday
     * @return 0-6 if valid, 0xFF if not set or error
     */
    uint8_t loadLastActiveWeekday();
    
    // ========================================================================
    // Screen Time Cache (R8.3)
    // ========================================================================
    
    /**
     * Save screen time allowance for offline display
     * @param dailyAllowanceSeconds Today's allowance in seconds
     * @return true if save successful
     */
    bool saveDailyAllowance(uint32_t dailyAllowanceSeconds);
    
    /**
     * Load cached daily allowance
     * @return Cached allowance in seconds, or 0 if not set
     */
    uint32_t loadDailyAllowance();
    
    /**
     * Save unlimited allowance flag
     * @param hasUnlimitedAllowance true if no time restriction (null from API)
     * @return true if save successful
     */
    bool saveUnlimitedAllowance(bool hasUnlimitedAllowance);
    
    /**
     * Load unlimited allowance flag
     * @return true if unlimited, false if limited or not set
     */
    bool loadUnlimitedAllowance();
    
    /**
     * Save last sync timestamp
     * @param timestamp Unix timestamp of last successful sync
     * @return true if save successful
     */
    bool saveLastSyncTime(int64_t timestamp);
    
    /**
     * Get last sync timestamp
     * @return Unix timestamp, or 0 if never synced
     */
    int64_t getLastSyncTime();
    
    /**
     * Save last NTP sync timestamp (Phase 6.7)
     * Used to control NTP sync frequency.
     * @param timestamp Unix timestamp of last NTP sync
     * @return true if save successful
     */
    bool saveLastNtpSyncTime(int64_t timestamp);
    
    /**
     * Get last NTP sync timestamp (Phase 6.7)
     * @return Unix timestamp, or 0 if never synced
     */
    int64_t getLastNtpSyncTime();
    
    // ========================================================================
    // Consumed Time Persistence
    // ========================================================================
    
    /**
     * Save consumed screen time for today
     * Also saves the weekday to detect day changes on load.
     * @param consumedSeconds Time consumed today in seconds
     * @param weekday Current weekday (0-6, Sunday-Saturday)
     * @return true if save successful
     */
    bool saveConsumedToday(uint32_t consumedSeconds, uint8_t weekday);
    
    /**
     * Load consumed screen time
     * Returns 0 if the saved weekday doesn't match current weekday (new day).
     * @param currentWeekday Current weekday to compare against
     * @return Consumed seconds if same day, 0 if new day or not set
     */
    uint32_t loadConsumedToday(uint8_t currentWeekday);
    
    /**
     * Clear consumed time (for day reset)
     * @return true if clear successful
     */
    bool clearConsumedToday();
    
    // ========================================================================
    // Brightness Persistence
    // ========================================================================
    
    /**
     * Save brightness level (1-4)
     * @param level Brightness level (1=Dim, 2=Normal, 3=Bright, 4=Very Bright)
     * @return true if save successful
     */
    bool saveBrightnessLevel(uint8_t level);
    
    /**
     * Load brightness level
     * @return Brightness level (1-4), or 0 if not set
     */
    uint8_t loadBrightnessLevel();
    
    // ========================================================================
    // Utility
    // ========================================================================
    
    /**
     * Clear all stored data
     * Use for factory reset or testing.
     * @return true if clear successful
     */
    bool clearAll();
    
    /**
     * Get stored data version
     * Used for data migration on app updates.
     * @return Data version number, or 0 if not set
     */
    uint8_t getDataVersion();
    
    /**
     * Print stored data to Serial (for debugging)
     */
    void debugPrint();

private:
    /**
     * Private constructor for singleton
     */
    PersistenceManager();
    
    /**
     * Destructor - closes NVS
     */
    ~PersistenceManager();
    
    /**
     * Open NVS namespace for reading/writing
     * @param readOnly true for read-only access
     * @return true if opened successfully
     */
    bool openNamespace(bool readOnly);
    
    /**
     * Close NVS namespace
     */
    void closeNamespace();
    
    /**
     * Migrate data from older versions if needed
     */
    void migrateDataIfNeeded();
    
    Preferences _prefs;
    bool _initialized;
    bool _namespaceOpen;
};

#endif // PERSISTENCE_H
