#include "task_statemachine.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "../state/state_machine.h"
#include "../rtos_types.h"
#include "../config.h"

// HAL headers
#include "../hal/relay.h"
#include "../hal/reed_switch.h"
#include "../hal/tof.h"
#include "../hal/nvs_store.h"
#include "../hal/ws2812.h"
#include "../hal/camera.h"
#include "../hal/sdcard.h"

// Firebase headers
#include "../firebase/fb_rtdb.h"

// Sibling task headers
#include "task_keypad_led.h"
#include "task_media.h"

// HAL driver for LED indicator (used in invalid PIN feedback)
#include "../hal/keybuffer_indicator_driver.h"

// ── Extern IPC objects (owned by main.cpp) ──────────────────────────────────
extern QueueHandle_t      RTDBQueue;
extern QueueHandle_t      CommandQueue;
extern SemaphoreHandle_t  I2C_Mutex;
extern SemaphoreHandle_t  SD_Mutex;
extern SemaphoreHandle_t  NVS_Mutex;

// ── File-scope state ────────────────────────────────────────────────────────
static String s_ondemand_timestamp = "";   // App-provided timestamp for on-demand photo
static bool fifo_cleanup_done = false;     // CVL-009: FIFO cleanup runs ONCE per IDLE entry

// ── Forward declarations ────────────────────────────────────────────────────
static void handle_idle();
static void handle_delivery(const char* pin_used);
static void handle_ondemand();
static void handle_error();
static void handle_remote_command(RemoteCommand cmd);
static void enqueue_rtdb(RTDBWriteType type, const char* key, const char* value_str, bool value_bool, int value_int = 0);

// ── Task entry point ────────────────────────────────────────────────────────
void task_sm_entry(void* pvParameters) {
    (void)pvParameters;

    Serial.println("[INFO] [SYSTEM] State-machine task started");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(RTOS_MIN_DELAY_MS));

        switch (sm_get_state()) {
            case STATE_IDLE:
                handle_idle();
                break;
            case STATE_DELIVERY:
                // Should not land here directly — delivery is entered via handle_idle
                break;
            case STATE_ONDEMAND:
                handle_ondemand();
                break;
            case STATE_ERROR:
                handle_error();
                break;
            default:
                // STATE_START — waiting for main.cpp to transition to IDLE
                break;
        }
    }
}

