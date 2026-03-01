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
