#pragma once

// 2. Firebase Cloud Storage Paths
// Format arguments: USER_UID, mac_address, timestamp, [timestamp]

#define PATH_STORAGE_BASE         "users/%s/devices/%s/storage"
#define PATH_STORAGE_MEDIA_FOLDER "users/%s/devices/%s/storage/%s"
#define PATH_STORAGE_IMAGE        "users/%s/devices/%s/storage/%s/img_%s.jpg"
#define PATH_STORAGE_VIDEO        "users/%s/devices/%s/storage/%s/vid_%s.mjpeg"
