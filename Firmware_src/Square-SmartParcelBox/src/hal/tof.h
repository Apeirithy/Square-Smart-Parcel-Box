#ifndef TOF_H
#define TOF_H

#include <Arduino.h>
#include <Wire.h>

bool tof_init(TwoWire &wire);
int16_t tof_read_distance_cm();
bool tof_is_full();
bool tof_is_available();

#endif // TOF_H
