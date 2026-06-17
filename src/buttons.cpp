#include "buttons.h"

Buttons::Buttons() {
  pins[0] = BUTTON_UP_PIN;
  pins[1] = BUTTON_DOWN_PIN;
  pins[2] = BUTTON_SEL_PIN;
  pins[3] = BUTTON_BACK_PIN;

  eventMap[0] = BTN_UP;
  eventMap[1] = BTN_DOWN;
  eventMap[2] = BTN_SELECT;
  eventMap[3] = BTN_BACK;

  longEventMap[0] = BTN_UP_LONG;
  longEventMap[1] = BTN_DOWN_LONG;
  longEventMap[2] = BTN_SELECT_LONG;
  longEventMap[3] = BTN_BACK_LONG;

  for (int i = 0; i < 4; i++) {
    isPressed[i] = false;
    lastFlickerState[i] = false;
    lastDebounceTime[i] = 0;
    pressStartTime[i] = 0;
    
    isRepeating[i] = false;
    longPressFired[i] = false;
    lastRepeatTime[i] = 0;
  }
}

void Buttons::begin() {
  for (int i = 0; i < 4; i++) {
    pinMode(pins[i], INPUT_PULLUP);
  }
}

ButtonEvent Buttons::read() {
  unsigned long now = millis();
  
  for (int i = 0; i < 4; i++) {
    bool currentReading = (digitalRead(pins[i]) == LOW);
    
    // Robust debounce logic
    if (currentReading != lastFlickerState[i]) {
      lastDebounceTime[i] = now;
      lastFlickerState[i] = currentReading;
    }
    
    if ((now - lastDebounceTime[i]) > DEBOUNCE_MS) {
      if (currentReading != isPressed[i]) {
        // Confirmed state change (edge)
        isPressed[i] = currentReading;
        
        if (isPressed[i]) {
          // Just pressed down
          isRepeating[i] = false;
          longPressFired[i] = false;
          pressStartTime[i] = now;
          return eventMap[i]; // Fire single event exactly once on press
        }
      }
    }
    
    // Continuous hold conditions
    if (isPressed[i]) {
      unsigned long holdTime = now - pressStartTime[i];
      
      // Auto-repeat (only UP and DOWN)
      if (i == 0 || i == 1) {
        if (!isRepeating[i] && holdTime >= REPEAT_START_MS) {
          isRepeating[i] = true;
          lastRepeatTime[i] = now;
          return eventMap[i];
        } else if (isRepeating[i] && (now - lastRepeatTime[i] >= REPEAT_INTERVAL_MS)) {
          lastRepeatTime[i] = now;
          return eventMap[i];
        }
      }
      
      // Long press (all buttons)
      if (!longPressFired[i] && holdTime >= LONG_PRESS_MS) {
        longPressFired[i] = true;
        return longEventMap[i];
      }
    }
  }
  
  return BTN_NONE;
}
