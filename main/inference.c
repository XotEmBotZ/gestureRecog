#include "inference.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_dsp.h"
#include "esp_heap_caps.h"

static const char* TAG = "INFERENCE";

#define BATCH_SIZE 10 // Process 10 samples at once from NVS

typedef struct {
    char label[16];
    float distance;
} knn_match_t;

// Structure to hold a batch of samples in memory
typedef struct {
    char labels[BATCH_SIZE][16];
    float* data; // Contiguous block of data for BATCH_SIZE float samples
    int count;
} sample_batch_t;

// Simple bubble sort to maintain top K matches
static void update_top_k(knn_match_t* top_k, int k, const char* label, float distance) {
    if (distance >= top_k[k-1].distance) return;

    // Insert and shift
    strncpy(top_k[k-1].label, label, sizeof(top_k[k-1].label) - 1);
    top_k[k-1].distance = distance;

    for (int i = k - 1; i > 0; i--) {
        if (top_k[i].distance < top_k[i-1].distance) {
            knn_match_t temp = top_k[i];
            top_k[i] = top_k[i-1];
            top_k[i-1] = temp;
        } else {
            break;
        }
    }
}

void run_knn_inference(float* current_norm_buffer, int num_channels, int buffer_size, int k) {
    nvs_iterator_t it = NULL;
    esp_err_t err = nvs_entry_find(NVS_DEFAULT_PART_NAME, "storage", NVS_TYPE_BLOB, &it);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        printf("KNN: No training data found in NVS.\n");
        return;
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error searching NVS: %s", esp_err_to_name(err));
        return;
    }

    nvs_handle_t my_handle;
    err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        nvs_release_iterator(it);
        return;
    }

    knn_match_t* top_k = malloc(sizeof(knn_match_t) * k);
    for (int i = 0; i < k; i++) {
        top_k[i].distance = INFINITY;
        memset(top_k[i].label, 0, sizeof(top_k[i].label));
    }

    int samples_checked = 0;
    size_t sample_size_floats = num_channels * buffer_size;
    size_t sample_size_bytes = sample_size_floats * sizeof(float);

    // Aligned buffer for SIMD difference calculation
    float* diff_f = heap_caps_aligned_alloc(16, sample_size_bytes, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!diff_f) {
        ESP_LOGE(TAG, "Failed to allocate diff buffer");
        nvs_close(my_handle);
        free(top_k);
        return;
    }

    // Pre-allocate a batch buffer for float data from NVS
    sample_batch_t batch;
    batch.data = malloc(sample_size_bytes * BATCH_SIZE);
    if (!batch.data) {
        ESP_LOGE(TAG, "Failed to allocate batch buffer");
        heap_caps_free(diff_f);
        nvs_close(my_handle);
        free(top_k);
        return;
    }

    while (it != NULL) {
        batch.count = 0;
        
        while (it != NULL && batch.count < BATCH_SIZE) {
            nvs_entry_info_t info;
            nvs_entry_info(it, &info);
            size_t required_size = 0;
            nvs_get_blob(my_handle, info.key, NULL, &required_size);
            
            if (required_size == sample_size_bytes) {
                if (nvs_get_blob(my_handle, info.key, batch.data + (batch.count * sample_size_floats), &required_size) == ESP_OK) {
                    strncpy(batch.labels[batch.count], info.key, 15);
                    batch.labels[batch.count][15] = '\0';
                    batch.count++;
                }
            }
            err = nvs_entry_next(&it);
            if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) break;
        }

        for (int b = 0; b < batch.count; b++) {
            float* stored_norm_sample = batch.data + (b * sample_size_floats);

            // --- ESP32-S3 SIMD ACCELERATED DISTANCE CALCULATION ---
            // 1. Vector Subtraction: diff = current - stored
            dsps_sub_f32(current_norm_buffer, stored_norm_sample, diff_f, sample_size_floats, 1, 1, 1);
            
            // 2. Dot Product: sum_sq_diff = diff . diff (Sum of squared differences)
            float sum_sq_diff = 0;
            dsps_dotprod_f32(diff_f, diff_f, &sum_sq_diff, sample_size_floats);
            
            // 3. Final Square Root
            float dist = sqrtf(sum_sq_diff);
            // -----------------------------------------------------

            update_top_k(top_k, k, batch.labels[b], dist);
            samples_checked++;
        }
    }

    if (samples_checked > 0) {
        printf("\n=== KNN INFERENCE REPORT (S3 SIMD ACCELERATED) ===\n");
        printf("Samples Processed: %d\n", samples_checked);
        printf("Best Match: [%s] (Dist: %.2f)\n", top_k[0].label, top_k[0].distance);
        printf("---------------------------\n");
        printf("Top %d Matches:\n", k);
        for (int i = 0; i < k; i++) {
            if (top_k[i].distance != INFINITY) {
                printf(" %d. %s (%.2f)\n", i + 1, top_k[i].label, top_k[i].distance);
            }
        }
        printf("===========================\n");
    } else {
        printf("KNN: No valid samples found for comparison.\n");
    }

    free(batch.data);
    heap_caps_free(diff_f);
    nvs_close(my_handle);
    free(top_k);
}
