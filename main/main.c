#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "lib.h"

#define BUFFER_SIZE 32
#define DISABLE_THRESHOLD 20

typedef enum { MODE_INFER, MODE_TRAIN } app_mode_t;

static const char* TAG = "APP";
static app_mode_t current_mode = MODE_INFER;
static int* global_buffer = NULL;
static int global_idx = 0;
static bool should_store = false;
static char store_label[16] = {0};

void save_buffer_to_nvs(const char* label, int* buffer, int idx) {
  nvs_handle_t my_handle;
  esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
    return;
  }

  // Create a linear copy of the buffer based on current idx
  // We want the most recent readings at the end
  size_t data_size = 5 * BUFFER_SIZE * sizeof(int);
  int* linear_buf = malloc(data_size);
  if (linear_buf == NULL) {
    nvs_close(my_handle);
    return;
  }

  for (int ch = 0; ch < 5; ch++) {
    for (int i = 0; i < BUFFER_SIZE; i++) {
      // The value at (idx + 1 + i) % BUFFER_SIZE is the i-th oldest reading
      int buffer_idx = (idx + 1 + i) % BUFFER_SIZE;
      linear_buf[ch * BUFFER_SIZE + i] = buffer[ch * BUFFER_SIZE + buffer_idx];
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
        printf("Available: mode train, mode infer, store <label>\n");
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
  
  global_buffer = calloc(5 * BUFFER_SIZE, sizeof(int));
  int min[5], max[5] = {0};
  for (int i = 0; i < 5; i++) {
    adc_oneshot_config_channel(adc, i, &channelCfg);
    int gpio;
    adc_oneshot_channel_to_io(ADC_UNIT_1, i, &gpio);
    gpio_set_pull_mode(gpio, GPIO_PULLDOWN_ONLY);
    min[i] = 4096;
  }

  xTaskCreate(console_task, "console", 4096, NULL, 5, NULL);

  bool isDisabled = false;
  while (1) {
    for (int i = 0; i < 5; i++) {
      adc_oneshot_read(adc, i, global_buffer + i * BUFFER_SIZE + global_idx);
    }
    
    if (current_mode == MODE_INFER) {
      setMinMax(global_buffer, 5, BUFFER_SIZE, min, max, &isDisabled, DISABLE_THRESHOLD);
      printRes(global_buffer, global_idx, 5, BUFFER_SIZE, min, max);
      printf(">d:%d\n", isDisabled);
    } else {
      // In TRAIN mode, maybe less printing, or indicate we are training
      printf(">mode:TRAIN idx:%d\n", global_idx);
    }

    if (should_store) {
      save_buffer_to_nvs(store_label, global_buffer, global_idx);
      should_store = false;
    }

    global_idx = (global_idx + 1) % BUFFER_SIZE;
    vTaskDelay(pdMS_TO_TICKS(100));
  };
}
