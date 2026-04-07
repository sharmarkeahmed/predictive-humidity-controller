#include "XPT2046.h"
#include "cmsis_os.h"

static inline void cs_low(XPT2046_t *dev)
{ HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_RESET); }

static inline void cs_high(XPT2046_t *dev)
{ HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_SET); }

static uint8_t spi_transfer(XPT2046_t *dev, uint8_t tx)
{
    uint8_t rx = 0;
    HAL_SPI_TransmitReceive(dev->hspi, &tx, &rx, 1, HAL_MAX_DELAY);
    return rx;
}

static uint16_t spi_transfer16(XPT2046_t *dev, uint8_t cmd)
{
    uint8_t tx[2] = { cmd, 0x00 };
    uint8_t rx[2] = { 0, 0 };
    HAL_SPI_TransmitReceive(dev->hspi, tx, rx, 2, HAL_MAX_DELAY);
    return ((uint16_t)rx[0] << 8) | rx[1];
}

static int16_t besttwoavg(int16_t x, int16_t y, int16_t z)
{
    int16_t da = (x > y) ? (x - y) : (y - x);
    int16_t db = (x > z) ? (x - z) : (z - x);
    int16_t dc = (z > y) ? (z - y) : (y - z);

    if (da <= db && da <= dc) return (x + y) >> 1;
    if (db <= da && db <= dc) return (x + z) >> 1;
    return (y + z) >> 1;
}

void XPT2046_init(XPT2046_t *dev, SPI_HandleTypeDef *hspi,
                  GPIO_TypeDef *cs_port, uint16_t cs_pin,
                  GPIO_TypeDef *irq_port, uint16_t irq_pin)
{
    dev->hspi     = hspi;
    dev->cs_port  = cs_port;
    dev->cs_pin   = cs_pin;
    dev->irq_port = irq_port;
    dev->irq_pin  = irq_pin;
    dev->xraw     = 0;
    dev->yraw     = 0;
    dev->zraw     = 0;
    dev->msraw    = 0;
    dev->rotation = 0;
    dev->minx     = 0;
    dev->maxx     = 4095;
    dev->miny     = 0;
    dev->maxy     = 4095;
    dev->isrWake  = (irq_port == NULL);
}

bool XPT2046_begin(XPT2046_t *dev)
{
    cs_high(dev);
    return true;
}

void XPT2046_irqHandler(XPT2046_t *dev)
{
    dev->isrWake = true;
}

bool XPT2046_tirqTouched(XPT2046_t *dev)
{
    return dev->isrWake;
}

bool XPT2046_touched(XPT2046_t *dev)
{
    if (dev->irq_port != NULL)
    {
        if (HAL_GPIO_ReadPin(dev->irq_port, dev->irq_pin) == GPIO_PIN_SET)
        {
            dev->isrWake = false;
            dev->zraw    = 0;
            return false;
        }
        dev->isrWake = true;
    }

    XPT2046_update(dev);
    return (dev->zraw >= XPT2046_Z_THRESHOLD);
}

void XPT2046_readData(XPT2046_t *dev, uint16_t *x, uint16_t *y, uint8_t *z)
{
    XPT2046_update(dev);
    *x = (uint16_t)dev->xraw;
    *y = (uint16_t)dev->yraw;
    *z = (uint8_t)dev->zraw;
}

TS_Point XPT2046_getPoint(XPT2046_t *dev)
{
    XPT2046_update(dev);
    TS_Point p = { dev->xraw, dev->yraw, dev->zraw };
    return p;
}

bool XPT2046_bufferEmpty(XPT2046_t *dev)
{
    return ((HAL_GetTick() - dev->msraw) < XPT2046_MSEC_THRESHOLD);
}

void XPT2046_setRotation(XPT2046_t *dev, uint8_t rotation)
{
    dev->rotation = rotation % 4;
}

/*
 * XPT2046_updateInternal()
 *
 * Fixed rotation mapping to match ILI9341 display orientation
 * The mapping ensures touch coordinates align with display pixels
 */
static void XPT2046_updateInternal(XPT2046_t *dev, bool force)
{
    if (!force && !dev->isrWake) return;

    uint32_t now = HAL_GetTick();

    if (!force && (now - dev->msraw) < XPT2046_MSEC_THRESHOLD) return;

    int16_t data[6];
    int32_t z;

    cs_low(dev);

    spi_transfer(dev, 0xB1);
    int16_t z1 = (int16_t)(spi_transfer16(dev, 0xC1) >> 3);
    z = z1 + 4095;
    int16_t z2 = (int16_t)(spi_transfer16(dev, 0x91) >> 3);
    z -= z2;

    if (z >= XPT2046_Z_THRESHOLD) {
        spi_transfer16(dev, 0x91);
        data[0] = (int16_t)(spi_transfer16(dev, 0xD1) >> 3);
        data[1] = (int16_t)(spi_transfer16(dev, 0x91) >> 3);
        data[2] = (int16_t)(spi_transfer16(dev, 0xD1) >> 3);
        data[3] = (int16_t)(spi_transfer16(dev, 0x91) >> 3);
    } else {
        data[0] = data[1] = data[2] = data[3] = 0;
    }

    data[4] = (int16_t)(spi_transfer16(dev, 0xD0) >> 3);
    data[5] = (int16_t)(spi_transfer16(dev, 0x00) >> 3);

    cs_high(dev);

    if (z < 0) z = 0;

    if (z < XPT2046_Z_THRESHOLD) {
        dev->zraw = 0;
        if (z < XPT2046_Z_THRESHOLD_INT && dev->irq_port != NULL) {
            dev->isrWake = false;
        }
        return;
    }

    dev->zraw  = (int16_t)z;
    dev->msraw = now;

    int16_t x = besttwoavg(data[0], data[2], data[4]);
    int16_t y = besttwoavg(data[1], data[3], data[5]);

    /* Raw coordinates from touch controller (0-4095) */
    int16_t xraw = x;
    int16_t yraw = y;

    /* Apply rotation to match display orientation */
    switch (dev->rotation) {
       case 0:
           dev->xraw = 4095 - y;
           dev->yraw = x;
           break;
       case 1:
           dev->xraw = x;
           dev->yraw = y;
           break;
       case 2:
           dev->xraw = y;
           dev->yraw = 4095 - x;
           break;
       default: /* 3 */
           dev->xraw = 4095 - x;
           dev->yraw = 4095 - y;
           break;
       }
}

void XPT2046_update(XPT2046_t *dev)
{
    XPT2046_updateInternal(dev, false);
}

/*
 * XPT2046_waitTouch()
 */
TS_Point XPT2046_waitTouch(XPT2046_t *ts)
{
    TS_Point p = {0, 0, 0};

    while (HAL_GPIO_ReadPin(ts->irq_port, ts->irq_pin) == GPIO_PIN_SET)
        osDelay(10);

    osDelay(50);

    if (HAL_GPIO_ReadPin(ts->irq_port, ts->irq_pin) == GPIO_PIN_SET)
        return p;

    ts->isrWake = true;
    ts->msraw   = 0;
    XPT2046_updateInternal(ts, true);

    p.x = ts->xraw;
    p.y = ts->yraw;
    p.z = ts->zraw;

    while (HAL_GPIO_ReadPin(ts->irq_port, ts->irq_pin) == GPIO_PIN_RESET)
        osDelay(10);

    return p;
}
