idf_component_register(SRCS main.c Http.h Http.c Wifi.h Wifi.c I2S.h I2S.c
                        PRIV_INCLUDE_DIRS .
                        PRIV_REQUIRES nvs_flash esp_psram esp_event esp_http_client esp_wifi) # added esp_psram
