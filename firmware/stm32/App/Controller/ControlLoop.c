#include "ControlLoop.h"
#include "main.h"
#include "cmsis_os.h"
#include <math.h>
#include <stdbool.h>

// Access sensor globals from main.c
extern volatile bool g_sht3x_sample_valid;
extern volatile struct {
    float humidity_percent;
    float temperature_c;
    uint32_t sample_tick;
} g_sht3x_sample;

// ESP8266 forecast globals from main.c
extern volatile bool g_esp8266_forecast_valid;

extern volatile struct {
    uint8_t  start_hour;
    float    temperature_f[8];
    uint8_t  humidity_percent[8];
    uint32_t received_tick;
} g_esp8266_forecast;

extern TIM_HandleTypeDef htim1;

// =====================
// USER CONFIG
// =====================

// PI gains (start conservative for hardware)
static float Kp = 2.0f;
static float Ki = 0.01f;

static float last_humidity = 0.0f;
static uint32_t last_tick = 0;

static float measured_rate = 0.5f;  // start with fake, will update

// Control limits
#define PWM_MAX       100.0f
#define PWM_MIN       0.0f
#define PWM_SAFE_MAX  80.0f   // safety cap for initial testing

// Integral limits (anti-windup protection)
#define INTEGRAL_MAX  1000.0f
#define INTEGRAL_MIN -1000.0f

// Sensor update period
#define CONTROL_DT    5.0f   // seconds (matches sensor rate)

// =====================
// GLOBAL OUTPUTS
// =====================

volatile float g_target_humidity = 50.0f; // placeholders until sensor uart is in
volatile float g_control_pwm = 0.0f;

// =====================
// INTERNAL STATE
// =====================

static float integral = 0.0f;
static uint32_t last_sample_tick = 0;

float compute_target_from_forecast(void)
{
    if (!g_esp8266_forecast_valid || !g_sht3x_sample_valid)
        return g_target_humidity; // keep previous

    float current_humidity = g_sht3x_sample.humidity_percent;

    for (int i = 1; i < 8; i++)
    {
        float temp = g_esp8266_forecast.temperature_f[i];

        // Simple temp → humidity mapping
        float target;
        if (temp < 60) target = 45;
        else if (temp < 75) target = 50;
        else if (temp < 85) target = 55;
        else target = 60;

        float error = target - current_humidity;

        float rate = measured_rate;

        if (rate < 0.05f) rate = 0.05f;  // safety

        float minutes_needed = fabs(error) / rate;

        float minutes_available = i * 60.0f;

        if (minutes_needed >= minutes_available)
        {
            return target;
        }
    }

    // fallback to next hour
    float temp = g_esp8266_forecast.temperature_f[1];
    return (temp < 75) ? 50 : 55;
}

// =====================
// INIT
// =====================

void ControlLoop_Init(void)
{
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);

    integral = 0.0f;
    last_sample_tick = 0;
}

// =====================
// MAIN CONTROL UPDATE
// =====================

void ControlLoop_Update(void)
{
    // If sensor not ready → shut off PWM
    if (!g_sht3x_sample_valid)
    {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
        g_control_pwm = 0;
        return;
    }

    // Only run control when NEW sensor data arrives
    if (g_sht3x_sample.sample_tick == last_sample_tick)
    {
        return;
    }

    last_sample_tick = g_sht3x_sample.sample_tick;

    float RH = g_sht3x_sample.humidity_percent;
    uint32_t now = g_sht3x_sample.sample_tick;

    if (last_tick != 0)
    {
        float dH = RH - last_humidity;
        float dt = (now - last_tick) / 1000.0f;  // ms → seconds

        if (dt > 0.1f)
        {
            float rate_per_min = (dH / dt) * 60.0f;

            // only learn when system is active
            if (fabs(g_control_pwm) > 5.0f)
            {
                measured_rate = 0.9f * measured_rate + 0.1f * fabs(rate_per_min);
            }
        }
    }

    last_humidity = RH;
    last_tick = now;

    g_target_humidity = compute_target_from_forecast();

    // Error
    float error = g_target_humidity - RH;

    // Optional deadband (prevents jitter)
    if (fabs(error) < 0.2f)
    {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
        g_control_pwm = 0;
        return;
    }

    // Integral
    integral += error * CONTROL_DT;

    // Clamp integral
    if (integral > INTEGRAL_MAX) integral = INTEGRAL_MAX;
    if (integral < INTEGRAL_MIN) integral = INTEGRAL_MIN;

    // PI Controller
    float pwm = Kp * error + Ki * integral;

    // Clamp Output
    if (pwm > PWM_MAX)
    {
        pwm = PWM_MAX;
        integral -= error * CONTROL_DT; // anti-windup
    }
    else if (pwm < PWM_MIN)
    {
        pwm = PWM_MIN;
        integral -= error * CONTROL_DT; // anti-windup
    }

    // ===== Safety Limit =====
    if (pwm > PWM_SAFE_MAX)
    {
        pwm = PWM_SAFE_MAX;
    }

    // Save output
    g_control_pwm = pwm;

    // ===== Apply PWM =====
    uint32_t pwm_counts = (uint32_t)((pwm / 100.0f) * 999.0f);

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pwm_counts);
}
