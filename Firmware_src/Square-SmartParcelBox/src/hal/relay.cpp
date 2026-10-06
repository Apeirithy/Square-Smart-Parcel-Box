#include "hal/relay.h"
#include <Arduino.h>
#include "pins.h"

bool relay_init() {
    pinMode(PIN_RELAY_SOLENOID, OUTPUT);
    digitalWrite(PIN_RELAY_SOLENOID, LOW);
    return true;
}

void relay_unlock() {
    digitalWrite(PIN_RELAY_SOLENOID, HIGH);
}

void relay_lock() {
    digitalWrite(PIN_RELAY_SOLENOID, LOW);
}

bool relay_is_unlocked() {
    return digitalRead(PIN_RELAY_SOLENOID) == HIGH? true : false;
}
