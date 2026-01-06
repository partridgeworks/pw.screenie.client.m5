/**
 * ui.cpp - User Interface implementation for Screen Time Tracker
 *
 * Implements all display rendering using M5GFX.
 *
 * @author Screen Time Tracker
 * @version 1.0
 */

#include "ui.h"
#include "timer.h"
#include "menu.h"
#include "app_state.h"
#include <M5Unified.h>
#include <LittleFS.h>
#include <time.h>

// Calculate avatar Y position to center the avatar+ring unit vertically
// between header and bottom of screen.
// Total unit height = avatar diameter + ring thickness on both sides:
static constexpr int AVATAR_UNIT_HEIGHT = (AVATAR_RADIUS + STATUS_RING_THICKNESS) * 2;

static constexpr int AVAILABLE_HEIGHT = SCREEN_HEIGHT - HEADER_Y - HEADER_HEIGHT;
// Avatar center Y = header bottom + offset to center the unit
static constexpr int AVATAR_Y = HEADER_Y + HEADER_HEIGHT + (AVAILABLE_HEIGHT / 2) - (UI_PADDING * 2);

// ============================================================================
// Constructor and Initialization
// ============================================================================

UI::UI(M5GFX &display)
    : _display(display), _canvas(&display), _needsFullRedraw(true), _lastUpdateMs(0), _infoDialogVisible(false), _currentNetworkStatus(NetworkStatus::DISCONNECTED), _lastDrawnSeconds(UINT32_MAX), _lastDrawnProgress(-1.0f), _lastDrawnRunning(false)
{
}

void UI::begin()
{
    // Configure display for landscape mode
    _display.setRotation(DISPLAY_ROTATION);
    _display.setBrightness(DISPLAY_BRIGHTNESS);
    _display.fillScreen(COLOR_BACKGROUND);

    _needsFullRedraw = true;
}

// ============================================================================
// Main Drawing Methods
// ============================================================================

void UI::drawMainScreen(const ScreenTimer &timer,
                        const char *userName,
                        char userInitial,
                        const char *avatarName,
                        bool isTimerRunning,
                        NetworkStatus networkStatus)
{
    // Update cached network status
    _currentNetworkStatus = networkStatus;

    // Wait for any pending display operations to complete
    _display.waitDisplay();

    // Batched Write #1 - Avatar
    _display.startWrite();
    // Clear background
    _display.fillScreen(COLOR_BACKGROUND);
    drawAvatar(userInitial, userName, avatarName, AVATAR_X, AVATAR_Y);
    _display.endWrite();
    _display.display();

    // Batched Write #2 - Everything else
    _display.startWrite();
    drawHeader(APP_NAME);
    drawNetworkStatusInHeader();
    drawDateOnMainScreen();
    drawChildName(userName, AVATAR_X, AVATAR_Y);

    // Draw dynamic elements
    TimerState timerState = timer.getState();
    drawTimerDisplay(timer.calculateRemainingSeconds(), timerState);
    drawProgressBar(timer);
    drawStatusRing(timer);

    // End batched write and flush to display
    _display.endWrite();
    _display.display();

    // Cache the drawn state for change detection
    _lastDrawnSeconds = timer.calculateRemainingSeconds();
    _lastDrawnProgress = timer.getProgress();
    _lastDrawnRunning = isTimerRunning;

    _needsFullRedraw = false;
    _lastUpdateMs = millis();
}

void UI::updateDynamicElements(const ScreenTimer &timer, bool isTimerRunning)
{
    // Rate limit updates
    uint32_t now = millis();
    if (now - _lastUpdateMs < MIN_REFRESH_INTERVAL_MS)
    {
        return;
    }

    if (_needsFullRedraw)
    {
        // Can't do partial update, need full redraw
        return;
    }

    // Get current values
    uint32_t currentSeconds = timer.calculateRemainingSeconds();
    float currentProgress = timer.getProgress();
    TimerState timerState = timer.getState();

    // Check if anything actually changed (prevents flicker when paused)
    bool secondsChanged = (currentSeconds != _lastDrawnSeconds);
    bool progressChanged = (currentProgress != _lastDrawnProgress);
    bool runningChanged = (isTimerRunning != _lastDrawnRunning);

    // Check if we need to update activation period visuals (every second during activation)
    bool needActivationUpdate = false;
    if (timer.isRunning() && timer.getSessionStartTime() > 0)
    {
        time_t now_time = time(nullptr);
        time_t sessionStart = timer.getSessionStartTime();
        uint32_t secondsSinceActivation = (uint32_t)(now_time - sessionStart);
        if (secondsSinceActivation < MINIMUM_SESSION_DURATION_SECONDS)
        {
            needActivationUpdate = true;
        }
    }

    // Skip update if nothing changed and not in activation period
    if (!secondsChanged && !progressChanged && !runningChanged && !needActivationUpdate)
    {
        return;
    }

    // Wait for any pending display operations
    _display.waitDisplay();

    // Start batched write for better performance
    _display.startWrite();

    // Only update timer display if seconds changed
    if (secondsChanged)
    {
        // Clear timer area with fixed position and dimensions
        // Timer baseline is at Y=61, clearance rectangle starts 42px above
        int timerClearX = UI_PADDING * 2;
        int timerClearY = 63;
        int timerClearWidth = 152;
        int timerClearHeight = 42;
        _display.fillRect(timerClearX, timerClearY, timerClearWidth, timerClearHeight, COLOR_BACKGROUND);
        drawTimerDisplay(currentSeconds, timerState);

        // Clear activation timer circle
        clearActivationRing(timer);
        _lastDrawnSeconds = currentSeconds;
    }

    // Only update progress bar if progress changed or in activation period
    if (progressChanged || needActivationUpdate)
    {
        drawProgressBar(timer);
        _lastDrawnProgress = currentProgress;
    }

    // Only update status ring if running state changed or in activation period
    if (runningChanged || needActivationUpdate)
    {
        drawStatusRing(timer);
        _lastDrawnRunning = isTimerRunning;
    }

    // End batched write and flush to display
    _display.endWrite();
    _display.display();

    _lastUpdateMs = now;
}

void UI::forceFullRedraw()
{
    _needsFullRedraw = true;
    // Reset cached state so next draw will update everything
    _lastDrawnSeconds = UINT32_MAX;
    _lastDrawnProgress = -1.0f;
    _lastDrawnRunning = false;
}

bool UI::needsFullRedraw() const
{
    return _needsFullRedraw;
}

// ============================================================================
// Static Element Drawing
// ============================================================================

