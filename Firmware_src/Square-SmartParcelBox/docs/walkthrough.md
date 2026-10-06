# Walkthrough — Phase 0 Implementation

Fase 0 dari rencana implementasi telah berhasil diimplementasikan. Berikut adalah ringkasan dari file-file yang telah dibuat dan diverifikasi:

## Perubahan yang Dilakukan

1. **[pins.h](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/pins.h)** [NEW]
   - Mendefinisikan semua pemetaan pin GPIO ESP32-S3 (WROOM) N16R8 untuk modul kamera (OV2640), SD card, WS2812 status LED, reed switch, relay solenoid, flash LED.
   - Mendefinisikan alamat I2C untuk Keypad PCF8574 (`0x20`), LED Indicator PCF8574 (`0x21`), dan ToF VL53L0X (`0x29`).

2. **[credentials.h](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/credentials.h)** [NEW]
   - Menyediakan placeholder kredensial yang dibutuhkan untuk Wi-Fi (SSID, password) dan integrasi Firebase (Database URL, Web API Key, Storage Bucket, email/password otentikasi perangkat, dan owner UID).

3. **[config.h](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/config.h)** [NEW]
   - Mendefinisikan seluruh konstanta non-pin seperti toleransi sensor ToF (`TOF_TOLERANCE_CM` senilai `2` cm) yang menggantikan `TOF_THRESHOLD_CM` sehingga threshold deteksi (38 cm) dihitung secara dinamis (`TOF_BOX_WIDTH_CM` - `TOF_TOLERANCE_CM`), timeout pintu (`10` detik), interval detak jantung (`30` detik), batas maksimal percobaan Wi-Fi (`25` kali), ukuran antrean RTDB/Command, jeda waktu percobaan ulang offline, debounce keypad (`200` ms), dan delay minimum RTOS (`10` ms).

4. **[ws2812.h](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/hal/ws2812.h) / [ws2812.cpp](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/hal/ws2812.cpp)** [NEW]
   - Mengimplementasikan driver status LED WS2812 menggunakan pustaka `Adafruit_NeoPixel`.
   - Menyediakan fungsi `ws2812_init()`, `ws2812_set_pattern()`, dan `ws2812_update()`.
   - Mengimplementasikan seluruh pola indikasi sistem (OFF, berkedip merah-biru setiap 1 detik secara non-blocking dengan `millis()`, solid biru, solid hijau, solid kuning, solid ungu, dan solid merah).

5. **[relay.h](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/hal/relay.h) / [relay.cpp](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/hal/relay.cpp)** [NEW]
   - Mengimplementasikan driver solenoid relay.
   - Menyediakan fungsi inisialisasi `relay_init()` (set GPIO ke mode `OUTPUT` dan default status `LOW`/terkunci).
   - Menyediakan fungsi kontrol `relay_unlock()` (set GPIO `HIGH` untuk membuka kunci) dan `relay_lock()` (set GPIO `LOW` untuk mengunci).
   - Menyediakan fungsi status `relay_is_unlocked()` untuk memeriksa status fisik saat ini. Catatan: Fungsi ini telah direvisi oleh pengguna menggunakan operator ternary untuk secara eksplisit mengembalikan `true`/`false` berdasarkan apakah pembacaan pin bernilai `HIGH`.

6. **[flash_led.h](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/hal/flash_led.h) / [flash_led.cpp](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/hal/flash_led.cpp)** [NEW]
   - Mengimplementasikan driver LED flash bantu (auxiliary flash LED) berbasis MOSFET (Active HIGH).
   - Menyediakan fungsi `flash_led_init()` (set GPIO ke mode `OUTPUT` dan default status `LOW`/mati).
   - Menyediakan fungsi kontrol `flash_led_on()` (set GPIO `HIGH` untuk menyalakan) dan `flash_led_off()` (set GPIO `LOW` untuk mematikan).

7. **[reed_switch.h](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/hal/reed_switch.h) / [reed_switch.cpp](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/hal/reed_switch.cpp)** [NEW]
   - Mengimplementasikan driver magnetic reed switch pintu.
   - Menyediakan fungsi inisialisasi `reed_switch_init()` (set GPIO `PIN_DOOR_SWITCH` ke mode `INPUT_PULLUP`).
   - Menyediakan fungsi status `reed_switch_is_closed()` (return `true` jika GPIO bernilai `HIGH`, pintu tertutup) dan `reed_switch_is_open()` (return `true` jika GPIO bernilai `LOW`, pintu terbuka).
   - Mengimplementasikan algoritma debounce non-blocking menggunakan `millis()` untuk memperbaiki masalah bouncing fisik (`REED_SWITCH_DEBOUNCE_MS = 50`).
   - Menambahkan fungsi baru `reed_switch_update()` untuk memproses status yang sekarang dipanggil secara terus-menerus di `loop()` pada `main.cpp`.
   - Fungsi `reed_switch_is_closed()` and `reed_switch_is_open()` sekarang mengembalikan status stabil yang telah di-debounce.

