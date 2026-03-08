#ifndef APP_SENSORS_SHT3X_H
#define APP_SENSORS_SHT3X_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"
#include "cmsis_os.h"

typedef struct
{
    float humidity_percent;
    float temperature_c;
    uint32_t sample_tick;
} sht3x_sample_t;

typedef struct
{
    I2C_HandleTypeDef *hi2c;
    uint16_t device_address;
    bool initialized;
    HAL_StatusTypeDef last_error;
} sht3x_t;

bool sht3x_init(sht3x_t *sensor,
                I2C_HandleTypeDef *hi2c,
                uint16_t device_address);

bool sht3x_read(sht3x_t *sensor,
                sht3x_sample_t *sample);

HAL_StatusTypeDef sht3x_read_raw(sht3x_t *sensor,
                                 uint8_t *buffer,
                                 uint16_t size,
                                 uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