void UI::drawHeader(const char *appName)
{
    // Draw header background (full width, no border)
    int headerX = 0;
    int headerWidth = SCREEN_WIDTH;

    _display.fillRect(headerX, HEADER_Y, headerWidth, HEADER_HEIGHT, COLOR_HEADER_BG);

    // Layout constants
    constexpr int ITEM_GAP = 6;
    constexpr int LOGO_WIDTH = 22;
    constexpr int LOGO_HEIGHT = 14;

    // Left-aligned items start position
    int currentX = ITEM_GAP;

    // 1. Draw logo placeholder (22x14 rounded white rectangle)
    int logoY = HEADER_Y + (HEADER_HEIGHT - LOGO_HEIGHT) / 2;
    _display.fillRoundRect(currentX, logoY, LOGO_WIDTH, LOGO_HEIGHT, 2, COLOR_TEXT_PRIMARY);

    // Draw closed eyes (dark gray curved arcs inside logo)
    constexpr uint16_t COLOR_LOGO_EYES = 0x4208; // Dark gray for eyes
    int eyeY = logoY + 3;                        // Near-top of logo
    int leftEyeX = currentX + 6;                 // Left eye center
    int rightEyeX = currentX + 16;               // Right eye center
    int eyeRadius = 3;

    // Draw curved arcs for closed eyes (small arcs curving upward like smiles)
    // Left eye - draw an arc by using fillArc or multiple small lines
    for (int i = -2; i <= 2; i++)
    {
        int yOffset = 2 - (i * i) / 2; // Inverted parabolic curve (smile shape)
        _display.drawPixel(leftEyeX + i, eyeY + yOffset, COLOR_LOGO_EYES);
        _display.drawPixel(leftEyeX + i, eyeY + yOffset + 1, COLOR_LOGO_EYES);
        _display.drawPixel(rightEyeX + i, eyeY + yOffset, COLOR_LOGO_EYES);
        _display.drawPixel(rightEyeX + i, eyeY + yOffset + 1, COLOR_LOGO_EYES);
    }

    // Mouth
    _display.drawLine(leftEyeX + 2, eyeY + 6, rightEyeX - 2, eyeY + 6, COLOR_LOGO_EYES); // Mouth line
    _display.drawLine(leftEyeX + 2, eyeY + 7, rightEyeX - 2, eyeY + 7, COLOR_LOGO_EYES); // Mouth line

    currentX += LOGO_WIDTH + ITEM_GAP;

    // 2. Draw app name with FreeSansBold9pt7b
    _display.setTextColor(COLOR_TEXT_PRIMARY);
    _display.setTextSize(1);
    _display.setFont(&fonts::FreeSans9pt7b);

    // Position text 4px below top of header
    int textY = HEADER_Y + 4; // 4px from top + baseline offset
    _display.setCursor(currentX, textY);
    _display.print(appName);
}

void UI::drawDateInHeader()
{
    // Date is no longer displayed in header - moved to main screen content area
    // This function is kept for compatibility but does nothing
}

void UI::drawDateOnMainScreen()
{
    // Get current time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Format: "Monday 9/Dec"
    const char *fullDays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    char monthStr[4];
    getMonthStr(timeinfo.tm_mon, monthStr);

    char dateBuffer[24];
    snprintf(dateBuffer, sizeof(dateBuffer), "%s %d %s",
             fullDays[timeinfo.tm_wday], timeinfo.tm_mday, monthStr);

    _display.setTextColor(COLOR_TEXT_SECONDARY);
    _display.setTextSize(1);
    _display.setFont(&fonts::FreeSans9pt7b);

    // Fixed position for date
    int dateX = UI_PADDING * 2;
    int dateY = 39;

    _display.setCursor(dateX, dateY);
    _display.print(dateBuffer);
}

void UI::drawAvatar(char initial, const char *userName, const char *avatarName, int x, int y)
{
    // Try to load and draw PNG avatar if avatarName is provided
    bool pngDrawn = false;

    if (avatarName != nullptr && avatarName[0] != '\0')
    {
        // Construct full path: /avatars/[avatarName]
        // Handle both cases: avatarName with or without .png extension
        char avatarPath[64];

        // Check if avatarName already ends with .png
        size_t len = strlen(avatarName);
        bool hasPngExtension = (len > 4 &&
                                strcmp(&avatarName[len - 4], ".png") == 0);

        if (hasPngExtension)
        {
            // Already has extension, use as-is
            snprintf(avatarPath, sizeof(avatarPath), "/avatars/%s", avatarName);
        }
        else
        {
            // No extension, append .png
            snprintf(avatarPath, sizeof(avatarPath), "/avatars/%s.png", avatarName);
        }

        // Try to load PNG from LittleFS
        if (LittleFS.exists(avatarPath))
        {
            // Draw background circle first (in case PNG has transparency)
            _display.fillCircle(x, y, AVATAR_RADIUS, COLOR_AVATAR_PRIMARY);

            // Load and draw PNG centered in the circle
            // PNG is 50x50, avatar circle is 32px diameter (16px radius)
            File avatarFile = LittleFS.open(avatarPath, "r");
            if (avatarFile)
            {
                // Calculate top-left position to center the image in the circle
                int imgX = x - AVATAR_RADIUS;
                int imgY = y - AVATAR_RADIUS;

                // Draw PNG at position - don't pass size params as they set clipping, not scaling
                _display.drawPng(&avatarFile, imgX, imgY);

                avatarFile.close();
                pngDrawn = true;

                // Clear any clipping that may have been set by drawPng
                _display.clearClipRect();

                Serial.printf("[UI] Drew PNG avatar: %s at (%d,%d)\n", avatarPath, x, y);
            }
            else
            {
                Serial.printf("[UI] Failed to open avatar file: %s\n", avatarPath);
            }
        }
        else
        {
            Serial.printf("[UI] Avatar PNG not found: %s\n", avatarPath);
        }
    }

    // Fallback: Draw initial letter if PNG not available
    if (!pngDrawn)
    {
        // Draw circular avatar with primary color
        _display.fillCircle(x, y, AVATAR_RADIUS, COLOR_AVATAR_PRIMARY);

        // Draw initial centered in circle using FreeSansBold9pt7b
        _display.setTextColor(COLOR_TEXT_PRIMARY); // White
        _display.setFont(&fonts::FreeSansBold9pt7b);
        _display.setTextSize(1);

        // Calculate text position for centering
        char initialStr[2] = {initial, '\0'};
        int textWidth = _display.textWidth(initialStr);
        int textX = x - (textWidth / 2);
        int textY = y - 6; // Center baseline vertically

        _display.setCursor(textX, textY);
        _display.print(initialStr);
    }
}

