#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

// HAL drivers
#include "hal/ws2812.h"
#include "hal/relay.h"
#include "hal/flash_led.h"
#include "hal/reed_switch.h"
#include "hal/keypad_driver.h"
#include "hal/keybuffer_indicator_driver.h"
#include "hal/tof.h"
#include "hal/camera.h"
#include "hal/sdcard.h"
#include "hal/nvs_store.h"

// System headers
#include "pins.h"
#include "config.h"
#include "credentials.h"
#include "state/state_machine.h"
#include "rtos_types.h"

// Firebase
#include "firebase/fb_rtdb.h"
#include "firebase/fb_storage.h"
#include <FirebaseClient.h>

// Task entry points
#include "tasks/task_network.h"
#include "tasks/task_keypad_led.h"
#include "tasks/task_statemachine.h"
#include "tasks/task_media.h"

// ── Global RTOS IPC Objects ─────────────────────────────────────────────────
QueueHandle_t      RTDBQueue    = NULL;
QueueHandle_t      CommandQueue = NULL;
SemaphoreHandle_t  I2C_Mutex    = NULL;
SemaphoreHandle_t  SD_Mutex     = NULL;
SemaphoreHandle_t  NVS_Mutex    = NULL;

extern FirebaseApp app;

// ── Hardware init status tracking ───────────────────────────────────────────
static bool hw_keypad_ok   = false;
static bool hw_led_ind_ok  = false;
static bool hw_tof_ok      = false;
static bool hw_camera_ok   = false;
static bool hw_sdcard_ok   = false;
static bool hw_nvs_ok      = false;
static bool hw_wifi_ok     = false;
static bool hw_firebase_ok = false;

// ── Check if any non-WiFi hardware had issues ───────────────────────────────
static bool has_hw_warnings() {
    return !hw_keypad_ok || !hw_led_ind_ok || !hw_tof_ok || 
           !hw_camera_ok || !hw_sdcard_ok;
}

