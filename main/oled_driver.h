#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c_master.h"

// Driver minimo SSD1306 128x64 via I2C, sem lib externa.
// Buffer de 1 bit por pixel (128x64/8 = 1024 bytes), texto via fonte 5x7 embutida.

bool oled_init(i2c_master_bus_handle_t bus, uint8_t endereco);
void oled_clear(void);
void oled_set_cursor(uint8_t coluna, uint8_t linha_de_texto); // linha_de_texto: 0-7 (cada linha = 8px)
void oled_print(const char *texto);
void oled_printf(const char *fmt, ...);
void oled_display(void); // envia o buffer inteiro pro display
