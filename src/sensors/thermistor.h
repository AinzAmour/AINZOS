#ifndef THERMISTOR_H
#define THERMISTOR_H

#include <Arduino.h>
#include <math.h>

// NTC Thermistor on GPIO 2 (expansion header pin 1)
#define NTC_PIN 2

// 10kΩ NTC, B=3950, voltage divider with 10kΩ fixed resistor
// Wiring: 3V3 ─[10kΩ fixed]─┬─ GPIO 2 (ADC)
//                             │
//                          [NTC 10kΩ]
//                             │
//                            GND

class ThermistorSensor {
public:
  void begin() {
    analogSetAttenuation(ADC_11db);  // Full 0–3.3V range
    pinMode(NTC_PIN, INPUT);
    _lastReadMs = 0;
    _cachedCelsius = 0.0f;
    _initialized = false;
  }

  float readCelsius() {
    uint32_t now = millis();
    if (_initialized && (now - _lastReadMs < 5000)) {
      return _cachedCelsius;
    }
    _cachedCelsius = _sampleTemperature();
    _lastReadMs = now;
    _initialized = true;
    return _cachedCelsius;
  }

  float readFahrenheit() {
    return readCelsius() * 9.0f / 5.0f + 32.0f;
  }

private:
  uint32_t _lastReadMs;
  float    _cachedCelsius;
  bool     _initialized;

  float _sampleTemperature() {
    // Read 10 ADC samples
    int readings[10];
    for (int i = 0; i < 10; i++) {
      readings[i] = analogRead(NTC_PIN);
      delayMicroseconds(200);
    }

    // Remove highest and lowest, average the remaining 8
    int minVal = readings[0], maxVal = readings[0];
    long sum = 0;
    for (int i = 0; i < 10; i++) {
      sum += readings[i];
      if (readings[i] < minVal) minVal = readings[i];
      if (readings[i] > maxVal) maxVal = readings[i];
    }
    sum -= minVal;
    sum -= maxVal;
    float avg = (float)sum / 8.0f;

    // Convert ADC to voltage, then to resistance
    float voltage = avg * (3.3f / 4095.0f);
    if (voltage <= 0.01f || voltage >= 3.29f) {
      return -999.0f;  // Sensor not connected or shorted
    }
    float resistance = 10000.0f * voltage / (3.3f - voltage);

    // Steinhart-Hart (simplified B-parameter equation)
    float steinhart = resistance / 10000.0f;   // R / R_nominal
    steinhart = log(steinhart);                 // ln(R/R0)
    steinhart /= 3950.0f;                       // B coefficient
    steinhart += 1.0f / (25.0f + 273.15f);     // + 1/T0 (25°C nominal)
    steinhart = 1.0f / steinhart;               // Invert
    float celsius = steinhart - 273.15f;        // Convert to Celsius

    return celsius;
  }
};

#endif