// ── handle_idle ─────────────────────────────────────────────────────────────
static void handle_idle() {
    // CVL-009: FIFO cleanup runs ONCE after entering STATE_IDLE
    if (!fifo_cleanup_done) {
        if (sdcard_is_available()) {
            xSemaphoreTake(SD_Mutex, portMAX_DELAY);
            sdcard_fifo_cleanup();
            xSemaphoreGive(SD_Mutex);
            Serial.println("[INFO] [SD_CARD] FIFO cleanup completed.");
        }
        fifo_cleanup_done = true;
    }

    // 1. Check if keypad buffer is full (4-digit PIN entered)
    if (task_keypad_led_is_buffer_full()) {
        const char* pin = task_keypad_led_get_buffer();
        Serial.printf("[INFO] [KEYPAD] Courier input buffer complete: %s\n", pin);
        Serial.printf("[INFO] [NVS] Verifying PIN %s with NVS...\n", pin);

        // Verify PIN using NVS (NVS is the source of truth)
        xSemaphoreTake(NVS_Mutex, portMAX_DELAY);
        bool valid = nvs_store_verify_pin(pin);
        xSemaphoreGive(NVS_Mutex);

        if (valid) {
            Serial.printf("[SUCCESS] [NVS] PIN %s is correct!\n", pin);

            // Copy pin before clearing buffer
            char pin_copy[PIN_LENGTH + 1];
            strncpy(pin_copy, pin, PIN_LENGTH);
            pin_copy[PIN_LENGTH] = '\0';

            task_keypad_led_clear_buffer();
            task_keypad_led_set_enabled(false);

            Serial.println("[INFO] [SYSTEM] Transitioning -> [STATE_DELIVERY]");
            sm_transition_to(STATE_DELIVERY);
            fifo_cleanup_done = false;  // Reset so FIFO cleanup runs on next IDLE entry
            handle_delivery(pin_copy);
        } else {
            Serial.println("[WARN] [NVS] Incorrect PIN. Resetting buffer.");
            task_keypad_led_clear_buffer();

            // Brief flash pattern to indicate invalid PIN
            xSemaphoreTake(I2C_Mutex, portMAX_DELAY);
            led_indicator_alternating_pattern();
            xSemaphoreGive(I2C_Mutex);

            vTaskDelay(pdMS_TO_TICKS(LED_ALT_INTERVAL_MS * 4));

            xSemaphoreTake(I2C_Mutex, portMAX_DELAY);
            led_indicator_stop_pattern();
            xSemaphoreGive(I2C_Mutex);
        }
    }

    // 2. Check CommandQueue (non-blocking)
    RemoteCommand cmd;
    if (xQueueReceive(CommandQueue, &cmd, 0) == pdTRUE) {
        handle_remote_command(cmd);
    }

    // SFS-010: Check for total isolation (no PINs + offline)
    if (!fb_rtdb_is_connected()) {
        xSemaphoreTake(NVS_Mutex, portMAX_DELAY);
        int pin_count = nvs_store_get_pin_count();
        xSemaphoreGive(NVS_Mutex);

        if (pin_count <= 0) {
            Serial.println("[ERROR] [SYSTEM] No local active pins, no connection to Firebase.");
            sm_transition_to(STATE_ERROR);
            return;
        }
    }
}

