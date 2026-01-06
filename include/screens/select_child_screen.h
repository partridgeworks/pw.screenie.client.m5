/**
 * select_child_screen.h - Select Child Screen for Screen Time Tracker
 * 
 * Displays a paged list of family members (children) for selection.
 * Shows one child at a time with avatar and name, with chevron arrows
 * indicating pagination.
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#ifndef SELECT_CHILD_SCREEN_H
#define SELECT_CHILD_SCREEN_H

#include "../screen.h"
#include "../app_state.h"
#include "../api_client.h"  // For FamilyMember struct
#include <M5GFX.h>

// Forward declarations
class ScreenManager;

// Note: FamilyMember struct is defined in api_client.h

/**
 * SelectChildScreen - Family member selection screen
 * 
 * Shows:
 * - Title "Select child"
 * - Large avatar circle with initial
 * - Child name below avatar
 * - Left/right chevron arrows for pagination
 * 
 * Button Mapping:
 * - Button A: Select the displayed child
 * - Button B: Next child (cycle through)
 * - Power click: Previous child
 * - Power hold: Power off
 */
class SelectChildScreen : public Screen {
public:
    /**
     * Constructor
     * @param display Reference to M5GFX display
     */
    explicit SelectChildScreen(M5GFX& display);
    
    /**
     * Set the screen manager (for navigation callbacks)
     * @param manager Pointer to screen manager
     */
    void setScreenManager(ScreenManager* manager);
    
    /**
     * Set the API client for fetching family members
     * @param api Pointer to ApiClient
     */
    void setApiClient(ApiClient* api);
    
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
    
    const char* getTitle() const override { return "Select child"; }
    bool showsHeader() const override { return false; }  // Custom full-screen layout
    bool needsFrequentUpdates() const override { return false; }
    
    // ========================================================================
    // Mock Data (for testing without API)
    // ========================================================================
    
    /**
     * Load mock family members for testing
     */
    void loadMockMembers();
    
    /**
     * Load family members from API
     * @return true if successful
     */
    bool loadMembersFromApi();

private:
    M5GFX& _display;
    ScreenManager* _screenManager;
    ApiClient* _apiClient;
    
    // Family members list
    static constexpr int MAX_MEMBERS = 8;
    FamilyMember _members[MAX_MEMBERS];
    int _memberCount;
    int _currentIndex;
    
    // Loading state
    bool _loading;
    
    // Layout constants
    static constexpr int TITLE_Y = 12;
    static constexpr int AVATAR_CENTER_Y = 65;
    static constexpr int AVATAR_LARGE_RADIUS = 25;
    static constexpr int NAME_Y = 105;  // Below avatar (65 + 25 radius + 15 spacing)
    static constexpr int CHEVRON_Y = 65;
    static constexpr int CHEVRON_LEFT_X = 20;
    static constexpr int CHEVRON_RIGHT_X = 220;
    
    // Drawing methods
    void drawBackground();
    void drawTitle();
    void drawCurrentChild();
    void drawAvatar(char initial, const char* avatarName, int centerX, int centerY, int radius);
    void drawName(const char* name);
    void drawChevrons();
    void drawChevronLeft(int x, int y);
    void drawChevronRight(int x, int y);
    void drawLoadingState();
    
    // Navigation
    void selectNext();
    void selectPrevious();
    void confirmSelection();
};

#endif // SELECT_CHILD_SCREEN_H
