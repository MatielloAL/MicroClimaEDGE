/* ==========================================================
   MicroClima EDGE - Firmware de producao
   Sistema continuo de controle de estufa: le sensores (BMP280 x2 +
   umidade de solo), aciona cooler/bomba via rele real, sinaliza
   status no LED RGB, mostra no OLED e publica telemetria no
   ThingsBoard via MQTT.
   O controle local nunca depende de Wi-Fi/MQTT.
   ========================================================== */

#include <Arduino.h>
#include "sensores.h"
#include "controle.h"
#include "display.h"
#include "telemetria.h"

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== MicroClima EDGE - iniciando ===");

  g_mutex_sensores = xSemaphoreCreateMutex();
  g_mutex_atuadores = xSemaphoreCreateMutex();

  xTaskCreatePinnedToCore(vTaskSensors,   "vTaskSensors",   4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(vTaskControl,   "vTaskControl",   4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(vTaskDisplay,   "vTaskDisplay",   4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(vTaskTelemetry, "vTaskTelemetry", 8192, NULL, 1, NULL, 0);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
