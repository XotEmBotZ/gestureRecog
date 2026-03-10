#include "preprocess.h"
#include "config.h"

void setMinMax(int* buffer, int num_channels, int buffer_size, int* min,
               int* max, bool* isDisabled, int disableThreshold) {
  *isDisabled = true;
  for (int i = 0; i < num_channels; i++) {
    min[i] = ADC_MAX_VALUE + 1;
    max[i] = 0;
    for (int j = 0; j < buffer_size; j++) {
      int val = *(buffer + i * buffer_size + j);
      if (val < min[i]) min[i] = val;
      if (val > max[i]) max[i] = val;
    }
    if (((max[i] - min[i]) >= disableThreshold)) *isDisabled = false;
  }
}

float preprocess_sample(int raw_val, int min, int max) {
  int range = max - min;
  if (range <= 0) return 0.0f;

  // Clamp raw_val to [min, max]
  if (raw_val < min) raw_val = min;
  if (raw_val > max) raw_val = max;

  return (float)(raw_val - min) / (float)range;
}

float apply_ema(float prev_val, int raw_val, float alpha) {
    return (alpha * (float)raw_val) + ((1.0f - alpha) * prev_val);
}

void update_calibration(int raw_val, int* cal_min, int* cal_max) {
    if (raw_val < *cal_min) *cal_min = raw_val;
    if (raw_val > *cal_max) *cal_max = raw_val;
}
