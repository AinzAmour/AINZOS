# HIZMOS-C3 Mini Tool

**Current Version**: `v2.2.1`

Firmware for the ESP32-C3 SuperMini featuring a 0.96" I2C OLED display, 4 tactile navigation buttons, a hardware-driven IR remote blaster, and pins allocated for an NRF24L01 module.

---

## 📌 Stable Hardware Config

- **Microcontroller**: ESP32-C3 SuperMini (160MHz CPU, 320KB SRAM, 4MB flash).
- **Display**: SSD1306 0.96" I2C OLED (128x64 pixels).
- **Interface**: 4 Navigation Buttons (Active Low, utilizing internal pull-ups).
- **Transmitter**: IR LED on GPIO 7.
- **Power**: 3.3V or 5V (USB-C powered).

### Wiring Map

#### 1. OLED Display
* **OLED SDA** ➡️ **GPIO 4**
* **OLED SCL** ➡️ **GPIO 5**
* **OLED VCC** ➡️ **3V3**
* **OLED GND** ➡️ **GND**

#### 2. Navigation Buttons
* **Button UP** ➡️ **GPIO 0** (GND when pressed)
* **Button DOWN** ➡️ **GPIO 1** (GND when pressed)
* **Button SELECT** ➡️ **GPIO 3** (GND when pressed)
* **Button BACK** ➡️ **GPIO 10** (GND when pressed)

#### 3. IR Transmitter
* **IR Transmit** ➡️ **GPIO 7** (Drives NPN transistor base)

#### 4. 2x4 RF/GPIO Expansion Header
A physical 2x4 pin header footprint intended for general GPIO use or future RF modules (like the NRF24L01 via an adapter).
> [!WARNING]
> **GPIO 8 and GPIO 9 are boot-sensitive (strapping pins)** on the ESP32-C3. They must not be pulled LOW or externally driven during boot, or the device will fail to start.

* **Row 1** ➡️ **GND** / **3V3**
* **Row 2** ➡️ **GPIO 2** / **GPIO 6**
* **Row 3** ➡️ **GPIO 20** / **GPIO 21**
* **Row 4** ➡️ **GPIO 8** / **GPIO 9**

### Recommended IR Transmitter Driver Circuit
```text
  IR GPIO (GPIO 7) ───[ 1kΩ Resistor ]─── Base (2N2222 / S8050 NPN Transistor)
  VCC (5V) ───[ 33Ω or 47Ω Resistor ]─── [ 2 IR LEDs in Series ] ─── Collector
  Transistor Emitter ─── GND (Common GND with ESP32)
```

*Note: The I2C bus is placed on GPIO 4/5 instead of 8/9 because 8 and 9 are bootstrapping pins on the ESP32-C3. Keeping them free avoids startup boot issues.*

---

## 💾 Resource Usage (Current Build)

- **RAM**: `16.4%` (53,684 bytes used out of 327,680 bytes)
- **Flash**: `35.2%` (1,085,106 bytes used out of 3,080,192 bytes, custom 2.9MB app partition enabled)

---

## 🚫 Disabled/Excluded Features

> [!NOTE]  
> To keep the flash footprint light and preserve stability, the following hardware modules are **disabled/not included** in this build:
> - **RGB (WS2812/SK6812) Support**: Disabled.
> - **IR (Infrared) Receiver**: Disabled (transmitting is supported).

---

## 📋 Completed Feature Checklist

- [x] **Core System**
  - [x] Asynchronous boot screen and menu state machine
  - [x] Multitasking-friendly button events (short press, long press)
  - [x] Persistent Settings saved to Non-Volatile Storage (NVS)
  - [x] Dynamic contrast adjust (discrete presetted cycle: 63 ➡️ 127 ➡️ 191 ➡️ 255 ➡️ 0) and Factory Reset
- [x] **Diagnostics Suite**
  - [x] I2C Scanner (detects devices from `0x08` to `0x77` with named address lookup)
  - [x] Button Test (visual display of raw pin levels and events)
  - [x] System Monitor (scrollable pager tracking uptime, heap, CPU, MACs, reset reason)
  - [x] OLED Pixel Test pattern generator
- [x] **Wi-Fi Scanner**
  - [x] Asynchronous passive scanning sorted by RSSI with SSID sanitization
  - [x] AP details card with OUI mac-to-vendor lookup
  - [x] Passive Probe Request sniffer with channel hopping and safety length checks
- [x] **BLE Scanner**
  - [x] Read-only BLE scan using lightweight `NimBLE`
  - [x] Sorting by RSSI, active scanning for friendly name resolution, suspect tag highlights
