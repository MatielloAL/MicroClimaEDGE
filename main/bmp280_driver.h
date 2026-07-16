#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c_master.h"

typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
} bmp280_calib_t;

typedef struct {
    i2c_master_dev_handle_t dev;
    bmp280_calib_t calib;
    bool ok;
} bmp280_t;

// Adiciona o BMP280 no barramento i2c_master ja criado (endereco 0x76 ou 0x77) e le a calibracao.
bool bmp280_init(i2c_master_bus_handle_t bus, uint8_t endereco, bmp280_t *out);

// Le temperatura (C) e pressao (Pa) compensadas. Retorna false se a leitura I2C falhar.
bool bmp280_read(bmp280_t *sensor, float *temperatura_c, float *pressao_pa);
