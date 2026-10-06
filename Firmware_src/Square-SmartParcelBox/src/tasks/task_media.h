#ifndef TASK_MEDIA_H
#define TASK_MEDIA_H

#include <Arduino.h>

void task_media_entry(void* pvParameters);
void task_media_request_photo(const char* timestamp);
void task_media_request_ondemand_photo(const char* timestamp);
void task_media_request_start_recording(const char* timestamp);
void task_media_request_stop_recording(const char* final_timestamp);
bool task_media_is_recording();
bool task_media_is_busy();

#endif // TASK_MEDIA_H
