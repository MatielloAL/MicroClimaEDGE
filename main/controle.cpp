#include <Arduino.h>
#include "controle.h"
#include "sensores.h"

#define PINO_BUZZER              4     // ativo em HIGH (fio fisico esta no GPIO4/D4)
#define PINO_LED_CHUVA           26    // canal R do modulo RGB (confirmado em teste de bancada)
#define LIMIAR_MARGEM_COOLER     0.5f  // liga se interno - externo >= isso
#define LIMIAR_ABSOLUTO_COOLER   35.0f // liga sempre acima disso, mesmo com externo tambem quente
#define HISTERESE_MARGEM         0.2f  // desliga se margem cair abaixo disso
#define HISTERESE_ABSOLUTA       33.0f // e o absoluto tambem cair abaixo disso
#define BOMBA_INTERVALO_MS       (15UL * 1000UL)                // TESTE: 15s (restaurar p/ 2min depois)
#define BOMBA_DURACAO_MS         (5UL * 1000UL)                // dura 5s
#define BOMBA_MANUAL_EXPIRA_MS   (60UL * 1000UL)               // override manual expira sozinho apos 60s

ActuatorState g_atuadores = {0};
SemaphoreHandle_t g_mutex_atuadores = NULL;

static bool decidir_cooler(bool cooler_atual, float t_int, float t_ext) {
  float diff = t_int - t_ext;
  if (!cooler_atual) {
    return (diff >= LIMIAR_MARGEM_COOLER) || (t_int >= LIMIAR_ABSOLUTO_COOLER);
  }
  bool deve_desligar = (diff < HISTERESE_MARGEM) && (t_int < HISTERESE_ABSOLUTA);
  return !deve_desligar;
}

void vTaskControl(void *pvParameters) {
  (void) pvParameters;

  pinMode(PINO_BUZZER, OUTPUT);
  digitalWrite(PINO_BUZZER, LOW); // repouso (desligado, pois e ativo em HIGH)
  pinMode(PINO_LED_CHUVA, OUTPUT);
  digitalWrite(PINO_LED_CHUVA, LOW); // repouso (desligado)

  unsigned long ultima_rega = millis();
  bool bomba_automatica_ativa = false;
  unsigned long inicio_rega = 0;

  for (;;) {
    SensorData leitura = {0};
    if (xSemaphoreTake(g_mutex_sensores, pdMS_TO_TICKS(100)) == pdTRUE) {
      leitura = g_sensores;
      xSemaphoreGive(g_mutex_sensores);
    }

    bool cooler_atual = false;
    bool manual_override = false;
    bool manual_valor = false;
    unsigned long manual_timestamp = 0;
    if (xSemaphoreTake(g_mutex_atuadores, pdMS_TO_TICKS(100)) == pdTRUE) {
      cooler_atual = g_atuadores.cooler_on;
      manual_override = g_atuadores.bomba_manual_override;
      manual_valor = g_atuadores.bomba_manual_valor;
      manual_timestamp = g_atuadores.bomba_manual_timestamp_ms;
      xSemaphoreGive(g_mutex_atuadores);
    }

    bool cooler_novo = decidir_cooler(cooler_atual, leitura.temp_int, leitura.temp_ext);

    unsigned long agora = millis();

    // override manual expira sozinho, senao um comando antigo do ThingsBoard
    // trava a bomba pra sempre fora do ciclo automatico.
    if (manual_override && (agora - manual_timestamp >= BOMBA_MANUAL_EXPIRA_MS)) {
      manual_override = false;
      if (xSemaphoreTake(g_mutex_atuadores, pdMS_TO_TICKS(100)) == pdTRUE) {
        g_atuadores.bomba_manual_override = false;
        xSemaphoreGive(g_mutex_atuadores);
      }
      Serial.println("[Controle] Override manual da bomba expirou, voltando ao automatico.");
    }

    if (!bomba_automatica_ativa && (agora - ultima_rega >= BOMBA_INTERVALO_MS)) {
      bomba_automatica_ativa = true;
      inicio_rega = agora;
      ultima_rega = agora;
    }
    if (bomba_automatica_ativa && (agora - inicio_rega >= BOMBA_DURACAO_MS)) {
      bomba_automatica_ativa = false;
    }

    bool bomba_final = manual_override ? manual_valor : bomba_automatica_ativa;

    bool buzzer_final = leitura.chuva || bomba_final; // toca na chuva OU enquanto a bomba rega

    if (xSemaphoreTake(g_mutex_atuadores, pdMS_TO_TICKS(100)) == pdTRUE) {
      g_atuadores.cooler_on = cooler_novo;
      g_atuadores.bomba_on = bomba_final;
      g_atuadores.buzzer_on = buzzer_final;
      xSemaphoreGive(g_mutex_atuadores);
    }

    digitalWrite(PINO_BUZZER, buzzer_final ? HIGH : LOW); // ativo em HIGH (confirmado em teste de bancada)
    digitalWrite(PINO_LED_CHUVA, leitura.chuva ? HIGH : LOW);

    static bool buzzer_final_anterior = false;
    if (buzzer_final != buzzer_final_anterior) {
      Serial.printf("[Controle] Buzzer %s (chuva=%s, bomba_final=%s) -> digitalWrite(PINO_BUZZER=%d, %s)\n",
                     buzzer_final ? "LIGOU" : "DESLIGOU",
                     leitura.chuva ? "true" : "false",
                     bomba_final ? "true" : "false",
                     PINO_BUZZER,
                     buzzer_final ? "LOW" : "HIGH");
      buzzer_final_anterior = buzzer_final;
    }

    static bool bomba_final_anterior = false;
    if (bomba_final != bomba_final_anterior) {
      Serial.printf("[Controle] Bomba %s (manual_override=%s)\n",
                     bomba_final ? "LIGOU" : "DESLIGOU",
                     manual_override ? "true" : "false");
      bomba_final_anterior = bomba_final;
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}
