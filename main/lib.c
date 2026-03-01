#include <stdbool.h>
#include <stdio.h>
#include "lib.h"

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
