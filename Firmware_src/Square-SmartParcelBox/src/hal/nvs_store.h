#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>

bool nvs_store_init();
int nvs_store_get_pin_count();
bool nvs_store_has_pins();
bool nvs_store_verify_pin(const char* pin);
int nvs_store_increment_used(const char* pin);  // Returns new used_quota, or -1 on failure
int nvs_store_get_remaining_quota(const char* pin);
bool nvs_store_save_pin(const char* pin, int quota, int used_quota);
bool nvs_store_save_all_pins(const char* json_str);
String nvs_store_get_all_pins_json();
bool nvs_store_delete_pin(const char* pin);
void nvs_store_clear_all();
bool nvs_store_is_available();
