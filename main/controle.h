#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct ActuatorState {
  bool cooler_on;
  bool bomba_on;
  bool bomba_manual_override;
  bool bomba_manual_valor;
  unsigned long bomba_manual_timestamp_ms; // quando o override foi setado (p/ expirar sozinho)
};

extern ActuatorState g_atuadores;
extern SemaphoreHandle_t g_mutex_atuadores;

void vTaskControl(void *pvParameters);
