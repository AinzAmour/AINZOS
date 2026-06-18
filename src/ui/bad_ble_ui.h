#ifndef BAD_BLE_UI_H
#define BAD_BLE_UI_H

#include "display.h"
#include "buttons.h"
#include "text_input_ui.h"
#include "ble/ble_hid.h"
#include "ble/ble_hid_payloads.h"

enum class BadBlePage {
  ModeMenu,      // Scrollable list of payloads + mouse actions
  ConnectRun,    // Advertising → connected → run payload
  CustomText     // TextInputUI for arbitrary string
};

class BadBleUI {
public:
  BadBleUI(DisplayWrapper* disp);
  ~BadBleUI();

  void enter();
  bool update(ButtonEvent btn);  // Returns false to exit back to parent menu
  void stopHID();                // External stop (called by LabUI on exit)

private:
  DisplayWrapper* display;
  BleHID hid;
  BadBlePage currentPage;

  // Mode menu
  int modeSel, modeTop;

  // Connect/Run state
  int selectedPayload;        // Index into combined payload list
  bool payloadExecuted;       // Payload has been sent
  unsigned long doneTimerMs;  // Timer for "Done!" display

  // Custom text input
  TextInputUI* textInput;
  char customTextBuf[33];
  bool customTextReady;

  // Drawing
  void drawModeMenu();
  void drawConnectRun();
  const char* getPayloadName(int idx);

  // Payload execution
  void executePayload(int idx);
  void executeMouseAction(int mouseIdx);

  // FreeRTOS task for async non-blocking payload execution (prevents watchdog resets)
  bool payloadRunning;
  TaskHandle_t payloadTaskHandle;
  static void payloadTaskFunc(void* pvParameters);
  void runPayload();
};

#endif // BAD_BLE_UI_H
