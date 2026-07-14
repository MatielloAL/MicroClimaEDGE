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
  bool chuva;
};

extern SensorData g_sensores;
extern SemaphoreHandle_t g_mutex_sensores;

void vTaskSensors(void *pvParameters);
