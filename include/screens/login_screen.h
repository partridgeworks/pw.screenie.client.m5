/**
 * login_screen.h - Login Screen for Screen Time Tracker
 * 
 * Displays a QR code and numeric pairing code for device-code authentication.
 * User scans QR code or enters numeric code on another device to authenticate.
 * 
 * Uses ApiClient for initiating login and PollingManager for checking completion.
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#ifndef LOGIN_SCREEN_H
#define LOGIN_SCREEN_H

#include "../screen.h"
#include "../menu.h"
#include "../polling_manager.h"
#include <M5GFX.h>

// Forward declarations
class ScreenManager;
class ApiClient;
class PollingManager;

/**
 * LoginState - Current state of the login process
 */
enum class LoginState {
    INITIALIZING,    // Making initial API call to get codes
    DISPLAYING_CODE, // Showing QR + numeric code, polling for completion
    SUCCESS,         // Login successful, transitioning out
    ERROR            // API error, showing retry option
};

/**
 * LoginScreen - Device-code authentication screen
 * 
 * Shows:
 * - QR code linking to pairing URL
 * - Numeric pairing code
 * - Polling indicator (animated dots)
 * - Error state with retry option
 * 
 * Button Mapping:
 * - Button A: Retry (when in ERROR state)
 * - Button B: (unused)
 * - Power: Back / Cancel
 */
class LoginScreen : public Screen {
public:
    /**
     * Constructor
     * @param display Reference to M5GFX display
     */
    explicit LoginScreen(M5GFX& display);
    
    /**
     * Set the screen manager (for navigation callbacks)
     * @param manager Pointer to screen manager
     */
    void setScreenManager(ScreenManager* manager);
    
    /**
     * Set the API client and polling manager
     * Must be called before using the screen for real API operations.
     * @param api Pointer to ApiClient
     * @param polling Pointer to PollingManager
     */
    void setApiClient(ApiClient* api, PollingManager* polling);
    
    // ========================================================================
    // Screen Lifecycle
    // ========================================================================
    
    void onEnter() override;
    void onExit() override;
    void onResume() override;
    void update() override;
    void draw() override;
    
    // ========================================================================
    // Input Handling
    // ========================================================================
    
    void onButtonA() override;
    void onButtonB() override;
    void onButtonPower() override;
    void onButtonPowerHold() override;
    
    // ========================================================================
    // Screen Metadata
    // ========================================================================
    
    const char* getTitle() const override { return "Login"; }
    bool showsHeader() const override { return false; }  // Full-screen for QR
    bool needsFrequentUpdates() const override { return true; }  // For animation
    bool hasMenu() const override { return true; }
    bool isMenuVisible() const override;
    
    // ========================================================================
    // Login State Management
    // ========================================================================
    
    /**
     * Set the pairing code (received from API)
     * This triggers QR code generation and display
     * @param pairingCode The code to embed in the QR URL
     */
    void setPairingCode(const char* pairingCode);
    
    /**
     * Set error state with message
     * @param errorMessage Error message to display
     */
    void setError(const char* errorMessage);
    
    /**
     * Mark login as successful
     * Triggers transition to next screen
     */
    void setLoginSuccess();
    
    /**
     * Initiate login using ApiClient
     * Gets device code and starts polling for completion.
     */
    void initiateLogin();
    
    /**
     * Simulate a mock login (for testing without API)
     * Sets a test pairing code and starts displaying
     */
    void simulateMockLogin();

private:
    M5GFX& _display;
    ScreenManager* _screenManager;
    ApiClient* _apiClient;
    PollingManager* _pollingManager;
    
    // Owned menu (just Sleep option)
    DropdownMenu _menu;
    
    // Login state
    LoginState _state;
    char _pairingCode[16];        // Numeric code (e.g., "482915")
    char _deviceCode[32];         // Device code for polling
    char _errorMessage[64];
    
    // QR code data
    static constexpr int QR_MAX_VERSION = 4;  // Version 4 = 33x33 modules, fits URLs up to ~78 chars
    static constexpr int QR_MODULE_COUNT = 33;  // For version 4
    uint8_t _qrData[QR_MODULE_COUNT * QR_MODULE_COUNT / 8 + QR_MODULE_COUNT];  // Bit array for QR
    int _qrSize;  // Actual size after generation
    bool _qrGenerated;
    
    // Animation
    uint32_t _lastAnimationMs;
    uint8_t _animationFrame;
    static constexpr uint32_t ANIMATION_INTERVAL_MS = 400;
    
    // Drawing methods
    void drawBackground();
    void drawTitle();
    void drawQRCode();
    void drawPairingCode();
    void drawPollingIndicator();
    void drawErrorState();
    void drawInstructions();
    
    // QR code generation
    bool generateQRCode(const char* url);
    bool getQRModule(int x, int y) const;
    
    // Polling callback (static to work with PollingManager)
    static void onLoginPollResult(const PollingResult& result, void* userData);
    void handleLoginResult(const PollingResult& result);
    
    // State transitions
    void transitionToSelectChild();
    
    // Menu setup
    void setupMenu();
    void destroyMenu();
    void drawMenu();
    
    // Menu callbacks
    static void onMenuSleepNow(int itemIndex, void* userData);
    static void onMenuSettings(int itemIndex, void* userData);
    static void onMenuParent(int itemIndex, void* userData);
    void doSleepNow();
    void doSettings();
    void doParent();
};

#endif // LOGIN_SCREEN_H
