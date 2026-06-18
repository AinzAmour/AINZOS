#include "ir_utils.h"
#include "pins.h"
#include <IRremoteESP8266.h>
#include <IRsend.h>

IRsend irsend(IR_LED_PIN);

void irBegin() {
  irsend.begin();
}

bool ir_manager_init() {
  irBegin();
  return true;
}

static void sendRcaRaw(uint32_t data) {
  uint16_t timings[51];
  timings[0] = 4000;
  timings[1] = 4000;
  int idx = 2;
  for (int i = 0; i < 24; i++) {
    bool bit = (data >> i) & 1;
    timings[idx++] = 500;
    timings[idx++] = bit ? 2000 : 1000;
  }
  timings[idx++] = 500;
  
  irsend.sendRaw(timings, 51, 38);
}


bool irSendCode(const IRCodeEntry& code) {
  switch (code.protocol) {
    case IR_PROTO_NEC:
      irsend.sendNEC(code.value ? code.value : irsend.encodeNEC(code.address, code.command));
      return true;
    case IR_PROTO_SAMSUNG:
      irsend.sendSAMSUNG(code.value ? code.value : irsend.encodeSAMSUNG(code.address, code.command));
      return true;
    case IR_PROTO_SONY:
      irsend.sendSony(code.value ? code.value : (code.command | ((uint32_t)code.address << 7)), code.bits);
      return true;
    case IR_PROTO_PANASONIC:
      irsend.sendPanasonic64(code.value);
      return true;
    case IR_PROTO_RC5:
      irsend.sendRC5(code.value ? code.value : irsend.encodeRC5(code.address, code.command), code.bits);
      return true;
    case IR_PROTO_RC6:
      irsend.sendRC6(code.value ? code.value : irsend.encodeRC6(code.address, code.command), code.bits);
      return true;
    default:
      return false;
  }
}

bool irSendUniversalSignal(const universal_ir_signal_t* sig) {
  if (!sig) return false;
  
  IRCodeEntry entry = {0};
  entry.address = sig->address;
  entry.command = sig->command;
  entry.label = sig->name;
  
  if (strcasecmp(sig->protocol, "NEC") == 0) {
    entry.protocol = IR_PROTO_NEC;
    entry.bits = 32;
  } else if (strcasecmp(sig->protocol, "NECext") == 0) {
    entry.protocol = IR_PROTO_NEC;
    entry.bits = 32;
    entry.value = irsend.encodeNEC(sig->address & 0xFFFF, sig->command & 0xFF);
  } else if (strcasecmp(sig->protocol, "Samsung32") == 0) {
    entry.protocol = IR_PROTO_SAMSUNG;
    entry.bits = 32;
  } else if (strcasecmp(sig->protocol, "SIRC") == 0) {
    entry.protocol = IR_PROTO_SONY;
    entry.bits = 12;
  } else if (strcasecmp(sig->protocol, "Kaseikyo") == 0) {
    entry.protocol = IR_PROTO_PANASONIC;
    entry.bits = 48;
    // Construct Kaseikyo 48-bit value
    uint8_t id = (sig->address >> 24) & 0x03;
    uint16_t vendor_id = (sig->address >> 8) & 0xFFFF;
    uint8_t genre1 = (sig->address >> 4) & 0x0F;
    uint8_t genre2 = sig->address & 0x0F;
    uint16_t data = sig->command & 0x3FF;
    
    uint8_t bytes[6];
    bytes[0] = vendor_id & 0xFF;
    bytes[1] = vendor_id >> 8;
    uint8_t vp = bytes[0] ^ bytes[1];
    vp = (vp & 0x0F) ^ (vp >> 4);
    bytes[2] = (vp & 0x0F) | (genre1 << 4);
    bytes[3] = ((data & 0x0F) << 4) | genre2;
    bytes[4] = (id << 6) | ((data >> 4) & 0x3F);
    bytes[5] = bytes[2] ^ bytes[3] ^ bytes[4];
    
    uint64_t val = 0;
    for (int i = 0; i < 6; i++) {
        val = (val << 8) | bytes[i];
    }
    entry.value = val;
  } else if (strcasecmp(sig->protocol, "RC5") == 0) {
    entry.protocol = IR_PROTO_RC5;
    entry.bits = 12;
  } else if (strcasecmp(sig->protocol, "RC6") == 0) {
    entry.protocol = IR_PROTO_RC6;
    entry.bits = 20;
  } else if (strcasecmp(sig->protocol, "RCA") == 0) {
    uint32_t data = (sig->address & 0x0F) | ((sig->command & 0xFF) << 4) | ((~sig->address & 0x0F) << 12) | ((~sig->command & 0xFF) << 16);
    sendRcaRaw(data);
    return true;
  } else if (strcasecmp(sig->protocol, "Pioneer") == 0) {
    irsend.sendNEC(irsend.encodeNEC(sig->address, sig->command));
    return true;
  } else {
    return false;
  }
  
  return irSendCode(entry);
}

