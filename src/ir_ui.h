#ifndef IR_UI_H
#define IR_UI_H

#include "app_types.h"
#include "display.h"
#include "buttons.h"
#include "config.h"
#include "ir_codes_db.h"
#include "ir_utils.h"

enum IRPageState : uint8_t {
  IR_PAGE_MAIN,
  IR_PAGE_UNIVERSAL,
  IR_PAGE_TV_BRANDS,
  IR_PAGE_TV_KEYS,
  IR_PAGE_OTHER_DEVICES,
  IR_PAGE_OTHER_KEYS,
  IR_PAGE_BLASTING
};

class IrUI {
public:
  IrUI(DisplayWrapper* disp);
  void enter();
  bool update(ButtonEvent btn);

private:
  DisplayWrapper* display;
  IRPageState pageState;
  
  int menuSelectedIndex;
  int menuTopIndex;
  
  uint8_t selectedBrand;
  IRDeviceType selectedDevice;
  
  // Universal blast properties
  const IRCodeEntry* blastCodes;
  uint8_t blastCount;
  uint8_t blastIndex;
  uint32_t lastBlastMs;
  const char* blastTitle;
  bool isUniversalPowerBlast; // If true, we blast using universal_ir.h
  
  // Status feedback properties (e.g. "Sent!" / "No code")
  const char* statusText;
  uint32_t statusStartMs;
  
  void drawActivePage();
  void sendSingleKey(IRKey key);
  void startUniversalBlast(const IRCodeEntry* codes, uint8_t count, const char* title);
  void startUniversalPowerBlast();
};

#endif // IR_UI_H
