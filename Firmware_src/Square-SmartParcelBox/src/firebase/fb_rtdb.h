#ifndef FB_RTDB_H
#define FB_RTDB_H

#include <Arduino.h>

// ── Initialization & Connection ─────────────────────────────────────────────
bool fb_rtdb_init();
bool fb_rtdb_is_connected();
const String& fb_rtdb_get_mac_address();
String fb_rtdb_get_timestamp_string();

// ── Device Entry ────────────────────────────────────────────────────────────
void fb_rtdb_create_device_entry();

// ── Heartbeat ───────────────────────────────────────────────────────────────
void fb_rtdb_update_heartbeat();

// ── Individual Status Updates (fix for overwrite bug) ───────────────────────
void fb_rtdb_update_is_open(bool value);
void fb_rtdb_update_is_unlocked(bool value);
void fb_rtdb_update_is_full(bool value);

// ── Delivery Logs ───────────────────────────────────────────────────────────
void fb_rtdb_add_delivery_log(const String& timestamp, const String& used_pin, const String& event_type, const String& photo_url);
void fb_rtdb_update_delivery_video_url(const String& log_id, const String& video_url);
void fb_rtdb_update_delivery_photo_url(const String& log_id, const String& photo_url);

// ── Stream & Commands ───────────────────────────────────────────────────────
void fb_rtdb_loop();
void fb_rtdb_start_stream();
void fb_rtdb_clear_command(const String& command_field);

// ── PIN Management ──────────────────────────────────────────────────────────
void fb_rtdb_set_pin_used_quota(const String& pin, int used_quota);
void fb_rtdb_delete_pin(const String& pin);

// ── Pending Command Timestamps (set by stream parser) ───────────────────────
String fb_rtdb_get_pending_photo_timestamp();
String fb_rtdb_get_pending_video_timestamp();

#endif
