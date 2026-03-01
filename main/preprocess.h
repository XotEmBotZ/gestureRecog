#ifndef PREPROCESS_H
#define PREPROCESS_H

#include <stdbool.h>

/**
 * @brief Preprocesses sensor data to find min/max values and check if sensor is active.
 * 
 * @param buffer Pointer to the sensor data buffer.
 * @param num_channels Number of sensor channels.
 * @param buffer_size Size of the circular buffer per channel.
 * @param min Array to store min values per channel.
 * @param max Array to store max values per channel.
 * @param isDisabled Pointer to boolean indicating if sensor is considered disabled (below threshold).
 * @param disableThreshold Threshold to consider sensor disabled.
 */
void setMinMax(int* buffer, int num_channels, int buffer_size, int* min, int* max,
               bool* isDisabled, int disableThreshold);

#endif // PREPROCESS_H
