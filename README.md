# Screenie - Client Device Code
This is the code for the Screenie hardware device, intended to be run on an m5stickc plus2 device.

Screenie is a screen time tracker for children. It connects via WiFi to sync time and displays remaining daily screen time with a countdown timer and progress bar.

See the file 'copilot-instructions.md' for further information.

See: https://screenie.org/developers for more information

## What You Need
1. **VS Code** - Download from https://code.visualstudio.com/
2. **PlatformIO Extension** - Install from VS Code Extensions marketplace
3. **USB Driver** - CH9102 driver (see below )
4. **USB-C Cable** - Data cable, not charging-only!

---

## Step-by-Step Setup

### Step 1: Install VS Code
Download and install from https://code.visualstudio.com/

### Step 2: Install PlatformIO Extension
1. Open VS Code and install the PlatformIO IDE extension

### Step 3: Install USB Driver (CH9102)
The M5StickC Plus2 uses a CH9102 USB-to-serial chip.

- **Windows/Older MacOS versions**: Download from https://docs.m5stack.com/en/download
  - Look for "CP34X USB Driver" and install it
- **Linux**: Usually works out of the box, but you may need to add your user to the `dialout` group:
  ```bash
  sudo usermod -a -G dialout $USER
  ```
  Then log out and back in.

### Step 4: Open This Project
1. In VS Code, go to **File → Open Folder**
2. Select this folder
3. PlatformIO will automatically detect it and set things up

### Step 5: Connect Your M5StickC Plus2
1. Plug in the USB-C cable to your computer
2. If port isn't recognized, try this:
   - Unplug the USB cable
   - **Long-press the power button** until the green LED lights up (power off)
   - Reconnect the USB cable

### Step 6: Configure WiFi Credentials
Before building, you need to set up your WiFi credentials:

1. Copy `include/credentials.example.h` to `include/credentials.h`
2. Edit `include/credentials.h` and replace the placeholder values:
   ```cpp
   #define WIFI_SSID     "YourWiFiNetworkName"
   #define WIFI_PASSWORD "YourWiFiPassword"
   ```
3. The `credentials.h` file is gitignored, so your password won't be committed

**Note:** If you skip this step, the app will still run but display a warning about missing WiFi configuration.

### Step 7: Build and Upload
1. Click the **PlatformIO icon** in the left sidebar (alien head)
2. Under **PROJECT TASKS → pw.screenie.m5 → General**:
   - Click **Build** to compile (check mark icon)
   - Click **Upload** to flash to device (arrow icon)

### Step 8: Monitor Serial Output (Optional)
1. Click **Monitor** in PlatformIO tasks (or `Ctrl+Alt+M`)
2. You'll see the counter and button press messages

---

## Understanding the Code

### The Library: M5Unified
M5Unified is the modern, unified library for all M5Stack devices. Key benefits:
- **Auto-detection**: Same code works on StickC, Plus, Plus2, Core, etc.
- **Includes M5GFX**: Powerful graphics library built in
- **Everything unified**: Buttons, display, speaker, sensors all in one API


