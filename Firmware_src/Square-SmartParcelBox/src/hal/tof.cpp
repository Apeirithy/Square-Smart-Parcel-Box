#include "tof.h"
#include "pins.h"
#include "config.h"
#include <Adafruit_VL53L0X.h>

static Adafruit_VL53L0X lox = Adafruit_VL53L0X();
static bool is_available = false;

bool tof_init(TwoWire &wire) {
    // Initialize the VL53L0X sensor.
    // The begin() method signature: begin(uint8_t i2c_addr = VL53L0X_I2C_ADDR, boolean debug = false, TwoWire *i2c = &Wire, SensorResolution_t resolution = VL53L0X_DEFAULT)
    if (lox.begin(I2C_ADDR_TOF, false, &wire)) {
        is_available = true;
        return true;
    }
    is_available = false;
    return false;
}

int16_t tof_read_distance_cm() {
    if (!is_available) {
        return -1;
    }

    VL53L0X_RangingMeasurementData_t measure;
    lox.rangingTest(&measure, false); 

    if (measure.RangeStatus != 4) { 
        return measure.RangeMilliMeter / 10;
    } else {
        return -1; 
    }
}

bool tof_is_full() {
    int16_t dist = tof_read_distance_cm();
    if (dist == -1) {
        return false;
    }
    if (dist <= (TOF_BOX_WIDTH_CM - TOF_TOLERANCE_CM)) {
        return true;
    }
    return false;
}

bool tof_is_available() {
    return is_available;
}