- [x] **Lab Tools (Wireless & BLE Testing)**
  - [x] **BLE Spam**: Apple Continuity (AirPods/Beats Proximity, AppleTV AutoFill/Connecting/Audio Sync/Color Balance, Setup New iPhone, and Apple Vision Pro; custom iOS 17 lockup crash simulation), Microsoft SwiftPair (generates random device names), Samsung EasySetup (Buds & Watch), Google FastPair (genuine and custom popups like Flipper Zero, Free Robux, Rickroll, Tesla, FBI), Flipper Zero, and Random mixed mode.
  - [x] **Wi-Fi Attacks**:
    - [x] *Beacon Spam*: SSID broadcasting (with support for custom SSIDs/lists, Rickroll mode, AP List mode).
    - [x] *Deauth Attack*: Disconnecting stations from target APs or broadcasting deauth packets.
    - [x] *Channel Switch*: Sending channel switch announcement frames.
    - [x] *DHCP Starvation*: Flooding DHCP discover requests to exhaust IP address pools.
    - [x] *EAPOL Logoff*: Flooding EAPOL logoff frames to force disconnection.
    - [x] *GTK Abuse*: Injecting packets to validate Group Temporal Key and test client isolation.
- [x] **IR Remote (Transmitter)**
  - [x] Universal Power/Mute/OK blasters (TV-B-Gone style)
  - [x] Universal Category blasters (Set-top Boxes, Projectors, Soundbars, AC Units)
  - [x] Virtual Brand Remotes (Samsung, LG, Sony, Panasonic, Philips) with Power/Mute/OK/Vol/Directions keys
  - [x] Compact AC Power ON/OFF blasters
- [x] **Utility Apps**
  - [x] Wi-Fi Channel Occupancy bar chart analyzer
  - [x] BLE Packet Inspector paged hex viewer
  - [x] Device Identity specs card (hardware summary)
  - [x] Stopwatch (pause/resume, long-press reset)
  - [x] Signal Meter (displays signal of strongest nearby Wi-Fi AP)
- [x] **Games**
  - [x] **Snake**: Score tracking, relative turning controls (UP to turn left, DOWN to turn right), body segment growth (max 60), wall/tail collision
  - [x] **Pong**: VS AI match, paddle controls, pause/resume, long-press reset, Game Over menu
  
---

## 🛠️ Build and Upload Commands

