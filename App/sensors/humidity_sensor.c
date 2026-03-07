#include "humidity_sensor.h"

#include <string.h>

/*
 * This is a device-agnostic scaffold. Replace the placeholder command,
 * parsing, and timing with the requirements for your specific sensor.
 */

#define HUMIDITY_SENSOR_DEFAULT_TIMEOUT_MS 100

static HAL_StatusTypeDef humidity_sensor_trigger_measurement(humidity_sensor_t *sensor);
static void humidity_sensor_parse_sample(const uint8_t *raw_data, humidity_sensor_sample_t *sample);

/// @brief Initializes the humidity sensor structure with the provided I2C handle and device address.
/// @param sensor Pointer to the humidity sensor structure to initialize.
/// @param hi2c Pointer to the I2C handle to use for communication with the sensor.
/// @param device_address (7-bit) I2C address of the humidity sensor.
/// @return true if the initialization was successful, otherwise return false
bool humidity_sensor_init(humidity_sensor_t *sensor, I2C_HandleTypeDef *hi2c, uint16_t device_address) {
    if ((sensor == NULL) || (hi2c == NULL)) {
        return false; // invalid input parameters
    }

    sensor->hi2c = hi2c;
    sensor->device_address = (uint16_t) ( device_address << 1 ); // I2C datasheets give 7-bit, but HAL expects 8-bit
    
    sensor->initialized = false;
    sensor->last_error = HAL_OK;

    if ( HAL_I2C_IsDeviceReady(sensor->hi2c, sensor->device_address, 3, 100) != HAL_OK ) {
        sensor->last_error = HAL_ERROR; 
        return false; 
    }

    sensor->initialized = true;
    return true;
}

bool humidity_sensor_read(humidity_sensor_t *sensor,
                          humidity_sensor_sample_t *sample)
{
    if ((sensor == NULL) || (sample == NULL) || !sensor->initialized) {
        return false; // invalid input parameters
    }

    uint8_t cmd[2] = {0x24, 0x00}; // per p. 10 (table 9) of datasheet, this corresponds to clock stretching disabled, high repeatability enabled, single shot mode
    // note that the STM32 HAL should automatically generate other I2C bus control signals

    uint8_t raw[6]; // data that will be received from sensor,
    // refer to p. 10 (table 9) of SHT3x datasheet for the expected format of the measurement data

    HAL_StatusTypeDef status;
    status = HAL_I2C_Master_Transmit(sensor->hi2c,
                                   sensor->device_address,
                                   cmd,
                                   sizeof(cmd),
                                   HUMIDITY_SENSOR_DEFAULT_TIMEOUT_MS);
   
    if (status != HAL_OK) {
        sensor->last_error = status;
        return false; // failed to send measurement command
    } 

    // Wait for measurement to complete (~15 ms for high repeatability)
    vTaskDelay(pdMS_TO_TICKS(20));              
    
    status = HAL_I2C_Master_Receive(sensor->hi2c,
                                  sensor->device_address,
                                  raw,
                                  sizeof(raw),
                                  HUMIDITY_SENSOR_DEFAULT_TIMEOUT_MS);

    if (status != HAL_OK) {
        sensor->last_error = status;
        return false; // failed to read measurement data
    }

    // Convert raw temperature
    uint16_t raw_temp = ((uint16_t)raw[0] << 8) | raw[1]; // combine the MSB and LSB bits together
    sample->temperature_c = -45.0f + 175.0f * ((float)raw_temp / 65535.0f); // temp formula on p. 14 of SHT3x datasheet

    // Convert raw humidity
    uint16_t raw_rh = ((uint16_t)raw[3] << 8) | raw[4]; // combine the MSB and LSB bits together
    sample->humidity_percent = 100.0f * ((float)raw_rh / 65535.0f); // humidity formula on p. 14 of SHT3x datasheet

    sample->sample_tick = xTaskGetTickCount();

    // TODO: add CRC checks for raw data (p. 14 of SHT3x datasheet) to verify data integrity

    return true;

}