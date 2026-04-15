#include "ControlLoop.h"
#include "main.h"
#include "cmsis_os.h"
#include <math.h>
#include <stdbool.h>

// =====================
// EXTERNAL GLOBALS
// =====================

extern volatile bool g_sht3x_sample_valid;
extern volatile struct {
    float humidity_percent;
    float temperature_c;
    uint32_t sample_tick;
} g_sht3x_sample;

extern volatile bool g_esp8266_forecast_valid;

extern volatile struct {
    uint8_t  start_hour;
    float    temperature_f[8];
    uint8_t  humidity_percent[8];
    uint32_t received_tick;
} g_esp8266_forecast;

// =====================
// USER PARAMETERS
// =====================

volatile float g_TL  = -30.0f;
volatile float g_TH  = -7.0f;
volatile float g_RHL = 15.0f;
volatile float g_RHH = 35.0f;

// =====================
// CONFIG
// =====================

#define RH_DEADBAND 2.0f

// =====================
// OUTPUTS
// =====================

volatile float g_target_humidity = 50.0f;
volatile uint8_t g_humidifier_on = 0;
volatile uint8_t g_dehumidifier_on = 0;

volatile control_mode_t g_control_mode = MODE_AUTO;

// =====================
// INTERNAL STATE
// =====================

static float last_rh = 0.0f;
static uint32_t last_tick = 0;

// Calibration
static float calib_start_rh = 0.0f;
static uint32_t calib_start_tick = 0;
static uint8_t calib_active = 0;

volatile float g_measured_rate = 0.5f; // %RH per minute

// =====================
// TEMP → RH GRAPH
// =====================

float controllerRH(float temp,
                   float tl,
                   float rhl,
                   float th,
                   float rhh)
{
    if (temp <= tl) return rhl;
    if (temp >= th) return rhh;

    float slope = (rhh - rhl) / (th - tl);
    return rhl + slope * (temp - tl);
}

// =====================
// CALIBRATION MODE
// =====================

void Calibration_Update(void)
{
    if (!g_sht3x_sample_valid)
        return;

    float rh = g_sht3x_sample.humidity_percent;
    uint32_t now = g_sht3x_sample.sample_tick;

    if (!calib_active)
    {
        calib_start_rh = rh;
        calib_start_tick = now;
        calib_active = 1;

        // Turn ON humidifier to measure response
        g_humidifier_on = 1;
        g_dehumidifier_on = 0;
        return;
    }

    float dt_min = (now - calib_start_tick) / 60000.0f;
    float drh = fabsf(rh - calib_start_rh);

    if (dt_min > 0.5f && drh > 1.0f)
    {
        g_measured_rate = drh / dt_min;

        if (g_measured_rate < 0.05f)
            g_measured_rate = 0.05f;

        // Stop outputs
        g_humidifier_on = 0;
        g_dehumidifier_on = 0;

        calib_active = 0;

        // Return to AUTO
        g_control_mode = MODE_AUTO;
    }
}

// =====================
// FORECAST CALCULATION
// =====================

float compute_target_from_forecast(void)
{
    if (!g_sht3x_sample_valid)
        return g_target_humidity;

    float measured_rh = g_sht3x_sample.humidity_percent;
    float rate = g_measured_rate;

    if (rate < 0.05f) rate = 0.05f;

    if (!g_esp8266_forecast_valid)
    {
        float temp_c = g_sht3x_sample.temperature_c;
        return controllerRH(temp_c, g_TL, g_RHL, g_TH, g_RHH);
    }

    for (int i = 1; i < 8; i++)
    {
        float temp_f = g_esp8266_forecast.temperature_f[i];
        float temp_c = (temp_f - 32.0f) * 5.0f / 9.0f;

        float target = controllerRH(temp_c, g_TL, g_RHL, g_TH, g_RHH);

        float error = fabsf(target - measured_rh);

        float minutes_needed = error / rate;
        float minutes_available = i * 60.0f;

        if (minutes_needed >= minutes_available)
        {
            return target;
        }
    }

    float temp_f = g_esp8266_forecast.temperature_f[1];
    float temp_c = (temp_f - 32.0f) * 5.0f / 9.0f;

    return controllerRH(temp_c, g_TL, g_RHL, g_TH, g_RHH);
}

// =====================
// INIT
// =====================

void ControlLoop_Init(void)
{
    last_rh = 0.0f;
    last_tick = 0;
    calib_active = 0;
}

// =====================
// MAIN UPDATE
// =====================

void ControlLoop_Update(void)
{
    if (!g_sht3x_sample_valid)
        return;

    float measured_rh = g_sht3x_sample.humidity_percent;

    // ===== MODE HANDLING =====
    if (g_control_mode == MODE_CALIBRATION)
    {
        Calibration_Update();
        return;
    }

    if (g_control_mode == MODE_AUTO)
    {
        g_target_humidity = compute_target_from_forecast();
    }
    else // MANUAL
    {
        float temp_c;

        if (g_esp8266_forecast_valid)
        {
            float temp_f = g_esp8266_forecast.temperature_f[0];
            temp_c = (temp_f - 32.0f) * 5.0f / 9.0f;
        }
        else
        {
            temp_c = g_sht3x_sample.temperature_c;
        }

        g_target_humidity = controllerRH(temp_c,
                                         g_TL,
                                         g_RHL,
                                         g_TH,
                                         g_RHH);
    }

    // Clamp
    if (g_target_humidity < 0.0f) g_target_humidity = 0.0f;
    if (g_target_humidity > 100.0f) g_target_humidity = 100.0f;

    // ===== HYSTERESIS =====
    float low  = g_target_humidity - RH_DEADBAND;
    float high = g_target_humidity + RH_DEADBAND;

    if (measured_rh < low)
    {
        g_humidifier_on = 1;
        g_dehumidifier_on = 0;
    }
    else if (measured_rh > high)
    {
        g_humidifier_on = 0;
        g_dehumidifier_on = 1;
    }
    else
    {
        g_humidifier_on = 0;
        g_dehumidifier_on = 0;
    }
}