// ── handle_delivery ─────────────────────────────────────────────────────────
static void handle_delivery(const char* pin_used) {
    Serial.println("[INFO] [DOOR] Solenoid unlocked.");

    // 1. Unlock relay
    relay_unlock();

    // 2. Enqueue RTDB status: isUnlocked = true
    enqueue_rtdb(RTDB_STATUS_IS_UNLOCKED, "", "", true);

    // 3. Wait for door open with timeout
    Serial.println("[INFO] [DOOR] Waiting for door to open...");
    unsigned long start = millis();
    bool door_opened = false;
    while (millis() - start < DOOR_TIMEOUT_MS) {
        reed_switch_update();
        if (reed_switch_is_open()) {
            door_opened = true;
            Serial.println("[INFO] [DOOR] Reed switch: OPEN.");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(RTOS_MIN_DELAY_MS));
    }

    if (!door_opened) {
        Serial.println("[WARN] [DOOR] Timeout — door was never opened. Locking and returning to IDLE.");
        relay_lock();
        enqueue_rtdb(RTDB_STATUS_IS_UNLOCKED, "", "", false);
        sm_transition_to(STATE_IDLE);
        fifo_cleanup_done = false;  // Reset for next IDLE entry
        task_keypad_led_set_enabled(true);
        return;
    }

    // 4. Door opened — update isOpen = true
    enqueue_rtdb(RTDB_STATUS_IS_OPEN, "", "", true);

    // 5. CVL-002: Start video recording AFTER door opens (FSD §4.3)
    // Timestamp is initially empty; will be captured after door closes
    String recording_ts = fb_rtdb_get_timestamp_string();
    if (recording_ts == "") recording_ts = String(millis());
    task_media_request_start_recording(recording_ts.c_str());

    // 6. Wait for door close
    Serial.println("[INFO] [DOOR] Waiting for door to close...");
    while (true) {
        reed_switch_update();
        if (reed_switch_is_closed()) {
            Serial.println("[INFO] [DOOR] Reed switch: CLOSED.");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(RTOS_MIN_DELAY_MS));
    }

    // 7. Get timestamp AFTER door closes (FSD §4.3 step 5)
    String ts = fb_rtdb_get_timestamp_string();
    if (ts == "") ts = String(millis());

    // 8. Lock relay
    relay_lock();
    Serial.println("[INFO] [DOOR] Relay locked.");

    // 9. Stop video recording
    task_media_request_stop_recording(ts.c_str());

    // Wait briefly for recording to actually stop
    while (task_media_is_recording()) { vTaskDelay(pdMS_TO_TICKS(50)); }

    // 10. Capture photo
    Serial.println("[INFO] [CAMERA] Flash ON. Capturing photos...");
    task_media_request_photo(ts.c_str());

    while (task_media_is_busy()) { vTaskDelay(pdMS_TO_TICKS(50)); }

    // 11. Read ToF sensor
    xSemaphoreTake(I2C_Mutex, portMAX_DELAY);
    bool is_full = tof_is_full();
    int dist = tof_read_distance_cm();
    xSemaphoreGive(I2C_Mutex);
    Serial.printf("[INFO] [TOF] Distance: %d cm. isFull: %s (Threshold: <=%dcm)\n", dist, is_full ? "true" : "false", TOF_BOX_WIDTH_CM-TOF_TOLERANCE_CM);

    // 12. Increment used quota in NVS (NVS is the source of truth)
    //     Flow: read used_quota from NVS → increment in NVS → overwrite RTDB with NVS value
    xSemaphoreTake(NVS_Mutex, portMAX_DELAY);
    int new_used = nvs_store_increment_used(pin_used);
    int remaining = nvs_store_get_remaining_quota(pin_used);
    xSemaphoreGive(NVS_Mutex);

    Serial.printf("[INFO] [NVS] PIN %s used_quota incremented to %d.\n", pin_used, new_used);

    // 13. Enqueue RTDB updates
    enqueue_rtdb(RTDB_STATUS_IS_OPEN, "", "", false);
    enqueue_rtdb(RTDB_STATUS_IS_UNLOCKED, "", "", false);
    enqueue_rtdb(RTDB_STATUS_IS_FULL, "", "", is_full);
    enqueue_rtdb(RTDB_DELIVERY_LOG, pin_used, ts.c_str(), false);  // false = PackageInsertion
    // Pass NVS-sourced used_quota to RTDB (NVS is the source of truth)
    enqueue_rtdb(RTDB_INCREMENT_PIN, pin_used, "", false, new_used);

    Serial.printf("[INFO] [FIREBASE] PIN %s used_quota queued for RTDB update (value=%d from NVS).\n", pin_used, new_used);

    // ACC-007: Delete exhausted PINs
    if (remaining <= 0) {
        Serial.printf("[INFO] [NVS] PIN %s quota exhausted. Deleting...\n", pin_used);
        xSemaphoreTake(NVS_Mutex, portMAX_DELAY);
        nvs_store_delete_pin(pin_used);
        xSemaphoreGive(NVS_Mutex);
        enqueue_rtdb(RTDB_DELETE_PIN, pin_used, "", false);
    }

    // 14. Transition back to IDLE, re-enable keypad
    sm_transition_to(STATE_IDLE);
    fifo_cleanup_done = false;  // Reset so FIFO cleanup runs on next IDLE entry
    task_keypad_led_set_enabled(true);
    Serial.println("[INFO] [SYSTEM] Delivery complete. Transitioning -> [STATE_IDLE]");
}

