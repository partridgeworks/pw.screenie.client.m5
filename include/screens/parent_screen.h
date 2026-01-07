/**
 * parent_screen.h - Parent Screen for Screen Time Tracker
 * 
 * Admin/parent access screen with button sequence unlock.
 * Provides access to parent-only functions like resetting screen time.
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#ifndef PARENT_SCREEN_H
#define PARENT_SCREEN_H

#include "../screen.h"
#include "../menu.h"
#include "../ui.h"
#include <M5GFX.h>

// Forward declarations
class ScreenManager;
class ScreenTimer;
class NetworkManager;

/**
 * ParentScreen - Parent/admin access screen
 * 
 * Shows:
 * - Standard header
 * - "Parent screen" title (centered)
 * - "Password to unlock." instruction
 * - "Button C to go back." hint
 * 
 * Unlock sequence: A A A A A B A
 * 
 * Once unlocked:
 * - Instruction changes to "Menu Unlocked"
 * - Parent menu becomes available (Button B to open)
 * 
 * Parent Menu Items:
 * - Reset time: Resets consumed screen time to zero
 * 
 * Button Mapping:
 * - Button A: Part of unlock sequence / Menu activate
 * - Button B: Part of unlock sequence / Open menu / Cycle items
 * - Power: Go back to previous screen
 */
class ParentScreen : public Screen {
public:
    /**
     * Constructor
     * @param display Reference to M5GFX display
     * @param ui Reference to UI for drawing
     * @param timer Reference to ScreenTimer for reset functionality
     */
    ParentScreen(M5GFX& display, UI& ui, ScreenTimer& timer);
    
    /**
     * Set the screen manager (for navigation callbacks)
     * @param manager Pointer to screen manager
     */
    void setScreenManager(ScreenManager* manager);
    
    /**
     * Set the network manager (for NTP sync on logout)
     * @param network Pointer to network manager
     */
    void setNetworkManager(NetworkManager* network);
    
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
    
    const char* getTitle() const override { return "Parent"; }
    bool showsHeader() const override { return true; }
    bool needsFrequentUpdates() const override { return false; }
    bool hasMenu() const override { return _isUnlocked; }
    bool isMenuVisible() const override { return _menu.isVisible(); }

private:
    M5GFX& _display;
    UI& _ui;
    ScreenTimer& _timer;
    ScreenManager* _screenManager;
    NetworkManager* _networkManager;
    DropdownMenu _menu;
    
    // Unlock state
    bool _isUnlocked;
    
    // Sequence tracking
    // Sequence: A A A A A B A (7 buttons)
    static constexpr int SEQUENCE_LENGTH = 7;
    int _sequenceIndex;  // Current position in expected sequence
    
    // Expected sequence: 0=A, 1=B
    static constexpr uint8_t EXPECTED_SEQUENCE[SEQUENCE_LENGTH] = {0, 0, 0, 0, 0, 1, 0};
    
    // Menu setup and teardown
    void setupMenu();
    void destroyMenu();
    
    // Drawing helpers
    void drawBackground();
    void drawHeader();
    void drawTitle();
    void drawInstruction();
    void drawHint();
    void drawMenu();
    
    // Sequence handling
    void processSequenceButton(uint8_t button);  // 0=A, 1=B
    void onUnlocked();
    
    // Menu action callbacks (static)
    static void onResetTimeSelected(int itemIndex, void* userData);
    static void onChangeChildSelected(int itemIndex, void* userData);
    static void onLogoutSelected(int itemIndex, void* userData);
    
    // Instance methods for menu actions
    void handleResetTime();
    void navigateToChangeChild();
    void handleLogout();
    
    // Helper to go back to previous screen
    void exitScreen();
};

#endif // PARENT_SCREEN_H
