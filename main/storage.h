#ifndef STORAGE_H
#define STORAGE_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Saves the sensor buffer to NVS.
 * 
 * @param label The NVS key/label for the data.
 * @param buffer Pointer to the data buffer.
 * @param idx Current index in the circular buffer.
 * @param num_channels Number of channels in the buffer.
 * @param buffer_size Size of the buffer per channel.
 */
void save_buffer_to_nvs(const char* label, int* buffer, int idx, int num_channels, int buffer_size);

/**
 * @brief Lists all stored labels and their values in the NVS storage namespace.
 * 
 * @param num_channels Expected number of channels.
 * @param buffer_size Expected buffer size per channel.
 */
void list_stored_buffers(int num_channels, int buffer_size);

#endif // STORAGE_H
