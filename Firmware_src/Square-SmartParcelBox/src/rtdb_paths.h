#pragma once

// 1. Firebase Realtime Database (RTDB) Paths
// Format arguments: USER_UID, mac_address, [timestamp]

#define PATH_RTDB_DEVICE_BASE     "users/%s/devices/%s"
#define PATH_RTDB_STATUS          "users/%s/devices/%s/status"
#define PATH_RTDB_REMOTE_SYNC     "users/%s/devices/%s/remote_sync"
#define PATH_RTDB_COMMANDS        "users/%s/devices/%s/remote_sync/commands"
#define PATH_RTDB_ACTIVE_PINS     "users/%s/devices/%s/remote_sync/active_pins"
#define PATH_RTDB_DELIVERY_LOGS   "users/%s/devices/%s/delivery_logs"
#define PATH_RTDB_DELIVERY_ENTRY  "users/%s/devices/%s/delivery_logs/%s"
