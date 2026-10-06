#ifndef SDCARD_H
#define SDCARD_H

#include <Arduino.h>

bool sdcard_init();

// File I/O
bool sdcard_save_file(const char* path, const uint8_t* data, size_t len);
bool sdcard_append_file(const char* path, const uint8_t* data, size_t len);
uint8_t* sdcard_read_file(const char* path, size_t *out_len);
bool sdcard_file_exists(const char* path);
bool sdcard_delete_file(const char* path);
bool sdcard_rename_file(const char* old_path, const char* new_path);
bool sdcard_delete_directory(const char* path);

// Status
uint32_t sdcard_get_free_space_mb();
uint32_t sdcard_get_size_mb();
const char* sdcard_get_type_string();
bool sdcard_is_available();

// Queue Management
void sdcard_queue_push_back(const String& filename);
void sdcard_queue_push_front(const String& filename);
String sdcard_queue_pop_front();
String sdcard_queue_peek_front();
bool sdcard_queue_remove(const String& filename);
bool sdcard_queue_is_empty();

// FIFO Cleanup
void sdcard_fifo_cleanup();

// DBG-008: Write-read verification test
bool sdcard_write_test();

#endif // SDCARD_H
