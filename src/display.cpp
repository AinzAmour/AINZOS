#include "display.h"
#include "pins.h"

DisplayWrapper::DisplayWrapper() 
  : display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET) {
}

bool DisplayWrapper::begin() {
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    return false;
  }
  display.setRotation(2);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.display();
  return true;
}

void DisplayWrapper::clear() {
  display.clearDisplay();
}

void DisplayWrapper::drawHeader(const char* title) {
  display.setTextSize(1);
  display.fillRect(0, 0, SCREEN_WIDTH, 10, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(2, 1);
  display.print(title);
  display.setTextColor(SSD1306_WHITE);
}

void DisplayWrapper::drawMenu(const char* title, const char* items[], int itemCount, int selectedIndex, int topIndex) {
  clear();
  drawHeader(title);
  
  display.setTextSize(1);
  int maxVisible = 5; 
  int startY = 12;
  
  for (int i = 0; i < maxVisible; i++) {
    int itemIdx = topIndex + i;
    if (itemIdx >= itemCount) break;
    
    int y = startY + (i * 10);
    
    if (itemIdx == selectedIndex) {
      display.fillRect(0, y, SCREEN_WIDTH, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    
    display.setCursor(2, y + 1);
    display.print(items[itemIdx]);
  }
  display.display();
}

void DisplayWrapper::drawInfoPage(const char* title, const char* lines[], int lineCount) {
  clear();
  drawHeader(title);
  display.setTextSize(1);
  int startY = 12;
  for (int i=0; i<lineCount && i<5; i++) {
    display.setCursor(2, startY + (i*10));
    display.print(lines[i]);
  }
  display.display();
}

void DisplayWrapper::drawProgress(const char* title, int percent, const char* subtitle) {}
void DisplayWrapper::drawWarning(const char* title, const char* message) {}
void DisplayWrapper::drawToast(const char* message, unsigned long duration) {}

void DisplayWrapper::setContrast(uint8_t contrast) {
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(contrast);
}

void DisplayWrapper::drawStatusBar(const char* screenName, RadioState radioState) {
  display.fillRect(0, 0, SCREEN_WIDTH, 8, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);

  if (radioState == RadioState::RADIO_WIFI) {
    display.setCursor(2, 0);
    display.print(F("[~]"));
  } else if (radioState == RadioState::RADIO_BLE) {
    display.setCursor(2, 0);
    display.print(F("[B]"));
  }

  char truncName[10];
  strncpy(truncName, screenName, 9);
  truncName[9] = '\0';
  uint8_t len = strlen(truncName);
  uint8_t x = 64 - (len * 6) / 2;
  display.setCursor(x, 0);
  display.print(truncName);

  unsigned long uptimeSecs = millis() / 1000;
  unsigned long mins = uptimeSecs / 60;
  unsigned long secs = uptimeSecs % 60;
  char timeBuf[6];
  if (mins >= 100) {
    unsigned long hours = mins / 60;
    unsigned long remainingMins = mins % 60;
    snprintf(timeBuf, sizeof(timeBuf), "%02lu:%02lu", hours, remainingMins);
  } else {
    snprintf(timeBuf, sizeof(timeBuf), "%02lu:%02lu", mins, secs);
  }
  display.setCursor(98, 0);
  display.print(timeBuf);

  display.setTextColor(SSD1306_WHITE);
  display.drawFastHLine(0, 9, SCREEN_WIDTH, SSD1306_WHITE);
}

void DisplayWrapper::drawSignalBars(uint16_t x, uint16_t y, uint8_t bars) {
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t barH = 2 + (i * 2);
    uint16_t bx   = x + (i * 4);
    uint16_t by   = y + (8 - barH);
    bool filled  = (i < bars);
    if (filled)
      display.fillRect(bx, by, 3, barH, SSD1306_WHITE);
    else
      display.drawRect(bx, by, 3, barH, SSD1306_WHITE);
  }
}

