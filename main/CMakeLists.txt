idf_component_register(SRCS "MicroClimaEdge.c" "sensores.c" "controle.c" "display.c" "telemetria.c"
                            "bmp280_driver.c" "oled_driver.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_wifi esp_netif esp_event nvs_flash mqtt esp_timer esp_adc esp_common)
