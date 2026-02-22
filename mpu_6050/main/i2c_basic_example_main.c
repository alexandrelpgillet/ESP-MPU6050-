/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/*
 * MPU6050 — 200 Hz com fila FreeRTOS entre núcleos
 *
 * Core 0 → tarefa produtora: lê o sensor a 200 Hz via vTaskDelayUntil
 *           e empurra amostras na fila FIFO.
 * Core 1 → tarefa consumidora: retira amostras da fila e envia
 *           em formato CSV pela UART0 (USB).
 *
 * Saída CSV:
 *   timestamp_us,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,temp_c
 */
#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/i2c_master.h"

static const char *TAG = "mpu6050";

/* ------------------------------------------------------------------ */
/*  I2C                                                                */
/* ------------------------------------------------------------------ */
#define I2C_MASTER_SCL_IO           CONFIG_I2C_MASTER_SCL
#define I2C_MASTER_SDA_IO           CONFIG_I2C_MASTER_SDA
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          CONFIG_I2C_MASTER_FREQUENCY
#define I2C_MASTER_TIMEOUT_MS       1000

/* ------------------------------------------------------------------ */
/*  MPU6050 — endereço e registradores                                 */
/* ------------------------------------------------------------------ */
#define MPU6050_SENSOR_ADDR         0x68

#define MPU6050_REG_SMPLRT_DIV     0x19
#define MPU6050_REG_CONFIG         0x1A
#define MPU6050_REG_GYRO_CONFIG    0x1B
#define MPU6050_REG_ACCEL_CONFIG   0x1C
#define MPU6050_REG_ACCEL_XOUT_H   0x3B  /* burst: ACCEL(6)+TEMP(2)+GYRO(6) */
#define MPU6050_REG_PWR_MGMT_1     0x6B
#define MPU6050_REG_WHO_AM_I       0x75

/* Fundo de escala */
#define MPU6050_ACCEL_FS_2G        0x00   /* ±2 g     → 16384 LSB/g      */
#define MPU6050_GYRO_FS_250DPS     0x00   /* ±250 °/s → 131  LSB/(°/s)   */
#define ACCEL_SENSITIVITY          16384.0f
#define GYRO_SENSITIVITY           131.0f

/* ------------------------------------------------------------------ */
/*  Fila FreeRTOS e semáforo de temporização                          */
/* ------------------------------------------------------------------ */
#define FIFO_SIZE        40       /* 200ms de buffer @ 200 Hz */
#define SAMPLE_PERIOD_US 5000     /* 5 ms = 200 Hz            */

typedef struct {
    int16_t accel_x, accel_y, accel_z;
    int16_t gyro_x,  gyro_y,  gyro_z;
    int16_t temp;
    int64_t timestamp_us;
} mpu6050_data_t;

static QueueHandle_t     sensor_queue;
static SemaphoreHandle_t sample_sem;
static volatile uint32_t dropped_samples = 0;

/* ------------------------------------------------------------------ */
/*  Primitivas I2C                                                     */
/* ------------------------------------------------------------------ */
static esp_err_t mpu6050_register_read(i2c_master_dev_handle_t dev,
                                       uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_transmit_receive(dev, &reg, 1, buf, len,
                                       I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

static esp_err_t mpu6050_register_write(i2c_master_dev_handle_t dev,
                                        uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = {reg, val};
    return i2c_master_transmit(dev, tx, sizeof(tx),
                               I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

/* ------------------------------------------------------------------ */
/*  Inicialização I2C                                                  */
/* ------------------------------------------------------------------ */
static void i2c_master_init(i2c_master_bus_handle_t *bus,
                            i2c_master_dev_handle_t *dev)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port            = I2C_MASTER_NUM,
        .sda_io_num          = I2C_MASTER_SDA_IO,
        .scl_io_num          = I2C_MASTER_SCL_IO,
        .clk_source          = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt   = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, bus));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = MPU6050_SENSOR_ADDR,
        .scl_speed_hz    = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus, &dev_cfg, dev));
}

/* ------------------------------------------------------------------ */
/*  Inicialização do MPU6050 para 200 Hz                               */
/*                                                                     */
/*  DLPF_CFG = 1 → Gyro output rate = 1 kHz, bw ≈ 188 Hz             */
/*  SMPLRT_DIV = 4 → 1000 / (1+4) = 200 Hz                           */
/* ------------------------------------------------------------------ */
static void mpu6050_init(i2c_master_dev_handle_t dev)
{
    uint8_t who;
    ESP_ERROR_CHECK(mpu6050_register_read(dev, MPU6050_REG_WHO_AM_I, &who, 1));
    ESP_LOGI(TAG, "WHO_AM_I = 0x%02X (esperado 0x68)", who);

    /* Acorda o sensor */
    ESP_ERROR_CHECK(mpu6050_register_write(dev, MPU6050_REG_PWR_MGMT_1,   0x00));
    vTaskDelay(pdMS_TO_TICKS(100));

    /* 200 Hz */
    ESP_ERROR_CHECK(mpu6050_register_write(dev, MPU6050_REG_CONFIG,        0x01));
    ESP_ERROR_CHECK(mpu6050_register_write(dev, MPU6050_REG_SMPLRT_DIV,   0x04));

    /* Fundo de escala */
    ESP_ERROR_CHECK(mpu6050_register_write(dev, MPU6050_REG_GYRO_CONFIG,  MPU6050_GYRO_FS_250DPS));
    ESP_ERROR_CHECK(mpu6050_register_write(dev, MPU6050_REG_ACCEL_CONFIG, MPU6050_ACCEL_FS_2G));

    ESP_LOGI(TAG, "MPU6050 configurado a 200 Hz");
}

