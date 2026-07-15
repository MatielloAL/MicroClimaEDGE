#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct SensorData {
  float temp_int;
  float pressao_int;
  bool bmp_int_ok;
  float temp_ext;
  float pressao_ext;
  bool bmp_ext_ok;
  int umidade_solo_raw;   // leitura bruta do ADC (0-4095), maior = mais seco
  float umidade_solo_pct; // aproximacao 0-100%, calibrar apos teste de bancada
  bool solo_seco;         // true se abaixo do limiar (precisa regar)
};

extern SensorData g_sensores;
extern SemaphoreHandle_t g_mutex_sensores;

void vTaskSensors(void *pvParameters);
