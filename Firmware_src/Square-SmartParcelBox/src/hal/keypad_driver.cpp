#include "hal/keypad_driver.h"
#include <Arduino.h>
#include "pins.h"
#include "config.h"

static TwoWire* g_wire = nullptr;
static bool initialized = false;

// 3x4 Matrix Keypad Layout
// Rows (P0-P3), Columns (P4-P6)
static const char KEY_MAP[4][3] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'} // '*' and '#' are supported
};

static char last_phys_key = '\0';
static char debounced_key = '\0';
static unsigned long last_debounce_time = 0;

bool keypad_init(TwoWire &wire) {
    g_wire = &wire;
    
    // Check if the device is present on the I2C bus
    g_wire->beginTransmission(I2C_ADDR_KEYPAD);
    if (g_wire->endTransmission() != 0) {
        initialized = false;
        return false;
    }
    
    // Write 0x7F to enable weak pull-ups on columns P4-P6 and set rows P0-P3 HIGH (P7 is LOW)
    g_wire->beginTransmission(I2C_ADDR_KEYPAD);
    g_wire->write(0x7F);
    if (g_wire->endTransmission() != 0) {
        initialized = false;
        return false;
    }
    
    initialized = true;
    last_phys_key = '\0';
    debounced_key = '\0';
    last_debounce_time = 0;
    return true;
}

char keypad_scan() {
    if (!initialized || g_wire == nullptr) {
        return '\0';
    }
    
    char phys_key = '\0';
    
    // Scan the matrix by setting one row LOW at a time and reading the columns
    for (int r = 0; r < 4; r++) {
        // Set row r to LOW, other rows to HIGH. Keep columns P4-P6 HIGH (enable weak pull-ups).
        // P7 is kept LOW (0).
        // So write mask is 0x70 | (~(1 << r) & 0x0F)
        uint8_t row_mask = 0x70 | (~(1 << r) & 0x0F);
        
        g_wire->beginTransmission(I2C_ADDR_KEYPAD);
        g_wire->write(row_mask);
        if (g_wire->endTransmission() != 0) {
            continue;
        }
        
        // Read the column pins
        g_wire->requestFrom((uint8_t)I2C_ADDR_KEYPAD, (uint8_t)1);
        if (g_wire->available()) {
            uint8_t read_val = g_wire->read();
            for (int c = 0; c < 3; c++) {
                // If the column bit (P4-P6) is LOW, the key at row r, col c is pressed
                if (!(read_val & (1 << (4 + c)))) {
                    phys_key = KEY_MAP[r][c];
                    break;
                }
            }
        }
        
        if (phys_key != '\0') {
            break; // Stop scanning if we found a pressed key
        }
    }
    
    // Reset columns and rows back to pull-up / HIGH state (0x7F)
    g_wire->beginTransmission(I2C_ADDR_KEYPAD);
    g_wire->write(0x7F);
    g_wire->endTransmission();
    
    // Debounce & Edge detection logic
    unsigned long now = millis();
    
    if (phys_key != last_phys_key) {
        last_debounce_time = now;
        last_phys_key = phys_key;
    }
    
    if ((now - last_debounce_time) >= KEYPAD_DEBOUNCE_MS) {
        if (phys_key != debounced_key) {
            char prev_debounced = debounced_key;
            debounced_key = phys_key;
            
            // Return character exactly once on the press event
            // (transition to a valid character from no key, or from another key)
            if (debounced_key != '\0') {
                // ACC-010: System SHALL ignore keypress from key (*) and (#)
                if (debounced_key == '*' || debounced_key == '#') {
                    return '\0';
                }
                return debounced_key;
            }
        }
    }
    
    return '\0';
}

bool keypad_is_available() {
    if (!initialized || g_wire == nullptr) {
        return false;
    }
    
    g_wire->beginTransmission(I2C_ADDR_KEYPAD);
    return (g_wire->endTransmission() == 0);
}
