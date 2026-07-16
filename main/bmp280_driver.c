#include "bmp280_driver.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bmp280";

#define REG_CALIB_START 0x88
#define REG_CHIP_ID      0xD0
#define REG_CTRL_MEAS    0xF4
#define REG_CONFIG       0xF5
#define REG_PRESS_MSB    0xF7

static bool escrever_registrador(bmp280_t *s, uint8_t reg, uint8_t valor) {
    uint8_t buf[2] = {reg, valor};
    return i2c_master_transmit(s->dev, buf, sizeof(buf), 100) == ESP_OK;
}

static bool ler_registradores(bmp280_t *s, uint8_t reg, uint8_t *dados, size_t n) {
    return i2c_master_transmit_receive(s->dev, &reg, 1, dados, n, 100) == ESP_OK;
}

bool bmp280_init(i2c_master_bus_handle_t bus, uint8_t endereco, bmp280_t *out) {
    memset(out, 0, sizeof(*out));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = endereco,
        .scl_speed_hz = 100000,
    };
    if (i2c_master_bus_add_device(bus, &dev_cfg, &out->dev) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao adicionar device 0x%02X no barramento", endereco);
        return false;
    }

    uint8_t chip_id = 0;
    bool leu_chip_id = false;
    for (int tentativa = 0; tentativa < 5 && !leu_chip_id; tentativa++) {
        if (tentativa > 0) vTaskDelay(pdMS_TO_TICKS(20));
        leu_chip_id = ler_registradores(out, REG_CHIP_ID, &chip_id, 1);
    }
    if (!leu_chip_id) {
        ESP_LOGE(TAG, "Falha ao ler chip_id em 0x%02X", endereco);
        return false;
    }
    if (chip_id != 0x58) {
        ESP_LOGW(TAG, "chip_id inesperado em 0x%02X: 0x%02X (esperado 0x58)", endereco, chip_id);
    }

    uint8_t calib[24];
    if (!ler_registradores(out, REG_CALIB_START, calib, sizeof(calib))) {
        ESP_LOGE(TAG, "Falha ao ler calibracao em 0x%02X", endereco);
        return false;
    }

    out->calib.dig_T1 = (uint16_t)(calib[0] | (calib[1] << 8));
    out->calib.dig_T2 = (int16_t)(calib[2] | (calib[3] << 8));
    out->calib.dig_T3 = (int16_t)(calib[4] | (calib[5] << 8));
    out->calib.dig_P1 = (uint16_t)(calib[6] | (calib[7] << 8));
    out->calib.dig_P2 = (int16_t)(calib[8] | (calib[9] << 8));
    out->calib.dig_P3 = (int16_t)(calib[10] | (calib[11] << 8));
    out->calib.dig_P4 = (int16_t)(calib[12] | (calib[13] << 8));
    out->calib.dig_P5 = (int16_t)(calib[14] | (calib[15] << 8));
    out->calib.dig_P6 = (int16_t)(calib[16] | (calib[17] << 8));
    out->calib.dig_P7 = (int16_t)(calib[18] | (calib[19] << 8));
    out->calib.dig_P8 = (int16_t)(calib[20] | (calib[21] << 8));
    out->calib.dig_P9 = (int16_t)(calib[22] | (calib[23] << 8));

    // osrs_t=1, osrs_p=1, modo normal
    if (!escrever_registrador(out, REG_CTRL_MEAS, 0x27)) return false;
    // standby 62.5ms, filtro off
    if (!escrever_registrador(out, REG_CONFIG, 0x00)) return false;

    out->ok = true;
    return true;
}

bool bmp280_read(bmp280_t *s, float *temperatura_c, float *pressao_pa) {
    if (!s->ok) return false;

    uint8_t dados[6];
    if (!ler_registradores(s, REG_PRESS_MSB, dados, sizeof(dados))) return false;

    int32_t adc_p = ((int32_t)dados[0] << 12) | ((int32_t)dados[1] << 4) | (dados[2] >> 4);
    int32_t adc_t = ((int32_t)dados[3] << 12) | ((int32_t)dados[4] << 4) | (dados[5] >> 4);

    bmp280_calib_t *c = &s->calib;
    double var1, var2, T, P;
    int32_t t_fine;

    var1 = (((double)adc_t) / 16384.0 - ((double)c->dig_T1) / 1024.0) * ((double)c->dig_T2);
    var2 = ((((double)adc_t) / 131072.0 - ((double)c->dig_T1) / 8192.0) *
            (((double)adc_t) / 131072.0 - ((double)c->dig_T1) / 8192.0)) * ((double)c->dig_T3);
    t_fine = (int32_t)(var1 + var2);
    T = (var1 + var2) / 5120.0;

    var1 = ((double)t_fine / 2.0) - 64000.0;
    var2 = var1 * var1 * ((double)c->dig_P6) / 32768.0;
    var2 = var2 + var1 * ((double)c->dig_P5) * 2.0;
    var2 = (var2 / 4.0) + (((double)c->dig_P4) * 65536.0);
    var1 = (((double)c->dig_P3) * var1 * var1 / 524288.0 + ((double)c->dig_P2) * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0) * ((double)c->dig_P1);
    if (var1 == 0.0) {
        P = 0;
    } else {
        P = 1048576.0 - (double)adc_p;
        P = (P - (var2 / 4096.0)) * 6250.0 / var1;
        var1 = ((double)c->dig_P9) * P * P / 2147483648.0;
        var2 = P * ((double)c->dig_P8) / 32768.0;
        P = P + (var1 + var2 + ((double)c->dig_P7)) / 16.0;
    }

    *temperatura_c = (float)T;
    *pressao_pa = (float)P;
    return true;
}
