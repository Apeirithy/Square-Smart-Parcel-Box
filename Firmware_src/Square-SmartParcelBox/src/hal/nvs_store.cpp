#include "hal/nvs_store.h"

static Preferences preferences;
static bool nvs_available = false;

static const char* PREFS_NAMESPACE = "box_prefs";
static const char* PINS_KEY = "active_pins";

static JsonDocument get_pins_doc() {
    String json_str = preferences.getString(PINS_KEY, "{}");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json_str);
    if (error) {
        doc.to<JsonObject>();
    } else if (!doc.is<JsonObject>()) {
        doc.to<JsonObject>();
    }
    return doc;
}

static bool save_pins_doc(const JsonDocument& doc) {
    String output;
    serializeJson(doc, output);
    size_t written = preferences.putString(PINS_KEY, output);
    return written == output.length();
}

bool nvs_store_init() {
    if (!preferences.begin(PREFS_NAMESPACE, false)) {
        return false;
    }
    nvs_available = true;
    
    JsonDocument doc = get_pins_doc();
    Serial.printf("[NVS] Init OK, Number of active PINs: %d\n", doc.as<JsonObject>().size());
    return true;
}

int nvs_store_get_pin_count() {
    if (!nvs_available) return 0;
    JsonDocument doc = get_pins_doc();
    return doc.as<JsonObject>().size();
}

bool nvs_store_has_pins() {
    return nvs_store_get_pin_count() > 0;
}

bool nvs_store_verify_pin(const char* pin) {
    if (!nvs_available) return false;
    JsonDocument doc = get_pins_doc();
    JsonObject obj = doc.as<JsonObject>();
    
    if (obj.containsKey(pin)) {
        int quota = obj[pin]["q"];
        int used = obj[pin]["u"];
        return used < quota;
    }
    return false;
}

int nvs_store_increment_used(const char* pin) {
    if (!nvs_available) return -1;
    JsonDocument doc = get_pins_doc();
    JsonObject obj = doc.as<JsonObject>();
    
    if (obj.containsKey(pin)) {
        int quota = obj[pin]["q"];
        int used = obj[pin]["u"];
        used++;
        
        if (used >= quota) {
            obj.remove(pin);
        } else {
            obj[pin]["u"] = used;
        }
        if (save_pins_doc(doc)) {
            return used;
        }
    }
    return -1;
}

int nvs_store_get_remaining_quota(const char* pin) {
    if (!nvs_available) return -1;
    JsonDocument doc = get_pins_doc();
    JsonObject obj = doc.as<JsonObject>();
    
    if (obj.containsKey(pin)) {
        int quota = obj[pin]["q"];
        int used = obj[pin]["u"];
        return quota - used;
    }
    return -1;
}

bool nvs_store_save_pin(const char* pin, int quota, int used_quota) {
    if (!nvs_available) return false;
    JsonDocument doc = get_pins_doc();
    JsonObject obj = doc.as<JsonObject>();
    
    JsonObject pinObj = obj[pin].to<JsonObject>();
    pinObj["q"] = quota;
    pinObj["u"] = used_quota;
    
    return save_pins_doc(doc);
}

bool nvs_store_save_all_pins(const char* json_str) {
    if (!nvs_available) return false;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json_str);
    if (error || !doc.is<JsonObject>()) {
        return false;
    }
    return save_pins_doc(doc);
}

String nvs_store_get_all_pins_json() {
    if (!nvs_available) return "{}";
    return preferences.getString(PINS_KEY, "{}");
}

bool nvs_store_delete_pin(const char* pin) {
    if (!nvs_available) return false;
    JsonDocument doc = get_pins_doc();
    JsonObject obj = doc.as<JsonObject>();
    
    if (obj.containsKey(pin)) {
        obj.remove(pin);
        return save_pins_doc(doc);
    }
    return false;
}

void nvs_store_clear_all() {
    if (!nvs_available) return;
    preferences.remove(PINS_KEY);
}

bool nvs_store_is_available() {
    return nvs_available;
}
