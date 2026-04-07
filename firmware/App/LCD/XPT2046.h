#ifndef XPT2046_H
#define XPT2046_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

#define XPT2046_MSEC_THRESHOLD  20
#define XPT2046_Z_THRESHOLD     100
#define XPT2046_Z_THRESHOLD_INT 50

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} TS_Point;

typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    GPIO_TypeDef *irq_port;
    uint16_t irq_pin;
    int16_t xraw, yraw, zraw;
    uint32_t msraw;
    uint8_t rotation;

    /* Calibration values */
    int16_t minx;
    int16_t maxx;
    int16_t miny;
    int16_t maxy;

    /* Set true by ISR; cleared when pressure drops below threshold */
    volatile bool isrWake;
} XPT2046_t;

#ifdef __cplusplus
extern "C" {
#endif

void XPT2046_init(XPT2046_t *dev, SPI_HandleTypeDef *hspi,
                  GPIO_TypeDef *cs_port, uint16_t cs_pin,
                  GPIO_TypeDef *irq_port, uint16_t irq_pin);
bool XPT2046_begin(XPT2046_t *dev);
void XPT2046_irqHandler(XPT2046_t *dev);
bool XPT2046_tirqTouched(XPT2046_t *dev);
bool XPT2046_touched(XPT2046_t *dev);
void XPT2046_readData(XPT2046_t *dev, uint16_t *x, uint16_t *y, uint8_t *z);
TS_Point XPT2046_getPoint(XPT2046_t *dev);
bool XPT2046_bufferEmpty(XPT2046_t *dev);
void XPT2046_setRotation(XPT2046_t *dev, uint8_t rotation);
void XPT2046_update(XPT2046_t *dev);
void touchscreen_full_diagnostic(void);
TS_Point XPT2046_waitTouch(XPT2046_t *ts);

#ifdef __cplusplus
}
#endif

#endif /* XPT2046_H */