Ensure you have [PlatformIO](https://platformio.org/) installed.

```bash
# Clean build artifacts
pio run -t clean

# Compile code
pio run

# Flash to device
pio run -t upload

# Open serial log
pio device monitor
```

---

## 🔍 BLE & Temperature Implementation Details

- **BLE Unknown Names**: Many BLE devices broadcast advertising packets without local names to conserve power. They appear as `Unknown`. Active scanning is enabled to request scan responses containing names, but if omitted by the device, it remains `Unknown`.
- **ESP32-C3 Chip Temperature**: The System and Uptime monitors display internal silicon die temperature. It is **not** an ambient room temperature sensor and is subject to self-heating.

---

## 🛡️ Safety Note: Authorized Testing Only

This firmware is designed solely for personal diagnostics and authorized penetration testing on networks/hardware you own or have explicit written consent to test. Unauthorized auditing may violate computer misuse and wiretapping regulations.

## 🎮 Navigation & Controls

The firmware supports an optimized **4-button configuration** (UP, DOWN, SELECT, BACK) for quick, complete navigation across all menus:

### 1. General Menu Navigation
- **UP / DOWN**: Scroll through menu items.
- **SELECT (Short Press)**: Enter submenus or confirm selections.
- **BACK (Short Press)**: Return to the previous screen (via Navigation Stack).

### 2. List & Pager Pagination
In lists and system pagers—including **System Monitor**, **Wi-Fi Scan Results**, **Probe Monitor**, **BLE Scan Results**, and **BLE Packet Inspector**:
- **UP_LONG / DOWN_LONG** (Long press of UP or DOWN): Page-level scroll. Jumps directly to the previous or next page for rapid traversal of long lists.

### 3. Game Controls
- **Snake**: Uses **relative turning controls** (UP turns left relative to the snake's current heading, DOWN turns right relative to the heading).
- **Pong**:
  - **UP / DOWN**: Move your paddle up and down.
  - **SELECT (Short Press)**: Pause/resume the game.
  - **SELECT (Long Press)**: Reset the match instantly.

### 4. Button Tester (Diagnostics)
- Visualizes raw GPIO levels and events for all 4 buttons (UP, DOWN, SELECT, BACK).
- **Exit Note**: Since a short press of the BACK button is captured as an active test event, you must **long-press BACK** (`BACK_LONG`) to exit the Button Tester.

---

## 🧪 Lab Tools Usage Guide

The **Lab Tools** menu provides deep testing capabilities. Below are details on key workflows:

### 1. Wi-Fi Beacon Spam
- **Random Mode**: Broadcasts random SSIDs if no target/custom list is configured.
- **Beacon List Mode**: Broadcasts a specific list of SSIDs.
- **Rickroll Mode**: Broadcasts the lyrics of Rick Astley's famous song.

### 2. GTK Abuse & Password Flow
- Initiating a **GTK Abuse** test requires inputting an SSID and password.
- Selecting GTK Abuse launches the **Text Input UI**:
  - **UP / DOWN (Short Press)**: Cycle current character forward or backward.
  - **UP / DOWN (Long Press)**: Move cursor left or right.
  - **SELECT (Short Press)**: Insert default character or advance cursor to the right.
  - **SELECT (Long Press)**: Finish/Submit the text input.
  - **BACK (Short Press)**: Backspace (delete character before cursor).
  - **BACK (Long Press)**: Cancel the input completely.

### 3. BLE Spam
- Select one of the targeting options (Apple, Microsoft, Samsung, Google, Flipper Zero, or Random Mixed).
- Once running, the firmware spams advertising packets. Press **SELECT** at any time on the status page to halt the attack.

---

## Changelog

### v2.2.1 (2026-06-18)
- **IR Transmitter Driver Fix:** Fixed potential crash by removing duplicate `irBegin()` (RMT driver re-initialization) from `IrUI::enter()`. The transmitter is now initialized once globally at boot time for maximum channel reliability.
- **NTC Thermistor Temperature Calibration:**
  - Upgraded thermistor readings to use calibrated `analogReadMilliVolts()` utilizing internal ESP32 factory calibration.
  - Refactored `thermistor.h` to introduce customizable macro constants (`NTC_NOMINAL`, `NTC_BETA`, `R_FIXED`).
  - Added software calibration for the NTC divider (using calibrated fixed resistor value `R_FIXED 5827.0f`) to yield precise 28.0°C ambient room temperatures.
  - Added live `NTC mV` (millivolts) readout to the **Diagnostics → System Monitor** screen to assist with future hardware analysis.

### v2.1.0 (2026-06-14 & 2026-06-15 updates)
- **4-Button Refactor:** Refactored the core input engine to use a 4-button configuration (UP, DOWN, SELECT, BACK), freeing up GPIO 2 and 6.
- **Expansion Header Conversion:** Converted the dedicated NRF24 footprint into a generic 2x4 RF/GPIO expansion header (GND, 3V3, GPIO 2, 6, 20, 21, 8, 9). Removed GPIO 11 references as it is not exposed on the edge pads.
- **Long-Press Pager Pagination:** Configured pagers across all features (Diagnostics, BLE Scanner, Wi-Fi Scanner, Utility Apps) to use long presses on UP (`BTN_UP_LONG`) and DOWN (`BTN_DOWN_LONG`) for page-level scrolling.
- **Text Input UI Refactor:** Adapted `TextInputUI` to work with 4 buttons: UP/DOWN cycle characters, long-press UP/DOWN move the cursor, short SELECT advances/appends, long-press SELECT submits, and BACK performs backspace/cancel.
- **Reverted Snake Controls:** Returned Snake to relative turning controls (UP turns left, DOWN turns right) to suit the 4-button layout.
- **Diagnostics Button Tester:** Reverted UI layout in `Diagnostics::drawButtonTest()` to display state for UP (0), DOWN (1), SELECT (3), and BACK (10) only.
- **Lab Tools:** Added a dedicated `Lab Tools` menu with a `LabUI` module exposing BLE and WiFi testing tools (Lab Tools → BLE / WiFi Attacks).
- **BLE Spam:** New BLE submenu items (Apple, Microsoft, Samsung, Google, Flipper Zero, Random) wired to `ble_spam_start(BLE_SPAM_*)` and `ble_spam_stop()`; status card and single-key stop behavior added.
- **WiFi Attacks:** Ported beacon-spam and GTK-abuse UI patterns. `Beacon Spam` can now run without a selected AP (random SSIDs) and a new `Beacon List` mode was added. `GTK Abuse` now requires SSID+password and prompts for a password via the new Text Input UI.
- **Build:** Project builds successfully after these changes (verified 2026-06-15).
