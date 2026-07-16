#include <string.h>
#include "telemetria.h"
#include "sensores.h"
#include "controle.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "mqtt_client.h"
#include "freertos/event_groups.h"

static const char *TAG = "telemetria";

#define WIFI_SSID     "Matiello"
#define WIFI_PASSWORD "12345678"
#define MQTT_URI      "mqtt://mqtt.thingsboard.cloud:1883"
#define MQTT_TOKEN    "iSmTKl2lfOKKti2MT3sl" // Access Token do device no ThingsBoard

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONECTADO_BIT BIT0

static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static volatile bool s_mqtt_conectado = false;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONECTADO_BIT);
        ESP_LOGW(TAG, "Wi-Fi desconectado, tentando reconectar...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "Wi-Fi conectado, IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONECTADO_BIT);
    }
}

static void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Conectando em \"%s\"...", WIFI_SSID);
}

static void processar_mensagem_mqtt(const char *topico, const char *dados, int len) {
    (void) topico;
    char buf[160];
    int n = (len < (int)sizeof(buf) - 1) ? len : (int)sizeof(buf) - 1;
    memcpy(buf, dados, n);
    buf[n] = '\0';

    if (strstr(buf, "bomba_manual") == NULL) return;

    bool valor = (strstr(buf, "true") != NULL);
    if (xSemaphoreTake(g_mutex_atuadores, pdMS_TO_TICKS(100)) == pdTRUE) {
        g_atuadores.bomba_manual_override = true;
        g_atuadores.bomba_manual_valor = valor;
        g_atuadores.bomba_manual_timestamp_ms = esp_timer_get_time() / 1000;
        xSemaphoreGive(g_mutex_atuadores);
    }
    ESP_LOGI(TAG, "bomba_manual recebido: %s", valor ? "true" : "false");
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data) {
    (void) handler_args;
    (void) base;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            s_mqtt_conectado = true;
            esp_mqtt_client_subscribe(s_mqtt_client, "v1/devices/me/attributes", 0);
            ESP_LOGI(TAG, "MQTT conectado ao ThingsBoard.");
            break;
        case MQTT_EVENT_DISCONNECTED:
            s_mqtt_conectado = false;
            break;
        case MQTT_EVENT_DATA:
            processar_mensagem_mqtt(event->topic, event->data, event->data_len);
            break;
        default:
            break;
    }
}

static void mqtt_init(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_URI,
        .credentials.username = MQTT_TOKEN,
    };
    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_mqtt_client);
}

static void publicar_telemetria(const SensorData *s, const ActuatorState *a) {
    char payload[256];
    snprintf(payload, sizeof(payload),
        "{\"temp_int\":%.2f,\"pressao_int\":%.2f,\"temp_ext\":%.2f,\"pressao_ext\":%.2f,"
        "\"umidade_solo_pct\":%.1f,\"solo_seco\":%s,\"cooler_on\":%s,\"bomba_on\":%s}",
        s->temp_int, s->pressao_int, s->temp_ext, s->pressao_ext,
        s->umidade_solo_pct,
        s->solo_seco ? "true" : "false",
        a->cooler_on ? "true" : "false",
        a->bomba_on ? "true" : "false");
    esp_mqtt_client_publish(s_mqtt_client, "v1/devices/me/telemetry", payload, 0, 0, 0);
}

void vTaskTelemetry(void *pvParameters) {
    (void) pvParameters;

    wifi_init_sta();
    mqtt_init();

    for (;;) {
        if (s_mqtt_conectado) {
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
            publicar_telemetria(&s, &a);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
