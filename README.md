# ESP32 + MPU6050 — Estudo para Sistema Inercial

Projeto de estudo do sensor inercial MPU6050 com ESP32, voltado à compreensão do seu funcionamento e à coleta de dados para futura implementação em um **sistema de navegação inercial (INS)**.

## Objetivo

Antes de integrar o MPU6050 em um sistema inercial real, este projeto investiga:

- O comportamento do sensor em condições controladas (bias, ruído, deriva do giroscópio)
- A taxa de amostragem máxima confiável via I2C
- A integridade dos dados em uma arquitetura produtora/consumidora com FreeRTOS
- O pipeline completo: leitura no ESP32 → transmissão serial → captura e armazenamento no PC

## Estrutura do projeto

```
ESP-MPU6050/
├── mpu_6050/          # Firmware ESP32 (ESP-IDF)
│   └── main/
│       └── i2c_basic_example_main.c
└── serial-read/       # Script Python de captura serial
    ├── main.py
    └── requirements.txt
```

## Sobre o MPU6050

O MPU6050 é uma IMU (Inertial Measurement Unit) de 6 DoF fabricada pela InvenSense, combinando:

- **Acelerômetro triaxial** — fundo de escala configurável (±2 / ±4 / ±8 / ±16 g)
- **Giroscópio triaxial** — fundo de escala configurável (±250 / ±500 / ±1000 / ±2000 °/s)
- **Sensor de temperatura** interno
- Interface **I2C** (até 400 kHz) ou SPI
- **DMP** (Digital Motion Processor) interno para fusão de sensores por hardware

Neste projeto, o MPU6050 é configurado em:

| Parâmetro | Valor |
|-----------|-------|
| Taxa de amostragem | **200 Hz** |
| Filtro passa-baixa (DLPF) | 188 Hz BW |
| Fundo de escala Accel | ±2 g (16 384 LSB/g) |
| Fundo de escala Gyro | ±250 °/s (131 LSB/°/s) |

## Arquitetura do sistema

```
┌─────────────────────────────────────┐        ┌──────────────────────┐
│              ESP32                  │  UART  │         PC           │
│                                     │ ──────>│                      │
│  Core 0          Core 1             │        │  serial-read/main.py │
│  ┌──────────┐   ┌───────────────┐  │        │                      │
│  │ Produtora│   │ Consumidora   │  │        │  ┌────────────────┐  │
│  │ 200 Hz   │──>│ CSV via UART0 │  │        │  │dados_mpu6050   │  │
│  │(esp_timer│   │               │  │        │  │    .csv        │  │
│  │+ I2C)    │   │               │  │        │  └────────────────┘  │
│  └──────────┘   └───────────────┘  │        └──────────────────────┘
│         ↑                           │
│   ┌─────────┐                       │
│   │ MPU6050 │ (I2C)                 │
│   └─────────┘                       │
└─────────────────────────────────────┘
```

- **Core 0** lê o MPU6050 a cada 5 ms (200 Hz), disparado por `esp_timer`, e empurra cada amostra em uma fila FreeRTOS.
- **Core 1** consome a fila e transmite os dados em CSV pela UART0 (USB-Serial).
- O script Python no PC recebe a serial, filtra logs e salva tudo em `.csv`.

## Formato dos dados coletados

```
timestamp_us,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,temp_c,dropped
428312,0.0012,-0.0034,1.0021,0.0153,-0.0076,0.0023,28.45,0
433318,0.0011,-0.0035,1.0019,0.0153,-0.0076,0.0023,28.45,0
```

| Campo | Unidade | Descrição |
|-------|---------|-----------|
| `timestamp_us` | µs | Tempo desde o boot (`esp_timer_get_time()`) |
| `ax_g` / `ay_g` / `az_g` | g | Aceleração nos eixos X, Y, Z |
| `gx_dps` / `gy_dps` / `gz_dps` | °/s | Velocidade angular nos eixos X, Y, Z |
| `temp_c` | °C | Temperatura interna do sensor |
| `dropped` | — | Amostras descartadas (fila cheia) |

## Como usar

### 1. Firmware (ESP32)

```bash
cd mpu_6050
idf.py -p /dev/ttyUSB0 flash monitor
```

Consulte [mpu_6050/README.md](mpu_6050/README.md) para detalhes de configuração de pinos e menuconfig.

### 2. Captura serial (PC)

```bash
cd serial-read
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python main.py
```

Consulte [serial-read/README.md](serial-read/README.md) para ajustar porta serial e arquivo de saída.

## Conexão de hardware

| ESP32 | MPU6050 |
|-------|---------|
| SDA (configurável via menuconfig) | SDA |
| SCL (configurável via menuconfig) | SCL |
| 3,3 V | VCC |
| GND | GND |

> Pull-ups externos não são necessários — o driver habilita os pull-ups internos do ESP32.

## Próximos passos (rumo ao INS)

- [ ] Caracterizar o **bias** e o **ruído** do acelerômetro e do giroscópio em estático
- [ ] Estimar a **deriva do giroscópio** (gyro drift) ao longo do tempo
- [ ] Implementar **calibração** (zero-rate offset e sensibilidade)
- [ ] Aplicar **filtro complementar** ou **filtro de Kalman** para fusão Accel+Gyro
- [ ] Integrar acelerações para estimativa de velocidade e posição (dead reckoning)
- [ ] Avaliar uso do **DMP** interno do MPU6050 para quaternions em tempo real
