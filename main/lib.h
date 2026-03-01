#ifndef LIB_H
#define LIB_H

#include <stdbool.h>

/**
 * @brief Prints sensor results for debugging.
 */
void printRes(int* buffer, int idx, int channel, int bufferSize, int* min,
              int* max);

#endif // LIB_H
