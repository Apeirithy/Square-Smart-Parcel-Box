#include "fb_rtdb.h"
#include <time.h>
#include <WiFi.h>
#include <FirebaseClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "../state/state_machine.h"
#include "credentials.h"
#include "config.h"
#include "hal/nvs_store.h"
#include "../rtdb_paths.h"

extern QueueHandle_t CommandQueue;

FirebaseApp app;
RealtimeDatabase Database;

// For non-blocking operations (set, push)
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);

// Streaming for remote_sync
WiFiClientSecure stream_ssl_client;
AsyncClient stream_aClient(stream_ssl_client);
static bool stream_started = false;

// Thread-safe cached Firebase connection status
static volatile bool s_app_ready_cached = false;

UserAuth user_auth(FIREBASE_WEBAPI_KEY, DEVICE_EMAIL, DEVICE_PASSWORD);

// ── Pending command timestamps from stream ──────────────────────────────────
static String s_pending_photo_ts = "";
static String s_pending_video_ts = "";

void processData(AsyncResult &aResult) {
    if (!aResult.isResult()) return;
}

const String& get_device_path() {
    static String device_path = "";
    if (device_path.length() == 0) {
        char buf[256];
        snprintf(buf, sizeof(buf), PATH_RTDB_DEVICE_BASE, USER_UID, fb_rtdb_get_mac_address().c_str());
        device_path = String(buf);
    }
    return device_path;
}

const String& get_commands_path() {
    static String commands_path = "";
    if (commands_path.length() == 0) {
        char buf[256];
        snprintf(buf, sizeof(buf), PATH_RTDB_COMMANDS, USER_UID, fb_rtdb_get_mac_address().c_str());
        commands_path = String(buf);
    }
    return commands_path;
}

const String& get_remote_sync_path() {
    static String sync_path = "";
    if (sync_path.length() == 0) {
        char buf[256];
        snprintf(buf, sizeof(buf), PATH_RTDB_REMOTE_SYNC, USER_UID, fb_rtdb_get_mac_address().c_str());
        sync_path = String(buf);
    }
    return sync_path;
}

