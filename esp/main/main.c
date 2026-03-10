#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "inference.h"
#include "lib.h"
#include "nvs_flash.h"
#include "preprocess.h"
#include "storage.h"

typedef enum { MODE_INFER, MODE_TRAIN, MODE_CALIBRATE } app_mode_t;

static const char* TAG = "APP";
static app_mode_t current_mode = MODE_INFER;
static int* global_buffer = NULL;
static float* global_norm_buffer = NULL;
static float* ema_state = NULL;
static int* cal_min = NULL;
static int* cal_max = NULL;
static int global_idx = 0;
static bool should_store = false;
static char store_label[MAX_LABEL_LENGTH] = {0};
static int k_neighbors = DEFAULT_K_NEIGHBORS;

void show_help() {
  printf("\n=== ESP-Ossicoloscope Help (File-Based Storage) ===\n");
  printf("Commands:\n");
  printf("  %s          - Show this help message\n", CMD_HELP);
  printf("  %s    - Switch to Training Mode\n", CMD_MODE_TRAIN);
  printf("  %s    - Switch to Inference Mode (KNN)\n", CMD_MODE_INFER);
  printf("  %s - Switch to Calibration Mode (Move fingers to extremes)\n", CMD_MODE_CALIBRATE);
  printf("  %s<label> - Append current preprocessed buffer to dataset file\n",
         CMD_STORE_PREFIX);
  printf("  %s          - List all stored records in dataset file\n", CMD_LIST);
  printf("  %s         - Permanently delete the dataset file\n", CMD_CLEAR);
  printf("  %s<number>    - Set number of neighbors for KNN (Current: %d)\n",
         CMD_K_PREFIX, k_neighbors);
  printf("==============================\n");
}

