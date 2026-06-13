#ifndef OUI_LITE_H
#define OUI_LITE_H

#include <Arduino.h>

struct OuiEntry {
    uint8_t oui[3];
    const char* vendor;
};

const OuiEntry OUI_TABLE[] PROGMEM = {
    {{0x18, 0xFE, 0x34}, "Espressif"},
    {{0xAC, 0xD0, 0x74}, "Espressif"},
    {{0x24, 0x0A, 0xC4}, "Espressif"},
    {{0x00, 0x50, 0xF2}, "Microsoft"},
    {{0xD8, 0x96, 0x95}, "TP-Link"},
    {{0x14, 0xCC, 0x20}, "TP-Link"},
    {{0x50, 0xC7, 0xBF}, "TP-Link"},
    {{0x18, 0x31, 0xBF}, "Realtek"},
    {{0x00, 0x17, 0xF2}, "Apple"},
    {{0xAC, 0xBC, 0x32}, "Apple"},
    {{0x3C, 0x22, 0xFB}, "Apple"},
    {{0x00, 0x1A, 0x11}, "Google"},
    {{0xF4, 0xF5, 0xDB}, "Google"},
    {{0xB0, 0x2A, 0x43}, "Xiaomi"},
    {{0x28, 0x6C, 0x07}, "Xiaomi"},
    {{0x50, 0x64, 0x2B}, "Netgear"},
    {{0x00, 0x14, 0xBF}, "Linksys"},
    {{0x00, 0x90, 0x4C}, "ASUS"},
    {{0x04, 0x92, 0x26}, "ASUS"},
    {{0xD0, 0x76, 0xE7}, "D-Link"},
};
#define OUI_TABLE_LEN (sizeof(OUI_TABLE) / sizeof(OUI_TABLE[0]))

inline const char* lookupOUI(const uint8_t* mac) {
    for (uint8_t i = 0; i < OUI_TABLE_LEN; i++) {
        OuiEntry e;
        memcpy_P(&e, &OUI_TABLE[i], sizeof(OuiEntry));
        if (memcmp(mac, e.oui, 3) == 0) return e.vendor;
    }
    return nullptr;
}

#endif
