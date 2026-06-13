#ifndef IR_CODES_DB_H
#define IR_CODES_DB_H

#include <stdint.h>
#include <pgmspace.h>

// Compact enum for supported protocols
enum IRProtocolType : uint8_t {
  IR_PROTO_NEC = 1,
  IR_PROTO_SONY = 2,
  IR_PROTO_RC5 = 3,
  IR_PROTO_RC6 = 4,
  IR_PROTO_PANASONIC = 8,
  IR_PROTO_SAMSUNG = 11,
  IR_PROTO_LG = 22
};

struct IRCodeEntry {
  uint8_t protocol;
  uint8_t bits;
  uint16_t address;
  uint16_t command;
  uint64_t value;
  const char* label;
};

// Key list
enum IRKey : uint8_t {
  IR_KEY_POWER,
  IR_KEY_MUTE,
  IR_KEY_OK,
  IR_KEY_VOL_UP,
  IR_KEY_VOL_DOWN,
  IR_KEY_UP,
  IR_KEY_DOWN,
  IR_KEY_SOURCE,
  IR_KEY_AC_POWER_ON,
  IR_KEY_AC_POWER_OFF
};

enum IRDeviceType : uint8_t {
  IR_DEV_STB,
  IR_DEV_PROJECTOR,
  IR_DEV_SOUNDBAR,
  IR_DEV_AC
};

// Labels for remote keys
static const char* const irKeyLabels[] PROGMEM = {
  "Power",
  "Mute",
  "OK",
  "Vol+",
  "Vol-",
  "Up",
  "Down",
  "Source",
  "Power ON",
  "Power OFF"
};

// TV Brand keys mapping: 7 keys for each brand
// Keys order: Power, Mute, OK, Vol+, Vol-, Up, Down
#if HIZMOS_ENABLE_IR_BRAND_REMOTE
static const IRCodeEntry tvSamsungRemote[] PROGMEM = {
  {IR_PROTO_SAMSUNG, 32, 0, 0, 0xE0E040BF, "Power"},
  {IR_PROTO_SAMSUNG, 32, 0, 0, 0xE0E0F00F, "Mute"},
  {IR_PROTO_SAMSUNG, 32, 0, 0, 0xE0E016E9, "OK"},
  {IR_PROTO_SAMSUNG, 32, 0, 0, 0xE0E0E01F, "Vol+"},
  {IR_PROTO_SAMSUNG, 32, 0, 0, 0xE0E0D02F, "Vol-"},
  {IR_PROTO_SAMSUNG, 32, 0, 0, 0xE0E006F9, "Up"},
  {IR_PROTO_SAMSUNG, 32, 0, 0, 0xE0E08679, "Down"}
};

static const IRCodeEntry tvLgRemote[] PROGMEM = {
  {IR_PROTO_NEC, 32, 0, 0, 0x20DF10EF, "Power"},
  {IR_PROTO_NEC, 32, 0, 0, 0x20DF906F, "Mute"},
  {IR_PROTO_NEC, 32, 0, 0, 0x20DF22DD, "OK"},
  {IR_PROTO_NEC, 32, 0, 0, 0x20DF40BF, "Vol+"},
  {IR_PROTO_NEC, 32, 0, 0, 0x20DFC03F, "Vol-"},
  {IR_PROTO_NEC, 32, 0, 0, 0x20DF02FD, "Up"},
  {IR_PROTO_NEC, 32, 0, 0, 0x20DF827D, "Down"}
};

static const IRCodeEntry tvSonyRemote[] PROGMEM = {
  {IR_PROTO_SONY, 12, 1, 0x15, 0xa90, "Power"},
  {IR_PROTO_SONY, 12, 1, 0x14, 0x290, "Mute"},
  {IR_PROTO_SONY, 12, 1, 0x65, 0xa70, "OK"},
  {IR_PROTO_SONY, 12, 1, 0x12, 0x490, "Vol+"},
  {IR_PROTO_SONY, 12, 1, 0x13, 0xc90, "Vol-"},
  {IR_PROTO_SONY, 12, 1, 0x74, 0x2f0, "Up"},
  {IR_PROTO_SONY, 12, 1, 0x75, 0xaf0, "Down"}
};

static const IRCodeEntry tvPanasonicRemote[] PROGMEM = {
  {IR_PROTO_PANASONIC, 48, 0, 0, 0x40040100BCBD, "Power"},
  {IR_PROTO_PANASONIC, 48, 0, 0, 0x400401004C4D, "Mute"},
  {IR_PROTO_PANASONIC, 48, 0, 0, 0x400401002C2D, "OK"},
  {IR_PROTO_PANASONIC, 48, 0, 0, 0x400401000405, "Vol+"},
  {IR_PROTO_PANASONIC, 48, 0, 0, 0x400401008485, "Vol-"},
  {IR_PROTO_PANASONIC, 48, 0, 0, 0x400401005253, "Up"},
  {IR_PROTO_PANASONIC, 48, 0, 0, 0x40040100D2D3, "Down"}
};

