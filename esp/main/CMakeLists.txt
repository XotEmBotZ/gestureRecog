idf_component_register(SRCS "lib.c" "main.c" "storage.c" "preprocess.c" "inference.c" "ble_uart.c"
                    PRIV_REQUIRES esp_adc driver esp_timer nvs_flash esp_partition esp-dsp spiffs nimble bt
                    INCLUDE_DIRS ".")