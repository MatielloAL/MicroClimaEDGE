#include <Arduino.h>
#include "controle.h"
#include "sensores.h"

#define PINO_RELE_BOMBA          14    // ativo em LOW - unico rele disponivel; cooler continua simulado
#define PINO_LED_R               26    // LED vermelho (so esse canal e usado; G/B do modulo RGB nao funcionam)
#define PINO_BUZZER              4     // ativo em HIGH (confirmado em teste de bancada)

#define LIMIAR_MARGEM_COOLER     0.5f  // liga se interno - externo >= isso
#define LIMIAR_ABSOLUTO_COOLER   35.0f // liga sempre acima disso, mesmo com externo tambem quente
#define HISTERESE_MARGEM         0.2f  // desliga se margem cair abaixo disso
#define HISTERESE_ABSOLUTA       33.0f // e o absoluto tambem cair abaixo disso
#define BOMBA_INTERVALO_MS       (20UL * 1000UL)                // rega automatica a cada 20s
#define BOMBA_DURACAO_MS         (10UL * 1000UL)                // dura 10s
#define BOMBA_MANUAL_EXPIRA_MS   (60UL * 1000UL)                // override manual expira sozinho apos 60s

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

// So o vermelho: acende quando o solo esta seco.
static void atualizar_led(bool solo_seco) {
  digitalWrite(PINO_LED_R, solo_seco ? HIGH : LOW);
}

void vTaskControl(void *pvParameters) {
  (void) pvParameters;

  pinMode(PINO_RELE_BOMBA, OUTPUT);
  digitalWrite(PINO_RELE_BOMBA, HIGH); // repouso (desligado, rele ativo em LOW)

  pinMode(PINO_LED_R, OUTPUT);
  pinMode(PINO_BUZZER, OUTPUT);
  digitalWrite(PINO_BUZZER, LOW); // repouso (desligado, pois e ativo em HIGH)

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

    if (xSemaphoreTake(g_mutex_atuadores, pdMS_TO_TICKS(100)) == pdTRUE) {
      g_atuadores.cooler_on = cooler_novo;
      g_atuadores.bomba_on = bomba_final;
      xSemaphoreGive(g_mutex_atuadores);
    }

    // cooler continua simulado (sem rele fisico ainda) - estado so aparece no LED/OLED/ThingsBoard
    digitalWrite(PINO_RELE_BOMBA, bomba_final ? LOW : HIGH);
    atualizar_led(leitura.solo_seco);
    // bipa (500ms on / 500ms off) em vez de tocar continuo - menos incomodo.
    // Fica mudo enquanto o rele da bomba estiver ligado, pra nao abafar o clique dele.
    bool buzzer_pisca = ((agora / 500) % 2) == 0;
    bool buzzer_final = leitura.solo_seco && buzzer_pisca && !bomba_final;
    digitalWrite(PINO_BUZZER, buzzer_final ? HIGH : LOW);

    static bool bomba_final_anterior = false;
    if (bomba_final != bomba_final_anterior) {
      Serial.printf("[Controle] Bomba (rele) %s (manual_override=%s)\n",
                     bomba_final ? "LIGOU" : "DESLIGOU",
                     manual_override ? "true" : "false");
      bomba_final_anterior = bomba_final;
    }

    static bool cooler_final_anterior = false;
    if (cooler_novo != cooler_final_anterior) {
      Serial.printf("[Controle] Cooler (rele) %s\n", cooler_novo ? "LIGOU" : "DESLIGOU");
      cooler_final_anterior = cooler_novo;
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}
