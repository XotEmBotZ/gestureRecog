#ifndef INFERENCE_H
#define INFERENCE_H

/**
 * @brief Runs K-Nearest Neighbors inference on the current sensor data.
 * 
 * Iterates through all stored samples in NVS, calculates Euclidean distance,
 * and finds the top K matches.
 * 
 * @param current_buffer Pointer to the current sensor data buffer.
 * @param num_channels Number of sensor channels.
 * @param buffer_size Size of the circular buffer per channel.
 * @param k Number of neighbors to consider.
 */
void run_knn_inference(int* current_buffer, int num_channels, int buffer_size, int k);

#endif // INFERENCE_H
