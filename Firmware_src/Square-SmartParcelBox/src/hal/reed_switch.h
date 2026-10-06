#ifndef HAL_REED_SWITCH_H
#define HAL_REED_SWITCH_H

#define REED_SWITCH_DEBOUNCE_MS 50

bool reed_switch_init();
void reed_switch_update();
bool reed_switch_is_closed();
bool reed_switch_is_open();

#endif // HAL_REED_SWITCH_H
