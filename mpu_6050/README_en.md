| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-H21 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | --------- | -------- | -------- | -------- |

# MPU6050 — 200 Hz with FreeRTOS Dual-Core

*🇧🇷 [Leia em Português](README.md)*

## Overview

This example demonstrates reading the MPU6050 inertial sensor at **200 Hz** using the ESP-IDF I2C master API, with a producer/consumer architecture across the two ESP32 cores:

- **Core 0 — producer task:** reads the MPU6050 registers every 5 ms (200 Hz), triggered by a periodic `esp_timer` via binary semaphore, and pushes each sample into a FreeRTOS queue.
- **Core 1 — consumer task:** dequeues samples and sends them in CSV format over UART0 (USB-Serial).

### CSV Output

```
timestamp_us,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,temp_c,dropped
```

| Field | Description |
|-------|-------------|
| `timestamp_us` | Timestamp in microseconds (`esp_timer_get_time()`) |
| `ax_g / ay_g / az_g` | Acceleration on X, Y, Z axes in **g** (full scale ±2 g) |
| `gx_dps / gy_dps / gz_dps` | Angular velocity in **°/s** (full scale ±250 °/s) |
| `temp_c` | Sensor internal temperature in **°C** |
| `dropped` | Accumulated dropped samples (queue full) |

## Hardware Required

- Espressif development board with ESP32 (dual-core)
- MPU6050 sensor connected via I2C

### Pin Assignment

![Diagram_EP32_MPU6050](docs/img/esp-mpu-diagram.svg)

| | SDA | SCL |
|---|---|---|
| ESP32 I2C Master | `I2C_MASTER_SDA` | `I2C_MASTER_SCL` |
| MPU6050 | SDA | SCL |

The default values for `I2C_MASTER_SDA`, `I2C_MASTER_SCL`, and `I2C_MASTER_FREQUENCY` are configurable via `menuconfig` → **Example Configuration**.

> **Note:** External pull-ups are not required — the driver enables the ESP32 internal pull-ups.

## MPU6050 Configuration

| Parameter | Value | Register |
|-----------|-------|----------|
| Sampling rate | 200 Hz | `SMPLRT_DIV = 0x04` |
| DLPF | 188 Hz BW (1 kHz output rate) | `CONFIG = 0x01` |
| Accel full scale | ±2 g → 16384 LSB/g | `ACCEL_CONFIG = 0x00` |
| Gyro full scale | ±250 °/s → 131 LSB/(°/s) | `GYRO_CONFIG = 0x00` |

## Build and Flash

```bash
idf.py -p PORT flash monitor
```

(To exit the serial monitor, press `Ctrl-]`.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure ESP-IDF.

## Example Output

```
I (328) mpu6050: WHO_AM_I = 0x68 (expected 0x68)
I (428) mpu6050: MPU6050 configured at 200 Hz
I (428) mpu6050: 200 Hz timer started
timestamp_us,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,temp_c,dropped
428312,0.0012,-0.0034,1.0021,0.0153,-0.0076,0.0023,28.45,0
433318,0.0011,-0.0035,1.0019,0.0153,-0.0076,0.0023,28.45,0
...
```

## Troubleshooting

- **WHO_AM_I different from 0x68:** check SDA/SCL wiring and the I2C address (AD0 pin low = `0x68`, high = `0x69`).
- **Increasing `dropped` count:** the consumer task cannot drain the queue fast enough — increase the queue size (`FIFO_SIZE`) or reduce the sampling rate.
- **Failed readings:** check sensor power supply (3.3 V) and the I2C frequency configured in `menuconfig`.