8. **[keypad_driver.h](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/hal/keypad_driver.h) / [keypad_driver.cpp](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/hal/keypad_driver.cpp)** [NEW]
   - Mengimplementasikan pustaka custom driver keypad matrix 3x4 menggunakan PCF8574 I/O Expander tanpa pustaka eksternal (ACC-009).
   - Baris (P0-P3) dikonfigurasi sebagai outputs (Active LOW) dan Kolom (P4-P6) dikonfigurasi sebagai quasi-bidirectional inputs dengan internal weak pull-up diaktifkan.
   - Tombol `*` dan `#` yang sebelumnya diabaikan kini dipetakan dan dikembalikan secara normal untuk memfasilitasi pengujian 3x4 matriks.
   - Mengimplementasikan mekanisme debouncing (`KEYPAD_DEBOUNCE_MS` = 200 ms) dan deteksi edge untuk menjamin karakter hanya dikembalikan sekali per satu kali penekanan tombol fisik (ACC-008).

9. **[keybuffer_indicator_driver.h](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/hal/keybuffer_indicator_driver.h) / [keybuffer_indicator_driver.cpp](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/hal/keybuffer_indicator_driver.cpp)** [NEW]
   - Mengimplementasikan driver LED Indicator (4x LED) via PCF8574 I2C expander pada alamat `0x21` (Active LOW).
   - Menyediakan fungsi inisialisasi dan pengaturan via `led_indicator_init()`, `led_indicator_set_count()`, dan `led_indicator_all_off()`.
   - Mengimplementasikan animasi indikator non-blocking menggunakan `led_indicator_alternating_pattern()`, `led_indicator_stop_pattern()`, dan `led_indicator_update()` yang dipanggil konstan di `loop()`.

10. **[tof.h](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/hal/tof.h) / [tof.cpp](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/hal/tof.cpp)** [NEW]
    - Mengimplementasikan driver sensor jarak Time of Flight (VL53L0X) menggunakan pustaka `Adafruit_VL53L0X`.
    - Menggunakan alamat I2C `0x29` dan membaca jarak dalam sentimeter.
    - Evaluasi kapasitas kotak (`tof_is_full()`) menggunakan ambang batas berdasarkan kalkulasi `TOF_BOX_WIDTH_CM` dikurangi `TOF_TOLERANCE_CM`.
    - Menyediakan fungsi `tof_init()`, `tof_read_distance_cm()`, `tof_is_full()`, dan `tof_is_available()`.

11. **[camera.h](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/hal/camera.h) / [camera.cpp](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/hal/camera.cpp)** [NEW]
    - Mengimplementasikan driver kamera OV2640 (Fase 4) untuk ESP32S3 menggunakan `esp_camera.h`.
    - Mengkonfigurasi penggunaan PSRAM untuk menyangga *frame buffer*.
    - Menginisialisasi format piksel ke JPEG dan resolusi awal UXGA (1600x1200).
    - Menyediakan fungsi manajemen seperti `camera_init()`, `camera_capture_photo()`, `camera_return_fb()`, pengaturan resolusi, serta `camera_is_available()`.
    - Melakukan modifikasi pada parameter `camera_config_t` demi stabilitas modul ESP32-S3 dan perbaikan galat `0x105`:
      - Menetapkan `config.sccb_i2c_port = 1` agar alokasi internal I2C kamera tidak bertabrakan dengan instance `Wire` (I2C 0) yang digunakan sensor-sensor lain.
      - Menurunkan `config.xclk_freq_hz` menjadi 10 MHz guna mengatasi *noise* dan perbaikan integritas sinyal transmisi *hardware*.
      - Mengubah batasan buffer DMA dengan menggunakan 1 *frame buffer* (`fb_count = 1`) dan JPEG Quality = 12 untuk mencegah gagalnya alokasi ruang memori untuk resolusi setinggi UXGA.

12. **[sdcard.h](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/hal/sdcard.h) / [sdcard.cpp](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/hal/sdcard.cpp)** [NEW]
    - Mengimplementasikan driver kartu memori MicroSD (Fase 4) menggunakan library bawaan `SD_MMC` dan beroperasi dalam mode jalur 1-bit.
    - Menyediakan fitur I/O berkas lengkap (`sdcard_save_file`, `sdcard_read_file`, `sdcard_file_exists`, `sdcard_delete_file`, `sdcard_delete_directory`).
    - Menyediakan API diagnostik kesehatan memori (`sdcard_get_free_space_mb`, `sdcard_get_size_mb`, `sdcard_get_type_string`).
    - Mengimplementasikan fitur struktur data antrean (*queue*) yang disinkronisasi ke `/upload_queue.txt` untuk mendukung operasi `push_back`, `push_front`, `pop_front`, dan `peek_front`.
    - Mengimplementasikan fungsi pelenyapan otomatis (*FIFO Cleanup*) menggunakan parameter ambang batas `SD_MIN_FREE_MB` untuk menghapus berkas-berkas tangkapan lama (sistem FIFO berpasangan) bila kapasitas kosong (*free space*) memori menipis.