void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); }

    // ── System Banner ───────────────────────────────────────────────────────
    Serial.println("[INFO] [SYSTEM] =============================================");
    Serial.println("[INFO] [SYSTEM] Square - Smart Parcel Box by REN");
    Serial.println("[INFO] [SYSTEM] =============================================");
    Serial.println("[INFO] [SYSTEM] Starting up. WS2812 pattern: RED-BLUE-BLINK");

    // ── Initialize WS2812 first (startup indicator) ─────────────────────────
    ws2812_init();
    ws2812_set_pattern(WS2812_RED_BLUE_BLINK);

    // ── Initialize I2C bus ──────────────────────────────────────────────────
    Wire.begin(PIN_SDA, PIN_SCL);

    // ── Initialize relay (starts locked = safe default per SFS-001) ─────────
    relay_init();

    // ── Initialize flash LED ────────────────────────────────────────────────
    flash_led_init();

    // ── Initialize reed switch ──────────────────────────────────────────────
    reed_switch_init();

    // ── Initialize Keypad via PCF8574 at 0x20 ───────────────────────────────
    hw_keypad_ok = keypad_init(Wire);
    Serial.printf("[INFO] [HARDWARE] Init Keypad PCF8574 at 0x20: %s\n", hw_keypad_ok ? "SUCCESS" : "FAILED");

    // ── Initialize LED Indicator via PCF8574 at 0x21 ────────────────────────
    hw_led_ind_ok = led_indicator_init(Wire);
    Serial.printf("[INFO] [HARDWARE] Init LED PCF8574 at 0x21: %s\n", hw_led_ind_ok ? "SUCCESS" : "FAILED");

    // ── Initialize ToF sensor VL53L0X at 0x29 ───────────────────────────────
    hw_tof_ok = tof_init(Wire);
    Serial.printf("[INFO] [HARDWARE] Init ToF VL53L0X at 0x29: %s\n", hw_tof_ok ? "SUCCESS" : "FAILED");

    // ── Initialize Camera OV2640 ────────────────────────────────────────────
    hw_camera_ok = camera_init();
    Serial.printf("[INFO] [HARDWARE] Init OV2640 Camera: %s\n", hw_camera_ok ? "SUCCESS" : "FAILED");

    // ── Initialize NVS ──────────────────────────────────────────────────────
    hw_nvs_ok = nvs_store_init();
    if (hw_nvs_ok) {
        // DBG-011: Only display pin count if NVS init succeeded
        int pin_count = nvs_store_get_pin_count();
        Serial.printf("[INFO] [NVS] Init NVS: SUCCESS. Found %d active pins.\n", pin_count);
    } else {
        Serial.println("[INFO] [NVS] Init NVS: FAILED.");
    }

    // ── Initialize MicroSD Card ─────────────────────────────────────────────
    hw_sdcard_ok = sdcard_init();
    if (hw_sdcard_ok) {
        // DBG-010: Only display card info if init succeeded
        Serial.printf("[INFO] [SD_CARD] Init MicroSD: SUCCESS. Type: %s, Size: %u MB.\n",
                      sdcard_get_type_string(), sdcard_get_size_mb());
        // DBG-008: Perform write-read verification test
        bool write_test = sdcard_write_test();
        Serial.printf("[INFO] [SD_CARD] Performing write test: %s\n", write_test ? "PASSED" : "FAILED");
        if (!write_test) hw_sdcard_ok = false;
    } else {
        Serial.println("[INFO] [SD_CARD] Init MicroSD: FAILED.");
    }

    // ── Create RTOS IPC Objects ──────────────────────────────────────────────
    RTDBQueue    = xQueueCreate(RTDB_QUEUE_DEPTH, sizeof(RTDBWriteRequest));
    CommandQueue = xQueueCreate(COMMAND_QUEUE_DEPTH, sizeof(RemoteCommand));
    I2C_Mutex    = xSemaphoreCreateMutex();
    SD_Mutex     = xSemaphoreCreateMutex();
    NVS_Mutex    = xSemaphoreCreateMutex();

    if (!RTDBQueue || !CommandQueue || !I2C_Mutex || !SD_Mutex || !NVS_Mutex) {
        Serial.println("[ERROR] [SYSTEM] Failed to create RTOS IPC objects!");
    }

    // ── Initialize State Machine ────────────────────────────────────────────
    sm_init();

    // ── Network Initialization (WiFi + NTP + Firebase) ──────────────────────
    if (hw_led_ind_ok) {
        led_indicator_alternating_pattern();
        Serial.println("[INFO] [SYSTEM] LED Expander alternating pattern: ON");
    }

    hw_wifi_ok = false;
    hw_firebase_ok = false;

    Serial.printf("[INFO] [WIFI] Connecting to %s (1/%d)...\n", WIFI_SSID, WIFI_MAX_RETRIES);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < WIFI_MAX_RETRIES) {
        delay(WIFI_RETRY_DELAY_MS);
        retries++;
        if (retries % 5 == 0 && retries < WIFI_MAX_RETRIES) {
            Serial.printf("[INFO] [WIFI] Connecting to %s (%d/%d)...\n", WIFI_SSID, retries + 1, WIFI_MAX_RETRIES);
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        hw_wifi_ok = true;
        Serial.printf("[SUCCESS] [WIFI] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[INFO] [WIFI] Init Wi-Fi module: SUCCESS. MAC: %s\n", fb_rtdb_get_mac_address().c_str());
        
        ws2812_set_pattern(WS2812_SOLID_BLUE);
        Serial.println("[INFO] [SYSTEM] WS2812 Set to: SOLID_BLUE.");
        
        if (hw_led_ind_ok) {
            led_indicator_stop_pattern();
            led_indicator_set_count(0);
            Serial.println("[INFO] [SYSTEM] LED Expander alternating pattern: OFF");
        }

        configTime(7 * 3600, 0, "pool.ntp.org", "id.pool.ntp.org");
        struct tm timeinfo;
        if(getLocalTime(&timeinfo, 5000)) {
            Serial.println("[INFO] [SYSTEM] Syncing NTP pool.ntp.org: SUCCESS");
        } else {
            Serial.println("[INFO] [SYSTEM] Syncing NTP pool.ntp.org: FAILED");
        }

        hw_firebase_ok = fb_rtdb_init();
    } else {
        Serial.printf("[INFO] [WIFI] Connecting to %s (%d/%d)... FAILED.\n", WIFI_SSID, WIFI_MAX_RETRIES, WIFI_MAX_RETRIES);
        if (hw_led_ind_ok) {
            led_indicator_stop_pattern();
            led_indicator_set_count(0);
            Serial.println("[INFO] [SYSTEM] LED Expander alternating pattern: OFF");
        }
    }

    // ── Determine final system state based on init results ───────────────────
    // Per FSD §6.6 and §6.7:
    if (hw_firebase_ok) {
        // Firebase connected — initialize storage and create device entry
        fb_storage_init();
        fb_rtdb_create_device_entry();
        fb_rtdb_update_heartbeat();

        if (has_hw_warnings()) {
            // Connected but hardware issues (except WiFi)
            ws2812_set_pattern(WS2812_SOLID_YELLOW);
            Serial.println("[INFO] [SYSTEM] WS2812 Set to: SOLID_YELLOW.");
            Serial.println("[INFO] [SYSTEM] Initialization completed with warnings. -> [STATE_IDLE]");
        } else {
            // Everything OK
            ws2812_set_pattern(WS2812_SOLID_GREEN);
            Serial.println("[INFO] [SYSTEM] WS2812 Set to: SOLID_GREEN.");
            Serial.println("[INFO] [SYSTEM] Initialization completed. -> [STATE_IDLE]");
        }
        sm_transition_to(STATE_IDLE);

    } else {
        // WiFi or Firebase failed — check NVS for offline operation
        Serial.println("[WARN] [FIREBASE] Network unavailable. Skipping authentication.");

        int active_pins = 0;
        if (hw_nvs_ok) {
            active_pins = nvs_store_get_pin_count();
        }

        if (hw_nvs_ok && active_pins > 0) {
            // Offline mode with active PINs — can still authenticate locally
            Serial.printf("[INFO] [NVS] Checking active pins... Found %d active pins. There's still hope!\n", active_pins);
            ws2812_set_pattern(WS2812_SOLID_PURPLE);
            Serial.println("[WARN] [SYSTEM] Entering Offline Mode. WS2812 Set to: SOLID_PURPLE.");
            Serial.println("[INFO] [SYSTEM] Initialization completed with warnings. -> [STATE_IDLE]");
            sm_transition_to(STATE_IDLE);
        } else {
            // Total isolation — no network AND no local PINs
            if (!hw_nvs_ok) {
                Serial.println("[INFO] [NVS] NVS failed to initialize.");
            } else {
                Serial.println("[INFO] [NVS] Checking active pins... 0 active pins found.");
            }
            Serial.println("[ERROR] [SYSTEM] No network and no active pins! System isolated.");
            ws2812_set_pattern(WS2812_SOLID_RED);
            Serial.println("[ERROR] [SYSTEM] WS2812 Set to: SOLID_RED.");
            Serial.println("[INFO] [SYSTEM] Halting operations. -> [STATE_ERROR]");
            sm_transition_to(STATE_ERROR);
        }
    }

    // ── Create FreeRTOS Tasks (per FSD §6.4) ────────────────────────────────
    // Tasks are created regardless of state — they check sm_get_state() internally
    xTaskCreatePinnedToCore(task_network_entry,     "Task_Network",     8192, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(task_keypad_led_entry,   "Task_Keypad_LED",  4096, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(task_sm_entry,           "Task_StateMachine", 8192, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(task_media_entry,         "Task_Media",       8192, NULL, 1, NULL, 1);

    // Enable keypad scanning (only if we're in IDLE state)
    if (sm_get_state() == STATE_IDLE) {
        task_keypad_led_set_enabled(true);
    }

    Serial.println("[INFO] [SYSTEM] All FreeRTOS tasks created. Scheduler running.");
}

// ── Arduino loop() — empty in production RTOS architecture ──────────────────
// All work is handled by FreeRTOS tasks.
void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
