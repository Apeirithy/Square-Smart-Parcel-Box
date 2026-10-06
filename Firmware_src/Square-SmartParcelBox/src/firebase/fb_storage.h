#ifndef FB_STORAGE_H
#define FB_STORAGE_H

#include <Arduino.h>

bool   fb_storage_init();
bool   fb_storage_upload_file(const String& local_filepath, const String& timestamp, const String& content_type, const String& delivery_log_id);
bool   fb_storage_is_available();
bool   fb_storage_is_uploading();
void   fb_storage_loop();

#endif // FB_STORAGE_H