// ── handle_remote_command ───────────────────────────────────────────────────
static void handle_remote_command(RemoteCommand cmd) {
    switch (cmd) {
        case CMD_UNLOCK:
            Serial.println("[INFO] [FIREBASE] Remote CMD_UNLOCK received.");
            relay_unlock();
            enqueue_rtdb(RTDB_STATUS_IS_UNLOCKED, "", "", true);
            enqueue_rtdb(RTDB_CLEAR_COMMAND, "unlock", "", false);
            break;

        case CMD_LOCK:
            Serial.println("[INFO] [FIREBASE] Remote CMD_LOCK received.");
            relay_lock();
            enqueue_rtdb(RTDB_STATUS_IS_UNLOCKED, "", "", false);
            enqueue_rtdb(RTDB_CLEAR_COMMAND, "lock", "", false);
            break;

        case CMD_LOCK_AND_UNLOCK:
            // Conflict resolution: prioritise lock (Section 5.1)
            Serial.println("[INFO] [FIREBASE] Remote CMD_LOCK_AND_UNLOCK — prioritising lock.");
            relay_lock();
            enqueue_rtdb(RTDB_STATUS_IS_UNLOCKED, "", "", false);
            enqueue_rtdb(RTDB_CLEAR_COMMAND, "lock", "", false);
            enqueue_rtdb(RTDB_CLEAR_COMMAND, "unlock", "", false);
            break;

        case CMD_REQUEST_PHOTO: {
            Serial.println("[INFO] [FIREBASE] On-demand snapshot request received!");
            String photo_ts = fb_rtdb_get_pending_photo_timestamp();
            if (photo_ts.length() > 0) {
                Serial.printf("[INFO] [FIREBASE] On-demand snapshot request received! Timestamp: %s\n", photo_ts.c_str());
                // Store timestamp for use in handle_ondemand
                s_ondemand_timestamp = photo_ts;
            }
            Serial.println("[INFO] [SYSTEM] Transitioning -> [STATE_ONDEMAND]");
            sm_transition_to(STATE_ONDEMAND);
            fifo_cleanup_done = false;  // Reset for next IDLE entry
            break;
        }

        case CMD_REQUEST_VIDEO: {
            Serial.println("[INFO] [FIREBASE] On-demand video request received");
            String video_ts = fb_rtdb_get_pending_video_timestamp();
            if (video_ts.length() == 0) {
                Serial.println("[ERROR] [FIREBASE] No video timestamp available");
                break;
            }
            Serial.printf("[INFO] [FIREBASE] On-demand video request received for timestamp: %s\n", video_ts.c_str());

            // Check if online — abort if offline (FSD §4.5)
            if (!fb_rtdb_is_connected()) {
                Serial.printf("[ERROR] [FIREBASE] vid_%s failed to upload. Aborting...\n", video_ts.c_str());
                enqueue_rtdb(RTDB_CLEAR_COMMAND, "request_video_id", "", false);
                break;
            }

            // Construct video path and push to front of upload queue
            char vid_path[64];
            snprintf(vid_path, sizeof(vid_path), "/%s/vid_%s.mjpeg", video_ts.c_str(), video_ts.c_str());

            xSemaphoreTake(SD_Mutex, portMAX_DELAY);
            bool exists = sdcard_file_exists(vid_path);
            if (exists) {
                sdcard_queue_push_front(vid_path);
                Serial.printf("[INFO] [SD_CARD] Pushing vid_%s.mjpeg to front of upload_queue.txt\n", video_ts.c_str());
            }
            xSemaphoreGive(SD_Mutex);

            if (!exists) {
                Serial.printf("[ERROR] [SD_CARD] Video file not found: %s\n", vid_path);
            }

            // Clear command after processing
            enqueue_rtdb(RTDB_CLEAR_COMMAND, "request_video_id", "", false);
            break;
        }

        default:
            break;
    }
}

