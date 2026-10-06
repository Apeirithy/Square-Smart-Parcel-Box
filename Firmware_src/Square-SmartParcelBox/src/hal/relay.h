#ifndef HAL_RELAY_H
#define HAL_RELAY_H

bool relay_init();
void relay_unlock();
void relay_lock();
bool relay_is_unlocked();

#endif // HAL_RELAY_H
