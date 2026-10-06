# Smart Parcel Box \- Functional Specification Document

## Document Information

| Field | Value |
| :---- | :---- |
| Version | 1.13 |
| Status | Draft |
| Created | 2026-02-25 |
| Updated | 2026-07-28 |

## 1\. Overview

### 1.1 Purpose

Square is an Internet of Things (IoT)-based smart parcel box system designed to address security issues in last-mile delivery logistics. The main purpose of this system is to prevent porch piracy and minimize unverified package issues (e.g., packages claimed to have been delivered but no physical evidence at the location).

Square solves these problems by transforming passive storage boxes into active safes that require courier authentication via a 4-digit limited-use PIN, the use of which can be customized by the owner. Additionally, the system provides peace of mind to owners by providing box capacity reports and authentic visual evidence (a photo of the package) sent directly to the owner's phone every time the door is closed.

### 1.2 System Context

The Square system operates through a sequential workflow that connects physical interactions in the field with data processing in the cloud and user applications. The system's main operational flow is as follows:

A. Package Receiving Flow (Courier to Square)

- **Courier Authentication:** The courier sees the 4-digit limited-use PIN printed on the delivery note and enters it via the 3x4 Matrix Keypad. After each 4-digit PIN entry, the system automatically verifies the PIN. Furthermore, after each incorrect 4-digit PIN entry, the system automatically resets the PIN to allow the courier to enter again. The system also provides visual feedback in the form of 4 LED indicators that illuminate according to the number of digits entered.  
- **Door Access:** The system validates the PIN. If correct, the Solenoid Lock will open. When the door is pulled, the Magnetic Reed Switch will release (move away), and the system will immediately start recording an MJPEG video to the MicroSD Card.  
- **Data Recording (Trigger Event):** After the courier places the package and closes the door again, the Reed Switch will be re-engaged. As soon as the door is closed, the system will capture the time data as a package receipt log timestamp, stop the MJPEG video recording, take a photo using the OV2640 camera, read the remaining distance using the ToF (Time-of-Flight) sensor, create a receipt entry in the Firebase Realtime Database and upload the photo evidence to Firebase Cloud Storage using the previously obtained timestamp as the photo evidence name.  
- **Capacity Logic (ToF Sensor):** The ToF sensor compares the distance read with the preset number (maximum width limit of the box). If the reflection distance is less than the preset number, the system detects an “Overload” status, indicating that the parcel must be emptied immediately.  
- **Local Storage & Transmission (Failsafe Mechanism):** To prevent data loss due to unstable internet connections, photos will first be stored on the local SD Card on the ESP32-S3. Once safely stored, the data will be uploaded to Firebase Cloud Storage via Wi-Fi to update the status.

B. User Application Features (Square App to User)

The Square mobile application acts as the main control center for homeowners, with the following functional features:

- **Visual History Log:** Users can view a gallery of all photos of packages that have been picked up, complete with timestamps (time and date of event).  
- **Remote Control:** Users can remotely unlock and lock the Solenoid Lock without needing to enter a PIN.  
- **PIN Generator with Usage Limits:** A feature to generate limited-use PIN that users can copy to their shipping notes when shopping online.  
- **Capacity Status (ToF Indicator):** Displays the status of the ToF sensor. If the preset threshold is exceeded, the application will provide a visual warning that the box is full/overloaded and must be emptied.  
- **On-Demand Snapshot:** A special button on the application that allows users to command the ESP32-S3 to take real-time photos of the conditions inside the Square whenever needed.  
- **Video On-Demand Request:** A special button on the application that allows users to command the ESP32-S3 to upload specific MJPEG video of the package insertion process to Firebase and retrieve the video to the user application.

## 2\. Hardware Architecture & Pin Mapping

### 2.1 Target Platform

| Main Components | Specifications / Type | Functional Role in the System |
| :---- | :---- | :---- |
| **Microcontroller Unit (MCU)** | ESP32-S3 (WROOM) N16R8 | Acts as a control center (state machine), processes Wi-Fi connections, and manages the camera's frame buffer using PSRAM. |
| **I/O Expander (Keypad)** | PCF8574 I/O Expander at address 0x20 | As an I/O Expander for 3x4 Matrix Keypad  |
| **I/O Expander (LED)** | PCF8574 I/O Expander at address 0x21 | As an I/O Expander for 4x 5mm LED indicator |
| **Camera Module** | OV2640 (2 Megapixel) | Records video while the package is inserted and takes photos after the package is inserted. Initialized with UXGA resolution JPG format (1600x1200) for photos and VGA resolution MJPEG format (640x480) for videos. |
| **Auxiliary Flash LED** | 3W HPL LED \+ IRLZ44N Logic-level MOSFET \+ Current limiting Chalk Resistor | Provides stable additional lighting during shooting. Controlled via logic-level MOSFETs to ensure instant response and allow LED brightness to be adjusted. |
| **4x 5mm LED indicator \+ resistor**  | Blue 5mm LED \+ 220Ω Resistor | Serves as visual feedback for the courier when entering the PIN, showing how many digits have been inputted. |
| **Capacity Sensor** | Time-of-Flight (ToF) VL53L0X at address 0x29. | Installed horizontally on the inner side wall at a specific threshold height (38 cm from the base, just below the top drop-slot mechanism). It acts as an optical tripwire, shooting an infrared beam across the 40 cm width of the box to detect if the package stack has reached maximum capacity. |
| **Door Sensor** | Magnetic Reed Switch | Provides a digital input to detect the physical status of the door. If the door is closed, the reading is (`HIGH`). Conversely, if the door is open, the reading is (`LOW`). |
| **Door Actuator** | Solenoid Door Lock 12V \+ Relay Module (physically configured to High Level Trigger/Active HIGH mode) | Physical locking mechanism. Activate (`HIGH`) to disengage the lock and inactivate (`LOW`) to engage the lock. |
| **Power Supply** |  |  |
| **5 Volt Power Source** | LM2596 3A DC-DC step-down (buck) switching regulator module | Provides 5 Volt power for MCU, sensors and LED from 12 Volt power |
| **12 Volt Power Source** | 12V 3A DC adapter | Provides 12 Volt power for the entire system from 220V AC power source |

### 2.2 Pin Assignments

#### ESP32-S3 (WROOM) N16R8 GPIO

