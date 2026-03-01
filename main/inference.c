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
#include "esp_timer.h"

static const char* TAG = "INFERENCE";

#define BATCH_SIZE 10 // Process 10 samples at once from NVS

typedef struct {
    char label[16];
    float distance;
} knn_match_t;

// This must match the structure in storage.c
typedef struct {
    char label[16];
    float data[]; 
} sample_blob_t;

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

void run_knn_inference(float* current_norm_buffer, int num_channels, int buffer_size, int k, int64_t _unused) {
    int64_t start_time = esp_timer_get_time();
    
    nvs_iterator_t it = NULL;
    esp_err_t err = nvs_entry_find(NVS_DEFAULT_PART_NAME, "storage", NVS_TYPE_BLOB, &it);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        printf("KNN: No training data found in NVS.\n");
        return;
    }

    nvs_handle_t my_handle;
    err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err != ESP_OK) {
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
    size_t expected_blob_size = sizeof(sample_blob_t) + sample_size_bytes;

    // Aligned buffer for SIMD difference calculation
    float* diff_f = heap_caps_aligned_alloc(16, sample_size_bytes, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    
    while (it != NULL) {
        // Load and process one by one (easier with the variable blob labels)
        // Batching can be re-added if we optimize the storage further, 
        // but for now let's ensure correctness with the new multi-label format.
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);

        size_t required_size = 0;
        nvs_get_blob(my_handle, info.key, NULL, &required_size);
        
        if (required_size == expected_blob_size) {
            sample_blob_t* blob = malloc(required_size);
            if (blob) {
                if (nvs_get_blob(my_handle, info.key, blob, &required_size) == ESP_OK) {
                    // --- ESP32-S3 SIMD ACCELERATED DISTANCE CALCULATION ---
                    dsps_sub_f32(current_norm_buffer, blob->data, diff_f, sample_size_floats, 1, 1, 1);
                    float sum_sq_diff = 0;
                    dsps_dotprod_f32(diff_f, diff_f, &sum_sq_diff, sample_size_floats);
                    float dist = sqrtf(sum_sq_diff);
                    // -----------------------------------------------------

                    update_top_k(top_k, k, blob->label, dist);
                    samples_checked++;
                }
                free(blob);
            }
        }

        err = nvs_entry_next(&it);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) break;
    }

    int64_t duration_us = esp_timer_get_time() - start_time;

    if (samples_checked > 0) {
        printf("\n=== KNN INFERENCE REPORT ===\n");
        printf("Time taken: %lld us\n", duration_us);
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

    heap_caps_free(diff_f);
    nvs_close(my_handle);
    free(top_k);
}
