#ifndef CONFIG_H
#define CONFIG_H

// Time-of-Flight Sensor settings
#define TOF_TOLERANCE_CM 5 // Revised by me from implementation phase 0 plan, using the tolerance instead of threshold.
#define TOF_BOX_WIDTH_CM 40 // The box width, as the reference.

// Timeouts and Intervals
#define DOOR_TIMEOUT_MS 10000 // Timeout waiting for courier to open the door after unlocked.
#define HEARTBEAT_INTERVAL_MS 30000 // Interval for heartbeat, sent every 30 seconds to be compared by apps. if the difference is more than 1 minute, then the system is offline.
#define WIFI_MAX_RETRIES 25
#define WIFI_RETRY_DELAY_MS 300
#define FLASH_WARMUP_MS 1000

// SD Card and queues
#define SD_MIN_FREE_MB 50 // Minimum empty space (in MB) for MicroSD. If during checking process below this value, then paired FIFO of MicroSD content is triggered.
#define RTDB_QUEUE_DEPTH 10
#define COMMAND_QUEUE_DEPTH 3

// Retry delays for offline uploads
#define RETRY_FIRST_FAIL_MS 300000 // 5 minutes waiting time before the second attempt of delivery logs push 
#define RETRY_SUBSEQUENT_MS 900000

// Debouncing and blink rates
#define KEYPAD_DEBOUNCE_MS 200
#define LED_ALT_INTERVAL_MS 250
#define ERROR_LOG_INTERVAL_MS 1000
#define WS2812_BLINK_INTERVAL_MS 1000
#define RTOS_MIN_DELAY_MS 10

// Automatic auth trigger at how many digits inputted
#define PIN_LENGTH 4

#endif // CONFIG_H
