#ifndef PINS_H
#define PINS_H

// WS2812 RGB LED
#define PIN_LED_WS2812 48

// Relay Solenoid
#define PIN_RELAY_SOLENOID 14

// Auxiliary Flash LED
#define PIN_AUX_FLASH 2

// Door Switch (Reed Switch)
#define PIN_DOOR_SWITCH 1

// I2C Pins
#define PIN_SDA 21
#define PIN_SCL 47

// I2C Addresses
#define I2C_ADDR_KEYPAD 0x20
#define I2C_ADDR_LED_IND 0x21
#define I2C_ADDR_TOF 0x29

// Camera Module Interface (OV2640)
#define PIN_CAM_D0 11
#define PIN_CAM_D1 9
#define PIN_CAM_D2 8
#define PIN_CAM_D3 10
#define PIN_CAM_D4 12
#define PIN_CAM_D5 18
#define PIN_CAM_D6 17
#define PIN_CAM_D7 16
#define PIN_CAM_XCLK 15
#define PIN_CAM_PCLK 13
#define PIN_CAM_VSYNC 6
#define PIN_CAM_HREF 7
#define PIN_CAM_SIOD 4
#define PIN_CAM_SIOC 5

// MicroSD Card
#define PIN_SD_DATA 40
#define PIN_SD_CLK 39
#define PIN_SD_CMD 38

#endif // PINS_H
