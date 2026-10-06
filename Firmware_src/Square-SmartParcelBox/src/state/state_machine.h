#pragma once
#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

enum SystemState {
    STATE_START,
    STATE_IDLE,
    STATE_DELIVERY,
    STATE_ONDEMAND,
    STATE_ERROR
};

enum RemoteCommand {
    CMD_NONE,
    CMD_UNLOCK,
    CMD_LOCK,
    CMD_LOCK_AND_UNLOCK,
    CMD_REQUEST_PHOTO,
    CMD_REQUEST_VIDEO
};

void sm_init();
SystemState sm_get_state();
const char* sm_get_state_name(SystemState state);
bool sm_transition_to(SystemState new_state);
bool sm_is_idle();
bool sm_is_error();

#endif // STATE_MACHINE_H