void UI::drawChildName(const char *userName, int x, int y)
{
    // Draw the child's name below the avatar if provided
    if (userName != nullptr && userName[0] != '\0')
    {
        _display.setTextColor(COLOR_TEXT_SECONDARY);
        _display.setFont(&fonts::Font0); // Very small font
        _display.setTextSize(1);

        // Calculate position: below avatar circle + ring, centered horizontally
        int nameWidth = _display.textWidth(userName);
        int nameX = x - (nameWidth / 2);
        int nameY = y + AVATAR_RADIUS + STATUS_RING_THICKNESS + 4; // Below avatar + ring + small gap

        _display.setCursor(nameX, nameY);
        _display.print(userName);
    }
}

// ============================================================================
// Dynamic Element Drawing
// ============================================================================

void UI::drawTimerDisplay(uint32_t remainingSeconds, TimerState state)
{
    // Check if unlimited allowance from AppState
    AppState &appState = AppState::getInstance();
    bool hasUnlimitedAllowance = appState.getScreenTime().hasUnlimitedAllowance;

    // Choose color based on state
    uint16_t timerColor = COLOR_TEXT_PRIMARY;
    if (state == TimerState::EXPIRED)
    {
        timerColor = COLOR_ACCENT_DANGER;
    }
    else if (remainingSeconds < 300)
    { // Less than 5 minutes
        timerColor = COLOR_ACCENT_WARNING;
    }

    // Fixed position for timer
    int timerX = UI_PADDING * 2;
    int timerY = 61;

    if (hasUnlimitedAllowance)
    {
        // Display "NO LIMIT" with smaller bold font, wrapped onto two lines
        _display.setTextColor(COLOR_ACCENT_SUCCESS, COLOR_BACKGROUND);
        _display.setFont(&fonts::FreeSansBold12pt7b);
        _display.setTextSize(1);

        // Draw "NO" on first line
        _display.setCursor(timerX, timerY - 10 + 5);
        _display.print("NO");

        // Draw "LIMIT" on second line
        _display.setCursor(timerX, timerY + 10 + 5);
        _display.print("LIMIT");
    }
    else if (state == TimerState::EXPIRED)
    {
        // Display "TIME UP" with smaller bold font, wrapped onto two lines
        _display.setTextColor(timerColor, COLOR_BACKGROUND);
        _display.setFont(&fonts::FreeSansBold12pt7b);
        _display.setTextSize(1);

        // Draw "TIME" on first line
        _display.setCursor(timerX, timerY - 10 + 5);
        _display.print("TIME");

        // Draw "UP" on second line
        _display.setCursor(timerX, timerY + 10 + 5);
        _display.print("UP");
    }
    else
    {
        // Display normal countdown timer
        _display.setTextColor(timerColor, COLOR_BACKGROUND);
        _display.setFont(&fonts::FreeSansBold24pt7b);
        _display.setTextSize(1);
        _display.setCursor(timerX, timerY);

        // Format time as HH:MM:SS or MM:SS depending on duration
        char timeBuffer[12];
        formatTime(remainingSeconds, timeBuffer, sizeof(timeBuffer));
        _display.print(timeBuffer);
    }
}

void UI::drawProgressBar(const ScreenTimer &timer)
{
    // Get values from timer
    float progress = timer.getProgress();
    uint32_t totalAllowanceSeconds = timer.getTotalAllowance();

    // Check if unlimited allowance from AppState
    AppState &appState = AppState::getInstance();
    bool hasUnlimitedAllowance = appState.getScreenTime().hasUnlimitedAllowance;

    // Clamp progress to 0.0 - 1.0
    if (progress < 0.0f)
        progress = 0.0f;
    if (progress > 1.0f)
        progress = 1.0f;

    constexpr int PROGRESS_BAR_RADIUS = 1; // Minimal corner rounding

    // Fixed position for progress bar
    int progressBarX = UI_PADDING * 2;
    int progressBarY = 108;

    if (hasUnlimitedAllowance)
    {
        // For unlimited allowance, draw empty progress bar
        _display.fillRoundRect(progressBarX, progressBarY,
                               PROGRESS_BAR_WIDTH, PROGRESS_BAR_HEIGHT,
                               PROGRESS_BAR_RADIUS,
                               COLOR_PROGRESS_BG);

        // Draw allowance label showing "Unlimited"
        _display.setFont(&fonts::Font0); // Small font for label
        _display.setTextSize(1);
        _display.setTextColor(COLOR_TEXT_SECONDARY);

        const char *unlimitedText = "Today's allowance: Unlimited"; // Status label
        int textWidth = _display.textWidth(unlimitedText);
        int textX = SCREEN_WIDTH - textWidth - UI_PADDING;
        int textY = SCREEN_HEIGHT - 8 - UI_PADDING;

        _display.setCursor(textX, textY);
        _display.print(unlimitedText);
        return;
    }

    // Draw background using the color from config.h
    _display.fillRoundRect(progressBarX, progressBarY,
                           PROGRESS_BAR_WIDTH, PROGRESS_BAR_HEIGHT,
                           PROGRESS_BAR_RADIUS,
                           COLOR_PROGRESS_BG);

    // Calculate fill width
    int fillWidth = (int)(PROGRESS_BAR_WIDTH * progress);
    if (fillWidth < 2)
    {
        fillWidth = 2; // Minimum visible width
    }

    // Draw filled portion with color based on progress
    uint16_t fillColor = getProgressColor(progress);

    if (progress > 0.01f)
    { // Only draw if there's meaningful progress
        _display.fillRoundRect(progressBarX, progressBarY,
                               fillWidth, PROGRESS_BAR_HEIGHT,
                               PROGRESS_BAR_RADIUS,
                               fillColor);
    }

    // Draw status label at bottom right of screen (decoupled from progress bar)
    // Shows "Activated: HH:MM ago" during first MINIMUM_SESSION_DURATION_SECONDS,
    // then reverts to "Today's allowance: HH:MM"
    char statusBuffer[32];
    bool isRecentlyActivated = false;

    // Check if timer is running and recently activated
    if (timer.isRunning() && timer.getSessionStartTime() > 0)
    {
        time_t now = time(nullptr);
        time_t sessionStart = timer.getSessionStartTime();
        uint32_t secondsSinceActivation = (uint32_t)(now - sessionStart);

        if (secondsSinceActivation < MINIMUM_SESSION_DURATION_SECONDS)
        {
            // Show "Activated: MM:SS ago"
            isRecentlyActivated = true;
            uint32_t mins = secondsSinceActivation / 60;
            uint32_t secs = secondsSinceActivation % 60;
            snprintf(statusBuffer, sizeof(statusBuffer), "Activated: %lu:%02lu ago",
                     (unsigned long)mins, (unsigned long)secs);
        }
    }

    if (!isRecentlyActivated)
    {
        // Show normal allowance label
        uint32_t hours = totalAllowanceSeconds / 3600;
        uint32_t minutes = (totalAllowanceSeconds % 3600) / 60;
        snprintf(statusBuffer, sizeof(statusBuffer), "Today's allowance: %lu:%02lu",
                 (unsigned long)hours, (unsigned long)minutes);
    }

    _display.setFont(&fonts::Font0); // Small font for label
    _display.setTextSize(1);
    _display.setTextColor(COLOR_TEXT_SECONDARY); // Use secondary color for less emphasis

    // Clear the status label area first to prevent text overlay
    int labelAreaX = SCREEN_WIDTH - 150; // Clear wider area for longest possible text
    int labelAreaY = SCREEN_HEIGHT - 8 - UI_PADDING - 2;
    int labelAreaWidth = 150;
    int labelAreaHeight = 10;
    _display.fillRect(labelAreaX, labelAreaY, labelAreaWidth, labelAreaHeight, COLOR_BACKGROUND);

    int textWidth = _display.textWidth(statusBuffer);
    int textX = SCREEN_WIDTH - textWidth - UI_PADDING; // Right-aligned with screen padding
    int textY = SCREEN_HEIGHT - 8 - UI_PADDING;        // Bottom of screen (Font0 is ~8px tall)

    _display.setCursor(textX, textY);
    _display.print(statusBuffer);
}

