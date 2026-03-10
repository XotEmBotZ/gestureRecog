#ifndef PREPROCESS_H
#define PREPROCESS_H

#include <stdbool.h>
#include "config.h"

/**
 * @brief Scans the buffer to find min/max values and check if sensor is active.
 * Used for dynamic movement detection.
 */
void setMinMax(int* buffer, int num_channels, int buffer_size, int* min, int* max,
               bool* isDisabled, int disableThreshold);

/**
 * @brief Converts a raw value (0-ADC_MAX_VALUE) to its relative position in [min, max].
 * Returns a float in range [0.0, 1.0].
 */
float preprocess_sample(int raw_val, int min, int max);

/**
 * @brief Applies Exponential Moving Average (EMA) to a raw signal.
 */
float apply_ema(float prev_val, int raw_val, float alpha);

/**
 * @brief Calibrates a channel by updating its absolute min/max if needed.
 */
void update_calibration(int raw_val, int* cal_min, int* cal_max);

#endif // PREPROCESS_H
