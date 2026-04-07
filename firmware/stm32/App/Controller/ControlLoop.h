#ifndef CONTROLS_CONTROLLOOP_H_
#define CONTROLS_CONTROLLOOP_H_

#include <stdbool.h>
#include <stdint.h>

// Public interface
void ControlLoop_Init(void);
void ControlLoop_Update(void);

// Shared outputs (for display or debugging)
extern volatile float g_target_humidity;
extern volatile float g_control_pwm;

#endif /* CONTROLS_CONTROLLOOP_H_ */
