#ifndef HAL_KEYPAD_DRIVER_H
#define HAL_KEYPAD_DRIVER_H

#include <Wire.h>

/**
 * @brief Initializes the custom keypad driver.
 * Checks presence of PCF8574 on I2C bus and enables column pull-ups.
 * @param wire The I2C TwoWire instance to use.
 * @return true if initialization succeeded, false otherwise.
 */
bool keypad_init(TwoWire &wire);

/**
 * @brief Scans the keypad matrix.
 * Applies debounce and edge detection to return the character pressed.
 * Keypresses from '*' and '#' are ignored (return '\0').
 * @return The character pressed ('0'-'9') exactly once per press event, or '\0'.
 */
char keypad_scan();

/**
 * @brief Returns the status of the keypad hardware availability.
 * @return true if initialized and responding to I2C checks, false otherwise.
 */
bool keypad_is_available();

#endif // HAL_KEYPAD_DRIVER_H
