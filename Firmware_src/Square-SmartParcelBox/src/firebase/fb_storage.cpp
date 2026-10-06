#include "fb_storage.h"
#include <WiFi.h>
#include <FirebaseClient.h>
#include <WiFiClientSecure.h>
#include <SD_MMC.h>
#include "credentials.h"
#include "config.h"
#include "../storage_paths.h"
#include "fb_rtdb.h"

// ── Shared auth from fb_rtdb.cpp ────────────────────────────────────────────
extern FirebaseApp app;

// ── Storage-specific networking (separate from RTDB) ────────────────────────
static WiFiClientSecure storage_ssl_client;
using AsyncClient = AsyncClientClass;
static AsyncClient storageClient(storage_ssl_client);

// ── Firebase Storage service object ─────────────────────────────────────────
Storage storage;

// ── File handle used by the SD_MMC file-operation callback ──────────────────
static File sdFile;

// ── Upload tracking ─────────────────────────────────────────────────────────
struct UploadTracker {
    bool    in_progress;        // Upload is currently running
    String  delivery_log_id;    // Timestamp key for the delivery log entry
    String  local_filepath;     // SD card path
    bool    is_photo;           // true = photo (.jpg), false = video (.mjpeg)
    unsigned long start_time;   // For Timeout Guard purposes
};
static UploadTracker tracker = {false, "", "", true, 0};

// ── Storage readiness flag ──────────────────────────────────────────────────
static bool storage_ready = false;

// ── GLOBAL VARS FOR ASYNC UPLOAD (ANTI-SCOPE BUG) ─────────────────────────
// These variables must be global so they aren't destroyed when the upload function exits
static String async_bucket_path;
static String async_content_type;
static String async_local_filepath;

// ── File-scope FileConfig ───────────────────────────────────────────────────
void storage_file_callback(File &file, const char *filename, file_operating_mode mode);
static FileConfig media_file("/dummy", storage_file_callback);

// ─────────────────────────────────────────────────────────────────────────────
// SD_MMC file-operation callback
// ─────────────────────────────────────────────────────────────────────────────
void storage_file_callback(File &file, const char *filename, file_operating_mode mode) {
    String fname = String(filename);
    if (!fname.startsWith("/")) {
        fname = "/" + fname;
    }

    // FIX #1: MUST close old file if the library calls it twice
    if (sdFile) {
        sdFile.close();
    }

    switch (mode) {
        case file_mode_open_read:
            sdFile = SD_MMC.open(fname, "r");
            if (!sdFile || !sdFile.available()) {
                Serial.printf("[ERROR] [FIREBASE] Failed to open '%s' for reading\n", fname.c_str());
            } else {
                Serial.printf("[SUCCESS] [FIREBASE] File opened, size: %d bytes\n", sdFile.size());
            }
            break;
        case file_mode_open_write:
            sdFile = SD_MMC.open(fname, "w");
            break;
        case file_mode_open_append:
            sdFile = SD_MMC.open(fname, "a");
            break;
        case file_mode_remove:
            SD_MMC.remove(fname);
            break;
        default:
            break;
    }
    file = sdFile;
}

