#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_spiffs.h"
#include "esp_log.h"
#include "config.h"
#include "ble_uart.h"

static const char* TAG = "STORAGE";

esp_err_t init_storage() {
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = SPIFFS_BASE_PATH,
      .partition_label = "storage",
      .max_files = SPIFFS_MAX_FILES,
      .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ESP_OK;
}

void save_buffer_to_spiffs(const char* label, float* buffer, int num_channels, int buffer_size) {
    FILE* f = fopen(DATASET_PATH, "ab"); // Open for appending in binary mode
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    sample_record_t record;
    strncpy(record.label, label, MAX_LABEL_LENGTH - 1);
    record.label[MAX_LABEL_LENGTH - 1] = '\0';
    memcpy(record.data, buffer, num_channels * buffer_size * sizeof(float));

    size_t written = fwrite(&record, sizeof(sample_record_t), 1, f);
    if (written != 1) {
        ESP_LOGE(TAG, "Error writing record to file");
    } else {
        ESP_LOGI(TAG, "Sample stored successfully: %s", label);
    }

    fclose(f);
}

void list_stored_buffers(int num_channels, int buffer_size) {
    FILE* f = fopen(DATASET_PATH, "rb");
    if (f == NULL) {
        telemetry_printf("No dataset found or failed to open file.\n");
        return;
    }

    sample_record_t record;
    int count = 0;
    telemetry_printf("Stored items in dataset:\n");
    while (fread(&record, sizeof(sample_record_t), 1, f) == 1) {
        telemetry_printf(">list:%d:%s\n", ++count, record.label);
    }
    telemetry_printf("Total samples: %d\n", count);
    fclose(f);
}

void read_sample_by_id(int id, int num_channels, int buffer_size) {
    FILE* f = fopen(DATASET_PATH, "rb");
    if (f == NULL) {
        telemetry_printf("No dataset found.\n");
        return;
    }

    sample_record_t record;
    int current_id = 0;
    bool found = false;
    while (fread(&record, sizeof(sample_record_t), 1, f) == 1) {
        current_id++;
        if (current_id == id) {
            found = true;
            telemetry_printf(">data_start:%d:%s\n", id, record.label);
            for (int ch = 0; ch < num_channels; ch++) {
                telemetry_printf(">ch:%d:", ch);
                for (int i = 0; i < buffer_size; i++) {
                    telemetry_printf("%.2f%s", record.data[ch * buffer_size + i], (i == buffer_size - 1) ? "" : ",");
                }
                telemetry_printf("\n");
            }
            telemetry_printf(">data_end\n");
            break;
        }
    }
    if (!found) {
        telemetry_printf("Sample ID %d not found.\n", id);
    }
    fclose(f);
}

void clear_stored_buffers() {
    if (remove(DATASET_PATH) == 0) {
        telemetry_printf("Dataset deleted successfully.\n");
    } else {
        telemetry_printf("No dataset file to delete or error occurred.\n");
    }
}
