#ifndef HAL_KEYBUFFER_INDICATOR_DRIVER_H
#define HAL_KEYBUFFER_INDICATOR_DRIVER_H

#include <Arduino.h>
#include <Wire.h>

bool led_indicator_init(TwoWire &wire);
void led_indicator_set_count(uint8_t count);
void led_indicator_all_off();
void led_indicator_alternating_pattern();
void led_indicator_stop_pattern();
bool led_indicator_is_available();
void led_indicator_update();

#endif // HAL_KEYBUFFER_INDICATOR_DRIVER_H
