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

void console_task(void* arg) {
  char line[64];
  while (1) {
    if (fgets(line, sizeof(line), stdin)) {
      line[strcspn(line, "\n")] = 0; // Remove newline
      if (strcmp(line, "mode train") == 0) {
        current_mode = MODE_TRAIN;
        printf("Switched to TRAIN mode\n");
      } else if (strcmp(line, "mode infer") == 0) {
        current_mode = MODE_INFER;
        printf("Switched to INFER mode\n");
      } else if (strcmp(line, "list") == 0) {
        list_stored_buffers(NUM_CHANNELS, BUFFER_SIZE);
      } else if (strncmp(line, "store ", 6) == 0) {
        if (current_mode != MODE_TRAIN) {
          printf("Error: Must be in TRAIN mode to store\n");
        } else {
          memset(store_label, 0, sizeof(store_label));
          strncpy(store_label, line + 6, sizeof(store_label) - 1);
          should_store = true;
        }
      } else {
        printf("Unknown command: %s\n", line);
        printf("Available: mode train, mode infer, store <label>, list\n");
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
  // 16-byte alignment for S3 SIMD
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

  while (1) {
    for (int i = 0; i < NUM_CHANNELS; i++) {
      adc_oneshot_read(adc, i, global_buffer + i * BUFFER_SIZE + global_idx);
    }
    
    // Always perform min/max calculation and normalization for consistent preprocessing
    setMinMax(global_buffer, NUM_CHANNELS, BUFFER_SIZE, min, max, &isDisabled, DISABLE_THRESHOLD);
    normalize_buffer(global_buffer, global_norm_buffer, global_idx, NUM_CHANNELS, BUFFER_SIZE, min, max);

    // Always print results for debug (works in both TRAIN and INFER modes)
    printRes(global_buffer, global_idx, NUM_CHANNELS, BUFFER_SIZE, min, max);
    printf(">d:%d mode:%s\n", isDisabled, (current_mode == MODE_TRAIN) ? "TRAIN" : "INFER");

    if (current_mode == MODE_INFER) {
      inference_timer += LOOP_PERIOD_MS;
      if (inference_timer >= INFERENCE_INTERVAL_MS) {
          run_knn_inference(global_norm_buffer, NUM_CHANNELS, BUFFER_SIZE, 3);
          inference_timer = 0;
      }
    }

    if (should_store) {
      // Store the already preprocessed (normalized) buffer
      save_buffer_to_nvs(store_label, global_norm_buffer, NUM_CHANNELS, BUFFER_SIZE);
      should_store = false;
    }

    global_idx = (global_idx + 1) % BUFFER_SIZE;
    vTaskDelay(pdMS_TO_TICKS(LOOP_PERIOD_MS));
  };
}