static const IRCodeEntry tvPhilipsRemote[] PROGMEM = {
  {IR_PROTO_RC5, 12, 0, 12, 0xC, "Power"},
  {IR_PROTO_RC5, 12, 0, 13, 0xD, "Mute"},
  {IR_PROTO_RC5, 12, 0, 87, 0x57, "OK"},
  {IR_PROTO_RC5, 12, 0, 16, 0x10, "Vol+"},
  {IR_PROTO_RC5, 12, 0, 17, 0x11, "Vol-"},
  {IR_PROTO_RC5, 12, 0, 80, 0x50, "Up"},
  {IR_PROTO_RC5, 12, 0, 81, 0x51, "Down"}
};
#endif

// Other Devices
#if HIZMOS_ENABLE_IR_OTHER_DEV
static const IRCodeEntry stbRemote[] PROGMEM = {
  {IR_PROTO_NEC, 32, 0, 0, 0x00FF02FD, "Power"},
  {IR_PROTO_NEC, 32, 0, 0, 0x00FF0AF5, "OK"},
  {IR_PROTO_NEC, 32, 0, 0, 0x00FF0EF1, "Up"},
  {IR_PROTO_NEC, 32, 0, 0, 0x00FF12ED, "Down"}
};

static const IRCodeEntry projectorRemote[] PROGMEM = {
  {IR_PROTO_NEC, 32, 0, 0, 0x00FDB04F, "Power"},
  {IR_PROTO_NEC, 32, 0, 0, 0x00FDD02F, "Source"},
  {IR_PROTO_NEC, 32, 0, 0, 0x00FD50AF, "OK"}
};

static const IRCodeEntry soundbarRemote[] PROGMEM = {
  {IR_PROTO_NEC, 32, 0, 0, 0x00FF10EF, "Power"},
  {IR_PROTO_NEC, 32, 0, 0, 0x00FF906F, "Mute"},
  {IR_PROTO_NEC, 32, 0, 0, 0x00FF40BF, "Vol+"},
  {IR_PROTO_NEC, 32, 0, 0, 0x00FFC03F, "Vol-"}
};

#if HIZMOS_ENABLE_IR_AC_CODES
static const IRCodeEntry acRemote[] PROGMEM = {
  {IR_PROTO_NEC, 32, 0, 0, 0x00FF52AD, "Power ON"},
  {IR_PROTO_NEC, 32, 0, 0, 0x00FF926D, "Power OFF"}
};
#endif
#endif

// Universal blasts starter arrays (Power is in universal_ir.h)
static const IRCodeEntry tvMuteBlastCodes[] PROGMEM = {
  {IR_PROTO_SAMSUNG, 32, 0, 0, 0xE0E0F00F, "Samsung"},
  {IR_PROTO_NEC, 32, 0, 0, 0x20DF906F, "LG"},
  {IR_PROTO_SONY, 12, 1, 0x14, 0x290, "Sony"},
  {IR_PROTO_PANASONIC, 48, 0, 0, 0x400401004C4D, "Panasonic"},
  {IR_PROTO_RC5, 12, 0, 13, 0xD, "Philips"}
};

static const IRCodeEntry tvOkBlastCodes[] PROGMEM = {
  {IR_PROTO_SAMSUNG, 32, 0, 0, 0xE0E016E9, "Samsung"},
  {IR_PROTO_NEC, 32, 0, 0, 0x20DF22DD, "LG"},
  {IR_PROTO_SONY, 12, 1, 0x65, 0xa70, "Sony"},
  {IR_PROTO_PANASONIC, 48, 0, 0, 0x400401002C2D, "Panasonic"},
  {IR_PROTO_RC5, 12, 0, 87, 0x57, "Philips"}
};

static const IRCodeEntry stbPowerBlastCodes[] PROGMEM = {
  {IR_PROTO_NEC, 32, 0, 0, 0x00FF02FD, "STB Nec"},
  {IR_PROTO_NEC, 32, 0, 0, 0x00FFA25D, "STB Cisco"},
  {IR_PROTO_RC5, 12, 0, 12, 0xC, "STB RC5"}
};

static const IRCodeEntry projectorPowerBlastCodes[] PROGMEM = {
  {IR_PROTO_NEC, 32, 0, 0, 0x00FDB04F, "Proj Nec"},
  {IR_PROTO_NEC, 32, 0, 0, 0x00FFD827, "Proj BenQ"},
  {IR_PROTO_NEC, 32, 0, 0, 0x4CB2807F, "Proj Epson"}
};

static const IRCodeEntry soundbarPowerBlastCodes[] PROGMEM = {
  {IR_PROTO_NEC, 32, 0, 0, 0x00FF10EF, "Sbar Nec"},
  {IR_PROTO_NEC, 32, 0, 0, 0x20DF10EF, "Sbar LG"},
  {IR_PROTO_SAMSUNG, 32, 0, 0, 0xE0E040BF, "Sbar Sams"}
};

#if HIZMOS_ENABLE_IR_AC_CODES
static const IRCodeEntry acPowerBlastCodes[] PROGMEM = {
  {IR_PROTO_NEC, 32, 0, 0, 0x00FF52AD, "AC Toggle"},
  {IR_PROTO_NEC, 32, 0, 0, 0x00FF926D, "AC Off"}
};
#endif

#endif // IR_CODES_DB_H
