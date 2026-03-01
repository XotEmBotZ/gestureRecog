#ifndef INFERENCE_H
#define INFERENCE_H

/**
 * @brief Runs K-Nearest Neighbors inference on the normalized sensor data.
 * 
 * Iterates through all stored samples in NVS, calculates Euclidean distance,
 * and finds the top K matches.
 * 
 * @param current_norm_buffer Pointer to the current normalized float data buffer.
 * @param num_channels Number of sensor channels.
 * @param buffer_size Size of the buffer per channel.
 * @param k Number of neighbors to consider.
 */
void run_knn_inference(float* current_norm_buffer, int num_channels, int buffer_size, int k);

#endif // INFERENCE_H
