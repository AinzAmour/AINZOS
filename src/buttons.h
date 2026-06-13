#ifndef BUTTONS_H
#define BUTTONS_H

#include <Arduino.h>
#include "app_types.h"
#include "pins.h"

class Buttons {
public:
  Buttons();
  void begin();
  ButtonEvent read();

private:
  int pins[4];
  ButtonEvent eventMap[4];
  ButtonEvent longEventMap[4];
  
  bool isPressed[4];
  bool lastFlickerState[4];
  unsigned long lastDebounceTime[4];
  unsigned long pressStartTime[4];
  
  unsigned long lastRepeatTime[4];
  bool isRepeating[4];
  bool longPressFired[4];

  static const uint16_t DEBOUNCE_MS = 50;
  static const uint16_t LONG_PRESS_MS = 600;
  static const uint16_t REPEAT_START_MS = 450;
  static const uint16_t REPEAT_INTERVAL_MS = 180;
};

#endif
