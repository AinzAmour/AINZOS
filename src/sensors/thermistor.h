#ifndef THERMISTOR_H
#define THERMISTOR_H

#include <Arduino.h>
#include <math.h>

// NTC Thermistor on GPIO 2 (expansion header pin 1)
#define NTC_PIN 2

// NTC Thermistor Calibration Parameters
#define NTC_NOMINAL 10000.0f  // Resistance at 25 degrees C (usually 10000 or 100000)
#define NTC_BETA 3950.0f      // Beta coefficient (usually 3950, 3435, etc.)
#define R_FIXED 5827.0f      // Calibrated fixed resistor value (originally 10000.0f)

// Wiring: 3V3 ─[R_FIXED]─┬─ GPIO 2 (ADC)
//                        │
//                  [NTC_NOMINAL]
//                        │
//                       GND

class ThermistorSensor {
public:
  void begin() {
    analogSetAttenuation(ADC_11db);  // Full 0–3.3V range
    pinMode(NTC_PIN, INPUT);
    _lastReadMs = 0;
    _cachedCelsius = 0.0f;
    _initialized = false;
    _lastRawADC = 0.0f;
  }

  float getLastRawADC() const {
    return _lastRawADC;
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
  float    _lastRawADC;

  float _sampleTemperature() {
    // Read 10 calibrated millivolts samples
    int readings[10];
    for (int i = 0; i < 10; i++) {
      readings[i] = analogReadMilliVolts(NTC_PIN);
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
    float avgMv = (float)sum / 8.0f;
    _lastRawADC = avgMv; // Now storing millivolts

    // Convert millivolts to voltage
    float voltage = avgMv / 1000.0f;
    if (voltage <= 0.01f || voltage >= 3.29f) {
      return -999.0f;  // Sensor not connected or shorted
    }
    float resistance = R_FIXED * voltage / (3.3f - voltage);

    // Steinhart-Hart (simplified B-parameter equation)
    float steinhart = resistance / NTC_NOMINAL;   // R / R_nominal
    steinhart = log(steinhart);                 // ln(R/R0)
    steinhart /= NTC_BETA;                      // B coefficient
    steinhart += 1.0f / (25.0f + 273.15f);     // + 1/T0 (25°C nominal)
    steinhart = 1.0f / steinhart;               // Invert
    float celsius = steinhart - 273.15f;        // Convert to Celsius

    return celsius;
  }
};

#endif
