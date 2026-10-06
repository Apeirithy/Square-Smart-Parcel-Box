#include "task_media.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "../hal/camera.h"
#include "../hal/sdcard.h"
#include "../hal/flash_led.h"
#include "../config.h"
#include "../sdcard_paths.h"

extern SemaphoreHandle_t SD_Mutex;

// ── Internal request types ──────────────────────────────────────────────────
enum MediaRequestType {
    MEDIA_NONE,
    MEDIA_PHOTO,
    MEDIA_PHOTO_ONDEMAND,
    MEDIA_START_RECORDING,
    MEDIA_STOP_RECORDING
};

struct MediaRequest {
    MediaRequestType type;
    char timestamp[64];
};

// ── Static state ────────────────────────────────────────────────────────────
static TaskHandle_t s_media_task_handle = NULL;
static volatile MediaRequest s_pending_request = { MEDIA_NONE, {0} };
static volatile bool s_is_recording = false;
static volatile bool s_stop_recording_flag = false;
static volatile bool s_is_busy = false;
static char s_final_timestamp[64] = "";

// ── Public API ──────────────────────────────────────────────────────────────

TaskHandle_t task_media_get_handle() {
    return s_media_task_handle;
}

void task_media_request_photo(const char* timestamp) {
    s_pending_request.type = MEDIA_PHOTO;
    strncpy((char*)s_pending_request.timestamp, timestamp, sizeof(s_pending_request.timestamp) - 1);
    ((char*)s_pending_request.timestamp)[sizeof(s_pending_request.timestamp) - 1] = '\0';
    if (s_media_task_handle) {
        xTaskNotifyGive(s_media_task_handle);
    }
}

void task_media_request_ondemand_photo(const char* timestamp) {
    s_pending_request.type = MEDIA_PHOTO_ONDEMAND;
    strncpy((char*)s_pending_request.timestamp, timestamp, sizeof(s_pending_request.timestamp) - 1);
    ((char*)s_pending_request.timestamp)[sizeof(s_pending_request.timestamp) - 1] = '\0';
    if (s_media_task_handle) {
        xTaskNotifyGive(s_media_task_handle);
    }
}

void task_media_request_start_recording(const char* timestamp) {
    s_pending_request.type = MEDIA_START_RECORDING;
    strncpy((char*)s_pending_request.timestamp, timestamp, sizeof(s_pending_request.timestamp) - 1);
    ((char*)s_pending_request.timestamp)[sizeof(s_pending_request.timestamp) - 1] = '\0';
    if (s_media_task_handle) {
        xTaskNotifyGive(s_media_task_handle);
    }
}

void task_media_request_stop_recording(const char* final_timestamp) {
    if (final_timestamp) {
        strncpy(s_final_timestamp, final_timestamp, sizeof(s_final_timestamp) - 1);
        s_final_timestamp[sizeof(s_final_timestamp) - 1] = '\0';
    } else {
        s_final_timestamp[0] = '\0';
    }
    s_stop_recording_flag = true;
    // Also notify to wake task if it's in between frames
    if (s_media_task_handle) {
        xTaskNotifyGive(s_media_task_handle);
    }
}

bool task_media_is_recording() {
    return s_is_recording;
}

bool task_media_is_busy() {
    return s_is_busy || s_pending_request.type != MEDIA_NONE;
}

// ── Internal helpers ────────────────────────────────────────────────────────

static void handle_photo_capture(const char* timestamp, bool ondemand) {
    Serial.printf("[INFO] [CAMERA] Capturing %s photo (ts=%s)\n", ondemand ? "on-demand" : "delivery", timestamp);

    // CVL-012: No capture if camera init failed
    if (!camera_is_available()) {
        Serial.println("[ERROR] [CAMERA] Camera failed! No video and photo capture!");
        return;
    }
    // CVL-010: No capture if SD card unavailable
    if (!sdcard_is_available()) {
        Serial.println("[ERROR] [SD_CARD] MicroSD failed! No video and photo capture!");
        return;
    }

    // CVL-011: Only turn on flash if camera is available
    //vTaskDelay(pdMS_TO_TICKS(3000));
    camera_set_resolution_photo();
    flash_led_on();
    Serial.println("[INFO] [CAMERA] Flash ON. Capturing photos...");
    vTaskDelay(pdMS_TO_TICKS(FLASH_WARMUP_MS));

    camera_fb_t* fb = camera_capture_photo();

    vTaskDelay(pdMS_TO_TICKS(1500));
    flash_led_off();
    Serial.println("[INFO] [CAMERA] Flash OFF.");

    if (fb == NULL || fb->len == 0) {
        Serial.println("[ERROR] [CAMERA] Photo capture FAILED");
        if (fb) camera_return_fb(fb);
        return;
    }

    Serial.printf("[INFO] [CAMERA] Photo captured: %u bytes\n", fb->len);

    // Build SD card path
    char path_buf[256];
    snprintf(path_buf, sizeof(path_buf), PATH_SDCARD_IMAGE, timestamp, timestamp);

    // Save to SD card and enqueue for upload
    xSemaphoreTake(SD_Mutex, portMAX_DELAY);
    bool saved = sdcard_save_file(path_buf, fb->buf, fb->len);
    if (saved) {
        if (ondemand) {
            sdcard_queue_push_front(path_buf);
        } else {
            sdcard_queue_push_back(path_buf);
        }
        Serial.printf("[SUCCESS] [CAMERA] Photo saved to: %s. Flash OFF.\n", path_buf);
    } else {
        Serial.printf("[ERROR] [SD_CARD] Failed to save: %s\n", path_buf);
    }
    xSemaphoreGive(SD_Mutex);

    camera_return_fb(fb);
}