void UI::clearActivationRing(const ScreenTimer &timer)
{
    // Always clear the activation ring area first (to remove any previous activation ring)
    // Draw as circles to ensure complete coverage (fillArc with 0-360 may leave gaps)
    int activationRingRadius = AVATAR_RADIUS + STATUS_RING_THICKNESS + 1;

    _display.fillArc(AVATAR_X, AVATAR_Y,
                     activationRingRadius, activationRingRadius + 2,
                     270, 270 + 360,
                     COLOR_BACKGROUND);
}

void UI::drawStatusRing(const ScreenTimer &timer)
{
    TimerState state = timer.getState();

    // Draw status ring around avatar (circle stroke with no fill)
    // Ring is drawn directly around the avatar with no gap
    uint16_t ringColor;

    switch (state)
    {
    case TimerState::EXPIRED:
        ringColor = COLOR_ACCENT_DANGER;
        break;
    case TimerState::RUNNING:
        ringColor = COLOR_ACCENT_SUCCESS;
        break;
    case TimerState::STOPPED:
    default:
        ringColor = COLOR_TEXT_PRIMARY; // White when inactive
        break;
    }

    // Draw the ring as multiple circles for thickness, starting from avatar edge
    for (int i = 0; i < STATUS_RING_THICKNESS; i++)
    {
        _display.drawCircle(AVATAR_X, AVATAR_Y, AVATAR_RADIUS + i + 1, ringColor);
    }

    // NB: Activation ring has already been cleared in updateDynamicElements
    int activationRingRadius = AVATAR_RADIUS + STATUS_RING_THICKNESS + 1;

    // Draw activation progress ring during the minimum session period (ie recently activated)
    if (timer.isRunning() && timer.getSessionStartTime() > 0)
    {
        time_t now = time(nullptr);
        time_t sessionStart = timer.getSessionStartTime();
        uint32_t secondsSinceActivation = (uint32_t)(now - sessionStart);

        if (secondsSinceActivation < MINIMUM_SESSION_DURATION_SECONDS)
        {
            // Calculate progress: what fraction of minimum session period remains?
            float remainingFraction = (float)(MINIMUM_SESSION_DURATION_SECONDS - secondsSinceActivation) / (float)MINIMUM_SESSION_DURATION_SECONDS;

            // Draw activation ring 2px wide, outside the status ring
            // Ring starts at 270 degrees (12 o'clock) and goes clockwise

            // Calculate arc angle (270 degrees = 75% remaining, for example)
            int arcDegrees = (int)(remainingFraction * 360.0f);

            // M5GFX fillArc: fillArc(x, y, r0, r1, angle0, angle1, color)
            // r0 = inner radius, r1 = outer radius
            // angle0 = start angle (270 = 12 o'clock), angle1 = end angle
            // Angles are in degrees, clockwise from 0 degrees (right/east)
            _display.fillArc(AVATAR_X, AVATAR_Y,
                             activationRingRadius, activationRingRadius + 2,
                             270, 270 + arcDegrees,
                             ringColor);
        }
    }
}

// ============================================================================
// Menu Drawing
// ============================================================================

