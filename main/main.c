#include <stdbool.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "lib.c"

#define BUFFER_SIZE 32
#define DISABLE_THRESHOLD 20

void app_main() {
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
  int* buffer = calloc(5 * BUFFER_SIZE, sizeof(int));
  int min[5], max[5] = {0}, idx = 0;
  for (int i = 0; i < 5; i++) {
    adc_oneshot_config_channel(adc, i, &channelCfg);
    int gpio;
    adc_oneshot_channel_to_io(ADC_UNIT_1, i, &gpio);
    gpio_set_pull_mode(gpio, GPIO_PULLDOWN_ONLY);
    min[i] = 4096;
  }
  bool isDisabled = false;
  while (1) {
    for (int i = 0; i < 5; i++) {
      adc_oneshot_read(adc, i, buffer + i * BUFFER_SIZE + idx);
    }
    setMinMax(buffer, 5, BUFFER_SIZE, min, max, &isDisabled, DISABLE_THRESHOLD);
    printRes(buffer, idx, 5, BUFFER_SIZE, min, max);
    printf(">d:%d\n", isDisabled);
    idx = (idx + 1) % BUFFER_SIZE;
    vTaskDelay(pdMS_TO_TICKS(100));
  };
}