void parse_stream_data(const String& path, const String& data) {
    if (data == "null" || data.length() == 0) return;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data);
    if (error) {
        Serial.printf("[ERROR] [FIREBASE] Stream JSON parse failed: %s\n", error.c_str());
        return;
    }

    // --- 1. CREATE OLD NVS SNAPSHOT ---
    // Fetch old NVS data BEFORE deletion to preserve used_quota
    String old_pins_json = nvs_store_get_all_pins_json();
    JsonDocument old_doc;
    deserializeJson(old_doc, old_pins_json);

    if (path == "/" || path == "") {
        if (doc["active_pins"].is<JsonObject>()) {
            // --- 2. CLEAR NVS SAFELY HERE ---
            nvs_store_clear_all(); 
            
            JsonObject pins = doc["active_pins"].as<JsonObject>();
            for (JsonPair kv : pins) {
                String pin = kv.key().c_str();
                int quota = kv.value()["quota"] | 0;
                
                int current_used = 0;
                // --- 3. RETRIEVE USED QUOTA FROM SNAPSHOT ---
                if (old_doc.containsKey(pin)) {
                    current_used = old_doc[pin]["u"] | 0; // "u" is the key for used_quota in NVS
                }
                
                // Save new pin with the preserved used_quota
                nvs_store_save_pin(pin.c_str(), quota, current_used);
            }
            Serial.printf("[INFO] [FIREBASE] Synced %d active pins from Firebase\n", nvs_store_get_pin_count());
        } else if (doc["active_pins"].isNull()) {
            nvs_store_clear_all();
        }

        // --- COMMANDS PARSING (Unchanged) ---
        if (doc["commands"].is<JsonObject>()) {
            JsonObject cmds = doc["commands"].as<JsonObject>();
            bool lock = cmds["lock"] | false;
            bool unlock = cmds["unlock"] | false;
            RemoteCommand cmd = CMD_NONE;
            if (lock && unlock) cmd = CMD_LOCK_AND_UNLOCK;
            else if (lock) cmd = CMD_LOCK;
            else if (unlock) cmd = CMD_UNLOCK;

            if (cmd != CMD_NONE) {
                xQueueSend(CommandQueue, &cmd, 0);
            }

            if (cmds["request_photo_ondemand"].is<const char*>()) {
                String ts = cmds["request_photo_ondemand"].as<const char*>();
                if (ts.length() > 0) {
                    s_pending_photo_ts = ts;
                    RemoteCommand p_cmd = CMD_REQUEST_PHOTO;
                    xQueueSend(CommandQueue, &p_cmd, 0);
                }
            }

            if (cmds["request_video_id"].is<const char*>()) {
                String ts = cmds["request_video_id"].as<const char*>();
                if (ts.length() > 0) {
                    s_pending_video_ts = ts;
                    RemoteCommand v_cmd = CMD_REQUEST_VIDEO;
                    xQueueSend(CommandQueue, &v_cmd, 0);
                }
            }
        }
    } else if (path.startsWith("/active_pins")) {
        // --- HANDLE UPDATE SPECIFIC PINS ---
        if (path == "/active_pins") {
            nvs_store_clear_all();
            if (doc.is<JsonObject>()) {
                for (JsonPair kv : doc.as<JsonObject>()) {
                    String pin = kv.key().c_str();
                    int quota = kv.value()["quota"] | 0;
                    
                    int current_used = 0;
                    if (old_doc.containsKey(pin)) {
                        current_used = old_doc[pin]["u"] | 0; // Preserve from snapshot
                    }
                    nvs_store_save_pin(pin.c_str(), quota, current_used);
                }
            }
        } else {
            // Update individual PIN (e.g. path: "/active_pins/1234")
            String pin = path.substring(13); // Length of "/active_pins/" is 13
            if (doc.isNull()) {
                nvs_store_delete_pin(pin.c_str());
            } else if (doc.is<JsonObject>()) {
                int quota = doc["quota"] | 0;
                
                int current_used = 0;
                if (old_doc.containsKey(pin)) {
                    current_used = old_doc[pin]["u"] | 0; // Preserve from snapshot
                }
                nvs_store_save_pin(pin.c_str(), quota, current_used);
            }
        }
    } else if (path.startsWith("/commands")) {
        // --- COMMANDS PATH HANDLING (Unchanged) ---
        if (path == "/commands") {
            if (doc.is<JsonObject>()) {
                JsonObject cmds = doc.as<JsonObject>();
                bool lock = cmds["lock"] | false;
                bool unlock = cmds["unlock"] | false;
                RemoteCommand cmd = CMD_NONE;
                if (lock && unlock) cmd = CMD_LOCK_AND_UNLOCK;
                else if (lock) cmd = CMD_LOCK;
                else if (unlock) cmd = CMD_UNLOCK;

                if (cmd != CMD_NONE) {
                    xQueueSend(CommandQueue, &cmd, 0);
                }

                if (cmds["request_photo_ondemand"].is<const char*>()) {
                    String ts = cmds["request_photo_ondemand"].as<const char*>();
                    if (ts.length() > 0) {
                        s_pending_photo_ts = ts;
                        RemoteCommand p_cmd = CMD_REQUEST_PHOTO;
                        xQueueSend(CommandQueue, &p_cmd, 0);
                    }
                }

                if (cmds["request_video_id"].is<const char*>()) {
                    String ts = cmds["request_video_id"].as<const char*>();
                    if (ts.length() > 0) {
                        s_pending_video_ts = ts;
                        RemoteCommand v_cmd = CMD_REQUEST_VIDEO;
                        xQueueSend(CommandQueue, &v_cmd, 0);
                    }
                }
            }
        } else if (path == "/commands/lock") {
            if (doc.as<bool>()) {
                RemoteCommand cmd = CMD_LOCK;
                xQueueSend(CommandQueue, &cmd, 0);
            }
        } else if (path == "/commands/unlock") {
            if (doc.as<bool>()) {
                RemoteCommand cmd = CMD_UNLOCK;
                xQueueSend(CommandQueue, &cmd, 0);
            }
        } else if (path == "/commands/request_photo_ondemand") {
            String ts = doc.as<String>();
            if (ts.length() > 0 && ts != "null") {
                s_pending_photo_ts = ts;
                RemoteCommand cmd = CMD_REQUEST_PHOTO;
                xQueueSend(CommandQueue, &cmd, 0);
            }
        } else if (path == "/commands/request_video_id") {
            String ts = doc.as<String>();
            if (ts.length() > 0 && ts != "null") {
                s_pending_video_ts = ts;
                RemoteCommand cmd = CMD_REQUEST_VIDEO;
                xQueueSend(CommandQueue, &cmd, 0);
            }
        }
    }
}

