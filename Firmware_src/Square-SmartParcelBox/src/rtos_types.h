#ifndef RTOS_TYPES_H
#define RTOS_TYPES_H
#include <Arduino.h>

// ── RTDB write request types ────────────────────────────────────────────────
enum RTDBWriteType {
    RTDB_HEARTBEAT,
    RTDB_STATUS_IS_OPEN,        // Individual status field: isOpen
    RTDB_STATUS_IS_UNLOCKED,    // Individual status field: isUnlocked  
    RTDB_STATUS_IS_FULL,        // Individual status field: isFull
    RTDB_DELIVERY_LOG,
    RTDB_CLEAR_COMMAND,
    RTDB_INCREMENT_PIN,
    RTDB_DELETE_PIN,
    RTDB_UPDATE_VIDEO_URL
};

// ── RTDB write request payload ──────────────────────────────────────────────
struct RTDBWriteRequest {
    RTDBWriteType type;
    char key[32];           // PIN code, command field name, log ID, etc.
    char value_str[256];    // Timestamp, URL, event_type, etc. (increased from 128)
    bool value_bool;        // Boolean value for status fields
    int value_int;          // Integer value (e.g. used_quota from NVS)
};

#endif