13. **[nvs_store.h](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/hal/nvs_store.h) / [nvs_store.cpp](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/hal/nvs_store.cpp)** [NEW]
    - Mengimplementasikan driver memori *Non-Volatile Storage* (NVS) persisten menggunakan *library* bawaan ESP32 `Preferences.h`.
    - Modul ini bertugas menampung senarai PIN akses kurir aktif dengan format objek *JSON string* termampatkan (menggunakan `ArduinoJson`) pada *namespace* `"box_prefs"`.
    - Menyediakan fitur CRUD (*Create, Read, Update, Delete*) entri PIN (`nvs_store_save_pin`, `nvs_store_delete_pin`, `nvs_store_clear_all`) serta pembacaan limit/kuota.
    - Mengimplementasikan logika pengamanan berlapis: perhitungan pemakaian ditambahkan via `nvs_store_increment_used`, dan apabila batas *quota* sudah tercapai, akses PIN tersebut seketika ditarik/dihapus secara otomatis.

14. **[state_machine.h](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/state/state_machine.h) / [state_machine.cpp](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/state/state_machine.cpp)** [NEW]
    - Mendirikan fondasi Fase 6 (Integrasi Logika Utama) dengan menerapkan arsitektur *State Machine* (Mesin Keadaan).
    - Mendefinisikan enumerasi `SystemState` (`STATE_START`, `STATE_IDLE`, `STATE_DELIVERY`, `STATE_ONDEMAND`, `STATE_ERROR`) dan `RemoteCommand`.
    - Mengimplementasikan fungsi validasi transisi yang ketat (`sm_transition_to`), yang memblokir secara absolut lompatan ilegal (contoh: memblokir upaya keluar dari `STATE_ERROR` atau melompat dari `STATE_START` ke `STATE_DELIVERY`).
    - Menyediakan fungsi pemantauan *status* yang komprehensif, mencakup `sm_get_state()`, `sm_is_idle()`, and `sm_is_error()`.

15. **[fb_rtdb.h](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/firebase/fb_rtdb.h) / [fb_rtdb.cpp](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/firebase/fb_rtdb.cpp)** [NEW]
    - Mengimplementasikan ulang secara presisi Fase 7 (*driver Firebase Realtime Database*) dengan meninjau langsung *source code* contoh `RealtimeDatabase/Asynchronous` bawaan `mobizt/FirebaseClient` (v2.x). Kini modul sepenuhnya memisahkan instance SSL Client ganda (*AsyncClient* biasa vs *Stream Client*) secara *non-blocking* murni tanpa resiko *crash* saat sinkronisasi berjalan.
    - Menyediakan fungsionalitas koneksi WiFi mandiri dengan perlindungan batas *retries* (tidak membekukan perangkat jika internet sedang *offline*).
    - Menyediakan rutin sinkronisasi otomatis (*Server-Sent Events / SSE*) via fungsi `fb_rtdb_stream_loop()` untuk memantau perintah kendali jarak jauh.
    - **Migrasi Callback SSE Stream**: Mengubah mekanisme pemantauan event SSE Stream dari metode *polling* manual (memeriksa `streamResult` di dalam loop) menjadi mekanisme berbasis callback (`fb_rtdb_stream_callback`). Langkah ini sesuai dengan rekomendasi pustaka `mobizt/FirebaseClient` untuk memastikan pengiriman event yang lebih andal serta manajemen status koneksi/rekoneksi secara otomatis di latar belakang.
    - **Log Diagnostik Stream**: Menambahkan pencetakan informasi event, pesan debug, dan galat (error) secara langsung ke Serial monitor di dalam `fb_rtdb_stream_callback` guna mempermudah verifikasi dan pemantauan aktivitas SSE.
    - Merangkum fungsi utilitas CRUD terpusat: `fb_rtdb_update_status`, `fb_rtdb_add_delivery_log`, `fb_rtdb_update_delivery_video_url`, `fb_rtdb_clear_command`, dan sinkronisasi kuota PIN kurir jarak jauh (`fb_rtdb_increment_pin_used_quota`, `fb_rtdb_delete_pin`).
    - **Penyempurnaan Eksekusi**: Menerapkan tipe `object_t` secara ketat untuk menjejalkan (*push*) objek JSON pada riwayat pengantaran agar dikenali secara terstruktur oleh Firebase. Selain itu, mengubah fungsi modifikasi status yang sebelumnya terpisah (tiga kali berturut-turut pada satu saluran) menjadi sebuah paket `Database.update()` komposit agar saluran asinkron tidak bertabrakan (*dropped*).
    - *Bugfix Lanjutan*: Menyelaraskan struktur *Firebase Realtime Database* pada `fb_rtdb_add_delivery_log` and `fb_rtdb_update_delivery_video_url` sesuai spesifikasi *FSD* Seksi 7.1 (menggunakan direktori `/delivery_logs/<timestamp>` dengan operasi *Set* alih-alih *Push* yang membuat UID acak), mengaktifkan cetak `Serial.printf` pada `fb_rtdb_stream_loop()` agar *event stream* dari Firebase dapat terpantau, serta memperbaiki `fb_rtdb_get_mac_address()` agar mengembalikan format MAC address yang sepenuhnya huruf kecil tanpa tanda titik dua (`:`).

