#include <stdbool.h>
#include <stdio.h>
#include "lib.h"
#include "config.h"
#include "ble_uart.h"

void printRes(int raw_val, float proc_val, int channel, int min, int max) {
    // Map the 0.0-1.0 proc_val back to ADC range for consistent plotter visualization
    // Matching 4096 scaling from commit 75e5b7c8741fa10fa379c9d4e97c141f801ee7d3
    int mapped = (int)(proc_val * 4096.0f);
    // Matching output format from commit 75e5b7c8741fa10fa379c9d4e97c141f801ee7d3:
    // >channel:mapped_val
    // >rchannel:raw_val
    telemetry_printf(">%d:%d\n>r%d:%d\n>max%d:%d\n>min%d:%d\n", 
           channel, mapped, channel, raw_val, channel, max, channel, min);
}
