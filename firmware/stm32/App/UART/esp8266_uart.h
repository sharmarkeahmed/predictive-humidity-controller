#ifndef APP_UART_ESP8266_UART_H
#define APP_UART_ESP8266_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

#define ESP8266_UART_START_BYTE           0xAAU
#define ESP8266_UART_END_BYTE             0x55U
#define ESP8266_UART_FORECAST_HOURS       8U
#define ESP8266_UART_TEMPERATURE_COUNT    ESP8266_UART_FORECAST_HOURS
#define ESP8266_UART_HUMIDITY_COUNT       ESP8266_UART_FORECAST_HOURS
#define ESP8266_UART_PAYLOAD_SIZE_BYTES   ((ESP8266_UART_TEMPERATURE_COUNT * sizeof(float)) + ESP8266_UART_HUMIDITY_COUNT)
#define ESP8266_UART_FRAME_SIZE_BYTES     (1U + ESP8266_UART_PAYLOAD_SIZE_BYTES + 1U)

typedef enum
{
    ESP8266_UART_RESULT_OK = 0,
    ESP8266_UART_RESULT_INVALID_ARGUMENT,
    ESP8266_UART_RESULT_NOT_INITIALIZED,
    ESP8266_UART_RESULT_UART_ERROR,
    ESP8266_UART_RESULT_FRAME_ERROR
} esp8266_uart_result_t;

typedef struct
{
    float temperature_f[ESP8266_UART_TEMPERATURE_COUNT];
    uint8_t humidity_percent[ESP8266_UART_HUMIDITY_COUNT];
    uint32_t received_tick;
} esp8266_forecast_t;

typedef struct
{
    UART_HandleTypeDef *huart;
    bool initialized;
    bool frame_in_progress;
    bool forecast_ready;
    uint8_t payload_index;
    uint32_t frame_start_tick;
    uint32_t frame_complete_tick;
    uint32_t frame_count;
    uint32_t frame_error_count;
    HAL_StatusTypeDef last_hal_status;
    esp8266_uart_result_t last_result;
    uint8_t payload[ESP8266_UART_PAYLOAD_SIZE_BYTES];
    esp8266_forecast_t latest_forecast;
} esp8266_uart_t;

bool esp8266_uart_init(esp8266_uart_t *state, UART_HandleTypeDef *huart);

bool esp8266_uart_process_byte(esp8266_uart_t *state, uint8_t byte);

bool esp8266_uart_receive_byte(esp8266_uart_t *state,
                               uint8_t *byte,
                               uint32_t timeout_ms);

bool esp8266_uart_receive_frame(esp8266_uart_t *state,
                                uint32_t timeout_ms);

bool esp8266_uart_get_latest_forecast(const esp8266_uart_t *state,
                                      esp8266_forecast_t *forecast);

bool esp8266_uart_has_fresh_forecast(const esp8266_uart_t *state);

void esp8266_uart_clear_forecast_flag(esp8266_uart_t *state);

#ifdef __cplusplus
}
#endif

#endif
