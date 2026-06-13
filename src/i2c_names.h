#ifndef I2C_NAMES_H
#define I2C_NAMES_H

#include <Arduino.h>

inline const char* i2cDeviceName(uint8_t addr) {
    switch (addr) {
        case 0x3C: case 0x3D: return "SSD1306 OLED";
        case 0x48: case 0x49:
        case 0x4A: case 0x4B: return "ADS1115 ADC";
        case 0x57:             return "EEPROM/DS3231";
        case 0x68:             return "MPU6050/DS3231";
        case 0x76: case 0x77: return "BME280/BMP280";
        case 0x36:             return "MAX17048 FuelGauge";
        case 0x28:             return "BNO055 IMU";
        default:               return nullptr;
    }
}

#endif