void console_task(void* arg) {
  char line[CONSOLE_LINE_BUFFER_SIZE];
  while (1) {
    if (fgets(line, sizeof(line), stdin)) {
      line[strcspn(line, "\n")] = 0;  // Remove newline
      if (strcmp(line, CMD_HELP) == 0) {
        show_help();
      } else if (strcmp(line, CMD_MODE_TRAIN) == 0) {
        current_mode = MODE_TRAIN;
        printf("Switched to TRAIN mode\n");
      } else if (strcmp(line, CMD_MODE_INFER) == 0) {
        current_mode = MODE_INFER;
        printf("Switched to INFER mode\n");
      } else if (strcmp(line, CMD_MODE_CALIBRATE) == 0) {
        current_mode = MODE_CALIBRATE;
        for (int i = 0; i < NUM_CHANNELS; i++) {
            cal_min[i] = ADC_MAX_VALUE;
            cal_max[i] = 0;
        }
        printf("Switched to CALIBRATE mode. Move fingers to their full range.\n");
      } else if (strcmp(line, CMD_LIST) == 0) {
        list_stored_buffers(NUM_CHANNELS, BUFFER_SIZE);
      } else if (strncmp(line, CMD_READ_PREFIX, strlen(CMD_READ_PREFIX)) == 0) {
        int id = atoi(line + strlen(CMD_READ_PREFIX));
        if (id > 0) {
          read_sample_by_id(id, NUM_CHANNELS, BUFFER_SIZE);
        } else {
          printf("Error: Invalid ID for read command\n");
        }
      } else if (strcmp(line, CMD_CLEAR) == 0) {
        clear_stored_buffers();
      } else if (strncmp(line, CMD_K_PREFIX, strlen(CMD_K_PREFIX)) == 0) {
        int val = atoi(line + strlen(CMD_K_PREFIX));
        if (val > 0 && val <= MAX_K_NEIGHBORS) {
          k_neighbors = val;
          printf("KNN K-neighbors set to: %d\n", k_neighbors);
        } else {
          printf("Error: K must be between 1 and %d\n", MAX_K_NEIGHBORS);
        }
      } else if (strncmp(line, CMD_STORE_PREFIX, strlen(CMD_STORE_PREFIX)) ==
                 0) {
        if (current_mode != MODE_TRAIN) {
          printf("Error: Must be in TRAIN mode to store\n");
        } else {
          memset(store_label, 0, sizeof(store_label));
          strncpy(store_label, line + strlen(CMD_STORE_PREFIX),
                  sizeof(store_label) - 1);
          should_store = true;
        }
      } else {
        printf("Unknown command: %s. Type 'help' for available commands.\n",
               line);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(LOOP_PERIOD_MS));
  }
}

void app_main() {
  // Initialize NVS (still needed for system/wifi components)
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // Initialize File System Storage
  ESP_ERROR_CHECK(init_storage());

  adc_oneshot_unit_handle_t adc;
  adc_oneshot_unit_init_cfg_t adcCfg = {
      .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
      .ulp_mode = ADC_ULP_MODE_DISABLE,
      .unit_id = ADC_UNIT,
  };
  adc_oneshot_chan_cfg_t channelCfg = {
      .atten = ADC_ATTEN,
      .bitwidth = ADC_BITWIDTH,
  };
  adc_oneshot_new_unit(&adcCfg, &adc);

  global_buffer = calloc(NUM_CHANNELS * BUFFER_SIZE, sizeof(int));
  global_norm_buffer = heap_caps_aligned_alloc(
      MEMORY_ALIGNMENT, NUM_CHANNELS * BUFFER_SIZE * sizeof(float),
      MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
  ema_state = calloc(NUM_CHANNELS, sizeof(float));
  cal_min = calloc(NUM_CHANNELS, sizeof(int));
  cal_max = calloc(NUM_CHANNELS, sizeof(int));

  int min[NUM_CHANNELS], max[NUM_CHANNELS] = {0};
  for (int i = 0; i < NUM_CHANNELS; i++) {
    adc_oneshot_config_channel(adc, i, &channelCfg);
    int gpio;
    adc_oneshot_channel_to_io(ADC_UNIT, i, &gpio);
    gpio_set_pull_mode(gpio, GPIO_PULLDOWN_ONLY);
    min[i] = ADC_MAX_VALUE + 1;
    cal_min[i] = ADC_MAX_VALUE;  // Start with high value
    cal_max[i] = 0;

    // Initialize EMA state with first reading
    int first_read;
    adc_oneshot_read(adc, i, &first_read);
    ema_state[i] = (float)first_read;
  }

  xTaskCreate(console_task, "console", CONSOLE_TASK_STACK_SIZE, NULL,
              CONSOLE_TASK_PRIO, NULL);

  bool isDisabled = false;
  int inference_timer = 0;
  float* linearized_norm_buf = heap_caps_aligned_alloc(
      MEMORY_ALIGNMENT, NUM_CHANNELS * BUFFER_SIZE * sizeof(float),
      MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);

  while (1) {
    int current_raw[NUM_CHANNELS];
    for (int i = 0; i < NUM_CHANNELS; i++) {
      adc_oneshot_read(adc, i, &current_raw[i]);
      
      // 1. Apply EMA Filtering
      ema_state[i] = apply_ema(ema_state[i], current_raw[i], EMA_ALPHA);
      int smoothed_val = (int)ema_state[i];

      // 2. Handle Calibration if in MODE_CALIBRATE
      if (current_mode == MODE_CALIBRATE) {
          update_calibration(smoothed_val, &cal_min[i], &cal_max[i]);
      }

      global_buffer[i * BUFFER_SIZE + global_idx] = smoothed_val;
    }

    // Keep dynamic min/max detection only for the isDisabled movement check
    setMinMax(global_buffer, NUM_CHANNELS, BUFFER_SIZE, min, max, &isDisabled,
              DISABLE_THRESHOLD);

    for (int i = 0; i < NUM_CHANNELS; i++) {
      // 3. Normalize using Global Calibration (fall back to dynamic if not calibrated)
      int target_min, target_max;
      
      // Check if global calibration has been performed (max > min)
      bool has_global_cal = (cal_max[i] > cal_min[i]);

      if (current_mode == MODE_CALIBRATE) {
          // While calibrating, we MUST show the global bounds expanding
          target_min = cal_min[i];
          target_max = cal_max[i];
      } else if (has_global_cal) {
          // If we have a global calibration, use it
          target_min = cal_min[i];
          target_max = cal_max[i];
      } else {
          // Otherwise fall back to dynamic windowing
          target_min = min[i];
          target_max = max[i];
      }
      
      float proc = preprocess_sample((int)ema_state[i], target_min, target_max);
      global_norm_buffer[i * BUFFER_SIZE + global_idx] = proc;
      
      printRes((int)ema_state[i], proc, i, target_min, target_max);
    }
    printf(">d:%d\n>mode:%s\n", (int)isDisabled,
           (current_mode == MODE_TRAIN) ? "TRAIN" : 
           (current_mode == MODE_CALIBRATE) ? "CALIBRATE" : "INFER");

    if (current_mode == MODE_INFER && !isDisabled) {
      inference_timer += LOOP_PERIOD_MS;
      if (inference_timer >= INFERENCE_INTERVAL_MS) {
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
          for (int i = 0; i < BUFFER_SIZE; i++) {
            int buf_idx = (global_idx + 1 + i) % BUFFER_SIZE;
            linearized_norm_buf[ch * BUFFER_SIZE + i] =
                global_norm_buffer[ch * BUFFER_SIZE + buf_idx];
          }
        }
        run_knn_inference(linearized_norm_buf, NUM_CHANNELS, BUFFER_SIZE,
                          k_neighbors, 0);
        inference_timer = 0;
      }
    }

    if (should_store) {
      for (int ch = 0; ch < NUM_CHANNELS; ch++) {
        for (int i = 0; i < BUFFER_SIZE; i++) {
          int buf_idx = (global_idx + 1 + i) % BUFFER_SIZE;
          linearized_norm_buf[ch * BUFFER_SIZE + i] =
              global_norm_buffer[ch * BUFFER_SIZE + buf_idx];
        }
      }
      save_buffer_to_spiffs(store_label, linearized_norm_buf, NUM_CHANNELS,
                            BUFFER_SIZE);
      should_store = false;
    }

    global_idx = (global_idx + 1) % BUFFER_SIZE;
    vTaskDelay(pdMS_TO_TICKS(LOOP_PERIOD_MS));
  };
}
