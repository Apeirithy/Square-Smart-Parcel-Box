#include "hal/flash_led.h"
#include <Arduino.h>
#include "pins.h"

bool flash_led_init() {
    pinMode(PIN_AUX_FLASH, OUTPUT);
    digitalWrite(PIN_AUX_FLASH, LOW);
    return true;
}

void flash_led_on() {
    digitalWrite(PIN_AUX_FLASH, HIGH);
}

void flash_led_off() {
    digitalWrite(PIN_AUX_FLASH, LOW);
}
