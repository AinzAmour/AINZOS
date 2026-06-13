#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#include "app_types.h"

class DisplayWrapper {
public:
  DisplayWrapper();
  bool begin();
  void clear();
  void drawHeader(const char* title);
  void drawStatusBar(const char* screenName, RadioState radioState);
  void drawSignalBars(uint16_t x, uint16_t y, uint8_t bars);
  void drawMenu(const char* title, const char* items[], int itemCount, int selectedIndex, int topIndex);
  void drawProgress(const char* title, int percent, const char* subtitle);
  void drawInfoPage(const char* title, const char* lines[], int lineCount);
  void drawWarning(const char* title, const char* message);
  void drawToast(const char* message, unsigned long duration);
  void setContrast(uint8_t contrast);
  
  Adafruit_SSD1306* getDriver() { return &display; }
  
private:
  Adafruit_SSD1306 display;
};

#endif
