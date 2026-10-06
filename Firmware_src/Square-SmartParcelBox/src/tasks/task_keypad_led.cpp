#include "task_keypad_led.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "../hal/keypad_driver.h"
#include "../hal/keybuffer_indicator_driver.h"
#include "../config.h"

extern SemaphoreHandle_t I2C_Mutex;

static char s_buffer[PIN_LENGTH + 1] = {0};
static uint8_t s_buffer_len = 0;
static bool s_buffer_full = false;
static bool s_enabled = false;

const char* task_keypad_led_get_buffer() {
    return s_buffer;
}

void task_keypad_led_clear_buffer() {
    memset(s_buffer, 0, sizeof(s_buffer));
    s_buffer_len = 0;
    s_buffer_full = false;

    // Reset LED indicators to 0
    xSemaphoreTake(I2C_Mutex, portMAX_DELAY);
    led_indicator_set_count(0);
    xSemaphoreGive(I2C_Mutex);
}

bool task_keypad_led_is_buffer_full() {
    return s_buffer_full;
}

void task_keypad_led_set_enabled(bool enabled) {
    s_enabled = enabled;
}

void task_keypad_led_entry(void* pvParameters) {
    (void)pvParameters;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(RTOS_MIN_DELAY_MS));

        if (!s_enabled) {
            continue;
        }

        // Scan keypad and update LED animations under I2C mutex
        xSemaphoreTake(I2C_Mutex, portMAX_DELAY);
        char key = keypad_scan();
        led_indicator_update();
        xSemaphoreGive(I2C_Mutex);

        // Process key if valid and buffer not yet full
        if (key != '\0' && !s_buffer_full) {
            s_buffer[s_buffer_len] = key;
            s_buffer_len++;
            s_buffer[s_buffer_len] = '\0';

            // Update LED count indicator
            xSemaphoreTake(I2C_Mutex, portMAX_DELAY);
            led_indicator_set_count(s_buffer_len);
            xSemaphoreGive(I2C_Mutex);

            Serial.printf("[KEYPAD] Key '%c' buffered (%d/%d)\n", key, s_buffer_len, PIN_LENGTH);

            if (s_buffer_len == PIN_LENGTH) {
                s_buffer_full = true;
                Serial.printf("[KEYPAD] Buffer full: %s\n", s_buffer);
            }
        }
    }
}
