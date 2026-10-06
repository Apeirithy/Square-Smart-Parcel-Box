#ifndef HAL_WS2812_H
#define HAL_WS2812_H

enum WS2812Pattern {
    WS2812_OFF,
    WS2812_RED_BLUE_BLINK,   // Startup: blink red-blue every 1 second
    WS2812_SOLID_BLUE,       // Wi-Fi connected
    WS2812_SOLID_GREEN,      // Firebase connected, no HW issues
    WS2812_SOLID_YELLOW,     // Firebase connected, partial HW issues
    WS2812_SOLID_PURPLE,     // Offline mode with NVS pins
    WS2812_SOLID_RED          // Fatal error / STATE_ERROR
};

bool ws2812_init();
void ws2812_set_pattern(WS2812Pattern pattern);
void ws2812_update();

#endif // HAL_WS2812_H