// ── handle_ondemand ─────────────────────────────────────────────────────────
static void handle_ondemand() {
    Serial.println("[INFO] [SYSTEM] Transitioning -> [STATE_ONDEMAND]");

    // Use app-provided timestamp if available, otherwise generate
    String ts = s_ondemand_timestamp;
    if (ts.length() == 0) {
        ts = fb_rtdb_get_timestamp_string();
        if (ts == "") ts = String(millis());
    }
    s_ondemand_timestamp = "";  // Reset

    Serial.printf("[INFO] [FIREBASE] On-demand snapshot request received! Timestamp: %s\n", ts.c_str());

    // CVL-010/CVL-012: Check camera and SD availability
    if (!camera_is_available() || !sdcard_is_available()) {
        if (!camera_is_available()) Serial.println("[ERROR] [CAMERA] Camera failed! No video and photo capture!");
        if (!sdcard_is_available()) Serial.println("[ERROR] [SD_CARD] MicroSD failed! No video and photo capture!");
        Serial.println("[ERROR] [SYSTEM] Snapshot sequence aborted. -> [STATE_IDLE]");
        enqueue_rtdb(RTDB_CLEAR_COMMAND, "request_photo_ondemand", "", false);
        sm_transition_to(STATE_IDLE);
        fifo_cleanup_done = false;  // Reset for next IDLE entry
        return;
    }

    // 1. Request on-demand photo from task_media (pushed to FRONT of queue)
    task_media_request_ondemand_photo(ts.c_str());
    if (task_media_is_busy()){
        Serial.println("[INFO] [SYSTEM] Waiting for camera capture process to finish...");
        while (task_media_is_busy()) { 
            vTaskDelay(pdMS_TO_TICKS(50)); 
        }
    }
    
    // 2. Read ToF sensor
    xSemaphoreTake(I2C_Mutex, portMAX_DELAY);
    bool is_full = tof_is_full();
    int dist = tof_read_distance_cm();
    xSemaphoreGive(I2C_Mutex);
    Serial.printf("[INFO] [TOF] Distance: %d cm. isFull: %s (Threshold: <38cm)\n", dist, is_full ? "true" : "false");

    // 3. Enqueue RTDB status
    enqueue_rtdb(RTDB_STATUS_IS_FULL, "", "", is_full);

    // 4. Create delivery log entry for on-demand snapshot (FSD §7.1)
    enqueue_rtdb(RTDB_DELIVERY_LOG, "", ts.c_str(), true);  // true = OnDemandSnapshot

    // 5. Check if online for upload decision
    if (!fb_rtdb_is_connected()) {
        // FSD §4.4 Step 5: If offline, cancel log and delete photo
        Serial.println("[ERROR] [SYSTEM] Snapshot sequence aborted (offline). -> [STATE_IDLE]");
    } else {
        Serial.println("[SUCCESS] [FIREBASE] Snapshot logs successfully uploaded.");
    }

    // 6. Clear the on-demand command
    enqueue_rtdb(RTDB_CLEAR_COMMAND, "request_photo_ondemand", "", false);

    // 7. Transition back to IDLE
    sm_transition_to(STATE_IDLE);
    fifo_cleanup_done = false;  // Reset for next IDLE entry
    Serial.println("[INFO] [SYSTEM] Snapshot sequence complete. -> [STATE_IDLE]");
}

// ── handle_error (DBG-012) ──────────────────────────────────────────────────
static void handle_error() {
    ws2812_set_pattern(WS2812_SOLID_RED);

    // Infinite loop — STATE_ERROR is a dead state
    while (true) {
        Serial.println("[ERROR] [SYSTEM] Fatal error occurred! No local active pins, no connection to Firebase. Please power cycle.");
        vTaskDelay(pdMS_TO_TICKS(ERROR_LOG_INTERVAL_MS));
    }
}

// ── Helper: enqueue RTDB write request ──────────────────────────────────────
static void enqueue_rtdb(RTDBWriteType type, const char* key, const char* value_str, bool value_bool, int value_int) {
    RTDBWriteRequest req;
    memset(&req, 0, sizeof(req));
    req.type = type;
    req.value_bool = value_bool;
    req.value_int = value_int;

    if (key != NULL) {
        strncpy(req.key, key, sizeof(req.key) - 1);
        req.key[sizeof(req.key) - 1] = '\0';
    }

    if (value_str != NULL) {
        strncpy(req.value_str, value_str, sizeof(req.value_str) - 1);
        req.value_str[sizeof(req.value_str) - 1] = '\0';
    }

    if (xQueueSend(RTDBQueue, &req, pdMS_TO_TICKS(100)) != pdTRUE) {
        Serial.printf("[WARN] [SYSTEM] RTDBQueue full, dropping write type=%d key=%s\n", type, key ? key : "null");
    }
}
