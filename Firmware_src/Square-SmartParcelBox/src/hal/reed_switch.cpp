#include "hal/reed_switch.h"
#include <Arduino.h>
#include "pins.h"

static int current_physical_state;
static int previous_physical_state;
static int debounced_state;
static unsigned long last_debounce_time = 0;

bool reed_switch_init() {
    pinMode(PIN_DOOR_SWITCH, INPUT);
    current_physical_state = digitalRead(PIN_DOOR_SWITCH);
    previous_physical_state = current_physical_state;
    debounced_state = current_physical_state;
    last_debounce_time = millis();
    return true;
}

void reed_switch_update() {
    unsigned long now = millis();
    current_physical_state = digitalRead(PIN_DOOR_SWITCH);

    if (current_physical_state != previous_physical_state) {
        last_debounce_time = now;
        previous_physical_state = current_physical_state;
    }

    if ((now - last_debounce_time) >= REED_SWITCH_DEBOUNCE_MS) {
        if (current_physical_state != debounced_state) {
            debounced_state = current_physical_state;
        }
    }
}

bool reed_switch_is_closed() {
    return debounced_state == HIGH;
}

bool reed_switch_is_open() {
    return debounced_state == LOW;
}
