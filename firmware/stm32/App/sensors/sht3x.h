#ifndef APP_SENSORS_SHT3X_H
#define APP_SENSORS_SHT3X_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_i2c.h"
#include "cmsis_os.h"

typedef struct
{
    float humidity_percent;
    float temperature_c;
    uint32_t sample_tick;
} sht3x_sample_t;

typedef enum
{
    SHT3X_RESULT_OK = 0,
    SHT3X_RESULT_INVALID_ARGUMENT,
    SHT3X_RESULT_NOT_INITIALIZED,
    SHT3X_RESULT_I2C_ERROR,
    SHT3X_RESULT_CRC_ERROR
} sht3x_result_t;

typedef struct
{
    I2C_HandleTypeDef *hi2c;
    uint16_t device_address;
    bool initialized;
    HAL_StatusTypeDef last_error;
    uint32_t last_i2c_error;
    sht3x_result_t last_result;
} sht3x_t;

bool sht3x_init(sht3x_t *sensor,
                I2C_HandleTypeDef *hi2c,
                uint16_t device_address);

bool sht3x_read(sht3x_t *sensor,
                sht3x_sample_t *sample);

#ifdef __cplusplus
}
#endif

#endif
