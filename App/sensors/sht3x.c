#include "sht3x.h"

#define SHT3X_DEFAULT_TIMEOUT_MS 100U
#define SHT3X_CRC8_POLYNOMIAL 0x31U // refer to table 20 of SHT3x datasheet for I2C CRC Properties
#define SHT3X_CRC8_INIT       0xFFU
#define SHT3X_CMD_MEASURE_HIGHREP_NO_STRETCH_MSB 0x24U
#define SHT3X_CMD_MEASURE_HIGHREP_NO_STRETCH_LSB 0x00U

#define SHT3X_TEMP_MSB_INDEX   0U
#define SHT3X_TEMP_LSB_INDEX   1U
#define SHT3X_TEMP_CRC_INDEX   2U
#define SHT3X_RH_MSB_INDEX     3U
#define SHT3X_RH_LSB_INDEX     4U
#define SHT3X_RH_CRC_INDEX     5U

static uint8_t sht3x_crc8(const uint8_t *data, uint8_t len);

/// @brief Initializes the SHT3x driver structure with the provided I2C handle and device address.
/// @param sensor Pointer to the SHT3x structure to initialize.
/// @param hi2c Pointer to the I2C handle to use for communication with the sensor.
/// @param device_address (7-bit) I2C address of the SHT3x.
/// @return true if the initialization was successful, otherwise return false
bool sht3x_init(sht3x_t *sensor, I2C_HandleTypeDef *hi2c, uint16_t device_address) {
    if ((sensor == NULL) || (hi2c == NULL)) {
        if (sensor != NULL) {
            sensor->last_result = SHT3X_RESULT_INVALID_ARGUMENT;
        }
        return false; // invalid input parameters
    }

    sensor->hi2c = hi2c;
    sensor->device_address = (uint16_t) ( device_address << 1 ); // I2C datasheets give 7-bit, but HAL expects 8-bit
    
    sensor->initialized = false;
    sensor->last_error = HAL_OK;
    sensor->last_i2c_error = HAL_I2C_ERROR_NONE;
    sensor->last_result = SHT3X_RESULT_OK;

    HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(sensor->hi2c,
                                                     sensor->device_address,
                                                     3,
                                                     SHT3X_DEFAULT_TIMEOUT_MS);
    if (status != HAL_OK) {
        sensor->last_error = status;
        sensor->last_i2c_error = HAL_I2C_GetError(sensor->hi2c);
        sensor->last_result = SHT3X_RESULT_I2C_ERROR;
        return false; 
    }

    sensor->initialized = true;
    sensor->last_result = SHT3X_RESULT_OK;
    return true;
}

bool sht3x_read(sht3x_t *sensor,
                sht3x_sample_t *sample)
{
    if ((sensor == NULL) || (sample == NULL)) {
        if (sensor != NULL) {
            sensor->last_result = SHT3X_RESULT_INVALID_ARGUMENT;
        }
        return false; // invalid input parameters
    }

    if (!sensor->initialized) {
        sensor->last_result = SHT3X_RESULT_NOT_INITIALIZED;
        return false; // invalid input parameters
    }

    uint8_t cmd[2] = {
        SHT3X_CMD_MEASURE_HIGHREP_NO_STRETCH_MSB,
        SHT3X_CMD_MEASURE_HIGHREP_NO_STRETCH_LSB
    }; // per p. 10 (table 9) of datasheet, this corresponds to clock stretching disabled, high repeatability enabled, single shot mode
    // note that the STM32 HAL should automatically generate other I2C bus control signals

    uint8_t raw[6]; // data layout: temp MSB, temp LSB, temp CRC, RH MSB, RH LSB, RH CRC

    HAL_StatusTypeDef status;
    status = HAL_I2C_Master_Transmit(sensor->hi2c,
                                   sensor->device_address,
                                   cmd,
                                   sizeof(cmd),
                                   SHT3X_DEFAULT_TIMEOUT_MS);
   
    if (status != HAL_OK) {
        sensor->last_error = status;
        sensor->last_i2c_error = HAL_I2C_GetError(sensor->hi2c);
        sensor->last_result = SHT3X_RESULT_I2C_ERROR;
        return false; // failed to send measurement command
    } 

    // Wait for measurement to complete (~15 ms for high repeatability)
    vTaskDelay(pdMS_TO_TICKS(20));              
    
    status = HAL_I2C_Master_Receive(sensor->hi2c,
                                  sensor->device_address,
                                  raw,
                                  sizeof(raw),
                                  SHT3X_DEFAULT_TIMEOUT_MS);

    if (status != HAL_OK) {
        sensor->last_error = status;
        sensor->last_i2c_error = HAL_I2C_GetError(sensor->hi2c);
        sensor->last_result = SHT3X_RESULT_I2C_ERROR;
        return false; // failed to read measurement data
    }

    // Verify CRC for both temperature and humidity data
    uint8_t temp_crc = sht3x_crc8(&raw[SHT3X_TEMP_MSB_INDEX], 2U);
    uint8_t rh_crc = sht3x_crc8(&raw[SHT3X_RH_MSB_INDEX], 2U);

    if ((temp_crc != raw[SHT3X_TEMP_CRC_INDEX]) || (rh_crc != raw[SHT3X_RH_CRC_INDEX]))
    {
        sensor->last_error = HAL_ERROR;
        sensor->last_i2c_error = HAL_I2C_ERROR_NONE;
        sensor->last_result = SHT3X_RESULT_CRC_ERROR;
        return false; // CRC mismatch: discard corrupted sample
    }

    // Convert raw temperature
    uint16_t raw_temp = ((uint16_t)raw[SHT3X_TEMP_MSB_INDEX] << 8) |
                        raw[SHT3X_TEMP_LSB_INDEX];
    sample->temperature_c = -45.0f + 175.0f * ((float)raw_temp / 65535.0f); // temp formula on p. 14 of SHT3x datasheet

    // Convert raw humidity
    uint16_t raw_rh = ((uint16_t)raw[SHT3X_RH_MSB_INDEX] << 8) |
                      raw[SHT3X_RH_LSB_INDEX];
    sample->humidity_percent = 100.0f * ((float)raw_rh / 65535.0f); // humidity formula on p. 14 of SHT3x datasheet

    sample->sample_tick = xTaskGetTickCount();
    sensor->last_error = HAL_OK;
    sensor->last_i2c_error = HAL_I2C_ERROR_NONE;
    sensor->last_result = SHT3X_RESULT_OK;

    return true;

}

/// @brief Computes the CRC-8 checksum for the given data using the polynomial specified in the SHT3x datasheet.
/// @param data Pointer to the input data for which the CRC is to be calculated.
/// @param len Length of the input data in bytes.
/// @return The computed CRC-8 checksum as an 8-bit unsigned integer.
static uint8_t sht3x_crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = SHT3X_CRC8_INIT;

    for (uint8_t i = 0; i < len; i++) { // bit by bit polynomial CRC-8 calculation
        crc ^= data[i];

        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if ((crc & 0x80U) != 0U)
            {
                crc = (uint8_t)((crc << 1) ^ SHT3X_CRC8_POLYNOMIAL);
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}
