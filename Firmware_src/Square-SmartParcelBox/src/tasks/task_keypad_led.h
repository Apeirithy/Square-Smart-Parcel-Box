#ifndef TASK_KEYPAD_LED_H
#define TASK_KEYPAD_LED_H

#include <Arduino.h>

void task_keypad_led_entry(void* pvParameters);
const char* task_keypad_led_get_buffer();
void task_keypad_led_clear_buffer();
bool task_keypad_led_is_buffer_full();
void task_keypad_led_set_enabled(bool enabled);

#endif // TASK_KEYPAD_LED_H
