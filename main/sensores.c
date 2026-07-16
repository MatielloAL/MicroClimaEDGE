#include "sensores.h"
#include "bmp280_driver.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

static const char *TAG = "sensores";

#define PINO_I2C_SDA        21
#define PINO_I2C_SCL        22
#define ENDERECO_BMP_INT    0x76
#define ENDERECO_BMP_EXT    0x77

#define ADC_UNIDADE          ADC_UNIT_1
#define ADC_CANAL_SOLO       ADC_CHANNEL_7 // GPIO35 no ADC1

// Limiares calibrados em bancada em 2026-07-14: seco no ar = 4095, mergulhado em agua ~1900
#define SOLO_RAW_SECO   3500
#define SOLO_RAW_UMIDO  1900

SensorData g_sensores = {0};
SemaphoreHandle_t g_mutex_sensores = NULL;
i2c_master_bus_handle_t g_i2c_bus = NULL;

static bmp280_t s_bmp_int;
static bmp280_t s_bmp_ext;
static adc_oneshot_unit_handle_t s_adc_handle;

bool sensores_i2c_bus_init(void) {
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PINO_I2C_SDA,
        .scl_io_num = PINO_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    if (i2c_new_master_bus(&bus_cfg, &g_i2c_bus) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao criar barramento I2C");
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // barramento recem-criado precisa assentar antes da 1a transacao
    return true;
}

static float solo_raw_para_pct(int raw) {
    float pct = 100.0f * (float)(SOLO_RAW_SECO - raw) / (float)(SOLO_RAW_SECO - SOLO_RAW_UMIDO);
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    return pct;
}

void vTaskSensors(void *pvParameters) {
    (void) pvParameters;

    bool int_ok = bmp280_init(g_i2c_bus, ENDERECO_BMP_INT, &s_bmp_int);
    bool ext_ok = bmp280_init(g_i2c_bus, ENDERECO_BMP_EXT, &s_bmp_ext);
    ESP_LOGI(TAG, "BMP interno: %s | BMP externo: %s",
             int_ok ? "OK" : "FALHA", ext_ok ? "OK" : "FALHA");

    adc_oneshot_unit_init_cfg_t adc_init_cfg = {
        .unit_id = ADC_UNIDADE,
    };
    adc_oneshot_new_unit(&adc_init_cfg, &s_adc_handle);

    adc_oneshot_chan_cfg_t adc_chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    adc_oneshot_config_channel(s_adc_handle, ADC_CANAL_SOLO, &adc_chan_cfg);

    for (;;) {
        float t_int = 0, p_int = 0, t_ext = 0, p_ext = 0;
        bool leu_int = bmp280_read(&s_bmp_int, &t_int, &p_int);
        bool leu_ext = bmp280_read(&s_bmp_ext, &t_ext, &p_ext);

        int solo_raw = 0;
        adc_oneshot_read(s_adc_handle, ADC_CANAL_SOLO, &solo_raw);
        float solo_pct = solo_raw_para_pct(solo_raw);
        bool solo_seco = (solo_raw >= SOLO_RAW_SECO);

        if (xSemaphoreTake(g_mutex_sensores, pdMS_TO_TICKS(100)) == pdTRUE) {
            g_sensores.temp_int = t_int;
            g_sensores.pressao_int = p_int;
            g_sensores.bmp_int_ok = leu_int;
            g_sensores.temp_ext = t_ext;
            g_sensores.pressao_ext = p_ext;
            g_sensores.bmp_ext_ok = leu_ext;
            g_sensores.umidade_solo_raw = solo_raw;
            g_sensores.umidade_solo_pct = solo_pct;
            g_sensores.solo_seco = solo_seco;
            xSemaphoreGive(g_mutex_sensores);
        }

        ESP_LOGI(TAG, "Solo: raw=%d pct=%.1f%% seco=%s",
                 solo_raw, solo_pct, solo_seco ? "true" : "false");

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