void streamCallback(AsyncResult &aResult) {
    if (!aResult.isResult()) return;

    if (aResult.isEvent()) {
        Serial.printf("[INFO] [FIREBASE] Event msg: %s, code: %d\n", aResult.appEvent().message().c_str(), aResult.appEvent().code());
    }

    if (aResult.isError()) {
        Serial.printf("[ERROR] [FIREBASE] Stream error: %s, code: %d\n",
                      aResult.error().message().c_str(), aResult.error().code());
        stream_started = false;
        return;
    }

    if (aResult.available()) {
        RealtimeDatabaseResult &result = aResult.to<RealtimeDatabaseResult>();
        String dataPath = result.dataPath();
        String data = result.to<const char*>();
        String eventType = result.event();

        // Serial.printf("[FB_RTDB] Stream event: %s, path: %s, data: %s\n", eventType.c_str(), dataPath.c_str(), data.c_str());
        
        parse_stream_data(dataPath, data);
    }
}

void fb_rtdb_start_stream() {
    if (!app.ready()) return;
    if (stream_started) return;

    Serial.println("[INFO] [FIREBASE] Starting SSE Stream for remote_sync...");
    Database.get(stream_aClient, get_remote_sync_path(), streamCallback, true /* SSE */, "streamRemoteSync");
    stream_started = true;
}

const String& fb_rtdb_get_mac_address() {
    static String mac = "";
    if (mac.length() == 0) {
        mac = WiFi.macAddress();
        mac.replace(":", "");
        mac.toLowerCase();
    }
    return mac;
}

String fb_rtdb_get_timestamp_string() {
    time_t now; time(&now);
    if(now < 1577836800) return ""; // Not synchronized (Year < 2020)
    return String(now);
}

String fb_rtdb_get_pending_photo_timestamp() {
    String ts = s_pending_photo_ts;
    s_pending_photo_ts = "";
    return ts;
}

String fb_rtdb_get_pending_video_timestamp() {
    String ts = s_pending_video_ts;
    s_pending_video_ts = "";
    return ts;
}

bool fb_rtdb_init() {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    ssl_client.setInsecure();
    stream_ssl_client.setInsecure();

    Serial.println("[INFO] [FIREBASE] Authenticating...");
    
    // Initialize Firebase — blocking form: waits up to 30 seconds for auth to complete.
    // This guarantees app.ready() == true when fb_rtdb_init() returns.
    initializeApp(aClient, app, getAuth(user_auth), 30000 /* ms timeout */, processData);

    // Only bind the Database service AFTER auth is confirmed.
    app.getApp<RealtimeDatabase>(Database);
    Database.url(FIREBASE_DATABASE_URL);

    if (app.ready()) {
        s_app_ready_cached = true;
        Serial.println("[SUCCESS] [FIREBASE] Connected to Firebase.");
        return true;
    } else {
        Serial.println("[WARN] [FIREBASE] Authentication failed or timed out.");
        return false;
    }
}

bool fb_rtdb_is_connected() {
    return (WiFi.status() == WL_CONNECTED && s_app_ready_cached);
}

void fb_rtdb_create_device_entry() {
    String jsonPayload = "{"
      "\"status/isOpen\":false,"
      "\"status/isUnlocked\":false,"
      "\"status/isFull\":false,"
      "\"status/last_heartbeat\":0,"
      "\"remote_sync/commands/unlock\":false,"
      "\"remote_sync/commands/lock\":false,"
      "\"remote_sync/commands/request_video_id\":\"\","
      "\"remote_sync/commands/request_photo_ondemand\":\"\""
    "}";
    Database.update(aClient, get_device_path(), object_t(jsonPayload), processData, "createDevice");
}

void fb_rtdb_update_heartbeat() {
    String ts = fb_rtdb_get_timestamp_string();
    if (ts == "") return;
    char buf[256];
    snprintf(buf, sizeof(buf), PATH_RTDB_STATUS, USER_UID, fb_rtdb_get_mac_address().c_str());
    String status_path = String(buf) + "/last_heartbeat";
    Database.set<int>(aClient, status_path, ts.toInt(), processData, "updateHeartbeat");
}

