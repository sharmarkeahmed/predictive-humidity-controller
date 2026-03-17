#include "esp8266_uart.h"

#include <string.h>

static void esp8266_uart_reset_frame(esp8266_uart_t *state);
static void esp8266_uart_decode_payload(esp8266_uart_t *state);
static void esp8266_uart_store_frame_error(esp8266_uart_t *state,
                                           esp8266_uart_result_t result);

bool esp8266_uart_init(esp8266_uart_t *state, UART_HandleTypeDef *huart)
{
    if ((state == NULL) || (huart == NULL)) {
        return false;
    }

    memset(state, 0, sizeof(*state));
    state->huart = huart;
    state->initialized = true;
    state->last_hal_status = HAL_OK;
    state->last_result = ESP8266_UART_RESULT_OK;

    return true;
}

bool esp8266_uart_process_byte(esp8266_uart_t *state, uint8_t byte)
{
    if ((state == NULL) || !state->initialized) {
        if (state != NULL) {
            state->last_result = ESP8266_UART_RESULT_NOT_INITIALIZED;
        }
        return false;
    }

    if (!state->frame_in_progress) {
        if (byte == ESP8266_UART_START_BYTE) {
            state->frame_in_progress = true;
            state->payload_index = 0U;
            state->frame_start_tick = HAL_GetTick();
            state->last_result = ESP8266_UART_RESULT_OK;
        }

        return false;
    }

    if (state->payload_index < ESP8266_UART_PAYLOAD_SIZE_BYTES) {
        state->payload[state->payload_index++] = byte;
        return false;
    }

    if (byte != ESP8266_UART_END_BYTE) {
        esp8266_uart_store_frame_error(state, ESP8266_UART_RESULT_FRAME_ERROR);

        if (byte == ESP8266_UART_START_BYTE) {
            state->frame_in_progress = true;
            state->payload_index = 0U;
            state->frame_start_tick = HAL_GetTick();
            state->last_result = ESP8266_UART_RESULT_OK;
        }

        return false;
    }

    esp8266_uart_decode_payload(state);
    state->frame_count++;
    state->frame_complete_tick = HAL_GetTick();
    state->latest_forecast.received_tick = state->frame_complete_tick;
    state->forecast_ready = true;
    state->last_result = ESP8266_UART_RESULT_OK;
    esp8266_uart_reset_frame(state);

    return true;
}

bool esp8266_uart_receive_byte(esp8266_uart_t *state,
                               uint8_t *byte,
                               uint32_t timeout_ms)
{
    if ((state == NULL) || (byte == NULL)) {
        if (state != NULL) {
            state->last_result = ESP8266_UART_RESULT_INVALID_ARGUMENT;
        }
        return false;
    }

    if (!state->initialized) {
        state->last_result = ESP8266_UART_RESULT_NOT_INITIALIZED;
        return false;
    }

    state->last_hal_status = HAL_UART_Receive(state->huart, byte, 1U, timeout_ms);
    if (state->last_hal_status != HAL_OK) {
        state->last_result = ESP8266_UART_RESULT_UART_ERROR;
        return false;
    }

    state->last_result = ESP8266_UART_RESULT_OK;
    return true;
}

bool esp8266_uart_receive_frame(esp8266_uart_t *state,
                                uint32_t timeout_ms)
{
    uint8_t byte = 0U;
    uint32_t start_tick;

    if (state == NULL) {
        return false;
    }

    if (!state->initialized) {
        state->last_result = ESP8266_UART_RESULT_NOT_INITIALIZED;
        return false;
    }

    start_tick = HAL_GetTick();

    while ((HAL_GetTick() - start_tick) < timeout_ms) {
        uint32_t remaining = timeout_ms - (HAL_GetTick() - start_tick);

        if (!esp8266_uart_receive_byte(state, &byte, remaining)) {
            return false;
        }

        if (esp8266_uart_process_byte(state, byte)) {
            return true;
        }
    }

    state->last_hal_status = HAL_TIMEOUT;
    state->last_result = ESP8266_UART_RESULT_UART_ERROR;
    return false;
}

bool esp8266_uart_get_latest_forecast(const esp8266_uart_t *state,
                                      esp8266_forecast_t *forecast)
{
    if ((state == NULL) || (forecast == NULL)) {
        return false;
    }

    if (!state->initialized || !state->forecast_ready) {
        return false;
    }

    memcpy(forecast, &state->latest_forecast, sizeof(*forecast));
    return true;
}

bool esp8266_uart_has_fresh_forecast(const esp8266_uart_t *state)
{
    if ((state == NULL) || !state->initialized) {
        return false;
    }

    return state->forecast_ready;
}

void esp8266_uart_clear_forecast_flag(esp8266_uart_t *state)
{
    if ((state == NULL) || !state->initialized) {
        return;
    }

    state->forecast_ready = false;
}

static void esp8266_uart_reset_frame(esp8266_uart_t *state)
{
    state->frame_in_progress = false;
    state->payload_index = 0U;
}

static void esp8266_uart_decode_payload(esp8266_uart_t *state)
{
    size_t offset = 0U;

    memcpy(state->latest_forecast.temperature_f,
           &state->payload[offset],
           sizeof(state->latest_forecast.temperature_f));
    offset += sizeof(state->latest_forecast.temperature_f);

    memcpy(state->latest_forecast.humidity_percent,
           &state->payload[offset],
           sizeof(state->latest_forecast.humidity_percent));
}

static void esp8266_uart_store_frame_error(esp8266_uart_t *state,
                                           esp8266_uart_result_t result)
{
    state->frame_error_count++;
    state->last_result = result;
    esp8266_uart_reset_frame(state);
}