void UI::drawMenu(const DropdownMenu &menu)
{
    if (!menu.isVisible())
    {
        return;
    }

    int itemCount = menu.getItemCount();
    int selectedIndex = menu.getSelectedIndex();

    _display.startWrite();

    // Fill entire area below header with dark gray background
    _display.fillRect(0, MENU_Y, SCREEN_WIDTH, MENU_HEIGHT, COLOR_MENU_BG);

    // Calculate content area (with padding, leaving space for chevron)
    // Move content up by 24px from original position
    int contentY = MENU_Y + MENU_PADDING - 24;
    int contentHeight = MENU_HEIGHT - (2 * MENU_PADDING) - MENU_CHEVRON_AREA_HEIGHT;
    int itemAreaHeight = contentHeight / MENU_VISIBLE_ITEMS;
    int itemGap = 8; // 8px gap between items

    // Draw the 3 visible items: previous, selected, next (with wrapping)
    for (int slot = 0; slot < MENU_VISIBLE_ITEMS && itemCount > 0; slot++)
    {
        // Calculate which item index to show in this slot
        int displayIndex;
        if (slot == 0)
        {
            // Previous item (wraps to end if at start)
            displayIndex = (selectedIndex - 1 + itemCount) % itemCount;
        }
        else if (slot == 1)
        {
            // Current selected item
            displayIndex = selectedIndex;
        }
        else
        {
            // Next item (wraps to start if at end)
            displayIndex = (selectedIndex + 1) % itemCount;
        }

        // Add gap spacing: slot 0 at base, slot 1 adds gap, slot 2 adds 2*gap
        int itemY = contentY + (slot * itemAreaHeight) + (slot * itemGap);

        // Set color based on slot (middle slot = selected = white, others = gray)
        if (slot == 1)
        {
            _display.setTextColor(COLOR_TEXT_PRIMARY); // White for selected
        }
        else
        {
            _display.setTextColor(COLOR_MENU_ITEM_GRAY); // Gray for prev/next
        }

        // Use FreeSansBold12pt7b for ~16px bold look
        _display.setFont(&fonts::FreeSansBold12pt7b);
        _display.setTextSize(1);

        // Get label and truncate to 18 characters max
        const char *rawLabel = menu.getItemLabel(displayIndex);
        char label[19]; // 18 chars + null terminator
        strncpy(label, rawLabel, 18);
        label[18] = '\0';

        // Center the text horizontally
        int textWidth = _display.textWidth(label);
        int textX = (SCREEN_WIDTH - textWidth) / 2;
        int textY = itemY + (itemAreaHeight + 12) / 2; // Center vertically (baseline adjustment for GFX font)

        _display.setCursor(textX, textY);
        _display.print(label);
    }

    // Draw downward chevron at bottom center
    int chevronY = MENU_Y + MENU_HEIGHT - MENU_CHEVRON_AREA_HEIGHT + 2;
    int chevronX = SCREEN_WIDTH / 2;
    int chevronSize = 6;

    _display.setColor(COLOR_TEXT_PRIMARY);
    // Draw simple V chevron
    _display.drawLine(chevronX - chevronSize, chevronY, chevronX, chevronY + chevronSize, COLOR_TEXT_PRIMARY);
    _display.drawLine(chevronX, chevronY + chevronSize, chevronX + chevronSize, chevronY, COLOR_TEXT_PRIMARY);
    // Make it slightly thicker
    _display.drawLine(chevronX - chevronSize, chevronY + 1, chevronX, chevronY + chevronSize + 1, COLOR_TEXT_PRIMARY);
    _display.drawLine(chevronX, chevronY + chevronSize + 1, chevronX + chevronSize, chevronY + 1, COLOR_TEXT_PRIMARY);

    _display.endWrite();
    _display.display();
}

void UI::clearMenu(const ScreenTimer &timer,
                   const char *userName,
                   char userInitial,
                   const char *avatarName,
                   bool isTimerRunning,
                   NetworkStatus networkStatus)
{
    // Full redraw to clear menu overlay
    drawMainScreen(timer, userName, userInitial, avatarName, isTimerRunning, networkStatus);
}

void UI::flashMenuItem(const DropdownMenu &menu, int itemIndex)
{
    if (!menu.isVisible() || itemIndex < 0 || itemIndex >= menu.getItemCount())
    {
        return;
    }

    // The flashed item is always the selected one (shown in the middle slot)
    int itemCount = menu.getItemCount();
    int selectedIndex = menu.getSelectedIndex();

    // Only flash if this is the selected item
    if (itemIndex != selectedIndex)
    {
        return;
    }

    // Calculate position (middle slot = slot 1)
    // Move content up by 24px from original position
    int contentY = MENU_Y + MENU_PADDING - 24;
    int contentHeight = MENU_HEIGHT - (2 * MENU_PADDING) - MENU_CHEVRON_AREA_HEIGHT;
    int itemAreaHeight = contentHeight / MENU_VISIBLE_ITEMS;
    int itemGap = 8;                                 // 8px gap between items
    int slotY = contentY + itemAreaHeight + itemGap; // Middle slot (slot 1)

    _display.startWrite();

    // Set font first so textWidth calculation is accurate
    _display.setFont(&fonts::FreeSansBold12pt7b);
    _display.setTextSize(1);

    // Get label and truncate to 18 characters max
    const char *rawLabel = menu.getItemLabel(itemIndex);
    char label[19]; // 18 chars + null terminator
    strncpy(label, rawLabel, 18);
    label[18] = '\0';

    int textWidth = _display.textWidth(label);
    int textX = (SCREEN_WIDTH - textWidth) / 2;
    int textY = slotY + (itemAreaHeight + 12) / 2; // Baseline adjustment for GFX font

    // Clear only the text area for this menu item (not full width to avoid affecting other items)
    // FreeSansBold12pt7b has approximate height of 20px
    int textHeight = 2;     // Approximate text height with some padding
    int clearY = textY - 6; // Adjust for baseline (ascent)
    _display.fillRect(textX - 2, clearY, textWidth + 4, textHeight, COLOR_MENU_BG);

    // Draw with flash color
    _display.setTextColor(COLOR_MENU_FLASH);
    _display.setCursor(textX, textY);
    _display.print(label);

    _display.endWrite();
    _display.display();
}

