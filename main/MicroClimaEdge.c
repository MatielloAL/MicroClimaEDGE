/* ==========================================================
   MicroClima EDGE - Firmware de producao (ESP-IDF puro)
   Sistema continuo de controle de estufa: le sensores (BMP280 x2 +
   umidade de solo), aciona cooler/bomba via rele real, sinaliza
   status no LED, mostra no OLED e publica telemetria no
   ThingsBoard via MQTT.
   O controle local nunca depende de Wi-Fi/MQTT.
   ========================================================== */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "sensores.h"
#include "controle.h"
#include "display.h"
#include "telemetria.h"

static const char *TAG = "main";

void app_main(void) {
    ESP_LOGI(TAG, "=== MicroClima EDGE - iniciando ===");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    g_mutex_sensores = xSemaphoreCreateMutex();
    g_mutex_atuadores = xSemaphoreCreateMutex();

    if (!sensores_i2c_bus_init()) {
        ESP_LOGE(TAG, "Falha fatal ao iniciar I2C, abortando.");
        return;
    }

    xTaskCreatePinnedToCore(vTaskSensors,   "vTaskSensors",   4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(vTaskControl,   "vTaskControl",   4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(vTaskDisplay,   "vTaskDisplay",   4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(vTaskTelemetry, "vTaskTelemetry", 8192, NULL, 1, NULL, 0);
}
