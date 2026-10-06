#include "state_machine.h"
#include <Arduino.h>

static SystemState current_state = STATE_START;

void sm_init() {
    current_state = STATE_START;
    Serial.println("[State] Initialized to STATE_START");
}

SystemState sm_get_state() {
    return current_state;
}

const char* sm_get_state_name(SystemState state) {
    switch(state) {
        case STATE_START: return "STATE_START";
        case STATE_IDLE: return "STATE_IDLE";
        case STATE_DELIVERY: return "STATE_DELIVERY";
        case STATE_ONDEMAND: return "STATE_ONDEMAND";
        case STATE_ERROR: return "STATE_ERROR";
        default: return "UNKNOWN";
    }
}

bool sm_transition_to(SystemState new_state) {
    bool valid = false;
    
    switch(current_state) {
        case STATE_START:
            if (new_state == STATE_IDLE || new_state == STATE_ERROR) valid = true;
            break;
        case STATE_IDLE:
            if (new_state == STATE_DELIVERY || new_state == STATE_ONDEMAND || new_state == STATE_ERROR) valid = true;
            break;
        case STATE_DELIVERY:
            if (new_state == STATE_IDLE || new_state == STATE_ERROR) valid = true;
            break;
        case STATE_ONDEMAND:
            if (new_state == STATE_IDLE) valid = true;
            break;
        case STATE_ERROR:
            // Dead state
            valid = false;
            break;
    }

    if (valid) {
        Serial.printf("[State] Transition: %s -> %s\n", sm_get_state_name(current_state), sm_get_state_name(new_state));
        current_state = new_state;
        return true;
    } else {
        Serial.printf("[State] Error: Invalid transition from %s to %s\n", sm_get_state_name(current_state), sm_get_state_name(new_state));
        return false;
    }
}

bool sm_is_idle() {
    return current_state == STATE_IDLE;
}

bool sm_is_error() {
    return current_state == STATE_ERROR;
}