16. **[main.cpp](file:///c:/Users/rafaelmarvinjo/Forgor/ForgorEmbedded/PlatformIO/Square-SmartParcelBox/src/main.cpp)** [MODIFY]
    - Mengimplementasikan skema pengetesan integrasi interaktif dari komponen fase yang telah diselesaikan (termasuk LED Indicator).
    - Inisialisasi semua komponen dilakukan pada `setup()` bersama I2C (`Wire`) dan komunikasi `Serial` (115200 baud). Pada baris terakhir fungsi `setup()`, blok logika transisi dites dengan memanggil `sm_init()` diikuti oleh serangkaian simulasi perpindahan (*stress-test* transisi `START` -> `IDLE` -> `DELIVERY` -> `ERROR` dan verifikasi penolakan ke `IDLE`). Setelah itu, inisialisasi `fb_rtdb_init()` dipanggil.
    - **Auto-start SSE Stream**: Auto-start stream telah dipindahkan dari `setup()` ke `loop()`. Sistem sekarang akan menunggu hingga `fb_rtdb_is_connected()` bernilai true (yang berarti aplikasi Firebase telah berhasil melakukan autentikasi secara asinkron). Perubahan ini dilakukan untuk memperbaiki *bug* di mana stream mencoba terhubung sebelum token autentikasi siap, yang sebelumnya menyebabkan banjir error `401 Unauthorized`.
    - **Detak Jantung Periodik (Heartbeat)**: Mengimplementasikan pengiriman detak jantung berkala di dalam rutin `loop()`. Jika perangkat terhubung dengan Firebase, status `/status/last_heartbeat` diperbarui setiap 30 detik sekali menggunakan interval `HEARTBEAT_INTERVAL_MS`.
    - Pada rutin `loop()`, pendengar pembaruan instan `fb_rtdb_stream_loop()` dari Firebase turut dipanggil.
    - Menu pengetesan interaktif via Keypad pada fungsi `loop()` diperbarui:
      - `1`: Buka kunci relay (`relay_unlock()`).
      - `2`: Kunci relay (`relay_lock()`).
      - `3`: Bertindak sebagai tombol *toggle* (sakelar bolak-balik) untuk menyalakan/mematikan lampu sorot bantu (Flash LED).
      - `4`: Mengeksekusi pengetesan rentetan fungsi *Firebase RTDB* secara berurutan (jika terkoneksi): mencetak MAC Address, mendaftarkan data perangkat baru, memperbarui *heartbeat*, mengubah *status flag* loker, menulis sampel *delivery log*, dan memulai sambungan *streaming* SSE jarak jauh.
      - `5`/`6`: Ganti pola WS2812 ke `SOLID GREEN` / `RED BLUE BLINK`.
      - `7`: Melakukan diagnostik fungsionalitas NVS (mendaftarkan PIN '1234' dengan kuota terbatas, mensimulasikan pemakaian hingga memicu mekanisme hapus-otomatis).
      - `8`: Menguji serangkaian fungsionalitas I/O MicroSD (penyimpanan berkas uji coba, manipulasi file *queue*, dan pemicuan mode *FIFO cleanup*).
      - `9`: Memicu pengambilan gambar oleh kamera OV2640 dan mencetak ukuran *frame buffer* sebelum membuangnya.
      - `*`/`#`: Mulai/hentikan animasi alternating indikator.
      - `0`: Cetak diagnostik status ketersediaan (Keypad, ToF, Kamera) dan nilai Relay/ToF ke Serial.
    - Status Reed Switch dilacak, perubahan terbuka/tertutup otomatis dicetak ke Serial.

## Hasil Pengujian

- Skema pengetesan berhasil diimplementasikan secara interaktif di `main.cpp`.
- Kode sistem secara keseluruhan berhasil dikompilasi menggunakan perintah PlatformIO (`pio run`) tanpa error.
- Semua file diletakkan sesuai struktur direktori di bawah folder `src/` dan `src/hal/`.
