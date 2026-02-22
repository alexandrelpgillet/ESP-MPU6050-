# serial-read

*🇺🇸 [Read in English](README_en.md)*

Script Python para capturar dados do MPU6050 via porta serial (USB) e salvá-los em CSV.

## Visão geral

O ESP32 envia leituras do MPU6050 pela UART em formato CSV. Este script lê essa saída, filtra mensagens de log do ESP-IDF e grava apenas os dados de sensor em um arquivo `.csv`.

## Pré-requisitos

- Python 3.7+
- ESP32 conectado via USB (padrão: `/dev/ttyUSB0`)

## Instalação

```bash
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

## Uso

```bash
python main.py
```

Pressione `Ctrl+C` para encerrar a captura.

## Configuração

Edite as constantes no topo de [main.py](main.py) conforme necessário:

| Constante | Padrão              | Descrição                        |
|-----------|---------------------|----------------------------------|
| `PORT`    | `/dev/ttyUSB0`      | Porta serial do ESP32            |
| `BAUD`    | `115200`            | Taxa de transmissão (baud rate)  |
| `OUTPUT`  | `dados_mpu6050.csv` | Arquivo de saída                 |

## Formato dos dados

O arquivo CSV gerado contém as seguintes colunas:

| Coluna         | Unidade | Descrição                              |
|----------------|---------|----------------------------------------|
| `timestamp_us` | µs      | Timestamp desde o boot do ESP32        |
| `ax_g`         | g       | Aceleração eixo X                      |
| `ay_g`         | g       | Aceleração eixo Y                      |
| `az_g`         | g       | Aceleração eixo Z                      |
| `gx_dps`       | °/s     | Velocidade angular eixo X              |
| `gy_dps`       | °/s     | Velocidade angular eixo Y              |
| `gz_dps`       | °/s     | Velocidade angular eixo Z              |
| `temp_c`       | °C      | Temperatura interna do MPU6050         |
| `dropped`      | —       | Pacotes descartados pelo FIFO do ESP32 |

## Saída no terminal

Durante a captura, cada leitura é exibida no formato:

```
[    12.345s] A: +0.012 -0.003 +0.998 g | G: +0.061 -0.030 +0.015 °/s | 27.3°C drop=0
```

Mensagens de log do ESP-IDF (prefixos `I`, `W`, `E`, `D`) são exibidas com o tag `[LOG]` e não são gravadas no CSV.
