| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-H21 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | --------- | -------- | -------- | -------- |

# MPU6050 — 200 Hz com FreeRTOS dual-core

*🇺🇸 [Read in English](README_en.md)*

## Overview

Este exemplo demonstra a leitura do sensor inercial MPU6050 a **200 Hz** usando a API de I2C master do ESP-IDF, com uma arquitetura produtora/consumidora entre os dois núcleos do ESP32:

- **Core 0 — tarefa produtora:** lê os registradores do MPU6050 a cada 5 ms (200 Hz), disparada por um `esp_timer` periódico via semáforo binário, e empurra cada amostra numa fila FreeRTOS.
- **Core 1 — tarefa consumidora:** retira amostras da fila e as envia em formato CSV pela UART0 (USB-Serial).

### Saída CSV

```
timestamp_us,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,temp_c,dropped
```

| Campo | Descrição |
|-------|-----------|
| `timestamp_us` | Timestamp em microssegundos (`esp_timer_get_time()`) |
| `ax_g / ay_g / az_g` | Aceleração nos eixos X, Y, Z em **g** (fundo de escala ±2 g) |
| `gx_dps / gy_dps / gz_dps` | Velocidade angular em **°/s** (fundo de escala ±250 °/s) |
| `temp_c` | Temperatura interna do sensor em **°C** |
| `dropped` | Amostras descartadas acumuladas (fila cheia) |

## Hardware Required

- Placa de desenvolvimento Espressif com ESP32 (dual-core)
- Sensor MPU6050 conectado via I2C

### Pin Assignment

![Diagram_EP32_MPU6050](docs/img/esp-mpu-diagram.svg)

| | SDA | SCL |
|---|---|---|
| ESP32 I2C Master | `I2C_MASTER_SDA` | `I2C_MASTER_SCL` |
| MPU6050 | SDA | SCL |

Os valores padrão de `I2C_MASTER_SDA`, `I2C_MASTER_SCL` e `I2C_MASTER_FREQUENCY` são configuráveis via `menuconfig` → **Example Configuration**.

> **Nota:** Pull-ups externos não são necessários — o driver habilita os pull-ups internos do ESP32.

## Configuração do MPU6050

| Parâmetro | Valor | Registrador |
|-----------|-------|-------------|
| Taxa de amostragem | 200 Hz | `SMPLRT_DIV = 0x04` |
| DLPF | 188 Hz BW (1 kHz output rate) | `CONFIG = 0x01` |
| Fundo de escala Accel | ±2 g → 16384 LSB/g | `ACCEL_CONFIG = 0x00` |
| Fundo de escala Gyro | ±250 °/s → 131 LSB/(°/s) | `GYRO_CONFIG = 0x00` |

## Build and Flash

```bash
idf.py -p PORT flash monitor
```

(Para sair do monitor serial, pressione `Ctrl-]`.)

Consulte o [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) para os passos completos de configuração do ESP-IDF.

## Example Output

```
I (328) mpu6050: WHO_AM_I = 0x68 (esperado 0x68)
I (428) mpu6050: MPU6050 configurado a 200 Hz
I (428) mpu6050: Timer 200 Hz iniciado
timestamp_us,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,temp_c,dropped
428312,0.0012,-0.0034,1.0021,0.0153,-0.0076,0.0023,28.45,0
433318,0.0011,-0.0035,1.0019,0.0153,-0.0076,0.0023,28.45,0
...
```

## Troubleshooting

- **WHO_AM_I diferente de 0x68:** verifique a fiação SDA/SCL e o endereço I2C (pino AD0 em nível baixo = `0x68`, nível alto = `0x69`).
- **`dropped` crescente:** a tarefa consumidora não consegue esvaziar a fila a tempo — aumente o tamanho da fila (`FIFO_SIZE`) ou reduza a taxa de amostragem.
- **Leituras falhando:** verifique a alimentação do sensor (3,3 V) e a frequência I2C configurada em `menuconfig`.
