#include "task_network.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <FirebaseClient.h>

#include "../rtos_types.h"
#include "../config.h"
#include "../sdcard_paths.h"

// Firebase headers
#include "../firebase/fb_rtdb.h"
#include "../firebase/fb_storage.h"

#include "../state/state_machine.h"

// HAL headers
#include "../hal/nvs_store.h"
#include "../hal/sdcard.h"

// ── Extern IPC objects (owned by main.cpp) ──────────────────────────────────
extern FirebaseApp        app;
extern QueueHandle_t      RTDBQueue;
extern QueueHandle_t      CommandQueue;
extern SemaphoreHandle_t  SD_Mutex;
extern SemaphoreHandle_t  NVS_Mutex;

// ── Retry tracking for file uploads ─────────────────────────────────────────
static int upload_retry_count = 0;
static unsigned long upload_retry_time = 0;

// ── Forward declarations ────────────────────────────────────────────────────
static void process_rtdb_queue();
static void process_upload_queue();
static void send_heartbeat();

// ── Task entry point ────────────────────────────────────────────────────────
void task_network_entry(void* pvParameters) {
    (void)pvParameters;

    Serial.println("[INFO] [SYSTEM] Network task started");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(RTOS_MIN_DELAY_MS));

        // Maintain Firebase connections
        app.loop();
        fb_rtdb_loop();
        fb_storage_loop();
        fb_rtdb_start_stream();

        // Process RTDB write queue
        process_rtdb_queue();

        // Process upload queue
        process_upload_queue();

        // Periodic heartbeat
        send_heartbeat();
    }
}

// ── process_rtdb_queue ──────────────────────────────────────────────────────
static void process_rtdb_queue() {
    RTDBWriteRequest req;

    // Non-blocking receive
    if (xQueueReceive(RTDBQueue, &req, 0) != pdTRUE) {
        return;
    }

    switch (req.type) {
        case RTDB_HEARTBEAT:
            Serial.println("[INFO] [FIREBASE] Processing RTDB_HEARTBEAT");
            fb_rtdb_update_heartbeat();
            break;

        case RTDB_STATUS_IS_OPEN:
            Serial.printf("[INFO] [FIREBASE] Status isOpen updated to %s.\n", req.value_bool ? "true" : "false");
            fb_rtdb_update_is_open(req.value_bool);
            break;

        case RTDB_STATUS_IS_UNLOCKED:
            Serial.printf("[INFO] [FIREBASE] Status isUnlocked updated to %s.\n", req.value_bool ? "true" : "false");
            fb_rtdb_update_is_unlocked(req.value_bool);
            break;

        case RTDB_STATUS_IS_FULL:
            Serial.printf("[INFO] [FIREBASE] Status isFull updated to %s.\n", req.value_bool ? "true" : "false");
            fb_rtdb_update_is_full(req.value_bool);
            break;

        case RTDB_DELIVERY_LOG: {
            String timestamp = String(req.value_str);
            String pin = String(req.key);
            String event_type = req.value_bool ? "OnDemandSnapshot" : "PackageInsertion";
            Serial.printf("[INFO] [FIREBASE] Adding new delivery logs entry...\n");
            fb_rtdb_add_delivery_log(timestamp, pin, event_type, "");
            Serial.printf("[SUCCESS] [FIREBASE] Delivery logs entry added\n");
            break;
        }

        case RTDB_CLEAR_COMMAND:
            Serial.printf("[INFO] [FIREBASE] Processing RTDB_CLEAR_COMMAND: field=%s\n", req.key);
            fb_rtdb_clear_command(String(req.key));
            break;

        case RTDB_INCREMENT_PIN:
            Serial.printf("[INFO] [FIREBASE] Processing RTDB_INCREMENT_PIN: pin=%s used_quota=%d (from NVS)\n", req.key, req.value_int);
            fb_rtdb_set_pin_used_quota(String(req.key), req.value_int);
            break;

        case RTDB_DELETE_PIN:
            Serial.printf("[INFO] [FIREBASE] Processing RTDB_DELETE_PIN: pin=%s\n", req.key);
            fb_rtdb_delete_pin(String(req.key));
            break;

        case RTDB_UPDATE_VIDEO_URL:
            Serial.printf("[INFO] [FIREBASE] Processing RTDB_UPDATE_VIDEO_URL: log_id=%s, url=%s\n", req.key, req.value_str);
            fb_rtdb_update_delivery_video_url(String(req.key), String(req.value_str));
            break;
    }
}

