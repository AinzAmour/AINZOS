#ifndef PINS_H
#define PINS_H

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define SDA_PIN 4
#define SCL_PIN 5

#define BUTTON_SEL_PIN 3
#define BUTTON_BACK_PIN 10
#define BUTTON_UP_PIN 0
#define BUTTON_DOWN_PIN 1

// 2x4 RF/GPIO Expansion Header (GND, 3V3, 2, 6, 20, 21, 8, 9)
#define RF_GPIO_PIN_1 2
#define RF_GPIO_PIN_2 6
#define RF_GPIO_PIN_3 20
#define RF_GPIO_PIN_4 21
#define RF_GPIO_PIN_5 8
#define RF_GPIO_PIN_6 9

#define SK6812_PIN 255  // DISABLED — GPIO11 is VDD_SPI on ESP32-C3
                          // Driving GPIO11 as output corrupts SPI flash
static_assert(SK6812_PIN != 11,
  "GPIO11 is VDD_SPI — never assign it as a GPIO output");
#define IR_LED_PIN 7

#endif
