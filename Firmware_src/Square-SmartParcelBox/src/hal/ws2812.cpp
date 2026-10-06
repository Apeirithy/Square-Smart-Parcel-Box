#include "hal/ws2812.h"
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "pins.h"
#include "config.h"

// 1 LED, pin PIN_LED_WS2812, NEO_GRB + NEO_KHZ800
static Adafruit_NeoPixel strip(1, PIN_LED_WS2812, NEO_GRB + NEO_KHZ800);

// pattern
static WS2812Pattern current_pattern = WS2812_OFF;

// for blinking
static unsigned long last_blink_time = 0; 
static bool blink_state = false;

static void apply_color(uint8_t r, uint8_t g, uint8_t b) {
    strip.setPixelColor(0, strip.Color(r, g, b));
    strip.show();
}

bool ws2812_init() {
    strip.begin();
    strip.show(); // Initialize all pixels to 'off'
    return true;
}

void ws2812_set_pattern(WS2812Pattern pattern) {
    if (current_pattern == pattern) {
        return;
    }
    current_pattern = pattern;
    last_blink_time = millis();
    blink_state = false; // start with first state

    switch (current_pattern) {
        case WS2812_OFF:
            apply_color(0, 0, 0);
            break;
        case WS2812_RED_BLUE_BLINK:
            apply_color(255, 0, 0); // start with Red
            break;
        case WS2812_SOLID_BLUE:
            apply_color(0, 0, 255);
            break;
        case WS2812_SOLID_GREEN:
            apply_color(0, 255, 0);
            break;
        case WS2812_SOLID_YELLOW:
            apply_color(255, 255, 0);
            break;
        case WS2812_SOLID_PURPLE:
            apply_color(128, 0, 128);
            break;
        case WS2812_SOLID_RED:
            apply_color(255, 0, 0);
            break;
    }
}

// Functions for blinking pattern, if the function above changes the current pattern to red_blue_blink, 
// this function below does the blinking work
void ws2812_update() {
    if (current_pattern == WS2812_RED_BLUE_BLINK) {
        unsigned long now = millis();
        if (now - last_blink_time >= WS2812_BLINK_INTERVAL_MS) {
            last_blink_time = now;
            blink_state = !blink_state;
            if (blink_state) {
                apply_color(0, 0, 255); // Blue
            } else {
                apply_color(255, 0, 0); // Red
            }
        }
    }
}
