#ifndef STORAGE_H
#define STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "config.h"

/**
 * @brief Structure for a single data record in the binary file.
 */
typedef struct {
    char label[MAX_LABEL_LENGTH];
    float data[NUM_CHANNELS * BUFFER_SIZE]; // Fixed size: NUM_CHANNELS * BUFFER_SIZE
} sample_record_t;

/**
 * @brief Initializes the SPIFFS filesystem.
 */
esp_err_t init_storage();

/**
 * @brief Saves the sensor buffer to SPIFFS by appending.
 */
void save_buffer_to_spiffs(const char* label, float* buffer, int num_channels, int buffer_size);

/**
 * @brief Lists all stored labels in the SPIFFS dataset.
 */
void list_stored_buffers(int num_channels, int buffer_size);

/**
 * @brief Reads a specific sample by ID and prints its data to console.
 */
void read_sample_by_id(int id, int num_channels, int buffer_size);

/**
 * @brief Deletes the dataset file.
 */
void clear_stored_buffers();

#endif // STORAGE_H
