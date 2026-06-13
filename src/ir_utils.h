#ifndef IR_UTILS_H
#define IR_UTILS_H

#include <Arduino.h>
#include "config.h"
#include "ir_codes_db.h"
#include "universal_ir.h"

// Initialize IR sender
void irBegin();
bool ir_manager_init();

// Direct code transmitter
bool irSendCode(const IRCodeEntry& code);

// Universal signal translator
bool irSendUniversalSignal(const universal_ir_signal_t* sig);

// Transmit key for specific brand
bool irSendBrandKey(uint8_t brand, IRKey key);

// Transmit key for other devices
bool irSendOtherDeviceKey(IRDeviceType device, IRKey key);

#endif // IR_UTILS_H