// ── process_upload_queue ────────────────────────────────────────────────────
static void process_upload_queue() {
    if (sm_get_state() != STATE_IDLE) { return; }

    // Only one upload at a time
    if (fb_storage_is_uploading()) {
        return;
    }

    // Check if we are in backoff period
    if (upload_retry_time > 0 && millis() < upload_retry_time) {
        return;
    }

    // Peek the front of the upload queue
    xSemaphoreTake(SD_Mutex, portMAX_DELAY);
    String front = sdcard_queue_peek_front();
    xSemaphoreGive(SD_Mutex);

    if (front.length() == 0) {
        return; // Queue empty
    }

    // Check if the file actually exists on the SD card
    xSemaphoreTake(SD_Mutex, portMAX_DELAY);
    bool exists = sdcard_file_exists(front.c_str());
    xSemaphoreGive(SD_Mutex);

    if (!exists) {
        Serial.printf("[WARN] [SD_CARD] File missing: %s. Removing from upload queue.\n", front.c_str());
        xSemaphoreTake(SD_Mutex, portMAX_DELAY);
        sdcard_queue_pop_front();
        xSemaphoreGive(SD_Mutex);
        
        // Reset retry mechanism so the next file processes immediately
        upload_retry_count = 0;
        upload_retry_time = 0;
        return;
    }

    if (!fb_storage_is_available()) {
        return; // Firebase Storage not ready
    }

    // Parse filename to extract timestamp and content_type
    String content_type;
    bool is_photo = true;
    if (front.endsWith(".jpg") || front.endsWith(".jpeg")) {
        content_type = "image/jpeg";
        is_photo = true;
    } else if (front.endsWith(".mjpeg")) {
        content_type = "video/x-motion-jpeg";
        is_photo = false;
    } else {
        content_type = "application/octet-stream";
    }

    // Extract timestamp from the path: /<timestamp>/img_<timestamp>.jpg
    String timestamp = "";
    int first_slash = front.indexOf('/', 1); // Skip leading /
    if (first_slash > 1) {
        timestamp = front.substring(1, first_slash);
    } else {
        // Fallback: use the filename part
        timestamp = fb_rtdb_get_timestamp_string();
    }

    // delivery_log_id is the timestamp
    String delivery_log_id = timestamp;

    Serial.printf("[INFO] [FIREBASE] Starting upload: %s (type=%s, ts=%s)\n", front.c_str(), content_type.c_str(), timestamp.c_str());

    bool started = fb_storage_upload_file(front, timestamp, content_type, delivery_log_id);

    if (started) {
        // On successful start, we pop it (the async callback handles completion)
        xSemaphoreTake(SD_Mutex, portMAX_DELAY);
        sdcard_queue_pop_front();
        xSemaphoreGive(SD_Mutex);
        Serial.printf("[SUCCESS] [FIREBASE] Upload started, popped from queue: %s\n", front.c_str());
        
        // Reset retry mechanism
        upload_retry_count = 0;
        upload_retry_time = 0;
    } else {
        Serial.printf("[ERROR] [FIREBASE] Upload failed to start: %s\n", front.c_str());
        
        // Apply backoff logic
        upload_retry_count++;
        unsigned long delay_ms = (upload_retry_count == 1) ? RETRY_FIRST_FAIL_MS : RETRY_SUBSEQUENT_MS;
        upload_retry_time = millis() + delay_ms;
        Serial.printf("[INFO] [SYSTEM] Retrying upload in %lu ms\n", delay_ms);
    }
}

// ── send_heartbeat ──────────────────────────────────────────────────────────
static unsigned long last_heartbeat_time = 0;

static void send_heartbeat() {
    unsigned long now = millis();
    if (now - last_heartbeat_time >= HEARTBEAT_INTERVAL_MS || last_heartbeat_time == 0) {
        last_heartbeat_time = now;
        if (fb_rtdb_is_connected()) {
            fb_rtdb_update_heartbeat();
            Serial.printf("[INFO] [FIREBASE] Sending heartbeat (%s)... SUCCESS\n", fb_rtdb_get_timestamp_string().c_str());
        }
    }
}
