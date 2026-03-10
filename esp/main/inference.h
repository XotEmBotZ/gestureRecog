#ifndef INFERENCE_H
#define INFERENCE_H

#include <stdint.h>

/**
 * @brief Runs K-Nearest Neighbors inference on the normalized sensor data.
 * 
 * @param current_norm_buffer Pointer to the current normalized float data buffer.
 * @param num_channels Number of sensor channels.
 * @param buffer_size Size of the buffer per channel.
 * @param k Number of neighbors to consider.
 * @param duration_us Duration taken for processing (to be printed in report).
 */
void run_knn_inference(float* current_norm_buffer, int num_channels, int buffer_size, int k, int64_t duration_us);

#endif // INFERENCE_H
