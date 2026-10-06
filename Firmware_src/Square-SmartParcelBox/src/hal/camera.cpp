#include "camera.h"
#include "../pins.h"

static bool s_camera_available = false;

bool camera_init() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = PIN_CAM_D0;
    config.pin_d1 = PIN_CAM_D1;
    config.pin_d2 = PIN_CAM_D2;
    config.pin_d3 = PIN_CAM_D3;
    config.pin_d4 = PIN_CAM_D4;
    config.pin_d5 = PIN_CAM_D5;
    config.pin_d6 = PIN_CAM_D6;
    config.pin_d7 = PIN_CAM_D7;
    config.pin_xclk = PIN_CAM_XCLK;
    config.pin_pclk = PIN_CAM_PCLK;
    config.pin_vsync = PIN_CAM_VSYNC;
    config.pin_href = PIN_CAM_HREF;
    config.pin_sccb_sda = PIN_CAM_SIOD;
    config.pin_sccb_scl = PIN_CAM_SIOC;
    config.pin_pwdn = -1;
    config.pin_reset = -1;
    config.sccb_i2c_port = 1;
    config.xclk_freq_hz = 10000000; // Lowered to 10MHz for stability
    config.pixel_format = PIXFORMAT_JPEG;

    if (psramFound()) {
        config.frame_size = FRAMESIZE_UXGA;
        config.jpeg_quality = 12; // Lowered quality a bit to save memory
        config.fb_count = 2; // Double Buffering
        config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
        config.frame_size = FRAMESIZE_SVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        s_camera_available = false;
        return false;
    }
    s_camera_available = true;
    return true;
}

camera_fb_t* camera_capture_photo() {
    if (!s_camera_available) return NULL;
    return esp_camera_fb_get();
}

void camera_return_fb(camera_fb_t *fb) {
    if (fb) {
        esp_camera_fb_return(fb);
    }
}

void camera_set_resolution_photo() {
    if (!s_camera_available) return;
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_framesize(s, FRAMESIZE_UXGA);
    }
}

void camera_set_resolution_video() {
    if (!s_camera_available) return;
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_framesize(s, FRAMESIZE_VGA);
    }
}

bool camera_is_available() {
    return s_camera_available;
}