// ─────────────────────────────────────────────────────────────────────────────
// Async result callback
// ─────────────────────────────────────────────────────────────────────────────
void processStorageData(AsyncResult &aResult) {
    if (!aResult.isResult())
        return;

    if (aResult.isEvent()) {
        Serial.printf("[INFO] [FIREBASE] Event: %s, msg: %s, code: %d\n",
                      aResult.uid().c_str(),
                      aResult.appEvent().message().c_str(),
                      aResult.appEvent().code());
    }

    if (aResult.isDebug()) {
        Serial.printf("[INFO] [FIREBASE] Debug: %s, msg: %s\n",
                      aResult.uid().c_str(),
                      aResult.debug().c_str());
    }

    if (aResult.isError()) {
        Serial.printf("[ERROR] [FIREBASE] Error: %s, msg: %s, code: %d\n",
                      aResult.uid().c_str(),
                      aResult.error().message().c_str(),
                      aResult.error().code());
        if (tracker.in_progress) {
            tracker.in_progress = false;
            if (sdFile) sdFile.close();
        }
    }

    if (aResult.uploadProgress()) {
        Serial.printf("[INFO] [FIREBASE] Upload %s: %d%% (%d / %d bytes)\n",
                      aResult.uid().c_str(),
                      aResult.uploadInfo().progress,
                      aResult.uploadInfo().uploaded,
                      aResult.uploadInfo().total);

        if (aResult.uploadInfo().total == aResult.uploadInfo().uploaded && aResult.uploadInfo().total > 0) {
            String download_url = aResult.uploadInfo().downloadUrl;
            Serial.printf("[SUCCESS] [FIREBASE] Upload complete!\n");
            Serial.printf("[INFO] [FIREBASE] Download URL: %s\n", download_url.c_str());

            if (download_url.length() > 0) {
                if (tracker.is_photo) {
                    fb_rtdb_update_delivery_photo_url(tracker.delivery_log_id, download_url);
                } else {
                    fb_rtdb_update_delivery_video_url(tracker.delivery_log_id, download_url);
                }
                Serial.printf("[SUCCESS] [FIREBASE] RTDB %s URL updated for log '%s'\n",
                              tracker.is_photo ? "photo" : "video",
                              tracker.delivery_log_id.c_str());
            }

            if (tracker.in_progress) {
                tracker.in_progress = false;
                if (sdFile) sdFile.close();
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void fb_storage_loop() {
    storage.loop();

    // FIX #2: TIMEOUT GUARD (ZOMBIE KILLER)
    // If upload is stuck for more than 60 seconds, force cancel!
    if (tracker.in_progress && (millis() - tracker.start_time > 60000)) {
        Serial.println("[ERROR] [FIREBASE] TIMEOUT! Upload stuck/zombie. Killing process...");
        tracker.in_progress = false;
        
        // Clean up memory and restart other systems
        storage_ssl_client.stop(); 
        if (sdFile) sdFile.close();
    }
}

bool fb_storage_init() {
    if (!app.ready()) {
        Serial.println("[ERROR] [FIREBASE] Cannot init — FirebaseApp not ready");
        return false;
    }

    storage_ssl_client.setInsecure();
    app.getApp<Storage>(storage);

    storage_ready = true;
    Serial.println("[SUCCESS] [FIREBASE] Storage Initialised OK");
    return true;
}

bool fb_storage_is_available() {
    return storage_ready && app.ready();
}

bool fb_storage_is_uploading() {
    return tracker.in_progress;
}

// ─────────────────────────────────────────────────────────────────────────────
// Non-blocking upload of a file from SD card to Firebase Cloud Storage.
// ─────────────────────────────────────────────────────────────────────────────
bool fb_storage_upload_file(const String& local_filepath,
                            const String& timestamp,
                            const String& content_type,
                            const String& delivery_log_id) {
    if (!fb_storage_is_available()) {
        Serial.println("[WARN] [FIREBASE] Upload skipped — storage not available");
        return false;
    }

    if (tracker.in_progress) {
        Serial.println("[WARN] [FIREBASE] Upload skipped — another upload already in progress");
        return false;
    }

    if (!SD_MMC.exists(local_filepath.c_str())) {
        Serial.printf("[ERROR] [FIREBASE] File not found on SD: %s\n", local_filepath.c_str());
        return false;
    }

    // ── FIX #3: CREATE LOCAL PATH FIRST, THEN COPY TO GLOBAL SCOPE ─────────────
    char bucket_path[256];
    bool is_photo = true;
    if (local_filepath.endsWith(".jpg") || local_filepath.endsWith(".jpeg")) {
        snprintf(bucket_path, sizeof(bucket_path), PATH_STORAGE_IMAGE,
                 USER_UID, fb_rtdb_get_mac_address().c_str(),
                 timestamp.c_str(), timestamp.c_str());
        is_photo = true;
    } else if (local_filepath.endsWith(".mjpeg")) {
        snprintf(bucket_path, sizeof(bucket_path), PATH_STORAGE_VIDEO,
                 USER_UID, fb_rtdb_get_mac_address().c_str(),
                 timestamp.c_str(), timestamp.c_str());
        is_photo = false;
    } else {
        String filename = local_filepath.substring(local_filepath.lastIndexOf('/') + 1);
        snprintf(bucket_path, sizeof(bucket_path), "users/%s/devices/%s/storage/%s/%s",
                 USER_UID, fb_rtdb_get_mac_address().c_str(),
                 timestamp.c_str(), filename.c_str());
        is_photo = true;
    }

    // ── COPY TO GLOBAL VARIABLE SO IT'S NOT DELETED BY C++ ──────────────────────
    async_bucket_path = String(bucket_path);
    async_content_type = content_type;
    async_local_filepath = local_filepath;

    Serial.printf("[INFO] [FIREBASE] Uploading %s → %s\n", async_local_filepath.c_str(), async_bucket_path.c_str());

    // ── Store tracking info ─────────────────────────────────────────────────
    tracker.delivery_log_id = delivery_log_id;
    tracker.local_filepath  = async_local_filepath;
    tracker.is_photo        = is_photo;
    tracker.in_progress     = true;
    tracker.start_time      = millis(); // Start timeout countdown!
    
    // FIX #4: Flush storage client before new upload
    storage_ssl_client.stop();

    // ── Reconfigure file-scope FileConfig using GLOBAL string ───────────────
    media_file.setFile(async_local_filepath.c_str(), storage_file_callback);

    // ── Fire async upload menggunakan pointer dari GLOBAL string ────────────
    storage.upload(storageClient,
                   FirebaseStorage::Parent(FIREBASE_STORAGE_BUCKET, async_bucket_path.c_str()),
                   getFile(media_file),
                   async_content_type.c_str(),
                   processStorageData,
                   "uploadTask");

    return true;
}