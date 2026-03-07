#ifndef APP_SENSORS_HUMIDITY_SENSOR_H
#define APP_SENSORS_HUMIDITY_SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

typedef struct
{
    float humidity_percent;
    float temperature_c;
    uint32_t sample_tick;
} humidity_sensor_sample_t;

typedef struct
{
    I2C_HandleTypeDef *hi2c;
    uint16_t device_address;
} humidity_sensor_t;

bool humidity_sensor_init(humidity_sensor_t *sensor,
                          I2C_HandleTypeDef *hi2c,
                          uint16_t device_address);

bool humidity_sensor_read(humidity_sensor_t *sensor,
                          humidity_sensor_sample_t *sample);

HAL_StatusTypeDef humidity_sensor_read_raw(humidity_sensor_t *sensor,
                                           uint8_t *buffer,
                                           uint16_t size,
                                           uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
