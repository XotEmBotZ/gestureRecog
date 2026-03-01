#ifndef LIB_H
#define LIB_H

#include <stdbool.h>

void setMinMax(int* buffer, int channel, int bufferSize, int* min, int* max,
               bool* isDisabled, int disableThreshold);

void printRes(int* buffer, int idx, int channel, int bufferSize, int* min,
              int* max);

#endif // LIB_H