bool irSendBrandKey(uint8_t brand, IRKey key) {
#if AINZOS_ENABLE_IR_BRAND_REMOTE
  const IRCodeEntry* list = nullptr;
  switch (brand) {
    case 0: list = tvSamsungRemote; break;
    case 1: list = tvLgRemote; break;
    case 2: list = tvSonyRemote; break;
    case 3: list = tvPanasonicRemote; break;
    case 4: list = tvPhilipsRemote; break;
    default: return false;
  }
  if (!list || key > IR_KEY_DOWN) return false;
  
  IRCodeEntry code;
  memcpy_P(&code, &list[key], sizeof(IRCodeEntry));
  return irSendCode(code);
#else
  return false;
#endif
}

bool irSendOtherDeviceKey(IRDeviceType device, IRKey key) {
#if AINZOS_ENABLE_IR_OTHER_DEV
  const IRCodeEntry* list = nullptr;
  uint8_t max_keys = 0;
  switch (device) {
    case IR_DEV_STB:
      list = stbRemote;
      max_keys = 4;
      break;
    case IR_DEV_PROJECTOR:
      list = projectorRemote;
      max_keys = 3;
      break;
    case IR_DEV_SOUNDBAR:
      list = soundbarRemote;
      max_keys = 4;
      break;
    case IR_DEV_AC:
#if AINZOS_ENABLE_IR_AC_CODES
      list = acRemote;
      max_keys = 2;
#endif
      break;
  }
  
  if (!list) return false;
  
  uint8_t key_idx = 0xFF;
  if (device == IR_DEV_AC) {
    if (key == IR_KEY_AC_POWER_ON) key_idx = 0;
    else if (key == IR_KEY_AC_POWER_OFF) key_idx = 1;
  } else {
    if (key == IR_KEY_POWER) key_idx = 0;
    else if (key == IR_KEY_MUTE && device == IR_DEV_SOUNDBAR) key_idx = 1;
    else if (key == IR_KEY_OK) {
      if (device == IR_DEV_STB) key_idx = 1;
      else if (device == IR_DEV_PROJECTOR) key_idx = 2;
    }
    else if (key == IR_KEY_SOURCE && device == IR_DEV_PROJECTOR) key_idx = 1;
    else if (key == IR_KEY_VOL_UP && device == IR_DEV_SOUNDBAR) key_idx = 2;
    else if (key == IR_KEY_VOL_DOWN && device == IR_DEV_SOUNDBAR) key_idx = 3;
    else if (key == IR_KEY_UP && device == IR_DEV_STB) key_idx = 2;
    else if (key == IR_KEY_DOWN && device == IR_DEV_STB) key_idx = 3;
  }
  
  if (key_idx >= max_keys) return false;
  
  IRCodeEntry code;
  memcpy_P(&code, &list[key_idx], sizeof(IRCodeEntry));
  return irSendCode(code);
#else
  return false;
#endif
}
