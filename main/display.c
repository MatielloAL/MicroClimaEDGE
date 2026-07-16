#include "display.h"
#include "sensores.h"
#include "controle.h"
#include "oled_driver.h"
#include "esp_log.h"

static const char *TAG = "display";
#define ENDERECO_OLED  0x3C

static void tela_sensores(const SensorData *s) {
    oled_clear();
    oled_set_cursor(0, 0);
    oled_print("-- INTERNO --");
    oled_set_cursor(0, 2);
    oled_printf("%.1fC %.0fPA", s->temp_int, s->pressao_int);
    oled_set_cursor(0, 4);
    oled_print("-- EXTERNO --");
    oled_set_cursor(0, 6);
    oled_printf("%.1fC %.0fPA", s->temp_ext, s->pressao_ext);
    oled_display();
}

static void tela_status(const SensorData *s, const ActuatorState *a) {
    oled_clear();
    oled_set_cursor(0, 0);
    oled_printf("COOLER: %s", a->cooler_on ? "ON" : "OFF");
    oled_set_cursor(0, 3);
    oled_printf("BOMBA:  %s", a->bomba_on ? "ON" : "OFF");
    oled_set_cursor(0, 6);
    oled_printf("SOLO: %.0f%% %s", s->umidade_solo_pct, s->solo_seco ? "(SECO)" : "");
    oled_display();
}

void vTaskDisplay(void *pvParameters) {
    (void) pvParameters;

    if (!oled_init(g_i2c_bus, ENDERECO_OLED)) {
        ESP_LOGE(TAG, "FALHA ao inicializar OLED");
        vTaskDelete(NULL);
        return;
    }

    bool tela_sensores_ativa = true;
    for (;;) {
        SensorData s = {0};
        ActuatorState a = {0};
        if (xSemaphoreTake(g_mutex_sensores, pdMS_TO_TICKS(100)) == pdTRUE) {
            s = g_sensores;
            xSemaphoreGive(g_mutex_sensores);
        }
        if (xSemaphoreTake(g_mutex_atuadores, pdMS_TO_TICKS(100)) == pdTRUE) {
            a = g_atuadores;
            xSemaphoreGive(g_mutex_atuadores);
        }

        if (tela_sensores_ativa) tela_sensores(&s); else tela_status(&s, &a);
        tela_sensores_ativa = !tela_sensores_ativa;

        vTaskDelay(pdMS_TO_TICKS(4000));
    }
}
