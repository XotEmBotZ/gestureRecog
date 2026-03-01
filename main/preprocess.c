#include "preprocess.h"

void setMinMax(int* buffer, int num_channels, int buffer_size, int* min, int* max,
               bool* isDisabled, int disableThreshold) {
  *isDisabled = false;
  for (int i = 0; i < num_channels; i++) {
    min[i] = 4096;
    max[i] = 0;
    for (int j = 0; j < buffer_size; j++) {
      int val = *(buffer + i * buffer_size + j);
      if (val < min[i]) min[i] = val;
      if (val > max[i]) max[i] = val;
    }
    if (!((max[i] - min[i]) >= disableThreshold)) *isDisabled = true;
  }
}

void normalize_buffer(int* raw_buffer, float* norm_buffer, int idx, int num_channels, int buffer_size, int* min, int* max) {
    for (int ch = 0; ch < num_channels; ch++) {
        int range = max[ch] - min[ch];
        for (int i = 0; i < buffer_size; i++) {
            // Linearize circular buffer: idx+1+i is the oldest to newest order
            int buffer_idx = (idx + 1 + i) % buffer_size;
            int raw_val = raw_buffer[ch * buffer_size + buffer_idx];
            
            if (range > 0) {
                norm_buffer[ch * buffer_size + i] = (float)(raw_val - min[ch]) / (float)range;
            } else {
                norm_buffer[ch * buffer_size + i] = 0.0f;
            }
        }
    }
}
