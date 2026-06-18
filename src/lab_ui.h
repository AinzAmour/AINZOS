#ifndef LAB_UI_H
#define LAB_UI_H

#include "display.h"
#include "buttons.h"
#include "text_input_ui.h"
#include "ui/evil_twin_ui.h"
#include "ui/bad_ble_ui.h"

extern "C" {
#include "ble_spam.h"
#include "wifi/beacon_spam.h"
#include "wifi/deauth_attack.h"
#include "wifi/channel_switch_attack.h"
#include "wifi/dhcp_starvation.h"
#include "wifi/eapol_logoff.h"
#include "wifi/gtk_abuse.h"
}

enum class LabPage { Menu, BLESpam, WiFiAttacks, WiFiScan, EvilTwin, BadBLE };

class LabUI {
public:
  LabUI(DisplayWrapper* disp);
  void enter();
  bool update(ButtonEvent btn);

private:
  DisplayWrapper* display;
  LabPage currentPage;

  int menuSel, menuTop;
  static const int LAB_MENU_COUNT = 2;
  const char* labMenuItems[LAB_MENU_COUNT] = { "BLE", "WiFi" };

  int bleSel, bleTop;
  bool spamRunning;
  static const int BLE_SPAM_COUNT = 7;
  const char* bleSpamItems[BLE_SPAM_COUNT] = {
    "Apple",
    "Microsoft",
    "Samsung",
    "Google",
    "Flipper Zero",
    "Random",
    "BadBLE"
  };
  const ble_spam_type_t bleSpamTypes[BLE_SPAM_COUNT] = {
    BLE_SPAM_APPLE,
    BLE_SPAM_MICROSOFT,
    BLE_SPAM_SAMSUNG,
    BLE_SPAM_GOOGLE,
    BLE_SPAM_FLIPPERZERO,
    BLE_SPAM_RANDOM,
    BLE_SPAM_RANDOM  // placeholder — BadBLE handled separately
  };

  int wifiSel, wifiTop;
  bool wifiAttackRunning;
  static const int WIFI_ATTACK_COUNT = 9;
  const char* wifiAttackItems[WIFI_ATTACK_COUNT] = {
    "Select Target",
    "Beacon Spam",
    "Beacon List",
    "Deauth Attack",
    "Channel Switch",
    "DHCP Starvation",
    "EAPOL Logoff",
    "GTK Abuse",
    "Evil Twin"
  };

  // Selected AP for WiFi attacks
  struct SelectedAP {
    char ssid[33];
    uint8_t bssid[6];
    uint8_t channel;
    bool valid;
  } selectedAP;

  // WiFi scan results storage
  static const int WIFI_SCAN_MAX = 32;
  struct FoundAP { char ssid[33]; uint8_t bssid[6]; uint8_t channel; int rssi; };
  FoundAP scanResults[WIFI_SCAN_MAX];
  int scanCount;
  int scanSel, scanTop;
  bool showTargetConfirm;
  uint32_t targetConfirmExpiresMs;
  void drawScanResultsInternal(int count, int sel, int top);

  // Text input helper for SSID/password prompts
  TextInputUI* textInput;
  char textInputBuf[33];
  bool textInputActive;

  void handleMenu(ButtonEvent btn);
  void handleBLESpam(ButtonEvent btn);
  void handleWiFiAttacks(ButtonEvent btn);
  void drawMenu();
  void drawBLESpamMenu();
  void drawSpamRunning();
  void drawWiFiAttackMenu();
  void drawWiFiAttackRunning();

  void stopAllWiFiAttacks();
  bool startSelectedWiFiAttack();
  void stopSelectedWiFiAttack();

  // Evil Twin
  EvilTwinUI* evilTwinUI;

  // BadBLE
  BadBleUI* badBleUI;
};

#endif
