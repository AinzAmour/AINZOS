# Hardware Regression Testing Checklist

Follow this checklist to perform a complete manual regression verification of the AINZOS-C3 firmware on physical hardware.

---

## 🖥️ 1. OLED Boot & Display Check
- [ ] Connect the ESP32-C3 to power via USB-C.
- [ ] Verify that the OLED display initializes immediately.
- [ ] Verify that the splash screen loads, displaying:
  * `AINZOS-C3` in text size 2.
  * `v1.0.1` in text size 1.
- [ ] Verify that the screen transitions to the Main Menu after ~2 seconds (or immediately if any button is pressed).

---

## 🕹️ 2. Button Navigation & Main Menu
- [ ] Test the **DOWN** button: Highlights the next menu item and scrolls the list down correctly.
- [ ] Test the **UP** button: Highlights the previous menu item and scrolls the list up correctly.
- [ ] Test that the selection wraps/scrolls smoothly without list items running off screen boundaries.
- [ ] Verify that pressing **SELECT** opens the highlighted submenu.
- [ ] Verify that pressing **BACK** from any submenu returns you immediately to the main menu.

---

## ⚙️ 3. Settings Persistence (NVS)
- [ ] Open the **Settings** menu.
- [ ] Select **Contrast** and adjust the value (e.g., lower or raise contrast). Confirm the OLED brightness shifts in real-time.
- [ ] Press **SELECT** to save the value to Non-Volatile Storage (NVS).
- [ ] Perform a hardware reset or power cycle the device. Confirm that the custom contrast level is loaded and applied at startup.
- [ ] Select **Factory Reset** from the Settings menu. Confirm that settings revert to defaults and the screen contrast resets to 128.

---

## 📊 4. Diagnostics Suite
- [ ] Open **Diagnostics** submenu.
- [ ] **I2C Scanner**: Confirm it scans and successfully lists the SSD1306 display address at `0x3C (OLED SSD1306)`.
- [ ] **Button Test**: Press each button (UP, DOWN, SELECT, BACK) and check that:
  * Raw states toggle from `0` to `1` (Active Low detection).
  * Last captured event prints correctly (e.g. `UP`, `SELECT_LONG`).
- [ ] **System Monitor**: Check that it displays uptime counting in seconds, Free Heap, CPU speed, and `Chip Temp: XX.X C` (representing internal die temperature).
- [ ] **OLED Test**: Verify that the diagnostic pattern fills the screen (corners, borders, diagonals) without garbage pixels.
- [ ] **Memory Info**: Verify that it displays RAM and Flash allocation numbers without overflowing.
- [ ] **Reset Reason**: Perform a hardware reset and software reset (via Settings Factory Reset), checking that the reset reasons align with the triggers (e.g., `Power On` or `Software Reset`).

---

## 📶 5. Wi-Fi Scanner (Repeated Tests)
- [ ] Open **Wi-Fi Tools ➡️ Scan Wi-Fi APs**.
- [ ] Verify that it displays `Scanning...` and returns nearby access points sorted by strongest RSSI first.
- [ ] Run the scan repeatedly (5+ times).
- [ ] Connect to the Serial Monitor via `pio device monitor` and verify that the heap memory stabilizes and does not leak after repeated scans (releasing memory via `WiFi.scanDelete()`).

---

## 📶 6. BLE Scanner (Repeated Tests)
- [ ] Open **BLE Tools ➡️ Scan BLE Devices**.
- [ ] Verify that it displays nearby BLE devices with RSSI levels and MAC addresses.
- [ ] Confirm that devices with local names are resolved, while others show as `Unknown`.
- [ ] Run the scan repeatedly. Check that the console monitor shows heap memory stabilizing (no dynamic memory build-up or memory leak crashes).

---

## 📱 7. Utility Apps
- [ ] Open **Apps** menu.
- [ ] **Stopwatch**: Press `SELECT` to start. Verify it counts milliseconds, seconds, and minutes. Press `SELECT` to pause, and hold `SELECT` (long press) to reset to `00:00.000`.
- [ ] **Counter**: Press `UP` to increment and `DOWN` to decrement. Press `SELECT` to reset to `0`.
- [ ] **Dice Roller**: Press `SELECT` to roll a standard 6-sided die (D6). Hold `SELECT` to roll a 100-sided die (D100).
- [ ] **Random Number**: Press `UP`/`DOWN` to change the maximum bounds (by powers of 10) and `SELECT` to generate a random number.
- [ ] **Uptime Monitor**: Verify it runs continuously, tracking system uptime and showing `Chip Temp: XX.X C` (updated 4 times per second).
- [ ] **Signal Meter**: Ensure a Wi-Fi scan was run first. Verify it shows the SSID and RSSI in dBm of the strongest local access point.

---

## 🐍 8. Snake Game
- [ ] Open **Games ➡️ Snake**.
- [ ] Verify that the game initializes with a snake length of 3 and food spawned on the screen grid.
- [ ] Test navigation buttons (`UP` turns left, `DOWN` turns right relative to heading).
- [ ] Eat food and check that the length grows by 1 segment (max 60) and the score increments.
- [ ] Intentionally crash into a wall or your own tail. Confirm that `GAME OVER` displays and pressing `SELECT` restarts the game.

---

## 🏓 9. Pong Game (Pause / Restart / Reset)
- [ ] Open **Games ➡️ Pong**.
- [ ] Verify the ball bounces off the top/bottom walls and the player's/AI's paddles.
- [ ] Press `SELECT` during gameplay: Confirm that the game pauses, displaying a centered `PAUSED` window, and stops ball movement. Press `SELECT` again to resume.
- [ ] Hold `SELECT` (long press) during gameplay: Confirm that it resets paddles and ball position immediately.
- [ ] Intentionally let the ball pass your paddle. Check that the screen clears and shows the Game Over screen:
  * `GAME OVER` at the top.
  * `Score: X` matching your current score.
  * `SEL Restart` instruction.
  * `BACK Exit` instruction.
- [ ] Press `SELECT` on Game Over: Confirm it restarts the game from the beginning with score `0`.
- [ ] Press `BACK` on Game Over: Confirm it exits back to the Games menu.
