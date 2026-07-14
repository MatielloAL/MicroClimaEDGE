# Adafruit Bus IO Library
# https://github.com/adafruit/Adafruit_BusIO
# MIT License

cmake_minimum_required(VERSION 3.5)

idf_component_register(SRCS "Adafruit_SSD1306.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES arduino-esp32 Adafruit_GFX Adafruit_BusIO)

project(Adafruit_SSD1306)
