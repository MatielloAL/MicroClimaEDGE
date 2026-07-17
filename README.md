# MicroClima EDGE

Firmware para ESP32 (ESP-IDF puro) que monitora o microclima de uma estufa/vaso
e controla automaticamente uma bomba d'água e um cooler, com telemetria em
tempo real para o ThingsBoard via MQTT.

O projeto é inspirado no [NIWA](https://niwa.io), um controlador open source
para cultivo (PLC de baixo custo para monitorar e automatizar estufas). A
MicroClima EDGE segue a mesma ideia — sensoriamento + automação local + painel
remoto — mas com um firmware próprio, mais enxuto, escrito diretamente sobre o
ESP-IDF.

## Visão geral

- Lê temperatura/pressão interna e externa (2x BMP280) e umidade do solo
  (sensor resistivo via ADC).
- Decide localmente se o cooler e a bomba devem ligar — **o controle nunca
  depende de Wi-Fi/MQTT**, só a telemetria depende da rede.
- Mostra o status em um display OLED e em um LED indicador.
- Publica os dados no ThingsBoard e aceita comando manual de bomba via MQTT.

## Hardware / pinagem

| Função              | Pino ESP32 | Observação                                   |
|---------------------|-----------|-----------------------------------------------|
| Relé da bomba        | GPIO 14   | Ativo em LOW. Único relé físico disponível hoje. |
| LED indicador (R)    | GPIO 26   | Só o canal vermelho funciona (G/B do módulo RGB não). |
| Buzzer               | GPIO 4    | Ativo em HIGH.                                 |
| I2C SDA / SCL        | GPIO 21 / 22 | Barramento compartilhado pelos dois BMP280 (0x76 interno, 0x77 externo). |
| Sensor de umidade do solo | GPIO 35 (ADC1 canal 7) | Calibrado em bancada: seco no ar ≈ 4095, submerso em água ≈ 1900. |

## Arquitetura

O firmware roda 4 tasks FreeRTOS independentes (`main/MicroClimaEdge.c`):

- **`vTaskSensors`** — lê os sensores a cada 2s e atualiza `g_sensores`.
- **`vTaskControl`** — a cada 500ms decide o estado de cooler/bomba/LED/buzzer
  e aciona o relé real da bomba.
- **`vTaskDisplay`** — atualiza o OLED com as leituras atuais.
- **`vTaskTelemetry`** — conecta no Wi-Fi/MQTT, publica telemetria a cada 2s e
  escuta comandos de override manual.

O estado compartilhado (`g_sensores` e `g_atuadores`) é protegido por mutex,
então uma falha de rede ou de leitura de sensor não trava as outras tasks.

## Lógica de controle

### Cooler (`decidir_cooler`, em `main/controle.c`)

Liga quando (estando desligado):
- `temperatura interna - temperatura externa >= 0.5°C`, **ou**
- `temperatura interna >= 35°C` (limiar absoluto, mesmo com o ambiente externo
  também quente).

Só desliga quando, ao mesmo tempo:
- a margem cai abaixo de `0.2°C`, **e**
- a temperatura interna cai abaixo de `33°C`.

Essa histerese evita que o cooler fique ligando/desligando repetidamente perto
do limiar.

> **Importante:** o cooler ainda é **simulado** — não existe relé físico
> ligado a ele hoje. O estado calculado aparece no LED, no OLED e no
> ThingsBoard, mas não aciona nenhum atuador real.

### Bomba d'água

- **Automática:** liga por 10s a cada 20s (`BOMBA_INTERVALO_MS` /
  `BOMBA_DURACAO_MS`) — valores de bancada/demo, para facilitar testes; não
  representam um ciclo de rega real.
- **Manual (via ThingsBoard):** um comando `bomba_manual` no MQTT força o
  estado da bomba e expira sozinho após 60s, para não travar o sistema fora
  do automático caso um comando antigo fique pendente.

## Telemetria (ThingsBoard / MQTT)

A cada 2s, se conectado, o dispositivo publica em `v1/devices/me/telemetry`:

```json
{
  "temp_int": 0.0, "pressao_int": 0.0,
  "temp_ext": 0.0, "pressao_ext": 0.0,
  "umidade_solo_pct": 0.0, "solo_seco": false,
  "cooler_on": false, "bomba_on": false
}
```

E escuta o atributo `bomba_manual` (`true`/`false`) em
`v1/devices/me/attributes` para o override manual da bomba.

> As credenciais de Wi-Fi e o token MQTT hoje estão fixos em
> `main/telemetria.c`. Antes de publicar/compartilhar este repositório,
> mova-os para `Kconfig`/`sdkconfig` locais (fora do controle de versão).

## Build e flash

Requer [ESP-IDF](https://docs.espressif.com/projects/esp-idf/) >= 5.0.

```bash
idf.py set-target esp32
idf.py build
idf.py -p <porta_serial> flash monitor
```

## Melhorias futuras

- **Bomba automática por umidade crítica do solo:** hoje a rega automática
  segue só um temporizador fixo (liga por 10s a cada 20s). A próxima
  evolução é a bomba ligar automaticamente sempre que o solo estiver **muito
  seco** (abaixo de um limiar crítico de `umidade_solo_pct`/`solo_seco`),
  independente do ciclo por tempo, para manter a planta viva mesmo se o
  temporizador ainda não tiver disparado.
- Substituir o cooler simulado por um relé físico.
- Mover credenciais de Wi-Fi/MQTT para configuração fora do código-fonte.
- Tornar os limiares de cooler, bomba e solo configuráveis via ThingsBoard
  (atributos compartilhados), em vez de constantes fixas no firmware.
- Recalibração periódica/adaptativa do sensor de umidade do solo.
