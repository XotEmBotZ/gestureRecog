#ifndef CONFIG_H
#define CONFIG_H

#include "hal/adc_types.h"

// --- Hardware Configuration ---
#define ADC_UNIT           ADC_UNIT_1
#define ADC_ATTEN          ADC_ATTEN_DB_12
#define ADC_BITWIDTH       ADC_BITWIDTH_12
#define ADC_MAX_VALUE      4095
#define ADC_MIN_VALUE      0

// --- Signal Processing ---
#define NUM_CHANNELS       5
#define BUFFER_SIZE        32
#define DISABLE_THRESHOLD  20
#define SAMPLING_FREQ_HZ   10

// --- Storage Configuration ---
#define SPIFFS_BASE_PATH   "/spiffs"
#define DATASET_FILENAME   "dataset.bin"
#define DATASET_PATH       SPIFFS_BASE_PATH "/" DATASET_FILENAME
#define MAX_LABEL_LENGTH   16
#define SPIFFS_MAX_FILES   5

// --- Inference Configuration ---
#define INFERENCE_BATCH_SIZE   20
#define DEFAULT_K_NEIGHBORS    3
#define MAX_K_NEIGHBORS        10
#define MEMORY_ALIGNMENT       16

// --- Task & Timing Configuration ---
#define CONSOLE_TASK_STACK_SIZE 4096
#define CONSOLE_TASK_PRIO       5
#define CONSOLE_LINE_BUFFER_SIZE 64
#define LOOP_PERIOD_MS          (1000 / SAMPLING_FREQ_HZ)
#define INFERENCE_INTERVAL_MS   1000

// --- Console Commands ---
#define CMD_HELP           "help"
#define CMD_MODE_TRAIN     "mode train"
#define CMD_MODE_INFER     "mode infer"
#define CMD_LIST           "list"
#define CMD_CLEAR          "clear"
#define CMD_K_PREFIX       "k "
#define CMD_STORE_PREFIX   "store "

// --- Debug Configuration ---
#define DEBUG_PRINT_SAMPLES_COUNT 5

#endif // CONFIG_H
