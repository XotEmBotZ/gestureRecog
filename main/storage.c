#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_spiffs.h"
#include "esp_log.h"
#include "config.h"

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
        printf("No dataset found or failed to open file.\n");
        return;
    }

    sample_record_t record;
    int count = 0;
    printf("Stored items in dataset:\n");
    while (fread(&record, sizeof(sample_record_t), 1, f) == 1) {
        printf("%d. Label: %s\n", ++count, record.label);
        // Print first few values of CH0 for verification
        printf("  CH0 (first %d): ", DEBUG_PRINT_SAMPLES_COUNT);
        for (int i = 0; i < DEBUG_PRINT_SAMPLES_COUNT; i++) {
            printf("%.2f%s", record.data[i], (i == DEBUG_PRINT_SAMPLES_COUNT - 1) ? "" : ", ");
        }
        printf("\n");
    }
    printf("Total samples: %d\n", count);
    fclose(f);
}

void clear_stored_buffers() {
    if (remove(DATASET_PATH) == 0) {
        printf("Dataset deleted successfully.\n");
    } else {
        printf("No dataset file to delete or error occurred.\n");
    }
}
