# Screen Time Tracker - Architecture Overview

A modular, screen-based architecture for the M5StickC-Plus 2 screen time tracker.

---

## Core Components

### AppState (`app_state.h/cpp`)
- **Singleton** holding all application state
- `UserSession` - login status, API key, selected child
- `ScreenTimeData` - daily allowance, time used, last sync
- `NetworkStatus` - current WiFi/connection state
- `determineInitialScreen()` - decides which screen to show on startup

### ScreenManager (`screen_manager.h/cpp`)
- Orchestrates screen lifecycle and transitions
- Maintains navigation history stack
- Routes button presses to active screen or overlay
- Manages dialog overlay display

### PersistenceManager (`persistence.h/cpp`)
- ESP32 NVS (Preferences) wrapper
- Saves/loads: session data, selected child, last weekday, NTP sync time, consumed screen time
- Namespace: `"screentimer"`

### ScreenTimer (`timer.h/cpp`)
- **Consumed-time model**: tracks time used, not time remaining
- `_consumedTodaySeconds` - time consumed in completed sessions today
- `_sessionStartTime` - Unix timestamp when current session started
- Remaining time = allowance - (consumedToday + currentSessionElapsed)
- Simplifies deep sleep (just preserve session start time)
- Enables easy API sync (send consumed time directly)

---

## Screens

All screens inherit from `Screen` base class with lifecycle methods:
- `onEnter()` / `onExit()` / `onResume()` / `onPause()`
- `update()` - called every loop iteration
- `draw()` - render screen content
- `handleButtonA/B/Power()` - input handling

| Screen | File | Purpose |
|--------|------|---------|
| **LoginScreen** | `screens/login_screen.h/cpp` | QR code + numeric code for device pairing |
| **SelectChildScreen** | `screens/select_child_screen.h/cpp` | Paged list to choose which child |
| **MainScreen** | `screens/main_screen.h/cpp` | Timer display, progress bar, main UI |
| **SyncScreen** | `screens/sync_screen.h/cpp` | Full-screen spinner during network ops |

### Screen Flow
```
App Start → Check Persistence
  ├── Not logged in → LoginScreen
  ├── Logged in, no child → SelectChildScreen  
  └── Logged in + child → MainScreen
```

---

## Overlays

### Dialog (`dialog.h/cpp`)
- Types: `INFO` (1 button), `CONFIRM` (2 buttons), `PROGRESS` (spinner)
- Full-screen modal overlay
- Button B cycles focus, Button A activates
- Callback on button press

### DropdownMenu (`menu.h/cpp`)
- Dropdown from top of screen
- Items with icons and callbacks
- Button B cycles items, Button A activates, Power closes

---

## Button Routing

```
Button Press
    ↓
ScreenManager.handleButton*()
    ↓
Dialog visible? → Route to Dialog
    ↓ (no)
Menu open? → Route to Menu
    ↓ (no)
Route to current Screen
```

| Button | General Action |
|--------|---------------|
| **A (front)** | Primary action / Activate / Start-Stop timer |
| **B (side)** | Secondary / Cycle items / Open menu |
| **Power** | Back / Close overlay / Long-hold = power off |

---

## Network & API

### NetworkManager (`network.h/cpp`)
- `ensureConnected()` - auto-connects WiFi if needed
- Keep-alive timer (30s) - stays connected briefly after operations
- Polling mode - prevents auto-disconnect during login/more-time polling
- `update()` - must be called in loop for keep-alive management

### ApiClient (`api_client.h/cpp`)
- Device-code login flow (QR + numeric code)
- Fetch family members, daily allowance
- Request more time, poll for approval

#### Pairing Lifecycle

Device pairing uses a device-code flow with the following statuses:

| Status | Description |
|--------|-------------|
| `issued` | Device requested a pairing code, waiting for user to scan QR |
| `linked` | User scanned QR and logged in on mobile, awaiting device poll |
| `paired` | Device polled and received API key - pairing complete |
| `expired` | Pairing code expired before completion |

```
Device                          Server                          Mobile App
   │                              │                                  │
   │──GET /pairing/devicecode────▶│                                  │
   │◀──pairingCode (issued)───────│                                  │
   │                              │                                  │
   │   [Display QR + code]        │                                  │
   │                              │◀───────User scans QR─────────────│
   │                              │         (status → linked)        │
   │                              │                                  │
   │──POST /pairing/{code}───────▶│                                  │
   │◀──apiKey (status=paired)─────│                                  │
   │                              │                                  │
   │   [Store API key, proceed]   │                                  │
```

**Polling behavior:**
- `issued` → continue polling (waiting for user to scan)
- `linked` → continue polling (API key issued on next poll)
- `paired` → success, store API key, proceed to next screen
- `expired` → error, prompt user to retry with new code

### PollingManager (`polling_manager.h/cpp`)
- Non-blocking background polling
- Login polling (waiting for QR scan)
- More-time polling (waiting for parent approval)
- Configurable intervals and timeouts
- `update()` - must be called in loop

---

## Other Modules

| Module | Purpose |
|--------|---------|
| `ScreenTimer` (`timer.h/cpp`) | Countdown timer with STOPPED/RUNNING/EXPIRED states |
| `UI` (`ui.h/cpp`) | Legacy drawing helpers, main screen layout |
| `Sound` (`sound.h/cpp`) | Beep tones for feedback |
| `config.h` | All constants: colors, layout, timing, API URLs |

---

## Main Loop (`main.cpp`)

```cpp
void loop() {
    M5.update();                    // Button state
    pollingManager.update();        // Background polling
    networkManager.update();        // WiFi keep-alive
    screenManager.update();         // Current screen logic
    screenManager.draw();           // Render
}
```

---

## File Structure

```
include/
├── app_state.h          # Centralized state singleton
├── api_client.h         # REST API client
├── config.h             # Constants
├── dialog.h             # Dialog overlay
├── header.h             # Header component
├── menu.h               # Dropdown menu
├── network.h            # WiFi manager
├── persistence.h        # NVS storage
├── polling_manager.h    # Background polling
├── screen.h             # Screen base class
├── screen_manager.h     # Screen orchestration
├── sound.h              # Audio feedback
├── timer.h              # Countdown timer
├── ui.h                 # Legacy UI helpers
└── screens/
    ├── login_screen.h
    ├── main_screen.h
    ├── select_child_screen.h
    └── sync_screen.h

src/
├── main.cpp             # Entry point, setup, loop
├── [matching .cpp files for each .h]
└── screens/
    └── [screen implementations]
```

---

## Key Patterns

- **Singleton**: `AppState::getInstance()`, `PersistenceManager::getInstance()`
- **Callbacks**: Menu actions, dialog buttons use `std::function` or static functions
- **Screen lifecycle**: Enter → Update/Draw loop → Pause/Resume → Exit
- **Overlay priority**: Dialog > Menu > Screen
- **RTC memory**: Timer state preserved across deep sleep (`RTC_DATA_ATTR`)

---

## See Also

- [ARCHITECTURE_PLAN.md](ARCHITECTURE_PLAN.md) - Detailed refactoring plan and requirements
