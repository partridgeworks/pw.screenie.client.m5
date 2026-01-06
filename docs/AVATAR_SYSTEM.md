# Avatar System Documentation

## Overview

The screen time tracker displays personalized avatar images for each child user. Avatars are rendered as PNG images stored in the LittleFS filesystem and displayed in a circular avatar area on the main screen.

---

## Architecture

### Storage: LittleFS Filesystem

- **Technology**: LittleFS (configured in `platformio.ini`)
- **Location**: `/data/avatars/` directory
- **Format**: 50x50 pixel PNG files with transparent backgrounds
- **Naming**: `[avatarName].png` (e.g., `1F3B1_color.png`)

### Avatar Rendering Flow

```
Server sends avatarName → Stored in UserSession → Persisted to NVS
                                    ↓
                          MainScreen passes to UI
                                    ↓
                    UI::drawAvatar loads PNG from LittleFS
                                    ↓
                        PNG scaled and drawn in avatar circle
                                    ↓
                      Fallback to initial letter if PNG missing
```

---

## Implementation Details

### Data Model (`app_state.h`)

```cpp
struct UserSession {
    char selectedChildAvatarName[32];  // Avatar filename with extension
    // e.g., "1F3B1_color.png"
};
```

### Persistence (`persistence.h/cpp`)

Avatar name is saved/loaded using NVS key `"childAvatar"`:

```cpp
constexpr const char* KEY_CHILD_AVATAR = "childAvatar";
```

Automatically persisted when:
- Child is selected
- Session is saved
- User logs in

### UI Rendering (`ui.h/cpp`)

The `UI::drawAvatar()` function handles avatar rendering:

```cpp
void UI::drawAvatar(char initial, const char* avatarName, int x, int y)
```

**Behavior**:
1. If `avatarName` is provided and file exists → Load and draw PNG
2. PNG is scaled to fit the 32px diameter avatar circle (AVATAR_RADIUS * 2)
3. Background circle drawn with `COLOR_AVATAR_PRIMARY` before PNG (handles transparency)
4. If PNG fails to load → Fallback to drawing initial letter

**File Path Construction**:
```cpp
// Smart handling - works with or without .png extension
if (avatarName ends with ".png") {
    path = "/avatars/" + avatarName;           // e.g., /avatars/1F3B1_color.png
} else {
    path = "/avatars/" + avatarName + ".png";  // e.g., /avatars/1F3B1_color.png
}
```

### Main Screen Integration

`MainScreen::drawFullScreen()` retrieves avatar name from `AppState` and passes to UI:

```cpp
const UserSession& session = state.getSession();
_ui.drawMainScreen(
    _timer,
    state.getDisplayName(),
    state.getAvatarInitial(),
    session.selectedChildAvatarName,  // ← Avatar name
    _timerRunning,
    state.getNetworkStatus()
);
```

---

## Avatar File Specifications

### Image Requirements

| Property | Value |
|----------|-------|
| Format | PNG (with alpha channel support) |
| Dimensions | 50x50 pixels |
| Display size | Scaled to 32x32 (AVATAR_RADIUS * 2) |
| Background | Transparent or solid color |
| **Server sends**: `"1F3B1_color.png"` (with extension)
- **Also supports**: `"1F3B1_color"` (without extension, for backward compatibility)
- **Filesystem expects**: `/avatars/1F3B1_color.png`
- **Storage**: Full filename including extension stored in `UserSession`

The system intelligently handles both formats - if the `.png` extension is already present, it uses it as-is; otherwise, it appends `.png`.

### Current Avatar Files

See `data/avatars/` directory. Examples:
- `1F344_color.png` - Mushroom
- `1F3B1_color.png` - Pool 8 Ball
- `1F436_color.png` - Dog Face
- `1F680_color.png` - Rocket
- `26BD_color.png` - Soccer Ball
- etc.

---

## API Integration

### Expected Server Response

When selecting a child or fetching family members, the API should return:

