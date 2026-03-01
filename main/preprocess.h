#ifndef PREPROCESS_H
#define PREPROCESS_H

#include <stdbool.h>

/**
 * @brief Scans the buffer to find min/max values and check if sensor is active.
 */
void setMinMax(int* buffer, int num_channels, int buffer_size, int* min, int* max,
               bool* isDisabled, int disableThreshold);

/**
 * @brief Converts a raw value (0-4095) to its relative position in [min, max].
 * Returns a float in range [0.0, 1.0].
 */
float preprocess_sample(int raw_val, int min, int max);

#endif // PREPROCESS_H
