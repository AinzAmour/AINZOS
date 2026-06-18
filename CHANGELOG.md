# Changelog

All notable changes to the HIZMOS-C3 project will be documented in this file.

---

## [v2.2.1] - 2026-06-18 (IR Stability & NTC Calibration)

### Fixed
- **IR Transmitter Stability**: Removed duplicate `irBegin()` call from `IrUI::enter()` to prevent RMT peripheral channel re-initialization crashes.

### Added
- **NTC Thermistor Temperature Calibration**:
  - Implemented calibrated temperature readings via `analogReadMilliVolts()`.
  - Created customizable calibration macros (`NTC_NOMINAL`, `NTC_BETA`, `R_FIXED`) in `thermistor.h`.
  - Soft-calibrated the voltage divider with `R_FIXED 5827.0f` to output accurate 28.0°C ambient room temperature.
  - Added real-time `NTC mV` (millivolts) readout on the Diagnostics System Monitor screen.

---

## [v2.1.0] - 2026-06-14 (Lab Tools & BLE Spam Release)

### Added
- **Lab Tools Module**: Implemented a comprehensive Wi-Fi and BLE testing suite accessible via the main menu.
  - **BLE Spam**: Ported reference Flipper Zero attacks including Apple Continuity (AirPods pairing, Nearby Action, and Custom iOS 17 Crash), Microsoft SwiftPair, Samsung EasySetup, Google FastPair (genuine & custom popups), and Flipper Zero spam. Features random MAC rotation and advertising interval optimization.
  - **Wi-Fi Attacks**: Integrated offensive testing tools including Beacon Spam (with support for custom lists and Rickroll/AP modes), Deauth Attack (AP/Station targets), Channel Switch Announcement, DHCP Starvation, EAPOL Logoff, and GTK Abuse validation.
- **Dynamic Radio Coexistence Management**: Configured automated deinitialization of NimBLE and Wi-Fi modes on state change to prevent single-core hardware conflicts.

---

## [v1.0.1] - 2026-06-12 (Stage 5.1 Polish Build)

### Added
- **Chip Temperature Monitoring**: Safely integrated ESP32-C3 internal temperature sensing using a version-checked `safeTemperatureRead` wrapper. Supported cores display `Chip Temp: XX.X C` in the Diagnostics System Monitor and Apps Uptime Monitor. Unsupported systems print `Chip Temp: Unsupported` without breaking compilation.
- **Pong Controls**: Added short `SELECT` press to pause/resume gameplay and long `SELECT` press to instantly reset/restart the ball and paddles.
- **Detailed Checklist Documentation**: Added hardware wiring guides, resource specifications, and safety notices to the project's documentation.

### Fixed
- **Pong Game Over Text Visibility**: Resolved an issue where the Game Over screen text was invisible on the OLED display. Modified the renderer to explicitly clear the display, reset the text size to `1`, and set the color to `SSD1306_WHITE` before rendering coordinate-aligned labels.
- **OLED I2C Bus Flooding**: Optimized the Pong Game Over rendering loop with a 250ms rate limit to prevent CPU hogging and I2C saturation.

### Security & Stability
- Confirmed stable memory recovery on repeated asynchronous Wi-Fi scans via `WiFi.scanDelete()` (preventing heap leaks).
- Configured BLE discovery to clear raw results after scanning to ensure RAM stability.
- Verified that radio operations are kept strictly separated to avoid coexistence conflicts on the single-core processor.

---

## [v1.0.0] - 2026-06-11 (Stage 5 - Apps & Games Release)

### Added
- **Utility Apps Module**: Implemented Stopwatch, Up/Down Counter, Dice Roller (D6/D100), Random Number Generator, Uptime Monitor, and Wi-Fi Signal Meter.
- **Games Module**: Implemented classical grid-based **Snake** (utilizing a fixed-size circular coordinate buffer to prevent dynamic heap fragmentation) and **Pong** (VS AI paddle controls).
- Configured dynamic update rate speed scales based on the configuration settings.

---

## [v0.4.0] - 2026-06-09 (Stage 4 - BLE Scanner)

### Added
- **BLE Scanner Tools**: Implemented read-only passive BLE scanning using the lightweight `NimBLE` library to save flash and memory space.
- Added BLE RSSI sorting, active scanning for friendly name resolution, saved results view, and strongest device display.

---

## [v0.3.0] - 2026-06-08 (Stage 3 - Wi-Fi Scanner)

### Added
- **Wi-Fi Scanner Tools**: Developed asynchronous Wi-Fi scanning showing nearby AP SSID, channel, and RSSI, sorted from strongest signal to weakest.

---

## [v0.2.0] - 2026-06-05 (Stage 2 - Diagnostics & Settings)

### Added
- **Diagnostics**: Implemented I2C address scanner, real-time button state visualizer, memory monitor, and reset reason reader.
- **Settings**: Persistent configuration parameters (contrast, speed presets) loaded and saved to Non-Volatile Storage (NVS).

---

## [v0.1.0] - 2026-06-01 (Stage 1 - Scaffold & UI Framework)

### Added
- PlatformIO project initialization for the ESP32-C3 SuperMini.
- Added I2C OLED SSD1306 driver wrapper and button navigation logic.
- Main menu system layout with temporary fallback cards.
