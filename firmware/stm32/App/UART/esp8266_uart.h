#pragma once
#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>
#define ESP8266_UART_START_BYTE 0xAA
#define ESP8266_UART_END_BYTE 0x55
#define ESP8266_UART_PAYLOAD_SIZE_BYTES 41
#define ESP8266_UART_FRAME_SIZE (1 + ESP8266_UART_PAYLOAD_SIZE_BYTES + 1)

#define ESP8266_UART_TX_PAYLOAD_SIZE_BYTES 24
#define ESP8266_UART_TX_FRAME_SIZE (1 + ESP8266_UART_TX_PAYLOAD_SIZE_BYTES + 1)



typedef enum
{
    ESP8266_UART_CMD_REQUEST_FORECAST = 0x01,
} esp8266_uart_command_t;


typedef enum
{
	ESP8266_UART_RESULT_OK = 0,
	ESP8266_UART_RESULT_NOT_INITIALIZED,
	ESP8266_UART_RESULT_UART_ERROR,
	ESP8266_UART_RESULT_FRAME_ERROR,
} esp8266_uart_result_t;

typedef struct
{
	uint8_t  start_hour;
	float temperature_f[8];
	uint8_t humidity_percent[8];
	uint32_t received_tick;
} esp8266_forecast_t;

typedef struct
{
    UART_HandleTypeDef *huart;
    bool initialized;

    uint8_t frame_buffer[ESP8266_UART_FRAME_SIZE];
    uint8_t frame_index;
    uint8_t rx_byte;
    bool forecast_ready;
    esp8266_forecast_t latest_forecast;
    uint32_t frame_count;
    uint32_t frame_error_count;

    uint8_t tx_frame_buffer[ESP8266_UART_TX_FRAME_SIZE];
    bool tx_in_progress;
    bool tx_done;
    uint32_t tx_count;

    HAL_StatusTypeDef last_hal_status;
    esp8266_uart_result_t last_result;
} esp8266_uart_t;


typedef struct
{
    float humidity_percent;
	float control_signal;
    float temperature_c;
    float target_humidity;
    float measured_rate;
    uint8_t humidifier_on;
    uint8_t dehumidifier_on;
    uint8_t control_mode;
    uint8_t sensor_valid;
} esp8266_tx_status_t;


bool esp8266_uart_init(esp8266_uart_t *state, UART_HandleTypeDef *huart);
void esp8266_uart_rx_byte(esp8266_uart_t *state, uint8_t byte);
bool esp8266_uart_receive_frame(esp8266_uart_t *state, uint32_t timeout_ms);
bool esp8266_uart_get_latest_forecast(const esp8266_uart_t *state, esp8266_forecast_t *forecast);
void esp8266_uart_clear_forecast_flag(esp8266_uart_t *state);
bool esp8266_uart_send_status(esp8266_uart_t *state, const esp8266_tx_status_t *status);
void esp8266_uart_tx_complete(esp8266_uart_t *state);