static void handle_video_recording(const char* timestamp) {
    Serial.printf("[INFO] [CAMERA] Starting video recording (ts=%s)\n", timestamp);

    // CVL-012: No capture if camera init failed
    if (!camera_is_available()) {
        Serial.println("[ERROR] [CAMERA] Camera failed! No video and photo capture!");
        return;
    }
    // CVL-010: No capture if SD card unavailable
    if (!sdcard_is_available()) {
        Serial.println("[ERROR] [SD_CARD] MicroSD failed! No video and photo capture!");
        return;
    }

    s_is_recording = true;
    s_stop_recording_flag = false;

    camera_set_resolution_video();
    vTaskDelay(pdMS_TO_TICKS(400));

    // Build SD card path for video (temp)
    char path_buf[256];
    snprintf(path_buf, sizeof(path_buf), "/temp_video.mjpeg");

    // Open/create video file for the first time with WRITE mode (to clean up old recordings)
    xSemaphoreTake(SD_Mutex, portMAX_DELAY);
    const uint8_t dummy_header = 0; 
    sdcard_save_file(path_buf, &dummy_header, 0); // Membuat file kosong baru
    xSemaphoreGive(SD_Mutex);

    // Recording loop: capture frames until stop is requested
    while (!s_stop_recording_flag) {
        camera_fb_t* fb = camera_capture_photo(); 
        if (fb != NULL && fb->len > 0) {
            xSemaphoreTake(SD_Mutex, portMAX_DELAY);
            
            // FIX: Use APPEND!
            // Now frame 2, 3, etc. will be appended after frame 1
            sdcard_append_file(path_buf, fb->buf, fb->len); 
            
            xSemaphoreGive(SD_Mutex);
        }
        if (fb) camera_return_fb(fb);

        vTaskDelay(pdMS_TO_TICKS(RTOS_MIN_DELAY_MS));
    }

    // Finalize: enqueue the recorded file for upload
    xSemaphoreTake(SD_Mutex, portMAX_DELAY);

    char final_path[256];
    if (s_final_timestamp[0] != '\0') {
        snprintf(final_path, sizeof(final_path), PATH_SDCARD_VIDEO, s_final_timestamp, s_final_timestamp);
    } else {
        snprintf(final_path, sizeof(final_path), PATH_SDCARD_VIDEO, timestamp, timestamp);
    }
    
    // Delete target if it exists, then rename
    if (sdcard_file_exists(final_path)) {
        sdcard_delete_file(final_path);
    }
    sdcard_rename_file(path_buf, final_path);

    xSemaphoreGive(SD_Mutex);

    s_is_recording = false;
    Serial.printf("[INFO] [CAMERA] Recording stopped, saved to %s (not queued)\n", final_path);
}

// ── Task entry point ────────────────────────────────────────────────────────

void task_media_entry(void* pvParameters) {
    (void)pvParameters;

    s_media_task_handle = xTaskGetCurrentTaskHandle();
    Serial.println("[INFO] [CAMERA] Media task started, waiting for requests...");

    while (1) {
        // Block until notified by a request function
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Copy and consume the pending request
        MediaRequest req;
        req.type = s_pending_request.type;
        strncpy(req.timestamp, (const char*)s_pending_request.timestamp, sizeof(req.timestamp));
        s_pending_request.type = MEDIA_NONE;

        s_is_busy = true;

        switch (req.type) {
            case MEDIA_PHOTO:
                handle_photo_capture(req.timestamp, false);
                break;
            case MEDIA_PHOTO_ONDEMAND:
                handle_photo_capture(req.timestamp, true);
                break;
            case MEDIA_START_RECORDING:
                handle_video_recording(req.timestamp);
                break;
            case MEDIA_STOP_RECORDING:
                // Handled via s_stop_recording_flag inside recording loop
                break;
            default:
                break;
        }

        s_is_busy = false;
    }
}
