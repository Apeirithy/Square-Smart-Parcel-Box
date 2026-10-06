#include "sdcard.h"
#include <SD_MMC.h>
#include <FS.h>
#include "../pins.h"
#include "../config.h"
#include "../sdcard_paths.h"
#include <vector>

static const char* QUEUE_FILE = PATH_SDCARD_UPLOAD_QUEUE;

bool sdcard_init() {
    SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_DATA);
    if (!SD_MMC.begin("/sdcard", true)) { // true = 1-bit mode
        return false;
    }

    if (!SD_MMC.exists(QUEUE_FILE)) {
        File f = SD_MMC.open(QUEUE_FILE, FILE_WRITE);
        if (f) f.close();
    }

    return true;
}

static void ensure_directories(const char* path) {
    String pathStr = path;
    int lastSlash = pathStr.lastIndexOf('/');
    if (lastSlash > 0) {
        String dir = pathStr.substring(0, lastSlash);
        if (!SD_MMC.exists(dir)) {
            String current = "";
            for (unsigned int i = 0; i < dir.length(); i++) {
                current += dir[i];
                if (dir[i] == '/' && current.length() > 1) {
                    SD_MMC.mkdir(current);
                }
            }
            SD_MMC.mkdir(dir);
        }
    }
}

bool sdcard_save_file(const char* path, const uint8_t* data, size_t len) {
    ensure_directories(path);
    File file = SD_MMC.open(path, FILE_WRITE);
    if (!file) {
        return false;
    }
    size_t written = file.write(data, len);
    file.close();
    return written == len;
}

bool sdcard_append_file(const char* path, const uint8_t* data, size_t len) {
    // Use FILE_APPEND so new photos are appended to the end of the existing file
    File file = SD_MMC.open(path, FILE_APPEND);
    if (!file) {
        return false;
    }
    size_t written = file.write(data, len);
    file.close();
    return written == len;
}

uint8_t* sdcard_read_file(const char* path, size_t *out_len) {
    File file = SD_MMC.open(path, FILE_READ);
    if (!file) {
        if (out_len) *out_len = 0;
        return nullptr;
    }
    
    size_t len = file.size();
    if (out_len) *out_len = len;
    
    uint8_t* buf = (uint8_t*)malloc(len);
    if (!buf) {
        file.close();
        if (out_len) *out_len = 0;
        return nullptr;
    }
    
    file.read(buf, len);
    file.close();
    return buf;
}

bool sdcard_file_exists(const char* path) {
    return SD_MMC.exists(path);
}

bool sdcard_delete_file(const char* path) {
    return SD_MMC.remove(path);
}

bool sdcard_rename_file(const char* old_path, const char* new_path) {
    ensure_directories(new_path);
    return SD_MMC.rename(old_path, new_path);
}

bool sdcard_delete_directory(const char* path) {
    File dir = SD_MMC.open(path);
    if (!dir || !dir.isDirectory()) {
        return false;
    }
    
    File file = dir.openNextFile();
    while (file) {
        String filePath = file.path();
        if (file.isDirectory()) {
            file.close();
            sdcard_delete_directory(filePath.c_str());
        } else {
            file.close();
            SD_MMC.remove(filePath.c_str());
        }
        file = dir.openNextFile();
    }
    return SD_MMC.rmdir(path);
}

uint32_t sdcard_get_free_space_mb() {
    uint64_t freeBytes = SD_MMC.totalBytes() - SD_MMC.usedBytes();
    return (uint32_t)(freeBytes / (1024 * 1024));
}

uint32_t sdcard_get_size_mb() {
    return (uint32_t)(SD_MMC.totalBytes() / (1024 * 1024));
}

const char* sdcard_get_type_string() {
    sdcard_type_t cardType = SD_MMC.cardType();
    switch (cardType) {
        case CARD_MMC: return "MMC";
        case CARD_SD: return "SDSC";
        case CARD_SDHC: return "SDHC";
        default: return "UNKNOWN";
    }
}

bool sdcard_is_available() {
    return SD_MMC.cardType() != CARD_NONE;
}

static std::vector<String> read_queue() {
    std::vector<String> queue;
    File file = SD_MMC.open(QUEUE_FILE, FILE_READ);
    if (!file) return queue;
    
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            queue.push_back(line);
        }
    }
    file.close();
    return queue;
}

static void write_queue(const std::vector<String>& queue) {
    File file = SD_MMC.open(QUEUE_FILE, FILE_WRITE);
    if (!file) return;
    
    for (const String& line : queue) {
        file.println(line);
    }
    file.close();
}

void sdcard_queue_push_back(const String& filename) {
    std::vector<String> queue = read_queue();
    queue.push_back(filename);
    write_queue(queue);
}

void sdcard_queue_push_front(const String& filename) {
    std::vector<String> queue = read_queue();
    queue.insert(queue.begin(), filename);
    write_queue(queue);
}

String sdcard_queue_pop_front() {
    std::vector<String> queue = read_queue();
    if (queue.empty()) return "";
    
    String front = queue.front();
    queue.erase(queue.begin());
    write_queue(queue);
    
    return front;
}

String sdcard_queue_peek_front() {
    std::vector<String> queue = read_queue();
    if (queue.empty()) return "";
    return queue.front();
}

bool sdcard_queue_remove(const String& filename) {
    std::vector<String> queue = read_queue();
    bool removed = false;
    
    for (auto it = queue.begin(); it != queue.end(); ) {
        if (*it == filename) {
            it = queue.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }
    
    if (removed) {
        write_queue(queue);
    }
    return removed;
}

bool sdcard_queue_is_empty() {
    std::vector<String> queue = read_queue();
    return queue.empty();
}

void sdcard_fifo_cleanup() {
    while (sdcard_get_free_space_mb() < SD_MIN_FREE_MB) {
        File root = SD_MMC.open("/");
        if (!root) break;
        
        File file = root.openNextFile();
        String oldest_dir = "";
        time_t oldest_time = -1;
        
        while (file) {
            String fname = file.name();
            if (file.isDirectory() && fname != "System Volume Information") {
                // Parse timestamp from directory name
                String dir_name_only = fname.startsWith("/") ? fname.substring(1) : fname;
                long t = dir_name_only.toInt();
                if (t > 0 && (oldest_time == -1 || t < oldest_time)) {
                    oldest_time = t;
                    oldest_dir = file.path();
                }
            }
            file.close();
            file = root.openNextFile();
        }
        root.close();
        
        if (oldest_dir.length() > 0) {
            sdcard_delete_directory(oldest_dir.c_str());
        } else {
            break; // No more directories to delete
        }
    }
}

// ── DBG-008: Write-read verification test ───────────────────────────────────
bool sdcard_write_test() {
    if (!sdcard_is_available()) return false;

    const char* test_path = "/test_verify.txt";
    const char* test_data = "SQUARE_VERIFY";

    // Write test data
    File f = SD_MMC.open(test_path, "w");
    if (!f) return false;
    f.print(test_data);
    f.close();

    // Read back and verify
    f = SD_MMC.open(test_path, "r");
    if (!f) return false;
    String readback = f.readString();
    f.close();

    // Clean up test file
    SD_MMC.remove(test_path);

    return (readback == test_data);
}

