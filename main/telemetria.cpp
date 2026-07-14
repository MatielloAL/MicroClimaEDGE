#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "telemetria.h"
#include "sensores.h"
#include "controle.h"

static const char* WIFI_SSID     = "CLARO_2670F7";
static const char* WIFI_PASSWORD = "Cu7nUUhvVT";
static const char* MQTT_HOST     = "mqtt.thingsboard.cloud";
static const uint16_t MQTT_PORT  = 1883;
static const char* MQTT_TOKEN    = "iSmTKl2lfOKKti2MT3sl"; // Access Token do device no ThingsBoard (validado via paho em 2026-07-14)

static WiFiClient g_wifi_client;
static PubSubClient g_mqtt(g_wifi_client);

static void mqtt_callback(char* topic, uint8_t* payload, unsigned int length) {
  char buf[128];
  unsigned int n = (length < sizeof(buf) - 1) ? length : sizeof(buf) - 1;
  memcpy(buf, payload, n);
  buf[n] = '\0';

  if (strstr(buf, "bomba_manual") == NULL) return;

  bool valor = (strstr(buf, "true") != NULL);
  if (xSemaphoreTake(g_mutex_atuadores, pdMS_TO_TICKS(100)) == pdTRUE) {
    g_atuadores.bomba_manual_override = true;
    g_atuadores.bomba_manual_valor = valor;
    g_atuadores.bomba_manual_timestamp_ms = millis();
    xSemaphoreGive(g_mutex_atuadores);
  }
  Serial.printf("[Telemetria] bomba_manual recebido: %s\n", valor ? "true" : "false");
}

static void conectar_wifi() {
  static unsigned long ultima_tentativa = 0;
  static bool tentativa_em_andamento = false;

  if (WiFi.status() == WL_CONNECTED) {
    tentativa_em_andamento = false;
    return;
  }

  unsigned long agora = millis();
  if (tentativa_em_andamento) {
    // Ja chamou WiFi.begin(); so espera resolver, sem chamar de novo (evita
    // ESP_ERR_WIFI_STATE por reentrancia). Desiste apos 10s e tenta de novo.
    if (agora - ultima_tentativa < 10000) return;
    tentativa_em_andamento = false;
  }

  // Garante que o driver esta limpo antes de cada nova tentativa
  // (evita "sta is connecting, cannot set config" de tentativa anterior presa).
  WiFi.disconnect();
  vTaskDelay(pdMS_TO_TICKS(100));

  Serial.printf("[Telemetria] Tentando conectar em \"%s\"...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  tentativa_em_andamento = true;
  ultima_tentativa = agora;
  vTaskDelay(pdMS_TO_TICKS(3000));
  Serial.printf("[Telemetria] WiFi.status() = %d (3=CONNECTED, 6=CONNECT_FAILED, 1=NO_SSID_AVAIL, 4=DISCONNECTED)\n", (int)WiFi.status());
  if (WiFi.status() == WL_CONNECTED) {
    tentativa_em_andamento = false;
    Serial.print("[Telemetria] Wi-Fi conectado, IP: ");
    Serial.println(WiFi.localIP());
  }
}

static void conectar_mqtt() {
  if (g_mqtt.connected()) return;
  String client_id = "MicroClimaEDGE-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  if (g_mqtt.connect(client_id.c_str(), MQTT_TOKEN, NULL)) {
    g_mqtt.subscribe("v1/devices/me/attributes");
    Serial.println("[Telemetria] MQTT conectado ao ThingsBoard.");
  }
}

static void publicar_telemetria(const SensorData &s, const ActuatorState &a) {
  char payload[256];
  snprintf(payload, sizeof(payload),
    "{\"temp_int\":%.2f,\"pressao_int\":%.2f,\"temp_ext\":%.2f,\"pressao_ext\":%.2f,"
    "\"chuva\":%s,\"cooler_on\":%s,\"bomba_on\":%s}",
    s.temp_int, s.pressao_int, s.temp_ext, s.pressao_ext,
    s.chuva ? "true" : "false",
    a.cooler_on ? "true" : "false",
    a.bomba_on ? "true" : "false");
  g_mqtt.publish("v1/devices/me/telemetry", payload);
}

void vTaskTelemetry(void *pvParameters) {
  (void) pvParameters;

  // Limpa qualquer estado/credencial salva na NVS que a ESP32 tenta usar
  // sozinha no boot (causa conflito "sta is connecting" com o WiFi.begin() abaixo).
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  vTaskDelay(pdMS_TO_TICKS(200));

  g_mqtt.setServer(MQTT_HOST, MQTT_PORT);
  g_mqtt.setCallback(mqtt_callback);

  for (;;) {
    conectar_wifi();
    if (WiFi.status() == WL_CONNECTED) {
      conectar_mqtt();
      if (g_mqtt.connected()) {
        g_mqtt.loop();

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
        publicar_telemetria(s, a);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}
