# HIZMOS-C3 Mini Tool

**Current Version**: `v2.0.3`

Firmware for the ESP32-C3 SuperMini featuring a 0.96" I2C OLED display, 4 tactile navigation buttons, and a hardware-driven IR remote blaster.

---

## 📌 Stable Hardware Config

- **Microcontroller**: ESP32-C3 SuperMini (160MHz CPU, 320KB SRAM, 4MB flash).
- **Display**: SSD1306 0.96" I2C OLED (128x64 pixels).
- **Interface**: 4 Navigation Buttons (Active Low, utilizing internal pull-ups).
- **Transmitter**: IR LED on GPIO 7.
- **Power**: 3.3V or 5V (USB-C powered).

### Wiring Map
* **OLED SDA** ➡️ **GPIO 4**
* **OLED SCL** ➡️ **GPIO 5**
* **OLED VCC** ➡️ **3V3**
* **OLED GND** ➡️ **GND**
* **Button UP** ➡️ **GPIO 0** (GND when pressed)
* **Button DOWN** ➡️ **GPIO 1** (GND when pressed)
* **Button SELECT** ➡️ **GPIO 3** (GND when pressed)
* **Button BACK** ➡️ **GPIO 10** (GND when pressed)
* **IR Transmit** ➡️ **GPIO 7** (Drives NPN transistor base)

### Recommended IR Transmitter Driver Circuit
```text
  IR GPIO (GPIO 7) ───[ 1kΩ Resistor ]─── Base (2N2222 / S8050 NPN Transistor)
  VCC (5V) ───[ 33Ω or 47Ω Resistor ]─── [ 2 IR LEDs in Series ] ─── Collector
  Transistor Emitter ─── GND (Common GND with ESP32)
```

*Note: The I2C bus is placed on GPIO 4/5 instead of 8/9 because 8 and 9 are bootstrapping pins on the ESP32-C3. Keeping them free avoids startup boot issues.*

---

## 💾 Resource Usage (Current Build)

- **RAM**: `13.7%` (45,012 bytes used out of 327,680 bytes)
- **Flash**: `32.0%` (984,954 bytes used out of 3,080,192 bytes, custom 2.9MB app partition enabled)

---

## 🚫 Disabled/Excluded Features

> [!NOTE]  
> To keep the flash footprint light and preserve stability, the following hardware modules and penetration testing tools are **disabled/not included** in this build:
> - **RGB (WS2812/SK6812) Support**: Disabled.
> - **IR (Infrared) Receiver**: Disabled (transmitting is supported).
> - **Lab Tools (Deauther, beacon spam, packet injection, Wi-Fi attacks)**: Disabled.

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
  - [x] **Snake**: Score tracking, body segment growth (max 60), wall/tail collision
  - [x] **Pong**: VS AI match, pause/resume, long-press reset, Game Over menu
  
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