/* ------------------------------------------------------------------ */
/*  Callback do esp_timer — executado a cada 5 ms (200 Hz)            */
/*  Apenas libera o semáforo; a leitura I2C fica na tarefa.           */
/* ------------------------------------------------------------------ */
static void sample_timer_cb(void *arg)
{
    xSemaphoreGive(sample_sem);
}

/* ------------------------------------------------------------------ */
/*  Tarefa Produtora — Core 0                                          */
/*  Lê o sensor a 200 Hz e envia amostras para a fila.                */
/* ------------------------------------------------------------------ */
static void producer_task(void *arg)
{
    i2c_master_dev_handle_t dev = (i2c_master_dev_handle_t)arg;
    uint8_t raw[14];

    while (1) {
        /* Aguarda o tick de 5 ms do esp_timer (resolução em µs) */
        xSemaphoreTake(sample_sem, portMAX_DELAY);

        esp_err_t ret = mpu6050_register_read(dev, MPU6050_REG_ACCEL_XOUT_H,
                                               raw, sizeof(raw));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Leitura falhou: %s", esp_err_to_name(ret));
            continue;
        }

        mpu6050_data_t sample = {
            .accel_x      = (int16_t)((raw[0]  << 8) | raw[1]),
            .accel_y      = (int16_t)((raw[2]  << 8) | raw[3]),
            .accel_z      = (int16_t)((raw[4]  << 8) | raw[5]),
            .temp         = (int16_t)((raw[6]  << 8) | raw[7]),
            .gyro_x       = (int16_t)((raw[8]  << 8) | raw[9]),
            .gyro_y       = (int16_t)((raw[10] << 8) | raw[11]),
            .gyro_z       = (int16_t)((raw[12] << 8) | raw[13]),
            .timestamp_us = esp_timer_get_time(),
        };

        if (xQueueSend(sensor_queue, &sample, 0) != pdTRUE) {
            dropped_samples++;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Tarefa Consumidora — Core 1                                        */
/*  Retira amostras da fila, converte e envia CSV pela UART.           */
/* ------------------------------------------------------------------ */
static void consumer_task(void *arg)
{
    mpu6050_data_t sample;

    /* Cabeçalho CSV */
    printf("timestamp_us,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,temp_c,dropped\n");

    while (1) {
        if (xQueueReceive(sensor_queue, &sample, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        float ax = sample.accel_x / ACCEL_SENSITIVITY;
        float ay = sample.accel_y / ACCEL_SENSITIVITY;
        float az = sample.accel_z / ACCEL_SENSITIVITY;
        float gx = sample.gyro_x  / GYRO_SENSITIVITY;
        float gy = sample.gyro_y  / GYRO_SENSITIVITY;
        float gz = sample.gyro_z  / GYRO_SENSITIVITY;
        float tc = (sample.temp / 340.0f) + 36.53f;

        /* dropped: amostras perdidas acumuladas desde o início */
        printf("%lld,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.2f,%lu\n",
               sample.timestamp_us, ax, ay, az, gx, gy, gz, tc,
               (unsigned long)dropped_samples);
    }
}

/* ------------------------------------------------------------------ */
/*  app_main                                                           */
/* ------------------------------------------------------------------ */
void app_main(void)
{
    /* Desabilita o buffer do stdout para que printf envie imediatamente pela UART */
    setvbuf(stdout, NULL, _IONBF, 0);

    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t dev;

    i2c_master_init(&bus, &dev);
    ESP_LOGI(TAG, "I2C inicializado");

    mpu6050_init(dev);

    sensor_queue = xQueueCreate(FIFO_SIZE, sizeof(mpu6050_data_t));
    assert(sensor_queue != NULL);

    sample_sem = xSemaphoreCreateBinary();
    assert(sample_sem != NULL);

    /* Produtor — Core 0, prioridade alta */
    xTaskCreatePinnedToCore(producer_task, "mpu_producer", 4096,
                            dev, 5, NULL, 0);

    /* Consumidor — Core 1, prioridade normal */
    xTaskCreatePinnedToCore(consumer_task, "mpu_consumer", 4096,
                            NULL, 4, NULL, 1);

    /* Timer periódico a 200 Hz (5000 µs) para disparar leituras */
    esp_timer_handle_t sample_timer;
    const esp_timer_create_args_t timer_args = {
        .callback        = sample_timer_cb,
        .name            = "mpu_sample",
        .dispatch_method = ESP_TIMER_TASK,  /* callback em contexto de task */
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &sample_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(sample_timer, SAMPLE_PERIOD_US));
    ESP_LOGI(TAG, "Timer 200 Hz iniciado");
}