```json
{
  "members": [
    {
      "userId": "child123",
      "name": "Alex",
      "position": "child",
      "avatarName": "1F3B1_color.png"
    }
  ]
}
```

**Note**: The system also supports `avatarName` without the `.png` extension for backward compatibility.

The `ApiClient` should populate `UserSession.selectedChildAvatarName` when parsing this data.

---

## Filesystem Management

### Uploading Avatars to Device

Use PlatformIO's filesystem upload feature:

```bash
~/.platformio/penv/bin/pio run -t uploadfs
```

This uploads the entire `data/` directory to the device's LittleFS partition.

### LittleFS Initialization

In `main.cpp`:

```cpp
if (!LittleFS.begin(true)) {  // true = format on fail
    Serial.println("[App] ERROR: LittleFS initialization failed");
    Serial.println("[App] Avatar images will not be available");
} else {
    Serial.println("[App] LittleFS initialized");
}
```

### Debugging Avatar Loading

Enable serial output to see avatar load status:

```
[UI] Drew PNG avatar: /avatars/1F3B1_color.png at (200,67)
```

Or if file not found:

```
[UI] Avatar PNG not found: /avatars/1F3B1_color.png
```

---

## Fallback Behavior

If PNG avatar cannot be loaded (file missing, filesystem error, etc.), the system automatically falls back to displaying the child's initial letter in the avatar circle.

**Fallback conditions**:
- `avatarName` is NULL or empty string
- Avatar file doesn't exist in `/avatars/`
- File open error
- PNG decode error

---

## Performance Considerations

### Memory

- PNG files are loaded on-demand (not cached in RAM)
- M5GFX handles PNG decoding efficiently
- 50x50 PNG files are small (~1-5KB each)

### Rendering

- PNGs are loaded and decoded each time main screen redraws
- For smooth performance, avoid frequent full screen redraws
- Use `updateDynamicElements()` when possible (doesn't redraw avatar)

---

## Adding New Avatars

1. Create 50x50 PNG image with transparent background
2. Name file using pattern: `[name]_color.png`
3. Copy to `data/avatars/` directory
4. Upload filesystem to device: `pio run -t uploadfs`
5. Update server API to return matching `avatarName` value

---

## Troubleshooting

### Avatar not displaying

**Check:**
1. LittleFS initialized successfully (check serial output)
2. Avatar file exists in `data/avatars/`
3. Filename matches exactly (case-sensitive)
4. Filesystem uploaded: `pio run -t uploadfs`
5. Avatar name stored in UserSession (use persistence debug print)

### Serial debugging

```cpp
Serial.printf("[UI] Avatar path: %s\n", avatarPath);
Serial.printf("[UI] File exists: %d\n", LittleFS.exists(avatarPath));
```

### List avatars on device

```cpp
File root = LittleFS.open("/avatars");
File file = root.openNextFile();
while (file) {
    Serial.printf("Avatar: %s\n", file.name());
    file = root.openNextFile();
}
```

---

## Future Enhancements

- **Avatar caching**: Cache decoded PNG in RAM for faster redraws
- **Dynamic download**: Fetch avatars from server on-demand
- **Custom avatars**: Allow users to upload custom avatar images
- **Avatar animations**: Support animated avatar formats
- **Compression**: Use more space-efficient formats if needed

---

## Related Files

| File | Purpose |
|------|---------|
| `include/app_state.h` | UserSession with avatarName field |
| `src/persistence.cpp` | Save/load avatar name to NVS |
| `include/ui.h` | Avatar rendering interface |
| `src/ui.cpp` | PNG loading and drawing implementation |
| `src/screens/main_screen.cpp` | Passes avatar name to UI |
| `src/main.cpp` | LittleFS initialization |
| `platformio.ini` | Filesystem configuration |
| `data/avatars/` | Avatar PNG files |

---

## References

- [M5GFX Documentation](https://github.com/m5stack/M5GFX)
- [LittleFS Arduino](https://github.com/lorol/LITTLEFS)
- [PNG File Format](https://en.wikipedia.org/wiki/PNG)
