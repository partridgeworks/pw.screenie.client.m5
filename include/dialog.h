/**
 * dialog.h - Dialog System for Screen Time Tracker
 * 
 * Provides reusable dialog overlays supporting:
 * - Info dialogs (1 button)
 * - Confirm dialogs (2 buttons)
 * - Progress dialogs (with progress bar or spinner)
 * 
 * Dialogs are full-screen overlays that capture input.
 * Button B cycles between buttons, Button A activates selected button.
 * 
 * @author Screen Time Tracker
 * @version 1.0
 */

#ifndef DIALOG_H
#define DIALOG_H

#include <M5GFX.h>
#include <stdint.h>

// Maximum lengths for dialog content
constexpr int DIALOG_MAX_TITLE_LEN = 32;
constexpr int DIALOG_MAX_MESSAGE_LEN = 128;
constexpr int DIALOG_MAX_BUTTON_LABEL_LEN = 16;
constexpr int DIALOG_MAX_BUTTONS = 2;

/**
 * DialogResult - Enumeration of button indices and special results
 */
enum class DialogResult {
    NONE = -1,      // Dialog still active, no result yet
    BUTTON_1 = 0,   // First button (left or single)
    BUTTON_2 = 1,   // Second button (right)
    DISMISSED = 99  // Dialog was dismissed without button press
};

/**
 * DialogType - Types of dialogs
 */
enum class DialogType {
    INFO,       // Single button (OK/dismiss)
    CONFIRM,    // Two buttons (e.g., Cancel/OK)
    PROGRESS    // Progress bar or spinner (no buttons until complete)
};

/**
 * DialogCallback - Function called when dialog is closed
 * 
 * @param result Which button was pressed (0 or 1) or DialogResult::DISMISSED
 * @param userData User data pointer passed to dialog
 */
using DialogCallback = void (*)(DialogResult result, void* userData);

/**
 * Dialog - Full-screen dialog overlay
 * 
 * Usage:
 *   Dialog dialog(display);
 *   dialog.showInfo("Title", "Message", "OK");
 *   // In main loop:
 *   if (btnB) dialog.handleButtonB();  // Cycle buttons
 *   if (btnA) dialog.handleButtonA();  // Activate selected
 *   dialog.draw();  // Redraw if needed
 *   if (dialog.isDismissed()) { handle result }
 */
class Dialog {
public:
    /**
     * Constructor
     * @param display Reference to M5GFX display
     */
    explicit Dialog(M5GFX& display);
    
    /**
     * Destructor
     */
    ~Dialog() = default;
    
    // ========================================================================
    // Factory Methods - Show different dialog types
    // ========================================================================
    
    /**
     * Show an info dialog with a single button
     * @param title Dialog title (shown in colored header)
     * @param message Main message text (supports simple word wrap)
     * @param buttonLabel Label for the button (default "OK")
     * @param callback Optional callback when button is pressed
     * @param userData Optional user data passed to callback
     */
    void showInfo(const char* title,
                  const char* message,
                  const char* buttonLabel = "OK",
                  DialogCallback callback = nullptr,
                  void* userData = nullptr);
    
    /**
     * Show a confirm dialog with two buttons
     * @param title Dialog title
     * @param message Main message text
     * @param button1Label Label for left button (typically Cancel)
     * @param button2Label Label for right button (typically OK/Confirm)
     * @param callback Callback when either button is pressed
     * @param userData Optional user data passed to callback
     */
    void showConfirm(const char* title,
                     const char* message,
                     const char* button1Label,
                     const char* button2Label,
                     DialogCallback callback = nullptr,
                     void* userData = nullptr);
    
    /**
     * Show a progress dialog (no buttons, shows spinner or progress bar)
     * @param title Dialog title
     * @param message Status message
     */
    void showProgress(const char* title, const char* message);
    
    // ========================================================================
    // State Updates
    // ========================================================================
    
    /**
     * Update progress (for progress dialogs)
     * @param progress Progress value 0.0 to 1.0, or -1 for indeterminate
     */
    void setProgress(float progress);
    
    /**
     * Update the message text
     * @param message New message
     */
    void setMessage(const char* message);
    
    /**
     * Complete a progress dialog, showing a button to dismiss
     * @param message Final message to show
     * @param buttonLabel Label for dismiss button
     */
    void completeProgress(const char* message, const char* buttonLabel = "OK");
    
    // ========================================================================
    // Input Handling
    // ========================================================================
    
    /**
     * Handle Button A press - activates currently selected button
     */
    void handleButtonA();
    
    /**
     * Handle Button B press - cycles between buttons
     */
    void handleButtonB();
    
    // ========================================================================
    // Drawing
    // ========================================================================
    
    /**
     * Draw the dialog
     * Should be called after state changes or input
     */
    void draw();
    
    /**
     * Check if dialog needs redraw
     * @return true if draw() should be called
     */
    bool needsRedraw() const;
    
    // ========================================================================
    // State Queries
    // ========================================================================
    
    /**
     * Check if dialog is currently visible
     * @return true if visible
     */
    bool isVisible() const;
    
    /**
     * Check if dialog has been dismissed
     * @return true if dismissed (result is available)
     */
    bool isDismissed() const;
    
    /**
     * Get the result of the dialog
     * @return Which button was pressed, or NONE if still active
     */
    DialogResult getResult() const;
    
    /**
     * Get currently selected button index
     * @return 0 for first button, 1 for second button
     */
    int getSelectedButton() const;
    
    /**
     * Dismiss the dialog without invoking callback
     */
    void dismiss();
    
    /**
     * Reset dialog to initial state
     */
    void reset();
    
    /**
     * Check if there's a pending callback to invoke
     * Used for deferred callback pattern - screen redraws before callback runs
     * @return true if a callback is pending
     */
    bool hasPendingCallback() const;
    
    /**
     * Invoke the pending callback if any
     * Should be called after screen has been redrawn to avoid UI freeze
     * Clears the pending callback state after invocation
     */
    void invokePendingCallback();

private:
    M5GFX& _display;
    
    // Dialog content
    char _title[DIALOG_MAX_TITLE_LEN];
    char _message[DIALOG_MAX_MESSAGE_LEN];
    char _buttons[DIALOG_MAX_BUTTONS][DIALOG_MAX_BUTTON_LABEL_LEN];
    int _buttonCount;
    
    // Dialog type and state
    DialogType _type;
    bool _visible;
    bool _dismissed;
    int _selectedButton;
    DialogResult _result;
    
    // Callback
    DialogCallback _callback;
    void* _userData;
    
    // Progress state
    float _progress;  // 0.0-1.0 or -1 for indeterminate
    uint8_t _spinnerFrame;  // For indeterminate spinner animation
    uint32_t _lastSpinnerUpdateMs;
    
    // Drawing state
    bool _needsRedraw;
    
    // Deferred callback state (for screen redraw before callback execution)
    bool _hasPendingCallback;
    DialogResult _pendingResult;
    
    // Private drawing helpers
    void drawBackground();
    void drawTitleBar();
    void drawMessage();
    void drawButtons();
    void drawProgressBar();
    void drawSpinner();
    void drawHint();
    
    // Helper to invoke callback and dismiss
    void finishWithResult(DialogResult result);
};

#endif // DIALOG_H
