#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include "sensores.h"

#define PINO_I2C_SDA        21
#define PINO_I2C_SCL        22
#define ENDERECO_BMP_INT    0x76
#define ENDERECO_BMP_EXT    0x77
#define PINO_SOLO_AO        35    // sensor de umidade de solo MH-Series, saida analogica

// Limiares calibrados em bancada em 2026-07-14: seco no ar = 4095, mergulhado em agua ~1900
#define SOLO_RAW_SECO   3500
#define SOLO_RAW_UMIDO  1900

static Adafruit_BMP280 bmp_interno(&Wire);
static Adafruit_BMP280 bmp_externo(&Wire);

SensorData g_sensores = {0};
SemaphoreHandle_t g_mutex_sensores = NULL;

static float solo_raw_para_pct(int raw) {
  float pct = 100.0f * (float)(SOLO_RAW_SECO - raw) / (float)(SOLO_RAW_SECO - SOLO_RAW_UMIDO);
  if (pct < 0.0f) pct = 0.0f;
  if (pct > 100.0f) pct = 100.0f;
  return pct;
}

void vTaskSensors(void *pvParameters) {
  (void) pvParameters;

  Wire.begin(PINO_I2C_SDA, PINO_I2C_SCL);
  pinMode(PINO_SOLO_AO, INPUT);
  bool int_ok = bmp_interno.begin(ENDERECO_BMP_INT);
  bool ext_ok = bmp_externo.begin(ENDERECO_BMP_EXT);
  Serial.printf("[Sensores] BMP interno: %s | BMP externo: %s\n",
                int_ok ? "OK" : "FALHA", ext_ok ? "OK" : "FALHA");

  for (;;) {
    float t_int = bmp_interno.readTemperature();
    float p_int = bmp_interno.readPressure();
    float t_ext = bmp_externo.readTemperature();
    float p_ext = bmp_externo.readPressure();
    int solo_raw = analogRead(PINO_SOLO_AO);
    float solo_pct = solo_raw_para_pct(solo_raw);
    bool solo_seco = (solo_raw >= SOLO_RAW_SECO);

    if (xSemaphoreTake(g_mutex_sensores, pdMS_TO_TICKS(100)) == pdTRUE) {
      g_sensores.temp_int = t_int;
      g_sensores.pressao_int = p_int;
      g_sensores.bmp_int_ok = !isnan(t_int);
      g_sensores.temp_ext = t_ext;
      g_sensores.pressao_ext = p_ext;
      g_sensores.bmp_ext_ok = !isnan(t_ext);
      g_sensores.umidade_solo_raw = solo_raw;
      g_sensores.umidade_solo_pct = solo_pct;
      g_sensores.solo_seco = solo_seco;
      xSemaphoreGive(g_mutex_sensores);
    }

    Serial.printf("[Sensores] Solo: raw=%d pct=%.1f%% seco=%s\n",
                  solo_raw, solo_pct, solo_seco ? "true" : "false");

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}
