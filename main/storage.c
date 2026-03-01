#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char* TAG = "STORAGE";

void save_buffer_to_nvs(const char* label, int* buffer, int idx, int num_channels, int buffer_size) {
  nvs_handle_t my_handle;
  esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
    return;
  }

  size_t data_size = num_channels * buffer_size * sizeof(int);
  int* linear_buf = malloc(data_size);
  if (linear_buf == NULL) {
    nvs_close(my_handle);
    return;
  }

  for (int ch = 0; ch < num_channels; ch++) {
    for (int i = 0; i < buffer_size; i++) {
      int buffer_idx = (idx + 1 + i) % buffer_size;
      linear_buf[ch * buffer_size + i] = buffer[ch * buffer_size + buffer_idx];
    }
  }

  err = nvs_set_blob(my_handle, label, linear_buf, data_size);
  if (err == ESP_OK) {
    err = nvs_commit(my_handle);
    if (err == ESP_OK) {
      ESP_LOGI(TAG, "Successfully stored buffer with label: %s", label);
    }
  } else {
    ESP_LOGE(TAG, "Failed to store blob: %s", esp_err_to_name(err));
  }

  nvs_close(my_handle);
  free(linear_buf);
}

void list_stored_buffers(int num_channels, int buffer_size) {
  nvs_iterator_t it = NULL;
  esp_err_t err = nvs_entry_find(NVS_DEFAULT_PART_NAME, "storage", NVS_TYPE_ANY, &it);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    printf("No stored items found.\n");
    return;
  }

  if (err != ESP_OK) {
    printf("Error searching NVS: %s\n", esp_err_to_name(err));
    return;
  }

  nvs_handle_t my_handle;
  err = nvs_open("storage", NVS_READONLY, &my_handle);
  if (err != ESP_OK) {
    printf("Error opening NVS handle: %s\n", esp_err_to_name(err));
    nvs_release_iterator(it);
    return;
  }

  printf("Stored items:\n");
  while (it != NULL) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);
    printf("Label: %s\n", info.key);

    if (info.type == NVS_TYPE_BLOB) {
      size_t required_size = 0;
      nvs_get_blob(my_handle, info.key, NULL, &required_size);
      if (required_size > 0) {
        int* data = malloc(required_size);
        if (data) {
          if (nvs_get_blob(my_handle, info.key, data, &required_size) == ESP_OK) {
            int num_ints = required_size / sizeof(int);
            for (int ch = 0; ch < num_channels; ch++) {
              printf("  CH%d: ", ch);
              for (int i = 0; i < buffer_size; i++) {
                int val_idx = ch * buffer_size + i;
                if (val_idx < num_ints) {
                  printf("%d%s", data[val_idx], (i == buffer_size - 1) ? "" : ",");
                }
              }
              printf("\n");
            }
          }
          free(data);
        }
      }
    }
    err = nvs_entry_next(&it);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
      break;
    }
  }
  nvs_close(my_handle);
}
