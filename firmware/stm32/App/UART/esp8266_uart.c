#include "esp8266_uart.h"
#include <string.h>
#include "cmsis_os.h"

bool esp8266_uart_init(esp8266_uart_t *state, UART_HandleTypeDef *huart)
{
    if (!state || !huart) return false;

    memset(state, 0, sizeof(*state));
    state->huart = huart;
    state->initialized = true;
    state->last_result = ESP8266_UART_RESULT_OK;
    state->last_hal_status = HAL_OK;
    state->tx_in_progress = false;
    state->tx_done = false;

    /* Start interrupt reception */
    HAL_UART_Receive_IT(state->huart, &state->rx_byte, 1);
    return true;
}

void esp8266_uart_rx_byte(esp8266_uart_t *state, uint8_t byte)
{
    if (!state || !state->initialized) return;

    /* Wait for start byte */
    if (state->frame_index == 0 && byte != ESP8266_UART_START_BYTE)
        return;

    state->frame_buffer[state->frame_index++] = byte;

    /* Full frame received */
    if (state->frame_index >= ESP8266_UART_FRAME_SIZE)
    {
        if (state->frame_buffer[ESP8266_UART_FRAME_SIZE - 1] != ESP8266_UART_END_BYTE)
        {
            state->frame_error_count++;
            state->frame_index = 0;
            return;
        }

        /* Copy payload */
        uint8_t *payload = &state->frame_buffer[1];
        state->latest_forecast.start_hour = payload[0];
        payload += 1;
        memcpy(state->latest_forecast.temperature_f, payload, sizeof(state->latest_forecast.temperature_f));
        memcpy(state->latest_forecast.humidity_percent,
               payload + sizeof(state->latest_forecast.temperature_f),
               sizeof(state->latest_forecast.humidity_percent));

        state->latest_forecast.received_tick = HAL_GetTick();
        state->forecast_ready = true;
        state->frame_count++;
        state->frame_index = 0;
    }
}

bool esp8266_uart_poll_frame(esp8266_uart_t *state, uint32_t timeout_ms)
{
    if (!state || !state->initialized) return false;

    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < timeout_ms)
    {
        if (state->forecast_ready)
            return true;
        osDelay(1);
    }

    state->last_hal_status = HAL_TIMEOUT;
    return false;
}

/* Blocking receive frame using polling */
bool esp8266_uart_receive_frame(esp8266_uart_t *state, uint32_t timeout_ms)
{
    if (!state || !state->initialized) return false;
    return esp8266_uart_poll_frame(state, timeout_ms);
}

bool esp8266_uart_get_latest_forecast(const esp8266_uart_t *state, esp8266_forecast_t *forecast)
{
    if (!state || !forecast || !state->forecast_ready) return false;
    memcpy(forecast, &state->latest_forecast, sizeof(*forecast));
    return true;
}

void esp8266_uart_clear_forecast_flag(esp8266_uart_t *state)
{
    if (!state) return;
    state->forecast_ready = false;
}

bool esp8266_uart_send_status(esp8266_uart_t *state,
                              const esp8266_tx_status_t *status)
{
    uint8_t *payload;

    if (!state || !state->initialized || !status) return false;
    if (state->tx_in_progress) return false;

    state->tx_frame_buffer[0] = ESP8266_UART_START_BYTE;
    payload = &state->tx_frame_buffer[1];

    memcpy(payload, status, sizeof(*status));

    state->tx_frame_buffer[ESP8266_UART_TX_FRAME_SIZE - 1] = ESP8266_UART_END_BYTE;

    state->tx_done = false;
    state->tx_in_progress = true;

    state->last_hal_status = HAL_UART_Transmit_IT(state->huart,
                                                  state->tx_frame_buffer,
                                                  ESP8266_UART_TX_FRAME_SIZE);

    if (state->last_hal_status != HAL_OK)
    {
        state->tx_in_progress = false;
        state->last_result = ESP8266_UART_RESULT_UART_ERROR;
        return false;
    }

    state->last_result = ESP8266_UART_RESULT_OK;
    return true;
}

void esp8266_uart_tx_complete(esp8266_uart_t *state)
{
    if (!state) return;

    state->tx_in_progress = false;
    state->tx_done = true;
    state->tx_count++;
}

