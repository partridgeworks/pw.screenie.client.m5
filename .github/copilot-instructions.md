This is the Screenie client application, an embedded application running on an M5StickC-Plus 2 device which is an ESP32-S3 device.

# Purpose
Screenie is a screen time tracker for children. It connects via WiFi to sync time and displays remaining daily screen time with a countdown timer and progress bar.  See https://screenie.org for more information.

# Data Trust Model
The app uses a client/server architecture, but with clear separation of data trust boundaries:

- Screenie web server: Acts as the Single Source of truth for screen time allowances (one way sync to screenie devices on request)

- Screenie device (only ever ONE device per child in a family) - Acts as the Single Source of truth for screen time consumption.  (one way sync to web server, but not guaranteed to happen)


# Platform and Build System
The application uses PlatformIO to manage libraries and to compile the code. See the README.md for setup instructions.

# Hardware
The application runs on an M5StickC-Plus 2 device. M5Unified auto-detects the board type.

## Buttons
There are three buttons (assuming holding device in landscape, rotated 180°):
- **Button A (BtnA)** - Front button on the face of the device
- **Button B (BtnB)** - Side button at the top of the device
- **Button C / Power (BtnPWR)** - Button on the bottom, doubles as power button

### Button API Notes
- Use `M5.BtnA.wasClicked()` for short clicks, `M5.BtnA.wasHold()` for long press
- BtnPWR only supports `wasClicked()` and `wasHold()` (not wasPressed/wasReleased)
- Must call `M5.update()` every loop iteration for button detection

## Other hardware
- SoC: ESP32-PICO-V3-02@Dual-Core Processor, up to 240MHz
- Flash: 8MB
- PSRAM: 2MB
- Wi-Fi: 2.4 GHz Wi-Fi
- LCD Screen: 1.14-inch, 135 x 240 Colorful TFT LCD, ST7789v2 (landscape: 240x135)
- RTC: BM8563
- Buzzer: Onboard Buzzer
- IMU: MPU6886
- Battery: 200mAh@3.7V

# Purpose
Screen time tracker for children. Connects via WiFi for NTP time sync, displays remaining daily screen time with countdown timer and progress bar.

# Building and Uploading
Uses PlatformIO. Build with `~/.platformio/penv/bin/pio run`, upload with `~/.platformio/penv/bin/pio run -t upload`.

---

# Best Practices

## M5Unified / M5GFX
- Use `M5.begin(cfg)` with auto-detection, not device-specific wrappers
- Use `M5.delay()` instead of `delay()` for proper timing
- Check landscape mode: `if (M5.Display.width() < M5.Display.height()) { M5.Display.setRotation(M5.Display.getRotation() ^ 1); }`

## Display Drawing (M5GFX)
- Batch drawing operations with `_display.startWrite()` / `_display.endWrite()`
- Call `_display.waitDisplay()` before drawing to prevent tearing
- Call `_display.display()` after drawing to flush buffer to screen
- Use `fillRoundRect()` for modern UI elements

## Button Handling
- Prefer `wasClicked()` over `wasPressed()`/`wasReleased()` for reliable detection
- Use `wasHold()` for long-press actions (built-in timing)
- Access power button via `M5.BtnPWR` (limited methods available)

## Screen Time Tracking Model
- Uses **consumed time model**: tracks time used, not time remaining
- `_consumedTodaySeconds` - total time consumed in completed sessions today
- `_sessionStartTime` - Unix timestamp when current session started
- Remaining time = allowance - (consumedToday + currentSessionElapsed)
- Consumed time is persisted to NVS every 60 seconds while timer runs
- On day change, consumed time is reset to 0

## Deep Sleep
- Uses ESP32 deep sleep with RTC memory to preserve timer state
- `tryGoToSleep()` validates conditions before sleeping
- Refuses sleep if timer is active AND < 10 minutes remaining (shows error dialog)
- Wake sources: Button A (EXT0) and timer (wakes 10 minutes before expiry)
- State stored in `RTC_DATA_ATTR` variables: consumed time, session start time, timer state
- `checkAndRestoreFromSleep()` in setup() restores state on wake
- Running sessions continue across sleep (session start time preserved)
- Auto-sleep triggers after `AUTO_SLEEP_DURATION_SECS` (180s) of inactivity
- Power button long-hold calls `M5.Power.powerOff()` (battery only)
- Must hold GPIO 4 during sleep to keep power mosfet on

---

# Code Structure

> **See [docs/ARCHITECTURE_README.md](../docs/ARCHITECTURE_README.md) for full architecture overview.**

## Key Modules

| Module | Purpose |
|--------|---------|
| `ScreenManager` | Screen lifecycle, transitions, button routing |
| `AppState` | Singleton with session, timer data, network status |
| `PersistenceManager` | ESP32 NVS storage for session/settings |
| `ApiClient` | REST API client (mock implementations) |
| `PollingManager` | Non-blocking login/more-time polling |
| `NetworkManager` | WiFi with auto-connect and keep-alive |
| `SessionManager` | Session and timer management |

## Screens

| Screen | Purpose |
|--------|---------|
| `LoginScreen` | QR code + numeric code for device pairing |
| `SelectChildScreen` | Choose which child is using device |
| `MainScreen` | Timer display, progress bar, main UI |
| `SyncScreen` | Spinner during network operations |

## File Organization

```
include/
├── config.h      # Constants, colors, layout values, timing
├── screen.h      # Screen base class
├── screen_manager.h
├── app_state.h
├── persistence.h
├── api_client.h
├── polling_manager.h
├── network.h
├── dialog.h
├── menu.h
├── timer.h
├── ui.h
├── sound.h
└── screens/
    ├── login_screen.h
    ├── main_screen.h
    ├── select_child_screen.h
    └── sync_screen.h

src/
├── main.cpp      # Entry point, setup, loop
├── [matching .cpp for each .h above]
└── screens/
    └── [screen implementations]
```

## Main Loop Pattern

```cpp
void loop() {
    M5.update();                    // Button state
    pollingManager.update();        // Background polling
    networkManager.update();        // WiFi keep-alive
    screenManager.update();         // Current screen logic
    screenManager.draw();           // Render
}
```

## Button Routing

```
Button Press → ScreenManager
  → Dialog visible? → Route to Dialog
  → Menu open? → Route to Menu  
  → Route to current Screen
```

## Button Mapping (Current)

| Button | Menu Open | Menu Closed | Function |
|--------|-----------|-------------|----------|
| A click | Activate selected item | Start/stop timer | `handleButtonA()` |
| B click | Cycle menu items | Open menu | `handleButtonB()` |
| C (PWR) click | Close menu (back) | No action | `handlePowerButton()` |
| C (PWR) hold | Power off | Power off | `handlePowerButton()` |


## API Calling
Endpoints MUST not have a trailing slash. Use templates defined in `api_client.h` for URL paths.