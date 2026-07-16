#pragma once
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/i2c_master.h"

typedef struct {
  float temp_int;
  float pressao_int;
  bool bmp_int_ok;
  float temp_ext;
  float pressao_ext;
  bool bmp_ext_ok;
  int umidade_solo_raw;   // leitura bruta do ADC (0-4095), maior = mais seco
  float umidade_solo_pct; // aproximacao 0-100%, calibrado em bancada
  bool solo_seco;         // true se acima do limiar (precisa regar)
} SensorData;

extern SensorData g_sensores;
extern SemaphoreHandle_t g_mutex_sensores;
extern i2c_master_bus_handle_t g_i2c_bus; // reutilizado pelo display

// Cria o barramento I2C compartilhado. Chamar uma vez em app_main() ANTES
// de criar as tasks de sensores e display (evita corrida na criacao do bus).
bool sensores_i2c_bus_init(void);

void vTaskSensors(void *pvParameters);
