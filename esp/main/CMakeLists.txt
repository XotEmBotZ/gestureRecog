<<<<<<< Updated upstream
idf_component_register(SRCS "lib.c" "main.c" "storage.c" "preprocess.c" "inference.c"
                    PRIV_REQUIRES esp_adc driver esp_timer nvs_flash esp_partition esp-dsp spiffs
=======
idf_component_register(SRCS "lib.c" "main.c" "storage.c" "preprocess.c" "inference.c" "ble_uart.c"
                    REQUIRES esp_adc driver esp_timer nvs_flash esp_partition esp-dsp spiffs bt
>>>>>>> Stashed changes
                    INCLUDE_DIRS ".")