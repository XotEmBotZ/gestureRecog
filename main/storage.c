#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char* TAG = "STORAGE";

typedef struct {
    char label[16];
    float data[]; // Flexible array member for the sensor data
} sample_blob_t;

void save_buffer_to_nvs(const char* label, float* buffer, int num_channels, int buffer_size) {
  nvs_handle_t storage_handle, meta_handle;
  esp_err_t err;

  // 1. Get or initialize the total sample count from 'meta' namespace
  uint32_t total_samples = 0;
  err = nvs_open("meta", NVS_READWRITE, &meta_handle);
  if (err == ESP_OK) {
    nvs_get_u32(meta_handle, "total", &total_samples);
  } else {
    ESP_LOGE(TAG, "Error opening meta namespace!");
    return;
  }

  // 2. Prepare the sample blob
  size_t data_size = num_channels * buffer_size * sizeof(float);
  size_t total_blob_size = sizeof(sample_blob_t) + data_size;
  sample_blob_t* blob = malloc(total_blob_size);
  if (!blob) {
    nvs_close(meta_handle);
    return;
  }
  strncpy(blob->label, label, 15);
  blob->label[15] = '\0';
  memcpy(blob->data, buffer, data_size);

  // 3. Save to 'storage' namespace with a unique key
  err = nvs_open("storage", NVS_READWRITE, &storage_handle);
  if (err != ESP_OK) {
    free(blob);
    nvs_close(meta_handle);
    return;
  }

  char key[16];
  snprintf(key, sizeof(key), "s%lu", (unsigned long)total_samples);
  
  err = nvs_set_blob(storage_handle, key, blob, total_blob_size);
  if (err == ESP_OK) {
    nvs_commit(storage_handle);
    
    // Update counter
    total_samples++;
    nvs_set_u32(meta_handle, "total", total_samples);
    nvs_commit(meta_handle);
    
    ESP_LOGI(TAG, "Stored sample %lu with label: %s", (unsigned long)total_samples - 1, label);
  } else {
    ESP_LOGE(TAG, "Failed to store blob: %s", esp_err_to_name(err));
  }

  free(blob);
  nvs_close(storage_handle);
  nvs_close(meta_handle);
}

void list_stored_buffers(int num_channels, int buffer_size) {
  nvs_iterator_t it = NULL;
  esp_err_t err = nvs_entry_find(NVS_DEFAULT_PART_NAME, "storage", NVS_TYPE_BLOB, &it);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    printf("No stored items found.\n");
    return;
  }

  nvs_handle_t my_handle;
  nvs_open("storage", NVS_READONLY, &my_handle);

  printf("Stored items:\n");
  while (it != NULL) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);

    size_t required_size = 0;
    nvs_get_blob(my_handle, info.key, NULL, &required_size);
    if (required_size > 0) {
      sample_blob_t* blob = malloc(required_size);
      if (blob) {
        if (nvs_get_blob(my_handle, info.key, blob, &required_size) == ESP_OK) {
          printf("Key: %s, Label: %s\n", info.key, blob->label);
          for (int ch = 0; ch < num_channels; ch++) {
            printf("  CH%d: ", ch);
            for (int i = 0; i < buffer_size; i++) {
              printf("%.2f%s", blob->data[ch * buffer_size + i], (i == buffer_size - 1) ? "" : ",");
            }
            printf("\n");
          }
        }
        free(blob);
      }
    }
    err = nvs_entry_next(&it);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) break;
  }
  nvs_close(my_handle);
}

void clear_stored_buffers() {
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
    }
    if (nvs_open("meta", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u32(h, "total", 0);
        nvs_commit(h);
        nvs_close(h);
    }
    printf("All stored data and counters cleared permanently.\n");
}
