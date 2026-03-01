idf_component_register(SRCS "lib.c" "main.c"
                    PRIV_REQUIRES esp_adc driver esp_timer nvs_flash nvs esp_partition
                    INCLUDE_DIRS ".")