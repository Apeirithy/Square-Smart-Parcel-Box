#include "keybuffer_indicator_driver.h"
#include "../pins.h"

static bool _led_indicator_available = false;
static TwoWire *_led_indicator_wire = nullptr;
static bool _led_indicator_animation = false;
static uint32_t _led_indicator_last_anim_time = 0;
static uint8_t _led_indicator_anim_step = 0;

static void write_leds(uint8_t val) {
    if (!_led_indicator_wire) return;
    _led_indicator_wire->beginTransmission(I2C_ADDR_LED_IND);
    _led_indicator_wire->write(val);
    _led_indicator_wire->endTransmission();
}

bool led_indicator_init(TwoWire &wire) {
    _led_indicator_wire = &wire;
    _led_indicator_available = false;
    
    // Test if device is on the bus
    _led_indicator_wire->beginTransmission(I2C_ADDR_LED_IND);
    if (_led_indicator_wire->endTransmission() == 0) {
        _led_indicator_available = true;
        led_indicator_all_off();
        return true;
    }
    return false;
}

void led_indicator_set_count(uint8_t count) {
    _led_indicator_animation = false;
    if (!_led_indicator_available) return;
    
    const uint8_t patterns[] = {0xFF, 0xFE, 0xFC, 0xF8, 0xF0};
    if (count > 4) count = 4;
    
    write_leds(patterns[count]);
}

void led_indicator_all_off() {
    led_indicator_set_count(0);
}

void led_indicator_alternating_pattern() {
    _led_indicator_animation = true;
    _led_indicator_anim_step = 0;
    _led_indicator_last_anim_time = millis();
    
    if (_led_indicator_available) {
        write_leds(0xFC); // Left side on
    }
}

void led_indicator_stop_pattern() {
    led_indicator_all_off();
}

bool led_indicator_is_available() {
    return _led_indicator_available;
}

void led_indicator_update() {
    if (!_led_indicator_available || !_led_indicator_animation) return;
    
    if (millis() - _led_indicator_last_anim_time >= 250) {
        _led_indicator_last_anim_time = millis();
        _led_indicator_anim_step = !_led_indicator_anim_step;
        
        uint8_t val = _led_indicator_anim_step ? 0xF3 : 0xFC; // Alternate right side and left side
        write_leds(val);
    }
}