| Code Constant | GPIO | I/O Type | Description & Hardware Limitations |
| :---- | :---- | :---- | :---- |
| **Internal Actuators** |  |  |  |
| `PIN_LED_WS2812` | 48 | Output | System status indicators. The patterns are described in the table below. |
| **External Sensors & Actuators** |  |  |  |
| `PIN_RELAY_SOLENOID` | 14 | Output | Control signal to the Relay module. Active `HIGH`. MUST NOT be pulled from the strapping pin. |
| `PIN_AUX_FLASH` | 2 | Output | Control signal to the IRLZ44N base pin. Active `HIGH`. |
| `PIN_DOOR_SWITCH` | 1 | Input | Signal from the Reed Switch. MUST be set as `INPUT`. Door closed \= `HIGH`, open \= `LOW`. |
| `PIN_SDA` | 21 | I/O | I2C Data line for PCF8574 I/O Expander and VL53L0X ToF Sensor (daisy chain). |
| `PIN_SCL` | 47 | I/O | I2C Clock line for PCF8574 I/O Expander and VL53L0X ToF Sensor (daisy chain). |
| **Camera Module Interface (OV2640)** |  |  |  |
| `PIN_CAM_D0` | 11 | Input | Camera DVP Data Line 0 (Y2). |
| `PIN_CAM_D1` | 9 | Input | Camera DVP Data Line 1 (Y3). |
| `PIN_CAM_D2` | 8 | Input | Camera DVP Data Line 2 (Y4). |
| `PIN_CAM_D3` | 10 | Input | Camera DVP Data Line 3 (Y5). |
| `PIN_CAM_D4` | 12 | Input | Camera DVP Data Line 4 (Y6). |
| `PIN_CAM_D5` | 18 | Input | Camera DVP Data Line 5 (Y7). |
| `PIN_CAM_D6` | 17 | Input | Camera DVP Data Line 6 (Y8). |
| `PIN_CAM_D7` | 16 | Input | Camera DVP Data Line 7 (Y9). |
| `PIN_CAM_XCLK` | 15 | Output | System Clock from ESP32-S3 to Camera (usually 20MHz). |
| `PIN_CAM_PCLK` | 13 | Input | Pixel Clock from Camera to ESP32-S3. |
| `PIN_CAM_VSYNC` | 6 | Input | Vertical Sync (marker for new frame). |
| `PIN_CAM_HREF` | 7 | Input | Horizontal Reference (marker for valid pixel line). |
| `PIN_CAM_SIOD` | 4 | I/O | SCCB Data Line for OV2640 register configuration. |
| `PIN_CAM_SIOC` | 5 | Output | SCCB Clock Line for OV2640 register configuration. |
| **MicroSD Card** |  |  |  |
| `PIN_SD_DATA` | 40 | I/O | SD Data line (1-bit mode). Connected internally |
| `PIN_SD_CLK` | 39 | Output | SD Clock line. Connected internally |
| `PIN_SD_CMD` | 38 | I/O | SD Command line. Connected internally |

#### PCF8574 I/O Expander (Address: 0x20)

