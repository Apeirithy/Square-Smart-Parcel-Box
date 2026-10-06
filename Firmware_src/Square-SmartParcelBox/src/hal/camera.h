#ifndef HAL_CAMERA_H
#define HAL_CAMERA_H

#include <Arduino.h>
#include "esp_camera.h"

bool camera_init();
camera_fb_t* camera_capture_photo();
void camera_return_fb(camera_fb_t *fb);
void camera_set_resolution_photo();
void camera_set_resolution_video();
bool camera_is_available();

#endif // HAL_CAMERA_H
