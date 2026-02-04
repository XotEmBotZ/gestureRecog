idf_component_register(SRCS "lib.c" "main.c"
                    PRIV_REQUIRES esp_adc driver esp_timer
                    INCLUDE_DIRS ".")