#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include "app_types.h"
#include "display.h"
#include "buttons.h"
#include "shared_pager.h"

enum class DiagPage {
  Menu,
  I2CScanner,
  ButtonTest,
  SysMonitor,
  OledTest,
  ResetReason
};

class Diagnostics {
public:
  Diagnostics(DisplayWrapper* disp);
  void enter();
  bool update(ButtonEvent btn); 

private:
  DisplayWrapper* display;
  DiagPage currentPage;
  
  int menuSelectedIndex;
  int menuTopIndex;
  
  Pager sysMonitorPager;
  
  static const int DIAG_MENU_COUNT = 5;
  const char* diagMenuItems[5] = {
    "I2C Scanner",
    "Button Test",
    "System Monitor",
    "OLED Test",
    "Reset Reason"
  };

  unsigned long lastRefreshTime;
  
  void drawMenu();
  void runI2CScanner();
  void drawButtonTest(ButtonEvent btn);
  void drawSysMonitor();
  void drawOledTest();
  void drawResetReason();
  void handleSysMonitor(ButtonEvent btn);
};

#endif
