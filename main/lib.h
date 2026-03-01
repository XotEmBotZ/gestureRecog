#ifndef LIB_H
#define LIB_H

#include <stdbool.h>

/**
 * @brief Prints sensor results for debugging.
 * 
 * @param raw_val Current raw ADC value.
 * @param proc_val Current preprocessed (normalized) value.
 * @param channel Channel index.
 * @param min Current window min.
 * @param max Current window max.
 */
void printRes(int raw_val, float proc_val, int channel, int min, int max);

#endif // LIB_H
