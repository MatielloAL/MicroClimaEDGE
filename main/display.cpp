#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "display.h"
#include "sensores.h"
#include "controle.h"

#define ENDERECO_OLED  0x3C
#define TELA_LARGURA   128
#define TELA_ALTURA    64

static Adafruit_SSD1306 display(TELA_LARGURA, TELA_ALTURA, &Wire, -1);

static void tela_sensores(const SensorData &s) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("-- Interno --");
  display.setCursor(0, 12);
  display.printf("%.1fC  %.0fPa\n", s.temp_int, s.pressao_int);
  display.setCursor(0, 32);
  display.println("-- Externo --");
  display.setCursor(0, 44);
  display.printf("%.1fC  %.0fPa\n", s.temp_ext, s.pressao_ext);
  display.display();
}

static void tela_status(const SensorData &s, const ActuatorState &a) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.printf("Cooler: %s\n", a.cooler_on ? "ON" : "OFF");
  display.setCursor(0, 20);
  display.printf("Bomba:  %s\n", a.bomba_on ? "ON" : "OFF");
  display.setCursor(0, 40);
  display.printf("Solo:   %.0f%% %s\n", s.umidade_solo_pct, s.solo_seco ? "(seco)" : "");
  display.display();
}

void vTaskDisplay(void *pvParameters) {
  (void) pvParameters;

  if (!display.begin(SSD1306_SWITCHCAPVCC, ENDERECO_OLED)) {
    Serial.println("[Display] FALHA ao inicializar OLED");
    vTaskDelete(NULL);
    return;
  }
  display.clearDisplay();
  display.display();

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

    if (tela_sensores_ativa) tela_sensores(s); else tela_status(s, a);
    tela_sensores_ativa = !tela_sensores_ativa;

    vTaskDelay(pdMS_TO_TICKS(4000));
  }
}
