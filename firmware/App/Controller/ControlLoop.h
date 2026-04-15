#ifndef INC_CONTROLLOOP_H_
#define INC_CONTROLLOOP_H_

#include <stdint.h>

// =====================
// MODE ENUM
// =====================

typedef enum {
    MODE_AUTO = 0,
    MODE_MANUAL,
    MODE_CALIBRATION
} control_mode_t;

// =====================
// FUNCTIONS
// =====================

void ControlLoop_Init(void);
void ControlLoop_Update(void);

// =====================
// GLOBAL OUTPUTS
// =====================

// Computed target humidity (setpoint)
extern volatile float g_target_humidity;

// Actuator states
extern volatile uint8_t g_humidifier_on;
extern volatile uint8_t g_dehumidifier_on;

// =====================
// CONTROL STATE
// =====================

extern volatile control_mode_t g_control_mode;

// =====================
// USER PARAMETERS
// =====================

extern volatile float g_TL;
extern volatile float g_TH;
extern volatile float g_RHL;
extern volatile float g_RHH;

// =====================
// CALIBRATION OUTPUT
// =====================

extern volatile float g_measured_rate;

#endif /* INC_CONTROLLOOP_H_ */
