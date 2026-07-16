#pragma once
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct {
  bool cooler_on;
  bool bomba_on;
  bool bomba_manual_override;
  bool bomba_manual_valor;
  int64_t bomba_manual_timestamp_ms; // quando o override foi setado (p/ expirar sozinho)
} ActuatorState;

extern ActuatorState g_atuadores;
extern SemaphoreHandle_t g_mutex_atuadores;

void vTaskControl(void *pvParameters);
