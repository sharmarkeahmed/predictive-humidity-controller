#include "humidity_sensor.h"

#include <string.h>

/*
 * This is a device-agnostic scaffold. Replace the placeholder command,
 * parsing, and timing with the requirements for your specific sensor.
 */

#define HUMIDITY_SENSOR_DEFAULT_TIMEOUT_MS 100U

static HAL_StatusTypeDef humidity_sensor_trigger_measurement(humidity_sensor_t *sensor);
static void humidity_sensor_parse_sample(const uint8_t *raw_data, humidity_sensor_sample_t *sample);

/// @brief Initializes the humidity sensor structure with the provided I2C handle and device address.
/// @param sensor Pointer to the humidity sensor structure to initialize.
/// @param hi2c Pointer to the I2C handle to use for communication with the sensor.
/// @param device_address 
/// @return 
bool humidity_sensor_init(humidity_sensor_t *sensor, I2C_HandleTypeDef *hi2c, uint16_t device_address) {
    if ((sensor == NULL) || (hi2c == NULL)) {
        return false; // invalid input parameters
    }

    sensor->hi2c = hi2c;
    sensor->device_address = device_address;

    return true;
}

bool humidity_sensor_read(humidity_sensor_t *sensor,
                          humidity_sensor_sample_t *sample)
{
    uint8_t raw_data[4] = {0};

    if ((sensor == NULL) || (sample == NULL))
    {
        return false;
    }

    if (humidity_sensor_trigger_measurement(sensor) != HAL_OK)
    {
        return false;
    }

    if (humidity_sensor_read_raw(sensor,
                                 raw_data,
                                 (uint16_t)sizeof(raw_data),
                                 HUMIDITY_SENSOR_DEFAULT_TIMEOUT_MS) != HAL_OK)
    {
        return false;
    }

    humidity_sensor_parse_sample(raw_data, sample);
    sample->sample_tick = HAL_GetTick();

    return true;
}

HAL_StatusTypeDef humidity_sensor_read_raw(humidity_sensor_t *sensor,
                                           uint8_t *buffer,
                                           uint16_t size,
                                           uint32_t timeout_ms)
{
    if ((sensor == NULL) || (sensor->hi2c == NULL) || (buffer == NULL) || (size == 0U))
    {
        return HAL_ERROR;
    }

    return HAL_I2C_Master_Receive(sensor->hi2c,
                                  sensor->device_address,
                                  buffer,
                                  size,
                                  timeout_ms);
}

static HAL_StatusTypeDef humidity_sensor_trigger_measurement(humidity_sensor_t *sensor)
{
    static const uint8_t measure_cmd[] = {0x00};

    if ((sensor == NULL) || (sensor->hi2c == NULL))
    {
        return HAL_ERROR;
    }

    return HAL_I2C_Master_Transmit(sensor->hi2c,
                                   sensor->device_address,
                                   (uint8_t *)measure_cmd,
                                   (uint16_t)sizeof(measure_cmd),
                                   HUMIDITY_SENSOR_DEFAULT_TIMEOUT_MS);
}

static void humidity_sensor_parse_sample(const uint8_t *raw_data,
                                         humidity_sensor_sample_t *sample)
{
    if ((raw_data == NULL) || (sample == NULL))
    {
        return;
    }

    memset(sample, 0, sizeof(*sample));

    /*
     * Placeholder conversion. Swap this out for the formula from your
     * sensor datasheet once you confirm the exact part.
     */
    sample->humidity_percent = (float)raw_data[0];
    sample->temperature_c = (float)raw_data[2];
}
