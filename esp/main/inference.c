#include "inference.h"
#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_dsp.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "config.h"
#include "ble_uart.h"

static const char* TAG = "INFERENCE";

typedef struct {
    char label[MAX_LABEL_LENGTH];
    float distance;
} knn_match_t;

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
    
    FILE* f = fopen(DATASET_PATH, "rb");
    if (f == NULL) {
        telemetry_printf("KNN: No dataset file found.\n");
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
    float* diff_f = heap_caps_aligned_alloc(MEMORY_ALIGNMENT, sample_size_bytes, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    
    // Buffer to hold a batch of records from file
    sample_record_t* record_batch = malloc(sizeof(sample_record_t) * INFERENCE_BATCH_SIZE);

    if (!diff_f || !record_batch) {
        ESP_LOGE(TAG, "Memory allocation failed for inference");
        if (diff_f) heap_caps_free(diff_f);
        if (record_batch) free(record_batch);
        fclose(f);
        free(top_k);
        return;
    }

    size_t records_read;
    while ((records_read = fread(record_batch, sizeof(sample_record_t), INFERENCE_BATCH_SIZE, f)) > 0) {
        for (size_t i = 0; i < records_read; i++) {
            // --- ESP32-S3 SIMD ACCELERATED DISTANCE CALCULATION ---
            // Subtract current sample from stored sample
            dsps_sub_f32(current_norm_buffer, record_batch[i].data, diff_f, sample_size_floats, 1, 1, 1);
            
            // Sum of squares via dot product
            float sum_sq_diff = 0;
            dsps_dotprod_f32(diff_f, diff_f, &sum_sq_diff, sample_size_floats);
            
            // Euclidean distance
            float dist = sqrtf(sum_sq_diff);
            // -----------------------------------------------------

            update_top_k(top_k, k, record_batch[i].label, dist);
            samples_checked++;
        }
    }

    int64_t duration_us = esp_timer_get_time() - start_time;

    if (samples_checked > 0) {
        telemetry_printf("\n=== KNN INFERENCE REPORT (FILE-BASED) ===\n");
        telemetry_printf(">time:%lld\n", duration_us);
        telemetry_printf("Samples Processed: %d\n", samples_checked);
        telemetry_printf("Best Match: [%s] (Dist: %.2f)\n", top_k[0].label, top_k[0].distance);
        telemetry_printf("---------------------------\n");
        telemetry_printf("Top %d Matches:\n", k);
        for (int i = 0; i < k; i++) {
            if (top_k[i].distance != INFINITY) {
                telemetry_printf(" %d. %s (%.2f)\n", i + 1, top_k[i].label, top_k[i].distance);
            }
        }
        telemetry_printf("===========================\n");
    } else {
        telemetry_printf("KNN: Dataset is empty.\n");
    }

    free(record_batch);
    heap_caps_free(diff_f);
    fclose(f);
    free(top_k);
}
