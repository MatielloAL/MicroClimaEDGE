idf_component_register(SRCS "MicroClimaEdge.cpp" "sensores.cpp" "controle.cpp" "display.cpp" "telemetria.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES Adafruit_BMP280_Library Adafruit_SSD1306 PubSubClient)
