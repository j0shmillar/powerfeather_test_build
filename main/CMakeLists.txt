idf_component_register(SRCS main.cpp mic.h mic.cpp cam.h cam.cpp wifi.cpp wifi.h
                        PRIV_INCLUDE_DIRS .
                        PRIV_REQUIRES nvs_flash esp_psram esp_event esp_http_client esp_wifi esp_timer esp_adc)