void UI::showNotification(const char *message, uint32_t durationMs)
{
    // Large notification panel design
    // Background: 206x92px, centered horizontally and vertically in content area
    constexpr int NOTIF_WIDTH = 206;
    constexpr int NOTIF_HEIGHT = 92;
    constexpr int NOTIF_X = (SCREEN_WIDTH - NOTIF_WIDTH) / 2;

    // Vertically center between header bottom and screen bottom
    int contentAreaTop = HEADER_Y + HEADER_HEIGHT;
    int contentAreaHeight = SCREEN_HEIGHT - contentAreaTop;
    int notifY = contentAreaTop + (contentAreaHeight - NOTIF_HEIGHT) / 2;

    // Info icon dimensions (oval with 'i')
    constexpr int ICON_OVAL_WIDTH = 32;
    constexpr int ICON_OVAL_HEIGHT = 40;
    constexpr int ICON_MARGIN_LEFT = 10;
    int iconCenterX = NOTIF_X + ICON_MARGIN_LEFT + (ICON_OVAL_WIDTH / 2);
    int iconCenterY = notifY + (NOTIF_HEIGHT / 2);

    // Text area starts after icon
    constexpr int TEXT_MARGIN_LEFT = 8;
    int textAreaX = NOTIF_X + ICON_MARGIN_LEFT + ICON_OVAL_WIDTH + TEXT_MARGIN_LEFT;
    int textAreaWidth = NOTIF_X + NOTIF_WIDTH - textAreaX - 10; // 10px right margin

    // Border color - very dark gray
    constexpr uint16_t COLOR_BORDER_DARK = 0x2104; // Very dark gray

    _display.startWrite();

    // Draw 1px dark gray border (draw slightly larger rect behind)
    _display.fillRect(NOTIF_X - 1, notifY - 1,
                      NOTIF_WIDTH + 2, NOTIF_HEIGHT + 2,
                      COLOR_BORDER_DARK);

    // Draw main background in header color (plum)
    _display.fillRect(NOTIF_X, notifY, NOTIF_WIDTH, NOTIF_HEIGHT, COLOR_HEADER_BG);

    // Draw white oval for info icon
    _display.fillEllipse(iconCenterX, iconCenterY,
                         ICON_OVAL_WIDTH / 2, ICON_OVAL_HEIGHT / 2,
                         COLOR_TEXT_PRIMARY);

    // Draw 'i' in the oval using header color
    // Draw the dot of the 'i'
    _display.fillCircle(iconCenterX, iconCenterY - 10, 3, COLOR_HEADER_BG);
    // Draw the stem of the 'i' as a rounded rect
    _display.fillRoundRect(iconCenterX - 3, iconCenterY - 3, 6, 18, 2, COLOR_HEADER_BG);

    // Set up text rendering - use FreeSans12pt7b for readable text on small screen
    _display.setFont(&fonts::FreeSansBold12pt7b);
    _display.setTextColor(COLOR_TEXT_PRIMARY);
    _display.setTextSize(1);

    // Word-wrap the message into up to 3 lines
    char lines[3][48]; // Up to 3 lines, 47 chars each + null
    int lineCount = 0;
    int lineHeight = 20; // Approximate line height for 12pt font

    // Simple word-wrap algorithm
    const char *ptr = message;
    for (int line = 0; line < 3 && *ptr; line++)
    {
        lines[line][0] = '\0';
        char tempLine[48] = "";

        while (*ptr)
        {
            // Skip leading spaces
            while (*ptr == ' ')
                ptr++;
            if (!*ptr)
                break;

            // Extract next word
            char word[48] = "";
            int wordIdx = 0;
            while (*ptr && *ptr != ' ' && wordIdx < 46)
            {
                word[wordIdx++] = *ptr++;
            }
            word[wordIdx] = '\0';

            // Try adding word to current line
            char testLine[96];
            if (tempLine[0])
            {
                snprintf(testLine, sizeof(testLine), "%s %s", tempLine, word);
            }
            else
            {
                strncpy(testLine, word, sizeof(testLine) - 1);
                testLine[sizeof(testLine) - 1] = '\0';
            }

            int testWidth = _display.textWidth(testLine);
            if (testWidth <= textAreaWidth || tempLine[0] == '\0')
            {
                // Word fits, add it to current line
                strncpy(tempLine, testLine, sizeof(tempLine) - 1);
                tempLine[sizeof(tempLine) - 1] = '\0';
            }
            else
            {
                // Word doesn't fit, put it back and break to next line
                ptr -= wordIdx; // Rewind to start of word
                break;
            }
        }

        strncpy(lines[line], tempLine, 47);
        lines[line][47] = '\0';
        if (lines[line][0])
            lineCount++;
    }

    // Calculate vertical starting position to center text block
    // For GFX fonts, Y is the baseline. Position first baseline near top of centered block.
    int totalTextHeight = lineCount * lineHeight;
    int textStartY = notifY + (NOTIF_HEIGHT - totalTextHeight) / 2 + lineHeight - 4 - 16;

    // Draw each line
    for (int i = 0; i < lineCount; i++)
    {
        int lineY = textStartY + (i * lineHeight);
        _display.setCursor(textAreaX, lineY);
        _display.print(lines[i]);
    }

    _display.endWrite();
    _display.display();

    // Add 1 seconds to the requested duration for better visibility on tiny screen
    // Note: Caller should use M5.delay() after this call to hold the notification
    if (durationMs > 0)
    {
        M5.delay(durationMs + 1000);

        // Flag that a full redraw is needed to clear the notification
        forceFullRedraw();
    }
}

// ============================================================================
// Helper Methods
// ============================================================================

void UI::formatTime(uint32_t seconds, char *buffer, size_t bufferSize)
{
    uint32_t hours = seconds / 3600;
    uint32_t minutes = (seconds % 3600) / 60;
    uint32_t secs = seconds % 60;

    // If more than 9 hours, display "LOTS!" as error indicator
    if (hours > 9)
    {
        snprintf(buffer, bufferSize, "LOTS!");
        return;
    }

    // Always format as H:MM:SS (max 7 chars + null)
    snprintf(buffer, bufferSize, "%lu:%02lu:%02lu",
             (unsigned long)hours, (unsigned long)minutes, (unsigned long)secs);

    // Ensure strict truncation to H:MM:SS format
    if (bufferSize >= 8)
    {
        buffer[7] = '\0';
    }
}

uint16_t UI::getProgressColor(float progress)
{
    if (progress > 0.7f)
    {
        return COLOR_PROGRESS_FILL; // normal
    }
    else if (progress > 0.2f)
    {
        return COLOR_ACCENT_WARNING; // Orange - getting low
    }
    else
    {
        return COLOR_ACCENT_DANGER; // Red - almost out
    }
}

void UI::getDayOfWeekStr(int dayOfWeek, char *buffer)
{
    const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    if (dayOfWeek >= 0 && dayOfWeek <= 6)
    {
        strcpy(buffer, days[dayOfWeek]);
    }
    else
    {
        strcpy(buffer, "???");
    }
}

void UI::getMonthStr(int month, char *buffer)
{
    const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if (month >= 0 && month <= 11)
    {
        strcpy(buffer, months[month]);
    }
    else
    {
        strcpy(buffer, "???");
    }
}

char UI::getNetworkStatusChar(NetworkStatus status)
{
    switch (status)
    {
    case NetworkStatus::DISCONNECTED:
        return '-';
    case NetworkStatus::CONNECTING:
        return '/';
    case NetworkStatus::CONNECTED:
        return 'O';
    case NetworkStatus::ERROR:
        return '!';
    default:
        return '?';
    }
}

