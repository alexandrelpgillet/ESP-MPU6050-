# serial-read

*🇧🇷 [Leia em Português](README.md)*

Python script to capture MPU6050 data via serial port (USB) and save it to CSV.

## Overview

The ESP32 sends MPU6050 readings over UART in CSV format. This script reads that output, filters ESP-IDF log messages, and writes only the sensor data to a `.csv` file.

## Prerequisites

- Python 3.7+
- ESP32 connected via USB (default: `/dev/ttyUSB0`)

## Installation

```bash
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

## Usage

```bash
python main.py
```

Press `Ctrl+C` to stop the capture.

## Configuration

Edit the constants at the top of [main.py](main.py) as needed:

| Constant | Default             | Description                      |
|----------|---------------------|----------------------------------|
| `PORT`   | `/dev/ttyUSB0`      | ESP32 serial port                |
| `BAUD`   | `115200`            | Baud rate                        |
| `OUTPUT` | `dados_mpu6050.csv` | Output file                      |

## Data Format

The generated CSV file contains the following columns:

| Column         | Unit    | Description                            |
|----------------|---------|----------------------------------------|
| `timestamp_us` | µs      | Timestamp since ESP32 boot             |
| `ax_g`         | g       | Acceleration on X axis                 |
| `ay_g`         | g       | Acceleration on Y axis                 |
| `az_g`         | g       | Acceleration on Z axis                 |
| `gx_dps`       | °/s     | Angular velocity on X axis             |
| `gy_dps`       | °/s     | Angular velocity on Y axis             |
| `gz_dps`       | °/s     | Angular velocity on Z axis             |
| `temp_c`       | °C      | MPU6050 internal temperature           |
| `dropped`      | —       | Packets dropped by ESP32 FIFO          |

## Terminal Output

During capture, each reading is displayed in the format:

```
[    12.345s] A: +0.012 -0.003 +0.998 g | G: +0.061 -0.030 +0.015 °/s | 27.3°C drop=0
```

ESP-IDF log messages (prefixes `I`, `W`, `E`, `D`) are displayed with the `[LOG]` tag and are not written to the CSV.
