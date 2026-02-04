#include <stdbool.h>
#include <stdio.h>

void setMinMax(int* buffer, int channel, int bufferSize, int* min, int* max,
               bool* isDisabled, int disableThreshold) {
  *isDisabled = false;
  for (int i = 0; i < channel; i++) {
    min[i] = 4096;
    max[i] = 0;
    for (int j = 0; j < bufferSize; j++) {
      if (*(buffer + i * bufferSize + j) < min[i])
        min[i] = *(buffer + i * bufferSize + j);
      if (*(buffer + i * bufferSize + j) > max[i])
        max[i] = *(buffer + i * bufferSize + j);
    }
    if (!((max[i] - min[i]) >= disableThreshold)) *isDisabled = true;
  }
}

void printRes(int* buffer, int idx, int channel, int bufferSize, int* min,
              int* max) {
  for (int i = 0; i < channel; i++) {
    int range = max[i] - min[i];
    int mapped =
        (range > 0)
            ? (((long)(*(buffer + i * bufferSize + idx) - min[i]) * 4096) /
               range)
            : 0;
    printf(">%d:%d\n>r%d:%d\n>max%d:%d\n>min%d:%d\n", i, mapped, i,
           *(buffer + i * bufferSize + idx), i, max[i], i, min[i]);
  }
}