void UI::drawNetworkStatusInHeader()
{
    // Layout constants for right-aligned items
    constexpr int ITEM_GAP = 6;
    constexpr int WIFI_WIDTH = 24;
    constexpr int WIFI_HEIGHT = 16;
    constexpr int BATTERY_WIDTH = 12;
    constexpr int BATTERY_HEIGHT = 16;

    // Calculate positions from right edge
    int batteryX = SCREEN_WIDTH - ITEM_GAP - BATTERY_WIDTH;
    int wifiX = batteryX - ITEM_GAP - WIFI_WIDTH;

    // Vertically center both indicators
    int wifiY = HEADER_Y + (HEADER_HEIGHT - WIFI_HEIGHT) / 2;
    int batteryY = HEADER_Y + (HEADER_HEIGHT - BATTERY_HEIGHT) / 2;

    // Choose color based on WiFi status
    uint16_t wifiColor = COLOR_TEXT_MUTED;
    if (_currentNetworkStatus == NetworkStatus::CONNECTED)
    {
        wifiColor = COLOR_ACCENT_SUCCESS;
    }
    else if (_currentNetworkStatus == NetworkStatus::CONNECTING)
    {
        wifiColor = COLOR_ACCENT_WARNING;
    }
    else if (_currentNetworkStatus == NetworkStatus::ERROR)
    {
        wifiColor = COLOR_ACCENT_DANGER;
    }

    // Draw WiFi indicator as triangle (point at bottom)
    // Triangle vertices: top-left, top-right, bottom-center
    int triX1 = wifiX; // Top-left
    int triY1 = wifiY;
    int triX2 = wifiX + WIFI_WIDTH; // Top-right
    int triY2 = wifiY;
    int triX3 = wifiX + WIFI_WIDTH / 2; // Bottom-center
    int triY3 = wifiY + WIFI_HEIGHT;

    _display.fillTriangle(triX1, triY1, triX2, triY2, triX3, triY3, wifiColor);

    // Overlay status character on triangle
    char statusChar = getNetworkStatusChar(_currentNetworkStatus);
    char statusStr[2] = {statusChar, '\0'};
    _display.setTextColor(COLOR_HEADER_BG); // Contrast with triangle
    _display.setTextSize(1);
    _display.setFont(&fonts::Font0);

    int textWidth = _display.textWidth(statusStr);
    int statusTextX = wifiX + (WIFI_WIDTH - textWidth) / 2;
    int statusTextY = wifiY + 2; // Near top of triangle
    _display.setCursor(statusTextX, statusTextY);
    _display.print(statusStr);

    // Draw battery indicator as 4 vertically stacked rectangles
    // Reflects real battery percentage:
    // >80%: 4 bars, >60%: 3 bars, >40%: 2 bars, >20%: 1 bar, <20%: 0 bars
    constexpr int RECT_HEIGHT = 3;
    constexpr int RECT_GAP = 1;
    int rectWidth = BATTERY_WIDTH;

    // Get actual battery level (0-100) and charging status
    int batteryLevel = M5.Power.getBatteryLevel();
    // M5stickC plus2 does not support isCharging detection - see https://community.m5stack.com/topic/6006/m5stickcp2-ischarging-inf-power-class-does-not-function
    bool isCharging = 0; // M5.Power.isCharging();

    // Determine how many bars to illuminate based on battery percentage
    int barsLit = 0;
    if (batteryLevel > 80)
    {
        barsLit = 4;
    }
    else if (batteryLevel > 60)
    {
        barsLit = 3;
    }
    else if (batteryLevel > 40)
    {
        barsLit = 2;
    }
    else if (batteryLevel > 20)
    {
        barsLit = 1;
    }
    else
    {
        barsLit = 0;
    }

    // Colors: white when lit, 25% gray when not
    constexpr uint16_t COLOR_BAR_LIT = 0xFFFF; // White
    constexpr uint16_t COLOR_BAR_DIM = 0x4208; // 25% gray (RGB 64,64,64 in RGB565)

    // Draw from bottom to top (bar 0 is bottom, bar 3 is top)
    // Array index 0 = bottom bar, drawn last (highest Y)
    for (int i = 0; i < 4; i++)
    {
        // Calculate Y position: i=0 is bottom (highest Y), i=3 is top (lowest Y)
        int barIndex = 3 - i; // Invert so we draw top to bottom
        int rectY = batteryY + i * (RECT_HEIGHT + RECT_GAP);

        // Bar at index barIndex is lit if barIndex < barsLit
        // But we want bottom bars lit first, so:
        // bar 0 (bottom) lit if barsLit >= 1
        // bar 1 lit if barsLit >= 2
        // bar 2 lit if barsLit >= 3
        // bar 3 (top) lit if barsLit >= 4
        bool isLit = (barIndex < barsLit);
        uint16_t color = isLit ? COLOR_BAR_LIT : COLOR_BAR_DIM;

        _display.fillRect(batteryX, rectY, rectWidth, RECT_HEIGHT, color);
    }

    // If charging, overlay a lightning bolt
    if (isCharging)
    {
        // Lightning bolt centered on battery indicator
        // Battery area: batteryX to batteryX+BATTERY_WIDTH, batteryY to batteryY+BATTERY_HEIGHT
        int boltCenterX = batteryX + BATTERY_WIDTH / 2;
        int boltCenterY = batteryY + BATTERY_HEIGHT / 2;

        // Lightning bolt shape - small and centered
        // Coordinates relative to center, scaled to fit in battery indicator
        // Lightning bolt points (forms a zigzag shape):
        //    /\
        //   /  \
        //  /----\
        //     /
        //    /

        // Simple lightning bolt vertices (7-point polygon)
        int x1 = boltCenterX + 1; // Top right of upper triangle
        int y1 = boltCenterY - 6;
        int x2 = boltCenterX - 3; // Left side
        int y2 = boltCenterY - 1;
        int x3 = boltCenterX; // Inner left
        int y3 = boltCenterY - 1;
        int x4 = boltCenterX - 2; // Bottom left of lower part
        int y4 = boltCenterY + 6;
        int x5 = boltCenterX + 2; // Right side
        int y5 = boltCenterY + 1;
        int x6 = boltCenterX - 1; // Inner right
        int y6 = boltCenterY + 1;

        // Draw lightning bolt as two triangles with stroke
        // Upper triangle (pointing down-left)
        // Draw black stroke (2px exterior) first
        constexpr uint16_t COLOR_STROKE = 0x0000; // Black stroke
        constexpr uint16_t COLOR_BOLT = 0xFFFF;   // White fill

        // Draw stroke by drawing slightly larger shapes in black (2px offset)
        _display.fillTriangle(x1 - 2, y1 - 2, x2 - 2, y2, x6 + 2, y6, COLOR_STROKE);
        _display.fillTriangle(x3 - 2, y3, x4 - 2, y4 + 2, x5 + 2, y5, COLOR_STROKE);

        // Draw white fill
        _display.fillTriangle(x1, y1, x2, y2, x6, y6, COLOR_BOLT);
        _display.fillTriangle(x3, y3, x4, y4, x5, y5, COLOR_BOLT);
    }
}

