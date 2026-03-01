#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

#include "lib.h"
#include "storage.h"
#include "preprocess.h"
#include "inference.h"

#define NUM_CHANNELS 5
#define BUFFER_SIZE 32
#define DISABLE_THRESHOLD 20
#define INFERENCE_INTERVAL_MS 1000
#define LOOP_PERIOD_MS 100

typedef enum { MODE_INFER, MODE_TRAIN } app_mode_t;

static const char* TAG = "APP";
static app_mode_t current_mode = MODE_INFER;
static int* global_buffer = NULL;
static float* global_norm_buffer = NULL;
static int global_idx = 0;
static bool should_store = false;
static char store_label[16] = {0};
static int k_neighbors = 3;

void show_help() {
    printf("\n=== ESP-Ossicoloscope Help ===\n");
    printf("Commands:\n");
    printf("  help          - Show this help message\n");
    printf("  mode train    - Switch to Training Mode\n");
    printf("  mode infer    - Switch to Inference Mode (KNN)\n");
    printf("  store <label> - Save current preprocessed buffer to NVS (Train mode only)\n");
    printf("  list          - List all stored labels and their values\n");
    printf("  clear         - Permanently delete all stored data from NVS\n");
    printf("  k <number>    - Set number of neighbors for KNN (Current: %d)\n", k_neighbors);
    printf("==============================\n");
}

void console_task(void* arg) {
  char line[64];
  while (1) {
    if (fgets(line, sizeof(line), stdin)) {
      line[strcspn(line, "\n")] = 0; // Remove newline
      if (strcmp(line, "help") == 0) {
        show_help();
      } else if (strcmp(line, "mode train") == 0) {
        current_mode = MODE_TRAIN;
        printf("Switched to TRAIN mode\n");
      } else if (strcmp(line, "mode infer") == 0) {
        current_mode = MODE_INFER;
        printf("Switched to INFER mode\n");
      } else if (strcmp(line, "list") == 0) {
        list_stored_buffers(NUM_CHANNELS, BUFFER_SIZE);
      } else if (strcmp(line, "clear") == 0) {
        clear_stored_buffers();
      } else if (strncmp(line, "k ", 2) == 0) {
        int val = atoi(line + 2);
        if (val > 0 && val <= 10) {
            k_neighbors = val;
            printf("KNN K-neighbors set to: %d\n", k_neighbors);
        } else {
            printf("Error: K must be between 1 and 10\n");
        }
      } else if (strncmp(line, "store ", 6) == 0) {
        if (current_mode != MODE_TRAIN) {
          printf("Error: Must be in TRAIN mode to store\n");
        } else {
          memset(store_label, 0, sizeof(store_label));
          strncpy(store_label, line + 6, sizeof(store_label) - 1);
          should_store = true;
        }
      } else {
        printf("Unknown command: %s. Type 'help' for available commands.\n", line);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void app_main() {
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  adc_oneshot_unit_handle_t adc;
  adc_oneshot_unit_init_cfg_t adcCfg = {
      .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
      .ulp_mode = ADC_ULP_MODE_DISABLE,
      .unit_id = ADC_UNIT_1,
  };
  adc_oneshot_chan_cfg_t channelCfg = {
      .atten = ADC_ATTEN_DB_12,
      .bitwidth = ADC_BITWIDTH_12,
  };
  adc_oneshot_new_unit(&adcCfg, &adc);
  
  global_buffer = calloc(NUM_CHANNELS * BUFFER_SIZE, sizeof(int));
  global_norm_buffer = heap_caps_aligned_alloc(16, NUM_CHANNELS * BUFFER_SIZE * sizeof(float), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
  
  int min[NUM_CHANNELS], max[NUM_CHANNELS] = {0};
  for (int i = 0; i < NUM_CHANNELS; i++) {
    adc_oneshot_config_channel(adc, i, &channelCfg);
    int gpio;
    adc_oneshot_channel_to_io(ADC_UNIT_1, i, &gpio);
    gpio_set_pull_mode(gpio, GPIO_PULLDOWN_ONLY);
    min[i] = 4096;
  }

  xTaskCreate(console_task, "console", 4096, NULL, 5, NULL);

  bool isDisabled = false;
  int inference_timer = 0;
  float* linearized_norm_buf = heap_caps_aligned_alloc(16, NUM_CHANNELS * BUFFER_SIZE * sizeof(float), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);

  while (1) {
    int current_raw[NUM_CHANNELS];
    for (int i = 0; i < NUM_CHANNELS; i++) {
      adc_oneshot_read(adc, i, &current_raw[i]);
      global_buffer[i * BUFFER_SIZE + global_idx] = current_raw[i];
    }
    
    setMinMax(global_buffer, NUM_CHANNELS, BUFFER_SIZE, min, max, &isDisabled, DISABLE_THRESHOLD);

    for (int i = 0; i < NUM_CHANNELS; i++) {
        float proc = preprocess_sample(current_raw[i], min[i], max[i]);
        global_norm_buffer[i * BUFFER_SIZE + global_idx] = proc;
        printRes(current_raw[i], proc, i, min[i], max[i]);
    }
    printf(">d:%d mode:%s\n", isDisabled, (current_mode == MODE_TRAIN) ? "TRAIN" : "INFER");

    if (current_mode == MODE_INFER) {
      inference_timer += LOOP_PERIOD_MS;
      if (inference_timer >= INFERENCE_INTERVAL_MS) {
          for (int ch = 0; ch < NUM_CHANNELS; ch++) {
              for (int i = 0; i < BUFFER_SIZE; i++) {
                  int buf_idx = (global_idx + 1 + i) % BUFFER_SIZE;
                  linearized_norm_buf[ch * BUFFER_SIZE + i] = global_norm_buffer[ch * BUFFER_SIZE + buf_idx];
              }
          }
          
          int64_t start_time = esp_timer_get_time();
          run_knn_inference(linearized_norm_buf, NUM_CHANNELS, BUFFER_SIZE, k_neighbors, 0); // Temporary 0
          int64_t end_time = esp_timer_get_time();
          
          // Re-running with correct duration for the print report inside the function
          // Alternatively, we can pass it or print outside. Let's fix the report to take the measurement.
          // Note: Measuring ONLY the core KNN logic inside run_knn_inference might be better.
          // I will update run_knn_inference to measure its own duration internally for more accuracy.
          
          inference_timer = 0;
      }
    }

    if (should_store) {
      for (int ch = 0; ch < NUM_CHANNELS; ch++) {
          for (int i = 0; i < BUFFER_SIZE; i++) {
              int buf_idx = (global_idx + 1 + i) % BUFFER_SIZE;
              linearized_norm_buf[ch * BUFFER_SIZE + i] = global_norm_buffer[ch * BUFFER_SIZE + buf_idx];
          }
      }
      save_buffer_to_nvs(store_label, linearized_norm_buf, NUM_CHANNELS, BUFFER_SIZE);
      should_store = false;
    }

    global_idx = (global_idx + 1) % BUFFER_SIZE;
    vTaskDelay(pdMS_TO_TICKS(LOOP_PERIOD_MS));
  };
}