void fb_rtdb_update_is_open(bool value) {
    char buf[256];
    snprintf(buf, sizeof(buf), PATH_RTDB_STATUS, USER_UID, fb_rtdb_get_mac_address().c_str());
    String path = String(buf) + "/isOpen";
    Database.set<bool>(aClient, path, value, processData, "setIsOpen");
}

void fb_rtdb_update_is_unlocked(bool value) {
    char buf[256];
    snprintf(buf, sizeof(buf), PATH_RTDB_STATUS, USER_UID, fb_rtdb_get_mac_address().c_str());
    String path = String(buf) + "/isUnlocked";
    Database.set<bool>(aClient, path, value, processData, "setIsUnlocked");
}

void fb_rtdb_update_is_full(bool value) {
    char buf[256];
    snprintf(buf, sizeof(buf), PATH_RTDB_STATUS, USER_UID, fb_rtdb_get_mac_address().c_str());
    String path = String(buf) + "/isFull";
    Database.set<bool>(aClient, path, value, processData, "setIsFull");
}

void fb_rtdb_add_delivery_log(const String& timestamp, const String& used_pin, const String& event_type, const String& photo_url) {
    String jsonString = "{\"used_pin\":\"" + used_pin + "\",\"event_type\":\"" + event_type + "\",\"photo_evidence_url\":\"" + photo_url + "\"}";
    char buf[256];
    snprintf(buf, sizeof(buf), PATH_RTDB_DELIVERY_ENTRY, USER_UID, fb_rtdb_get_mac_address().c_str(), timestamp.c_str());
    Database.set<object_t>(aClient, String(buf), object_t(jsonString), processData, "addDeliveryLog");
}

void fb_rtdb_update_delivery_video_url(const String& log_id, const String& video_url) {
    char buf[256];
    snprintf(buf, sizeof(buf), PATH_RTDB_DELIVERY_ENTRY, USER_UID, fb_rtdb_get_mac_address().c_str(), log_id.c_str());
    String path = String(buf) + "/video_evidence_url";
    Database.set<String>(aClient, path, video_url, processData, "updateVideoUrl");
}

void fb_rtdb_update_delivery_photo_url(const String& log_id, const String& photo_url) {
    char buf[256];
    snprintf(buf, sizeof(buf), PATH_RTDB_DELIVERY_ENTRY, USER_UID, fb_rtdb_get_mac_address().c_str(), log_id.c_str());
    String path = String(buf) + "/photo_evidence_url";
    Database.set<String>(aClient, path, photo_url, processData, "updatePhotoUrl");
}

void fb_rtdb_loop() {
    Database.loop();
    s_app_ready_cached = app.ready();
}

void fb_rtdb_clear_command(const String& command_field) {
    String command_path = get_commands_path() + "/" + command_field;

    if (command_field == "unlock" || command_field == "lock") {
        Database.set<bool>(aClient, command_path, false, processData, "clearCommand");
    } else if (command_field == "request_video_id" || command_field == "request_photo_ondemand") {
        Database.set<String>(aClient, command_path, "", processData, "clearCommand");
    }
}

void fb_rtdb_set_pin_used_quota(const String& pin, int used_quota) {
    if (!fb_rtdb_is_connected()) return;
    
    char buf[256];
    snprintf(buf, sizeof(buf), PATH_RTDB_ACTIVE_PINS, USER_UID, fb_rtdb_get_mac_address().c_str());
    String path = String(buf) + "/" + pin + "/used_quota";
    
    Database.set<int>(aClient, path, used_quota, processData, "setPinQuota");
    Serial.printf("[INFO] [FIREBASE] PIN %s used_quota set to %d (from NVS)\n", pin.c_str(), used_quota);
}

void fb_rtdb_delete_pin(const String& pin) {
    char buf[256];
    snprintf(buf, sizeof(buf), PATH_RTDB_ACTIVE_PINS, USER_UID, fb_rtdb_get_mac_address().c_str());
    String path = String(buf) + "/" + pin;
    Database.remove(aClient, path, processData, "deletePin");
}