void UI::updateNetworkStatus(NetworkStatus status)
{
    _currentNetworkStatus = status;

    // Only update the header area if not showing a dialog
    if (!_infoDialogVisible)
    {
        _display.startWrite();

        // Clear the right-aligned indicators area (WiFi + Battery + gaps)
        constexpr int INDICATORS_WIDTH = 24 + 6 + 12 + 6; // WiFi + gap + Battery + padding
        int clearX = SCREEN_WIDTH - INDICATORS_WIDTH;
        _display.fillRect(clearX, HEADER_Y, INDICATORS_WIDTH, HEADER_HEIGHT, COLOR_HEADER_BG);
        drawNetworkStatusInHeader();

        _display.endWrite();
        _display.display();
    }
}

void UI::updateBatteryIndicator()
{
    // Only update if not showing a dialog
    if (!_infoDialogVisible)
    {
        _display.startWrite();

        // Clear just the battery indicator area and redraw
        // Battery is at the right edge of header
        constexpr int BATTERY_WIDTH = 12;
        constexpr int ITEM_GAP = 6;
        int batteryX = SCREEN_WIDTH - ITEM_GAP - BATTERY_WIDTH;

        // Clear battery area
        _display.fillRect(batteryX, HEADER_Y, BATTERY_WIDTH + ITEM_GAP, HEADER_HEIGHT, COLOR_HEADER_BG);

        // Redraw network status (which includes battery indicator)
        drawNetworkStatusInHeader();

        _display.endWrite();
        _display.display();
    }
}

// ============================================================================
// Shared Header Drawing (Public Methods)
// ============================================================================

void UI::drawNetworkStatusInHeader(NetworkStatus networkStatus)
{
    _currentNetworkStatus = networkStatus;
    drawNetworkStatusInHeader();
}

void UI::drawStandardHeader(const char *title, NetworkStatus networkStatus)
{
    _currentNetworkStatus = networkStatus;
    drawHeader(title);
    drawNetworkStatusInHeader();
}

// ============================================================================
// Info Dialog
// ============================================================================

void UI::showInfoDialog(const char *title,
                        const char *message,
                        const char *buttonLabel)
{
    _infoDialogVisible = true;

    _display.waitDisplay();
    _display.startWrite();

    // Fill entire screen with background
    _display.fillScreen(COLOR_BACKGROUND);

    // Draw dialog border/frame
    int dialogMargin = UI_PADDING * 2;
    int dialogX = dialogMargin;
    int dialogY = dialogMargin;
    int dialogWidth = SCREEN_WIDTH - (dialogMargin * 2);
    int dialogHeight = SCREEN_HEIGHT - (dialogMargin * 2);

    // Draw dialog background with subtle border
    _display.fillRoundRect(dialogX - 2, dialogY - 2,
                           dialogWidth + 4, dialogHeight + 4,
                           6, COLOR_BORDER);
    _display.fillRoundRect(dialogX, dialogY,
                           dialogWidth, dialogHeight,
                           5, COLOR_HEADER_BG);

    // Draw title bar
    int titleBarHeight = 28;
    _display.fillRoundRect(dialogX, dialogY,
                           dialogWidth, titleBarHeight,
                           5, COLOR_ACCENT_DANGER);
    // Square off bottom corners of title bar
    _display.fillRect(dialogX, dialogY + titleBarHeight - 5,
                      dialogWidth, 5, COLOR_ACCENT_DANGER);

    // Draw title text
    _display.setTextColor(COLOR_TEXT_PRIMARY);
    _display.setFont(&fonts::Font2);
    _display.setTextSize(1);

    int titleWidth = _display.textWidth(title);
    int titleX = dialogX + (dialogWidth - titleWidth) / 2;
    int titleY = dialogY + (titleBarHeight - 12) / 2;
    _display.setCursor(titleX, titleY);
    _display.print(title);

    // Draw message text (word-wrapped, centered)
    _display.setTextColor(COLOR_TEXT_SECONDARY);
    _display.setFont(&fonts::Font0);
    _display.setTextSize(1);

    int messageY = dialogY + titleBarHeight + 12;
    int messageAreaWidth = dialogWidth - 16;
    int messageX = dialogX + 8;

    // Simple word wrap - split message by newlines and spaces
    char msgBuffer[256];
    strncpy(msgBuffer, message, sizeof(msgBuffer) - 1);
    msgBuffer[sizeof(msgBuffer) - 1] = '\0';

    char *line = msgBuffer;
    int lineY = messageY;
    int lineHeight = 12;

    // Draw each character, wrapping at word boundaries
    _display.setCursor(messageX, lineY);
    int currentX = messageX;
    for (size_t i = 0; i < strlen(message); i++)
    {
        char c = message[i];
        if (c == '\n')
        {
            lineY += lineHeight;
            currentX = messageX;
            _display.setCursor(currentX, lineY);
        }
        else
        {
            int charWidth = _display.textWidth(String(c).c_str());
            if (currentX + charWidth > dialogX + dialogWidth - 8)
            {
                lineY += lineHeight;
                currentX = messageX;
                _display.setCursor(currentX, lineY);
            }
            _display.print(c);
            currentX += charWidth;
        }
    }

    // Draw button at bottom
    int buttonWidth = 60;
    int buttonHeight = 22;
    int buttonX = dialogX + (dialogWidth - buttonWidth) / 2;
    int buttonY = dialogY + dialogHeight - buttonHeight - 8;

    _display.fillRoundRect(buttonX, buttonY, buttonWidth, buttonHeight,
                           3, COLOR_ACCENT_PRIMARY);

    // Button label
    _display.setTextColor(COLOR_TEXT_PRIMARY);
    _display.setFont(&fonts::Font0);
    int labelWidth = _display.textWidth(buttonLabel);
    int labelX = buttonX + (buttonWidth - labelWidth) / 2;
    int labelY = buttonY + (buttonHeight - 8) / 2;
    _display.setCursor(labelX, labelY);
    _display.print(buttonLabel);

    // Draw hint that Button A dismisses
    _display.setTextColor(COLOR_TEXT_MUTED);
    _display.setFont(&fonts::Font0);
    const char *hint = "Press front button";
    int hintWidth = _display.textWidth(hint);
    int hintX = dialogX + (dialogWidth - hintWidth) / 2;
    int hintY = buttonY + buttonHeight + 4;
    _display.setCursor(hintX, hintY);
    _display.print(hint);

    _display.endWrite();
    _display.display();
}

bool UI::isInfoDialogVisible() const
{
    return _infoDialogVisible;
}

void UI::dismissInfoDialog()
{
    _infoDialogVisible = false;
    _needsFullRedraw = true;
}
