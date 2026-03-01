#include <stdbool.h>
#include <stdio.h>
#include "lib.h"
#include "config.h"

void printRes(int raw_val, float proc_val, int channel, int min, int max) {
    // Map the 0.0-1.0 proc_val back to ADC range for consistent plotter visualization
    int mapped = (int)(proc_val * (float)ADC_MAX_VALUE);
    // As per user instructions: prefix >r* is the completely preprocessed one
    // >channel:raw_val
    // >rchannel:mapped_val
    printf(">%d:%d\n>r%d:%d\n>max%d:%d\n>min%d:%d\n", 
           channel, raw_val, channel, mapped, channel, max, channel, min);
}