| Code Constant | Pin | I/O Type | Description & Hardware Limitations |
| :---- | :---- | :---- | :---- |
| **3x4 Matrix Keypad** |  |  |  |
| `PIN_KYPD_R1` | P0 | Output | Keypad Row 1 (1, 2, 3). |
| `PIN_KYPD_R2` | P1 | Output | Keypad Row 2 (4, 5, 6). |
| `PIN_KYPD_R3` | P2 | Output | Keypad Row 3 (7, 8, 9). |
| `PIN_KYPD_R4` | P3 | Output | Keypad Row 4 (\*, 0, \#). |
| `PIN_KYPD_C1` | P4 | Input | Keypad Column 1 (1, 4, 7, \*). Acts as quasi-bidirectional input (Written `HIGH` to enable weak pull-up for reading). |
| `PIN_KYPD_C2` | P5 | Input | Keypad Column 2 (2, 5, 8, 0). Acts as quasi-bidirectional input (Written `HIGH` to enable weak pull-up for reading). |
| `PIN_KYPD_C3` | P6 | Input | Keypad Column 3 (3, 6, 9, \#). Acts as quasi-bidirectional input (Written `HIGH` to enable weak pull-up for reading). |

#### PCF8574 I/O Expander (Address: 0x21)

| Code Constant | Pin | I/O Type | Description & Hardware Limitations |
| :---- | :---- | :---- | :---- |
| **LED Indicator** |  |  |  |
| `PIN_KYPD_IND_0` | P0 | Output | First digit input indicator. Active `LOW`. |
| `PIN_KYPD_IND_1` | P1 | Output | Second digit input indicator. Active `LOW`. |
| `PIN_KYPD_IND_2` | P2 | Output | Third digit input indicator. Active `LOW`. |
| `PIN_KYPD_IND_3` | P3 | Output | Fourth digit input indicator. Active `LOW`. |

## 3\. Functional Requirements

### 3.1 Access Control

- **ACC-001**: System SHALL validate the limited use PIN entered by the courier using the keypad automatically on the 4th keystroke before opening the box.  
- **ACC-002**: System SHALL allow remote unlocking/locking of the solenoid lock via the Square mobile app.  
- **ACC-003**: System SHALL log every valid access attempt with a Unix timestamp.  
- **ACC-004**: System SHALL automatically reset the 4-digit input buffer if an incorrect PIN is entered.  
- **ACC-005**: System SHALL provide visual feedback using 4 LEDs to display the number of digits entered.  
- **ACC-006**: System SHALL add 1 amount of used quota (`used_quota`) to the PIN used by the courier in NVS and Firebase Realtime Database after the package receipt log timestamp is obtained.  
- **ACC-007**: System SHALL delete 4-digit PIN entries in NVS and Firebase Realtime Database where all quota has been used.  
- **ACC-008**: System SHALL implement an edge detection (debouncing) mechanism to ensure that physical keypad inputs are registered EXACTLY ONCE per valid keystroke.  
- **ACC-009**: System SHALL implements a custom keypad library to allow flexible mapping without relying on third-party keypad libraries.   
- **ACC-010**: System SHALL ignore keypress from key (`*`) and (`#`).

### 3.2 Camera & Visual Logging

- **CVL-001**: System SHALL take evidence using two methods, namely a video of the package insertion followed by a photo of the package receipt.  
- **CVL-002**: System SHALL start recording MJPEG video once the door reed switch detects the door is open and stop recording once the door reed switch detects the door is closed.  
- **CVL-003**: System SHALL save the recorded MJPEG video to the MicroSD Card, the system SHALL NOT upload the video without a request from the Square mobile app.  
- **CVL-004**: System SHALL take a JPG image using the OV2640 camera after the door is closed.  
- **CVL-005**: System SHALL save the captured JPG images to a MicroSD Card before uploading to the Firebase Cloud Storage platform.  
- **CVL-006**: System SHALL use the naming format (`img_1752587495.jpg`) for image capture and (`vid_1752587495.mjpeg`) for MJPEG video capture. Where `1752587495` is an example of a Unix timestamp taken at file creation.  
- **CVL-007**: System SHALL implement capacity threshold based Paired FIFO deletion of oldest visual evidence in SD Card. When the oldest video is deleted, its corresponding photo MUST also be deleted.  
- **CVL-008**: System SHALL implement capacity threshold based Paired FIFO deletion of oldest visual evidence in SD Card with threshold of remaining empty storage of 50MB. The system CAN delete more than one pair of oldest visual evidence UNTIL the remaining space is back at 50MB.  
- **CVL-009**: System SHALL initialize capacity threshold based Paired FIFO deletion of oldest visual evidence in SD Card at `STATE_IDLE` ONLY ONCE after the system goes into the state.  
- **CVL-010:** System SHALL NOT take pictures and record videos if a MicroSD Card is not available.  
- **CVL-011:** System SHALL NOT turn on flash if the camera initialization fails.  
- **CVL-012**: System SHALL NOT perform any picture taking and video recording if the camera initialization fails.  
- **CVL-013**: System SHALL NOT delete shipment entries in the Firebase Realtime Database.  
- **CVL-014**: System SHALL use the path `/users/user_uid/devices/mac_address/storage` to store photos and videos in Firebase Cloud Storage.  
- **CVL-015**: System SHALL do resolution change before the capture process is initiated (minimum delay is 400ms).

### 3.3 Notification & Connectivity

- **NCC-001**: System SHALL detect if the box storage is full using ToF (Time-of-Flight) sensor.  
- **NCC-002**: System SHALL update status when the storage is full (ToF sensor detects the distance below the threshold).  
- **NCC-003**: System SHALL send visual proof when the package arrives and the box is full.  
- **NCC-004**: System SHALL upload the video requested by the Square application in the `request_video_id` entry in the Firebase Realtime Database.  
- **NCC-005**: System SHALL prioritize video uploads requested by the Square mobile app on the upload queue `upload_queue.txt` on the MicroSD Card.  
- **NCC-006**: System SHALL use hardcoded credentials in the firmware to connect to Wi-Fi and Firebase. The required hardcoded credentials are listed in the table below.  
- **NCC-007**: System SHALL listen for data changes at `remote_sync` nodes to receive commands and sync pins ONLY when there are changes.  
- **NCC-008**: System SHALL implement heartbeat mechanism by updating unix timestamp on Firebase Realtime Database to monitor the device’s connectivity status at a fixed 30 seconds interval.  
- **NCC-009**: System SHALL ignore the used\_quota field value from the stream.

### 3.4 Hardcoded Credentials Definition

| Variable | Description |
| :---- | :---- |
| WIFI\_SSID | User's local Wi-Fi SSID. |
| WIFI\_PASSWORD | User's local Wi-Fi password. |
| FIREBASE\_DATABASE\_URL | Firebase Realtime Database URL (e.g., `project-id-default-rtdb.firebaseio.com`).  |
| FIREBASE\_WEBAPI\_KEY | Firebase Web API Key that is used to identify the Firebase project. |
| FIREBASE\_STORAGE\_BUCKET | Firebase Storage bucket location used to store uploaded .jpg and .mjpeg media. |
| DEVICE\_EMAIL | Dedicated Firebase Authentication email for this specific ESP32 hardware node. |
| DEVICE\_PASSWORD | Password for the dedicated hardware authentication account. |
| USER\_UID | The owner's Firebase Auth UID. Used for constructing the RTDB and Storage directory paths (e.g., `/users/{USER_UID}/...`).  |

## 4\. Network & Data Flow

### 4.1 Communication Protocol

| Data Type / Component | Communication Protocol | Port / Security | Functional Description |
| :---- | :---- | :---- | :---- |
| **Commands & Active Pins Sync (Firebase Realtime Database)** | Server-Sent Events (SSE) | Port 443 / TLS 1.2 | The ESP32-S3 maintains a persistent HTTP Keep-Alive connection (SSE) to listen for commands and PIN updates.  |
| **Status & Log Sync (Firebase Realtime Database)** | HTTPS | Port 443 / TLS 1.2 | The ESP32-S3 updates status and delivery logs using HTTP POST/PATCH for status updates and heartbeat. |
| **Visual Evidence (Storage)** | HTTPS | Port 443 / TLS 1.2 | Sending image (.jpg) and video (.mjpeg) files from a MicroSD Card to Firebase Cloud Storage using an HTTP POST Request. HTTPS was chosen because it is optimal for secure transfer of large data payloads. |
| **Local Buffering (SD Card)** | SPI Bus (Internal) | N/A (Hardware) | Serial peripheral interface protocol for offline writing of photo and video files to a MicroSD card with FAT32 file system format. |

### 4.2 Initial Connection Data Flow

1. **Credential Granting:** The Square system is programmed using the hardcoded credentials listed in the table below.  
2. **Power Connection:** The Square system is connected to a power supply, and all WS2812 indicators will alternately flash red and blue every 1 second.  
3. **Peripheral Initialization:** While the indicators remain flashing, the system initializes all components (keypad, camera, microSD, Wi-Fi, NVS, TOF, and indicator LEDs). The system then attempts to connect to Wi-Fi and Firebase, indicated by the input pin indicators flashing in an alternating pattern from left to right and right to left, alternating between LEDs every 250ms.  
4. **Connection to Wi-Fi and Firebase:** Once connected to Wi-Fi, the WS2812 indicators will glow solid blue. If the Wi-Fi connection fails, the WS2812 indicators will turn red. After successfully connecting to Wi-Fi, the next step is to sync to NTP server (pool.ntp.org). Then continue to connect to Firebase. If the connection to Firebase is successful, the WS2812 LED will turn green.  
5. **Creating an Entry in Firebase:** Using the UID information in the hardcoded credentials, the Square system MUST create a new device entry with its MAC address, along with its sub-entries (delivery logs, status, commands) and submit its heartbeat as last\_heartbeat.

### 4.3 Package Receipt Data Flow

1. **Authentication Request:** The courier enters a PIN via the 3x4 Matrix Keypad. If the input is 4 digits, the system verifies the code against the local cache (NVS).  
2. **Access Granted:** If the PIN is valid, the ESP32 triggers the Solenoid Relay (HIGH) to unlock the door.  
3. **Event Trigger:** The courier opens the door, and the reed switch detects the door status change to "Open." The system immediately starts MJPEG recording. No flash is triggered at this stage to prevent courier inconvenience.  
4. **System Activation:** The courier inserts the package and closes the door. The Reed Switch returns to the "Closed" status, the Solenoid Relay (Low) locks the door, and the system triggers the Post-Delivery Detection routine.  
5. **Visual Image Capture:** As soon as the door is locked, the packet reception log timestamp is captured, MJPEG recording is stopped, the recorded video is saved to the MicroSD Card (`vid_[timestamp].mjpeg`), and the MOSFET triggers the 3W LED flash to a continuous-on state. The OV2640 camera module waits for a 1000ms warm-up period (to stabilize the AWB/AEC), captures one high-resolution frame, and then the LED flash is turned off.  
6. **Local Buffering:** The image is saved to the MicroSD Card (`img_[timestamp].jpg`). The image file name is inserted into the upload queue upload\_queue.txt on the MicroSD Card.  
7. **Capacity Threshold Detection:** The Time-of-Flight (ToF) sensor measures the horizontal distance across a box (standard width \~40 cm). If the stack of packages reaches a height of 38 cm and breaks the ToF laser beam (readings \< 35 cm with a 5 cm error tolerance).  
8. **Cloud Sync:**  
- If Online: The system uploads logs to the Firebase Realtime Database and sends an HTTP POST (Photo) to Firebase Cloud Storage.  
- If Offline: The system retries with the following interval scheme: the first failure waits 5 minutes, the second failure and all subsequent failures wait 15 minutes per attempt.

### 4.4 On-Demand Snapshot Data Flow

1. **Command Reception:** The Square system receives a command to perform an On-Demand Snapshot with the timestamp provided by the Square mobile app. The Square system immediately prepares to take the image.  
2. **Visual Image Capture:** A MOSFET triggers the 3W LED flash to a continuous-on state. The OV2640 camera module waits for a 1000ms warm-up period (to stabilize the AWB/AEC), captures one high-resolution frame, and then the LED flash turns off.  
3. **Local Buffering & Queue Prioritization:** The image is stored on the MicroSD card (`img_[timestamp].jpg`). Then, the image file name is inserted at the front of the upload queue (`upload_queue.txt`) on the MicroSD Card. This prioritizes the user's on-demand snapshot request over standard background uploads.  
4. **Capacity Threshold Detection:** The Time-of-Flight (ToF) sensor measures the horizontal distance across a box (standard width \~40 cm). If the stack of packages reaches a height of 38 cm and breaks the ToF laser beam (readings \< 38 cm with a 2 cm error tolerance).  
5. **Cloud Synchronization:**  
- If Online: The system uploads the log to the Firebase Realtime Database and sends an HTTP POST (Photo) to Firebase Cloud Storage.  
- If Offline: The system cancels the log entry and deletes the captured photo.  
6. **Database Synchronization & Cleanup:** After a successful upload, the `request_photo_ondemand` becomes empty, the app detects this change and immediately accesses the delivery log and looks for an entry with the given timestamp. Then, the app accesses the image link and displays it to the user.

### 4.5 On-Demand Video Request Data Flow

1. **Video Request Command:** The user requests video evidence via the mobile app, which writes the target log's timestamp into the `request_video_id` field in the Firebase Realtime Database. The ESP32-S3 detects this command and prepares to retrieve the corresponding video (`vid_[timestamp].mjpeg`) from the MicroSD Card.  
2. **Queue Prioritization:** The requested video filename is inserted at the front of the upload queue (`upload_queue.txt`) on the MicroSD Card. This prioritizes the user's on-demand retrieval over standard background uploads.  
3. **Cloud Synchronization:**  
- If Online: The system retrieves the video file from the MicroSD Card and executes an HTTP POST request to upload the payload to Firebase Cloud Storage.  
- If Offline: The system immediately aborts the upload process and ignores the request to prevent massive queue blockages upon network reconnection.  
4. **Database Synchronization & Cleanup:** Upon successful upload, the ESP32-S3 updates the `video_evidence_url` field within the specific delivery log entry in the Firebase Realtime Database. Finally, the system resets the `request_video_id` command field to an empty string "" to signal completion and allow the mobile app to display the video.

## 5\. Cloud-Device Interface

### 5.1 Cloud-to-Device Commands 

### RTDB Path: devices/{mac\_address}/remote\_sync/commands

| JSON Key | Type | Action MUST be done by ESP32-S3 | Feedback |
| :---- | :---- | :---- | :---- |
| `request_video_id` | String | If string is not empty (filled with timestamp from mobile app): Start On-Demand Video Request flow. | After the video with the timestamp is uploaded, ESP-S3 MUST empty the `request_video_id` key value to "". |
| `unlock` | Boolean | If `true`: Activate Relay (`HIGH`) to disengage the solenoid lock. | After the relay is activated, ESP32-S3 MUST rewrite the value of `unlock` to `false`. |
| `lock` | Boolean | If `true`: Deactivate Relay (`LOW`) to engage the solenoid lock. | After the relay is deactivated, ESP32-S3  MUST rewrite the value of `lock` to `false`. |
| `request_photo_ondemand` | String | If string is not empty (filled with timestamp from mobile app): Start On-Demand Snapshot flow. | After the photo with the timestamp is uploaded, ESP-S3 MUST empty the `request_photo_ondemand` key value to "". |

**Conflict Resolution:** If both unlock and lock command fields are simultaneously true, the system SHALL prioritize the lock command to engage the solenoid (security-first principle). After executing, both fields MUST be reset to false.

### 5.2 Device-to-Cloud Status

### RTDB Path: devices/{mac\_address}/status

| JSON Key | Type | Condition | Value Definition |
| :---- | :---- | :---- | :---- |
| `last_heartbeat` | String | Refresh every 30 seconds with the key value of the current timestamp.  | `timestamp`: Unix timestamp in seconds of the current timestamp in ESP32. |
| `isOpen` | Boolean | Refreshed every time there are changes in Magnetic Reed Switch physical status. | `true`: The box door is opened. `false`: The box door is closed. |
| `isUnlocked` | Boolean | Refreshed every time there are changes in the logic status of `PIN_RELAY_SOLENOID` GPIO. | `true`: Solenoid active (unlocked).   `false`: Solenoid deactivated (locked). |
| `isFull` | Boolean | Measured and sent 1 time right after the door closing and photo taking sequence is completed. | `true`: ToF distance \< 38 cm (Box is full). `false`: ToF distance ≥ 38 cm (Box is empty). |

## 6\. Non-Functional Requirements

### 6.1 Performance

- **PRF-001**: System SHALL execute solenoid lock response within 2 seconds of a valid PIN entry.   
- **PRF-002**: System SHALL send detection data from the device to arrive to the user's smartphone in no more than 5 seconds under stable network conditions.   
- **PRF-003**: System SHALL complete the transmission of visual evidence to the firebase within 20 seconds under stable network conditions.  
- **PRF-004**: System SHALL implement FreeRTOS for better utilization of CPU resources in handling tasks with diverse loads and preventing conflicts due to more than one hardware using the same communication protocol (e.g., I2C, SPI) simultaneously according to the given table.

### 6.2 Security/Fail-safe

- **SFS-001**: System SHALL implement a failsafe mechanism to keep the door closed (solenoid inactive) when disconnected from the power source.  
- **SFS-002**: System SHALL have the ability to authenticate PINs even when disconnected from Wi-Fi with valid PIN synchronization between Firebase Realtime Database and NVS.  
- **SFS-003**: System SHALL have the ability to capture visual evidence either images or videos even when disconnected from Wi-Fi.  
- **SFS-004**: System SHALL re-lock if within 10 seconds after the PIN is correct, the door is not opened by the courier.  
- **SFS-005**: System SHALL NOT send box fullness status if it is disconnected from firebase.  
- **SFS-006**: System SHALL limit Wi-Fi connection attempts to a maximum of 25 at a polling rate of 300ms. If this limit is reached, the connection is considered a failure, and the user must power cycle before reconnecting.  
- **SFS-007**: System SHALL continue with the system activation phase even if there is a failed hardware initialization.  
- **SFS-008**: System SHALL remain locked on any keypad input if there is no active pin stored and it is not connected to the Firebase Realtime Database.  
- **SFS-009**: System SHALL prevent Task Watchdog Timer (TWDT) timeouts by implementing a minimum non-blocking delay of 10ms using \`vTaskDelay(pdMS\_TO\_TICKS(10))\` to every continuous execution loop (e.g., \`while(1)\` or polling loops) within any FreeRTOS task.  
- **SFS-010**: System SHALL go into STATE\_ERROR if the system runs out of PIN quota AND is not connected to the Firebase.

### 6.3 Debugging

- **DBG-001**: System SHALL log every hardware connection and initialization to the serial monitor as specified at Debug Log Formatting.  
- **DBG-002**: System SHALL log network-related connection initialization to the serial monitor as specified at Debug Log Formatting.  
- **DBG-003**: System SHALL log every firebase process to the serial monitor as specified at Debug Log Formatting.  
- **DBG-004**: System SHALL log every change from sensor and hardware actions to the serial monitor as specified at Debug Log Formatting.  
- **DBG-005**: System SHALL implement well-commented code for easier debugging.  
- **DBG-006**: System SHALL implement a state machine to constrain the system's behavior to well-defined states, making it easier to isolate and debug unexpected transitions.  
- **DBG-007**: System SHALL implement the debug LED patterns in the table below.  
- **DBG-008**: System SHALL perform a dummy write-and-read verification on the MicroSD Card during the peripheral initialization phase and log the final status (PASSED/FAILED) to the serial monitor.   
- **DBG-009**: System SHALL NOT get and display the Wi-Fi module MAC Address in the serial monitor if Wi-Fi initialization fails.  
- **DBG-010**: System SHALL NOT get and display the type and size of MicroSD card if MicroSD card initialization fails.  
- **DBG-011**: System SHALL NOT get and display the number of active pins in NVS if NVS initialization fails..  
- **DBG-012**: System SHALL send fatal error logs at STATE\_ERROR every 1 second.

### 6.4 RTOS Tasks

| Name | Priority | Core | Hardware resources used by the task | Purpose |
| :---- | :---- | :---- | :---- | :---- |
| Task\_Network | 2 | 0 | Wi-Fi, SPI (MicroSD), NVS | Maintains the Wi-Fi connection, performs Firebase Realtime Database handshakes, reads incoming commands and data from Firebase, and processes photo/video uploads to Firebase Storage. |
| Task\_Keypad\_LED | 4 \- Highest | 1 | I2C (Keypad, LED Input Indicator) | Listens for key presses from the PCF8574 keypad I/O expander, records each digit input, and drives the LED indicators accordingly. |
| Task\_StateMachine  | 3 | 1 | Relay Solenoid, I2C (ToF Sensor), WS2812 RGB LED, Reed Switch, NVS | Manage state transitions, monitor all triggers that cause state changes, and coordinate overall system behavior.  |
| Task\_Media | 1 \- Low | 1 | SPI (MicroSD), Flash LED, OV2640 | Handles all media-related operations: recording MJPEG video and still images via the OV2640, controlling the auxiliary flash for photo capture, and writing media files to the MicroSD Card.  |

### 6.5 RTOS IPC (Inter-Process Communication)

| Name | Type | Purpose |
| :---- | :---- | :---- |
| RTDBQueue  | Queue | Queue for outgoing Firebase Realtime Database write operations. Ensures that status updates and log entries are sent sequentially without blocking other tasks. The queue depth is 10\. |
| CommandQueue | Queue (Enum) | Buffers incoming remote commands (e.g., unlock, take photo) parsed from Firebase RTDB stream by `Task_Network`. Ensures asynchronous and non-blocking command execution by the `Task_StateMachine`. The queue depth is 3\. |
| I2C\_Mutex | Mutex | Prevents simultaneous I2C bus access between all three I2C devices (keypad, LED indicator, and ToF sensor). Only one device can use the bus at a time. |
| SD\_Mutex | Mutex | Prevents simultaneous SPI bus access between Task\_Network and Task\_Media in accessing the MicroSD Card. |
| NVS\_Mutex | Mutex | Prevents simultaneous access when reading and modifying the active\_pins JSON string. |

### 6.6 Device States

| State | Trigger In | Trigger Out | Explanation |
| :---- | :---- | :---- | :---- |
| STATE\_START | Power ON or Hardware Reset. | To `STATE_IDLE`: Wi-Fi connected successfully with OR without other hardware problems (Except Wi-Fi) OR Wi-Fi failed AND valid PINs exist in NVS. To `STATE_ERROR`: Wi-Fi failed AND NVS `active_pins` is empty. | Initializes hardware components and WS2812 indicator. Attempts Wi-Fi/Firebase connection with limited attempt. Checks NVS for local active PINs. |
| STATE\_IDLE | Completion of `STATE_START`, `STATE_DELIVERY`, or `STATE_ONDEMAND`.  | To `STATE_DELIVERY`: A valid 4-digit PIN is entered via keypad. To `STATE_ONDEMAND`: `request_photo_ondemand` command received from Firebase Realtime Database. | Standby mode. The system actively listens for physical keypad input or remote Firebase commands without executing heavy tasks.  |
| STATE\_DELIVERY | Valid PIN successfully authenticated (locally or via cloud).  | To `STATE_IDLE`: Delivery sequence completed (media uploaded or queued) OR 10-second timeout triggered if the door is never opened after unlocking.  | Triggered after every 4-digit PIN input. Authenticates the PIN against the local NVS cache if offline, or against Firebase Realtime Database if online. Performs video recording, image capture, ToF measurement, uploads to `storage`, and writes to delivery logs. If it is correct and the door is still closed after 10 seconds, it returns to idle state. |
| STATE\_ONDEMAND | `request_photo_ondemand` command set to request `timestamp` in Firebase Realtime Database.  | To `STATE_IDLE`: Capture and upload sequence completed, and Firebase Realtime Database command is reset to "".  | Performs an on-demand visual capture without unlocking the door. Takes a photo, measures ToF, uploads to `storage`, and writes to delivery logs.  |
| STATE\_ERROR | From `STATE_START`: Total offline isolation (Wi-Fi failed AND (empty NVS OR Failed NVS)).  From `STATE_IDLE/DELIVERY`: The remaining quota in NVS is used up (0) AND the system is disconnected from Firebase.   | None (Dead state). Requires physical power cycle / hard reset by the user.  | Halts all actuator and sensor operations. WS2812 LED turns solid red. Sends a fatal error log every 1 second. |

### 6.7 Debug LED Patterns

| System Condition | WS2812 RGB LED | 4-digit password LED indicator  |
| :---- | :---- | :---- |
| The system is powered up and is initializing the hardware. | Red blue blink every 1 second | All OFF |
| The system has finished initiating the Wi-Fi and is trying to connect. | Red blue blink every 1 second | The flashing pattern alternates from left to right and right to left, alternating between LEDs every 250ms. |
| The system is successfully connected to Wi-Fi. | Solid blue | All OFF |
| The system (failed to connect to Wi-Fi OR Wi-Fi initialization failed OR failed to connect to Firebase) AND ( NVS has NO active pins OR NVS FAILED to initialize). | Solid red | All OFF |
| The system (failed to connect to Wi-Fi OR Wi-Fi initialization failed OR failed to connect to Firebase) AND NVS HAS active pins. | Solid purple | All OFF (Waiting for pin insertion) |
| The system is successfully connected to Firebase and there are no hardware initialization issues. | Solid green | All OFF (Waiting for pin insertion) |
| The system successfully connected to Firebase but there was a hardware initialization issue (except Wi-Fi). | Solid yellow | All OFF (Waiting for pin insertion) |

### 6.8 Debug Log Formatting Examples

#### 6.8.1 Startup and non network-related hardware initialization (STATE\_START)

**Scenario:**   
Non network-related hardware initialization: SUCCESS  
\`\`\`  
\[INFO\] \[SYSTEM\] \=============================================  
\[INFO\] \[SYSTEM\] Square \- Smart Parcel Box by REN  
\[INFO\] \[SYSTEM\] \=============================================  
\[INFO\] \[SYSTEM\] Starting up. WS2812 pattern: RED-BLUE-BLINK  
\[INFO\] \[HARDWARE\] Init Keypad PCF8574 at 0x20: SUCCESS (or FAILED)  
\[INFO\] \[HARDWARE\] Init LED PCF8574 at 0x21: SUCCESS (or FAILED)  
\[INFO\] \[HARDWARE\] Init ToF VL53L0X at 0x29: SUCCESS (or FAILED)  
\[INFO\] \[HARDWARE\] Init OV2640 Camera: SUCCESS (or FAILED)  
\[INFO\] \[NVS\] Init NVS: SUCCESS (or FAILED). Found 3 active pins. (or 0 active pins)  
\[INFO\] \[SD\_CARD\] Init MicroSD: SUCCESS (or FAILED). Type: SDHC, Size: 7284 MB.  
\[INFO\] \[SD\_CARD\] Performing write test: PASSED (or FAILED)  
\`\`\`

#### 6.8.2 Network-related hardware initialization and connection (STATE\_START)

**Scenario:**   
Non network-related hardware initialization: SUCCESS  
Wi-Fi initialization SUCCESS AND Wi-Fi Connection SUCCESS AND Firebase connection SUCCESS  
\`\`\`  
\[INFO\] \[WIFI\] Init Wi-Fi module: SUCCESS (or FAILED). MAC: A1:B2:C3:D4:E5:F6  
\[INFO\] \[SYSTEM\] LED Expander alternating pattern: ON  
\[INFO\] \[WIFI\] Connecting to My\_WiFi\_SSID (1/25)...  
\[INFO\] \[WIFI\] Connecting to My\_WiFi\_SSID (2/25)...  
\[SUCCESS\] \[WIFI\] Connected. IP: 192.168.1.168  
\[INFO\] \[SYSTEM\] LED Expander alternating pattern: OFF  
\[INFO\] \[SYSTEM\] WS2812 pattern: SOLID\_BLUE  
\[INFO\] \[SYSTEM\] Syncing NTP pool.ntp.org: SUCCESS  
\[INFO\] \[FIREBASE\] Authenticating...  
\[SUCCESS\] \[FIREBASE\] Connected to Firebase.   
\[INFO\] \[SYSTEM\] WS2812 Set to: SOLID\_GREEN.   
\[INFO\] \[SYSTEM\] Initialization completed. \-\> \[STATE\_IDLE\]  
\`\`\`  
**Scenario:**   
Non network-related hardware initialization: PARTIAL  
Wi-Fi initialization SUCCESS AND Wi-Fi Connection SUCCESS AND Firebase connection SUCCESS

\`\`\`  
\[INFO\] \[WIFI\] Init Wi-Fi module: SUCCESS. MAC: A1:B2:C3:D4:E5:F6  
\[INFO\] \[SYSTEM\] LED Expander alternating pattern: ON  
\[INFO\] \[WIFI\] Connecting to My\_WiFi\_SSID (1/25)...  
\[INFO\] \[WIFI\] Connecting to My\_WiFi\_SSID (2/25)...  
\[SUCCESS\] \[WIFI\] Connected. IP: 192.168.1.168  
\[INFO\] \[SYSTEM\] LED Expander alternating pattern: OFF  
\[INFO\] \[SYSTEM\] WS2812 pattern: SOLID\_BLUE  
\[INFO\] \[SYSTEM\] Syncing NTP pool.ntp.org: SUCCESS  
\[INFO\] \[FIREBASE\] Authenticating...  
\[SUCCESS\] \[FIREBASE\] Connected to Firebase.   
\[INFO\] \[SYSTEM\] WS2812 Set to: SOLID\_YELLOW.   
\[INFO\] \[SYSTEM\] Initialization completed with warnings. \-\> \[STATE\_IDLE\]  
\`\`\`  
**Scenario:**   
Non network-related hardware initialization: SUCCESS OR PARTIAL  
Wi-Fi initialization FAILED OR Wi-Fi Connection FAILED OR Firebase connection FAILED  
NVS HAS active pins  
\`\`\`  
\[INFO\] \[SYSTEM\] LED Expander alternating pattern: ON  
\[INFO\] \[WIFI\] Connecting to My\_WiFi\_SSID (1/25)...  
.  
.  
.  
\[INFO\] \[WIFI\] Connecting to My\_WiFi\_SSID (25/25)... FAILED.  
\[INFO\] \[SYSTEM\] LED Expander alternating pattern: OFF  
\[WARN\] \[FIREBASE\] Network unavailable. Skipping authentication.  
\[INFO\] \[NVS\] Checking active pins... Found 3 active pins. There's still hope\!  
\[WARN\] \[SYSTEM\] Entering Offline Mode. WS2812 Set to: SOLID\_PURPLE.   
\[INFO\] \[SYSTEM\] Initialization completed with warnings. \-\> \[STATE\_IDLE\]  
\`\`\`  
**Scenario:**   
Non network-related hardware initialization: SUCCESS OR PARTIAL  
Wi-Fi initialization FAILED OR Wi-Fi Connection FAILED OR Firebase connection FAILED  
NVS HAS NO active pins OR NVS FAILED to initialize.  
\`\`\`  
\[INFO\] \[SYSTEM\] LED Expander alternating pattern: ON  
\[INFO\] \[WIFI\] Connecting to My\_WiFi\_SSID (1/25)...  
.  
.  
.  
\[INFO\] \[WIFI\] Connecting to My\_WiFi\_SSID (25/25)... FAILED.  
\[INFO\] \[SYSTEM\] LED Expander alternating pattern: OFF  
\[WARN\] \[FIREBASE\] Network unavailable. Skipping authentication.  
\[INFO\] \[NVS\] Checking active pins... 0 active pins found.  
\[ERROR\] \[SYSTEM\] No network and no active pins\! System isolated.  
\[ERROR\] \[SYSTEM\] WS2812 Set to: SOLID\_RED.  
\[INFO\] \[SYSTEM\] Halting operations. \-\> \[STATE\_ERROR\]

\`\`\`

#### 6.8.3 System Background Maintenance (STATE\_IDLE)

\`\`\`  
\[WARN\] \[SD\_CARD\] Space \< 50MB. Performing Paired FIFO...  
\[SUCCESS\] \[SD\_CARD\] Deleted pair 1752587495\.  
\[INFO\] \[FIREBASE\] Sending heartbeat (1752597477)... SUCCESS  
\[ERROR\] \[FIREBASE\] Sending heartbeat (1752597477)... FAILED  
\[INFO\] \[FIREBASE\] New active pins detected (2026) with quota 3  
\[INFO\] \[NVS\] Active pins (2026) saved to NVS\!  
\[INFO\] \[FIREBASE\]   
\`\`\`

#### 6.8.4 Courier Authentication (STATE\_IDLE)

\`\`\`  
\[INFO\] \[KEYPAD\] Key pressed: 1  
\[INFO\] \[KEYPAD\] Key pressed: 5  
\[INFO\] \[KEYPAD\] Key pressed: 5  
\[INFO\] \[KEYPAD\] Key pressed: 4  
\[INFO\] \[KEYPAD\] Courier input buffer complete: 1554  
\[INFO\] \[FIREBASE\] Verifying PIN with Firebase...  
\[WARN\] \[FIREBASE\] Incorrect PIN. Resetting buffer.  
\[INFO\] \[NVS\] Verifying PIN 2026 with NVS (Offline mode)...  
\[SUCCESS\] \[NVS\] PIN 2026 is correct\!   
\[INFO\] \[SYSTEM\] Transitioning \-\> \[STATE\_DELIVERY\]  
\`\`\`

#### 6.8.5 Delivery Flow & Evidence Recording (STATE\_DELIVERY)

\`\`\`  
\[INFO\] \[DOOR\] Solenoid unlocked.   
\[INFO\] \[FIREBASE\] Status isUnlocked updated to true.   
Waiting for the courier (10s timeout)...  
\[INFO\] \[DOOR\] Reed switch: OPEN.   
\[INFO\] \[FIREBASE\] Status isOpen updated to true.   
\[INFO\] \[DOOR\] Starting video capture...  
\[INFO\] \[DOOR\] Reed switch: CLOSED.   
\[INFO\] \[FIREBASE\] Status isOpen updated to false.  
Timestamp: 1752587495\. Solenoid locked.  
\[INFO\] \[FIREBASE\] Status isUnlocked updated to false.  
\[INFO\] \[NVS\] PIN 2026 used\_quota incremented.  
\[INFO\] \[FIREBASE\] PIN 2026 used\_quota incremented.  
\[INFO\] \[CAMERA\] Stopping video. Saved to: vid\_1752587495.mjpeg  
\[INFO\] \[CAMERA\] Flash ON. Capturing photos...   
\[SUCCESS\] \[CAMERA\] Photo saved to: img\_1752587495.jpg. Flash OFF.  
\[INFO\] \[SD\_CARD\] Pushing img\_1752587495.jpg to upload\_queue.txt  
\[INFO\] \[TOF\] Distance: 39 cm. isFull: false (Threshold: \<38cm)  
\[INFO\] \[FIREBASE\] Status isFull updated to false.  
\[INFO\] \[FIREBASE\] Adding new delivery logs entry...  
\[SUCCESS\] \[FIREBASE\] Delivery logs entry added  
\[INFO\] \[SYSTEM\] Delivery sequence complete. \-\> \[STATE\_IDLE\]  
\`\`\`

#### 6.8.6 On-Demand Request

**Scenario:**  
SUCCESSFUL.  
\`\`\`  
\[INFO\] \[FIREBASE\] On-demand video request received for timestamp: 1752587495  
\[INFO\] \[SD\_CARD\] Pushing vid\_1752587495.mjpeg to front of upload\_queue.txt  
\[INFO\] \[FIREBASE\] Uploading vid\_1752587495.mjpeg...  
\[SUCCESS\] \[FIREBASE\] vid\_1752587495.mjpeg uploaded successfully.  
\[INFO\] \[FIREBASE\] Updating video\_evidence\_url in RTDB.  
\[INFO\] \[FIREBASE\] Request satisfied.

\[INFO\] \[FIREBASE\] On-demand snapshot request received\! Timestamp: 1752587598  
\[INFO\] \[SYSTEM\] Transitioning \-\> \[STATE\_ONDEMAND\]  
\[INFO\] \[CAMERA\] Flash ON. Capturing photo...   
\[SUCCESS\] \[CAMERA\] Photo saved: img\_1752587598.jpg. Flash OFF.  
\[INFO\] \[SD\_CARD\] Pushing img\_1752587598.jpg to front of upload\_queue.txt  
\[INFO\] \[TOF\] Distance: 15 cm. isFull: true  
\[INFO\] \[FIREBASE\] Status isFull updated to true.  
\[INFO\] \[FIREBASE\] Uploading snapshot logs...   
\[SUCCESS\] \[FIREBASE\] Snapshot logs successfully uploaded.   
\[INFO\] \[SYSTEM\] Snapshot sequence complete. \-\> \[STATE\_IDLE\]  
\`\`\`  
**Scenario:**  
FAILED.  
\`\`\`  
\[INFO\] \[FIREBASE\] On-demand video request received for timestamp: 1752587495  
\[INFO\] \[SD\_CARD\] Pushing vid\_1752587495.mjpeg to front of upload\_queue.txt  
\[INFO\] \[FIREBASE\] Uploading vid\_1752587495.mjpeg...  
\[ERROR\] \[FIREBASE\] vid\_1752587495.mjpeg failed to upload. Aborting…  
\[INFO\] \[SD\_CARD\] Deleting vid\_1752587495.mjpeg from upload\_queue.txt  
\[ERROR\] \[FIREBASE\] Request not satisfied\!

\[INFO\] \[FIREBASE\] On-demand snapshot request received\! Timestamp: 1752587598  
\[INFO\] \[SYSTEM\] Transitioning \-\> \[STATE\_ONDEMAND\]  
\[ERROR\] \[CAMERA\] Camera failed\! No video and photo capture\!  
\[ERROR\] \[SD\_CARD\] MicroSD failed\! No video and photo capture\!   
\[ERROR\] \[SYSTEM\] Snapshot sequence aborted. \-\> \[STATE\_IDLE\]  
\`\`\`

#### 6.8.7 Remote Lock Control

\`\`\`  
\[INFO\] \[FIREBASE\] Door unlock command received\!  
\[INFO\] \[DOOR\] Solenoid unlocked.   
\[INFO\] \[FIREBASE\] Status isUnlocked updated to true.   
\[INFO\] \[FIREBASE\] Clearing unlock command… Command satisfied

\[INFO\] \[FIREBASE\] Door lock request received\!  
\[INFO\] \[DOOR\] Solenoid locked.   
\[INFO\] \[FIREBASE\] Status isUnlocked updated to false.   
\[INFO\] \[FIREBASE\] Clearing lock command… Command satisfied

\[ERROR\] \[FIREBASE\] Door lock and unlock request received simultaneously\!  
\[INFO\] \[DOOR\] Solenoid locked.   
\[INFO\] \[FIREBASE\] Status isUnlocked updated to false.   
\[INFO\] \[FIREBASE\] Clearing lock and unlock command… Failsafe approach  
\`\`\`

#### 6.8.8 Fatal Error (STATE\_ERROR)

\`\`\`  
\[ERROR\] \[SYSTEM\] Fatal error occurred\! No local active pins, no connection to Firebase. Please power cycle.   
\[ERROR\] \[SYSTEM\] Fatal error occurred\! No local active pins, no connection to Firebase. Please power cycle.  
\`\`\`

## 7\. Data Schema & Storage Architecture

### 7.1 Firebase Realtime Database JSON Tree

\`\`\`json  
{  
"users": {  
    "user\_uid": {  
        "username": "benson.dunwoody",  
        "devices": {  
            "25d57a9eba89": {  
                "delivery\_logs": {  
                    "1752587495": {  
                        "used\_pin": "4584",  
                        "event\_type": "PackageInsertion",  
                        "photo\_evidence\_url": "firebase\_storage\_url\_for\_img\_1752587495.jpg",  
                        "video\_evidence\_url": "firebase\_storage\_url\_for\_vid\_1752587495.mjpeg"  
                    },  
                    "1752587598": {  
                    "used\_pin": "",  
                    "event\_type": "OnDemandSnapshot",  
                    "photo\_evidence\_url": "firebase\_storage\_url\_for\_img\_1752587598.jpg"  
                    }  
                },  
                "device\_name": "mySquare",  
                "remote\_sync": {  
                    "active\_pins": {  
                        "4584": {  
                            "quota": 5,  
                            "used\_quota": 1  
                        }  
                    },  
                    "commands": {  
                        "request\_video\_id": "",  
                        "unlock": false,  
                        "lock": false,  
                        "request\_photo\_ondemand": ""  
                    }  
                },  
                "status": {  
                    "last\_heartbeat": "1752597477",  
                    "isFull": false,  
                    "isOpen": false,  
                    "isUnlocked": false  
                }  
            }  
        }  
    }  
  }  
}  
\`\`\`  
**Structure Purpose:**

- **users/**: Contains all authenticated user entries. Each entry has a name with a UID from Firebase Authentication.  
- **user\_uid/**: Contains user profile data, including username (using local part of email address e.g, benson.dunwoody if the email used for user registration in mobile app is benson.dunwoody@gmail.com), and connected devices.  
- **devices/**: Contains all of the user's Square Box entries. Each entry is named using the device's 12-digit hexadecimal MAC address.   
- **25d57a9eba89/**: Example of a paired Square device entry. Contains active PIN, device name, shipping log, status, and commands.  
- **delivery\_logs/**: Contains all packet reception and on-demand snapshot log entries for this device. Each entry is named based on a Unix timestamp (the timestamp is based on the time the door closed after the packet was inserted, or the time the application triggered the on-demand snapshot).  
- **remote\_sync/**: Encapsulate entries that are streamed to the paired Square device every time there is a change.   
- **active\_pins/**: Contains active PINs for the device. Each entry stores the total quota and the number of times the PIN has been used.   
- **commands/**: Contains fields used by the mobile app to interact with Square remotely, including requesting video uploads, locking or unlocking the solenoid, and triggering manual photo capture.  
- **status/**: Contains the real-time status of the Square box, which is used by the mobile app to display: connection status to Firebase Realtime Database (determined by the mobile app by calculating the delta between last\_heartbeat and the current time), box fullness level (ToF based), door open status (reed switch) and locked status (relay).  
- **1752587495/**: Example delivery log entry. The key is the Unix timestamp of the door closing after the package was inserted (or the time of a manual trigger). This entry contains the URL of the photo and video evidence, the PIN used to authorize access (blank if manually triggered), and the event type (`OnDemandSnapshot` or `PackageInsertion`).

### 7.2 Firebase Storage Schema

\`\`\`  
/  
└── users/  
    └── {user\_uid}/  
        ├── profile.jpg  
        └── devices/  
            └── {12\_digit\_mac\_address\_hex}/  
                └── storage/  
                    ├── 1752587495/  
                     |    ├── img\_1752587495.jpg  
                     |    └── vid\_1752587495.mjpeg  
                    └── 1752587598/  
                          └── img\_1752587598.jpg  
\`\`\`  
**Folder Purpose:**

- **storage/**: The primary storage for all newly uploaded media. All new delivery media and on-demand snapshots are uploaded here by default.  
- **1752587495/**: An example of a delivery media folder with a packet insertion video (the user requested the video from the app), 1752587495, is a folder named after the Unix timestamp of the packet capture, measured when the door was closed again. Or during manual triggering, measured at the time of the request in the app.  
- **1752587598/**: An example of a delivery media folder without a packet insertion video (either the user has never requested a video from the application or it is an entry from a manual trigger), 1752587598 is a folder named based on the Unix timestamp of the packet capture, measured when the door is closed again. Or during manual triggering, measured at the time of the request in the app.

### 7.3 MicroSD Card Directory

\`\`\`  
/  
├── upload\_queue.txt  
├── 1752587495/  
│   ├── img\_1752587495.jpg  
│   └── vid\_1752587495.mjpeg  
└── 1752587598/  
     └── img\_1752587598.jpg  
\`\`\`  
**Purpose:**

- **upload\_queue.txt**: This is a file containing a queue for file uploads. When a new photo or video arrives from a packet, the photo's file name is added to the queue. The ESP32 then reads the queue and uploads it to Firebase. Once the upload is complete, the file name is removed from the queue. For user-requested photos or photos requested via on-demand snapshots, the file name is also added to the queue. If there is currently a queue, the user's request will be first in line (to be uploaded first).  
- **1752587495/**: For example, the local media folder 1752587495 is a folder named after the Unix timestamp of the packet capture, measured when the door is closed again. Or during manual triggering, measured at the request time.

### 7.4 NVS Schema

| Key | Data Type | Max Size / Format | Functional Description |
| :---- | :---- | :---- | :---- |
| active\_pins | String (str) | 512 bytes (Serialized JSON) | Stores a list of active PINs containing their max quota ('q') and used quota ('u') in a minified JSON string format. Example: `{"1234":{"q":3,"u":1}, "8976":{"q":1,"u":0}}`. |

### 7.5 Flash Partition Scheme

Using default 16MB partition scheme provided by the ESP32 Arduino core:

| Name | Type | SubType | Offset | Size | Flags |
| :---- | :---- | :---- | :---- | :---- | :---- |
| nvs | data | nvs | 0x9000 | 0x5000 (20 KB) |  |
| otadata | data | ota | 0xE000 | 0x2000 (8 KB) |  |
| app0 | app | ota\_0 | 0x10000 | 0x640000 (6,25 MB) |  |
| app1 | app | ota\_1 | 0x650000 | 0x640000 (6,25 MB) |  |
| spiffs | data | spiffs | 0xC90000 | 0x360000 (3,37 MB) |  |
| coredump | data | coredump | 0xFF0000 | 0x10000 (64 KB) |  |

## 8\. Dependencies

### 8.1 Used libraries

- mobizt/FirebaseClient @ ^2.2.9  
- adafruit/Adafruit\_VL53L0X @ ^1.2.4  
- adafruit/Adafruit\_NeoPixel @ ^ 1.15.5  
- bblanchon/ArduinoJson@ ^7.4.3

## 9\. Project Source Code Structure

\`\`\`  
Square-SmartParcelBox/  
├── platformio.ini  
├── src/  
│   ├── main.cpp                  ← Entry point: Task initialization  
│   ├── pins.h                    ← All pin constants (MUST be the only source)  
│   ├── credentials.h             ← All hardcoded credentials  
│   ├── config.h                  ← Non-pin constants (threshold, timeout, etc)  
│   │  
│   ├── hal/  
│   │   ├── ws2812.h / .cpp       ← WS2812 driver (Adafruit\_NeoPixel wrapper)  
│   │   ├── relay.h / .cpp        ← Solenoid relay driver  
│   │   ├── flash\_led.h / .cpp    ← Aux flash LED driver  
│   │   ├── reed\_switch.h / .cpp  ← Magnetic reed switch driver  
│   │   ├── tof.h / .cpp          ← VL53L0X ToF sensor driver  
│   │   ├── camera.h / .cpp       ← OV2640 driver (foto \+ video)  
│   │   ├── sdcard.h / .cpp       ← MicroSD driver (mount, R/W, queue)  
│   │   ├── keypad\_driver.h / .cpp ← Custom keypad library  
│   │   ├── keybuffer\_indicator\_driver.h / .cpp ← Custom keypad input buffer LED indicator  
│   │   └── nvs\_store.h / .cpp    ← NVS driver (Preferences wrapper)  
│   │  
│   ├── tasks/  
│   │   ├── task\_network.h / .cpp  
│   │   ├── task\_keypad\_led.h / .cpp  
│   │   ├── task\_statemachine.h / .cpp  
│   │   └── task\_media.h / .cpp  
│   │  
│   ├── state/  
│   │   └── state\_machine.h / .cpp ← State enum \+ state transition logic  
│   │  
│   └── firebase/  
│       ├── fb\_rtdb.h / .cpp       ← All Firebase RTDB operations  
│       └── fb\_storage.h / .cpp    ← All Firebase Storage upload operation  
│  
└── lib/  
    └── (empty — all library via platformio.ini)  
\`\`\`

## 

## Revision History

| Version | Date | Author | Changes |
| :---- | :---- | :---- | :---- |
| 1.0 | 2026-02-25 | REN | Initial specification |
| 1.1 | 2026-03-14 | REN | Change the data flow to not to take photo when the door is opened, but when the package is inserted and the door is closed |
| 1.2 | 2026-03-15 | REN | Inverting the logic of the door switch |
| 1.3 | 2026-03-30 | REN | Add FreeRTOS for better utilization of CPU resources |
| 1.4 | 2026-04-05 | REN | Removed send logs via WSS and added requirements for well-commented code |
| 1.5 | 2026-04-27 | REN | Updated the specifications regarding the relay module and clarified its logic requirements (Active low) |
| 1.6 | 2026-04-30 | REN | Added additional clauses to clarify debugging process |
| 1.7 | 2026-05-18 | REN | Clarifying and adding details (e.g, FreeRTOS Task, FreeRTOS IPC, System States, and Firebase RTDB, Firebase Storage, MicroSD Card, and NVS scheme) Adding a new feature: MJPEG capture. Adding `adafruit/Adafruit_NeoPixel @ ^ 1.15.5` library dependency for WS2812 RGB LED indicator control. |
| 1.8 | 2026-05-21 | REN | Added FIREBASE\_STORAGE\_BUCKET as one of the hardcoded credentials. Using default 16MB partitions instead of custom partitions. Added new debugging requirements (DBG-008) to do write tests to MicroSD Card. Added new access control requirements (ACC-008) to ensure that physical keypad inputs are registered exactly once per valid keystroke. |
| 1.9 | 2026-05-22 | REN | Added SFS-009 to solve the task watchdog timer issue. Removed `robtillaart/I2CKeyPad @ ^0.5.0` library dependency, using custom library instead. Adding `bblanchon/ArduinoJson@ ^7.4.3` library dependency for JSON serialization (useful for storing active\_pins in NVS). |
| 1.10 | 2026-05-23 | REN | Added clause about NTP syncing at Initial Connection Data Flow. |
| 1.11 | 2026-05-24 | REN | Added requirements for debug log formatting. Added DBG-010 until DBG-012 about debug logs details of failed devices. Added SFS-010 about transition to STATE\_ERROR. Added mutex for NVS. Added Queue to buffer received commands (as enum). |
| 1.12 | 2026-05-25 | REN | Changed RTDB JSON to encapsulate active\_pins and command into remote\_sync. |
| 1.13 | 2026-07-28 | REN | Added CVL-015 to address missing AWB/AEC calibration and 0KB video issue. |

