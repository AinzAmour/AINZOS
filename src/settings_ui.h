#ifndef SETTINGS_UI_H
#define SETTINGS_UI_H

#include "app_types.h"
#include "display.h"
#include "buttons.h"
#include "settings.h"

enum class SetPage {
  Menu,
  ConfirmReset,
  ClockSaverMenu
};

class SettingsUI {
public:
  SettingsUI(DisplayWrapper* disp, SettingsManager* sm);
  void enter();
  bool update(ButtonEvent btn); 

private:
  DisplayWrapper* display;
  SettingsManager* settings;
  SetPage currentPage;
  
  int menuSelectedIndex;
  int menuTopIndex;
  
  static const int SET_MENU_COUNT = 11;
  void drawMenu();
  void drawConfirm();
  void handleSelection();
  void drawClockSaverMenu();
  void handleClockSaverSelection();
};

#endif
