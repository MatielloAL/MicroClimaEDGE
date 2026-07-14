#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include "sensores.h"

#define PINO_I2C_SDA      21
#define PINO_I2C_SCL      22
#define ENDERECO_BMP_INT  0x76
#define ENDERECO_BMP_EXT  0x77
#define PINO_SENSOR_CHUVA 34

static Adafruit_BMP280 bmp_interno(&Wire);
static Adafruit_BMP280 bmp_externo(&Wire);

SensorData g_sensores = {0};
SemaphoreHandle_t g_mutex_sensores = NULL;

void vTaskSensors(void *pvParameters) {
  (void) pvParameters;

  Wire.begin(PINO_I2C_SDA, PINO_I2C_SCL);
  pinMode(PINO_SENSOR_CHUVA, INPUT);
  bool int_ok = bmp_interno.begin(ENDERECO_BMP_INT);
  bool ext_ok = bmp_externo.begin(ENDERECO_BMP_EXT);
  Serial.printf("[Sensores] BMP interno: %s | BMP externo: %s\n",
                int_ok ? "OK" : "FALHA", ext_ok ? "OK" : "FALHA");

  for (;;) {
    float t_int = bmp_interno.readTemperature();
    float p_int = bmp_interno.readPressure();
    float t_ext = bmp_externo.readTemperature();
    float p_ext = bmp_externo.readPressure();
    bool chuva = (digitalRead(PINO_SENSOR_CHUVA) == HIGH);

    if (xSemaphoreTake(g_mutex_sensores, pdMS_TO_TICKS(100)) == pdTRUE) {
      g_sensores.temp_int = t_int;
      g_sensores.pressao_int = p_int;
      g_sensores.bmp_int_ok = !isnan(t_int);
      g_sensores.temp_ext = t_ext;
      g_sensores.pressao_ext = p_ext;
      g_sensores.bmp_ext_ok = !isnan(t_ext);
      g_sensores.chuva = chuva;
      xSemaphoreGive(g_mutex_sensores);
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}
