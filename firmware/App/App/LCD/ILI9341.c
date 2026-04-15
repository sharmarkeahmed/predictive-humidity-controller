#include "cmsis_os.h"
#include "ILI9341.h"

/* =========================================================================
 * Low-level helpers
 * All drawing keeps CS low for the entire transaction.
 * ========================================================================= */

#define CS_LOW(dev)  HAL_GPIO_WritePin((dev)->cs_port, (dev)->cs_pin, GPIO_PIN_RESET)
#define CS_HIGH(dev) HAL_GPIO_WritePin((dev)->cs_port, (dev)->cs_pin, GPIO_PIN_SET)
#define DC_CMD(dev)  HAL_GPIO_WritePin((dev)->dc_port, (dev)->dc_pin, GPIO_PIN_RESET)
#define DC_DAT(dev)  HAL_GPIO_WritePin((dev)->dc_port, (dev)->dc_pin, GPIO_PIN_SET)

/* Send one command byte — CS must already be low */
static inline void write_cmd(ILI9341_t3 *dev, uint8_t cmd)
{
    DC_CMD(dev);
    HAL_SPI_Transmit(dev->hspi, &cmd, 1, HAL_MAX_DELAY);
}

/* Send one data byte — CS must already be low */
static inline void write_dat8(ILI9341_t3 *dev, uint8_t dat)
{
    DC_DAT(dev);
    HAL_SPI_Transmit(dev->hspi, &dat, 1, HAL_MAX_DELAY);
}

/* Send one 16-bit data word — CS must already be low */
static inline void write_dat16(ILI9341_t3 *dev, uint16_t dat)
{
    uint8_t buf[2] = { (uint8_t)(dat >> 8), (uint8_t)(dat & 0xFF) };
    DC_DAT(dev);
    HAL_SPI_Transmit(dev->hspi, buf, 2, HAL_MAX_DELAY);
}

/*
 * Set GRAM address window.
 * CS must be low on entry and remains low on exit.
 * DC is left high (data) on exit so the caller can immediately
 * start streaming pixel data.
 */
static void set_addr_window(ILI9341_t3 *dev,
                            uint16_t x0, uint16_t y0,
                            uint16_t x1, uint16_t y1)
{
    /* Column address set */
    write_cmd(dev, ILI9341_CASET);
    DC_DAT(dev);
    uint8_t caset[4] = {
        (uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF),
        (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF)
    };
    HAL_SPI_Transmit(dev->hspi, caset, 4, HAL_MAX_DELAY);

    /* Page address set */
    write_cmd(dev, ILI9341_PASET);
    DC_DAT(dev);
    uint8_t paset[4] = {
        (uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF),
        (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF)
    };
    HAL_SPI_Transmit(dev->hspi, paset, 4, HAL_MAX_DELAY);

    /* Memory write — after this DC stays high for pixel data */
    write_cmd(dev, ILI9341_RAMWR);
    DC_DAT(dev);
}

/* =========================================================================
 * Init command table
 * ========================================================================= */
static const uint8_t ili9341_init_cmds[] = {
    4, 0xEF, 0x03, 0x80, 0x02,
    4, 0xCF, 0x00, 0xC1, 0x30,
    5, 0xED, 0x64, 0x03, 0x12, 0x81,
    4, 0xE8, 0x85, 0x00, 0x78,
    6, 0xCB, 0x39, 0x2C, 0x00, 0x34, 0x02,
    2, 0xF7, 0x20,
    3, 0xEA, 0x00, 0x00,
    2, ILI9341_PWCTR1,   0x23,
    2, ILI9341_PWCTR2,   0x10,
    3, ILI9341_VMCTR1,   0x3E, 0x28,
    2, ILI9341_VMCTR2,   0x86,
    2, ILI9341_MADCTL,   0x48,
    2, ILI9341_PIXFMT,   0x55,
    3, ILI9341_FRMCTR1,  0x00, 0x18,
    4, ILI9341_DFUNCTR,  0x08, 0x82, 0x27,
    2, 0xF2, 0x00,
    2, ILI9341_GAMMASET, 0x01,
    16, ILI9341_GMCTRP1,
        0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08,
        0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00,
    16, ILI9341_GMCTRN1,
        0x00, 0x0E, 0x14, 0x03, 0x11, 0x07,
        0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F,
    3, 0xB1, 0x00, 0x10,
    0
};

/* =========================================================================
 * Public API
 * ========================================================================= */

void ILI9341_init(ILI9341_t3 *dev, SPI_HandleTypeDef *hspi,
                  GPIO_TypeDef *cs_port,  uint16_t cs_pin,
                  GPIO_TypeDef *dc_port,  uint16_t dc_pin,
                  GPIO_TypeDef *rst_port, uint16_t rst_pin)
{
    dev->hspi      = hspi;
    dev->cs_port   = cs_port;   dev->cs_pin  = cs_pin;
    dev->dc_port   = dc_port;   dev->dc_pin  = dc_pin;
    dev->rst_port  = rst_port;  dev->rst_pin = rst_pin;
    dev->_width    = ILI9341_TFTWIDTH;
    dev->_height   = ILI9341_TFTHEIGHT;
    dev->rotation  = 0;
    dev->cursor_x  = dev->cursor_y = 0;
    dev->textsize  = 1;
    dev->textcolor = dev->textbgcolor = 0xFFFF;
    dev->wrap      = true;
    dev->font      = NULL;
}

void ILI9341_begin(ILI9341_t3 *dev)
{
    /*
     * Hard reset must be done by the caller (StartLCDTask) using osDelay()
     * before calling ILI9341_begin() — HAL_Delay() blocks the FreeRTOS
     * scheduler and must not be used inside a task.
     */

    CS_LOW(dev);

    /* Walk init table — entire sequence sent with CS held low */
    const uint8_t *p = ili9341_init_cmds;
    while (1) {
        uint8_t cnt = *p++;
        if (cnt == 0) break;
        cnt--;                          /* cnt is now number of data bytes */
        write_cmd(dev, *p++);           /* command byte */
        DC_DAT(dev);
        while (cnt-- > 0) {
            uint8_t d = *p++;
            HAL_SPI_Transmit(dev->hspi, &d, 1, HAL_MAX_DELAY);
        }
    }

    write_cmd(dev, ILI9341_SLPOUT);
    CS_HIGH(dev);
    osDelay(120);

    CS_LOW(dev);
    write_cmd(dev, ILI9341_DISPON);
    CS_HIGH(dev);
}

/* ---- address window / push ---- */

void ILI9341_setAddrWindow(ILI9341_t3 *dev,
                           uint16_t x0, uint16_t y0,
                           uint16_t x1, uint16_t y1)
{
    CS_LOW(dev);
    set_addr_window(dev, x0, y0, x1, y1);
    CS_HIGH(dev);
}

void ILI9341_pushColor(ILI9341_t3 *dev, uint16_t color)
{
    CS_LOW(dev);
    write_dat16(dev, color);
    CS_HIGH(dev);
}

/* ---- pixel primitives ---- */

void ILI9341_drawPixel(ILI9341_t3 *dev, int16_t x, int16_t y, uint16_t color)
{
    if (x < 0 || x >= dev->_width || y < 0 || y >= dev->_height) return;
    CS_LOW(dev);
    set_addr_window(dev, (uint16_t)x, (uint16_t)y,
                         (uint16_t)x, (uint16_t)y);
    uint8_t buf[2] = { (uint8_t)(color >> 8), (uint8_t)(color & 0xFF) };
    HAL_SPI_Transmit(dev->hspi, buf, 2, HAL_MAX_DELAY);
    CS_HIGH(dev);
}

void ILI9341_drawFastVLine(ILI9341_t3 *dev,
                           int16_t x, int16_t y, int16_t h, uint16_t color)
{
    if (x < 0 || x >= dev->_width || y >= dev->_height) return;
    if (y < 0) { h += y; y = 0; }
    if (y + h > dev->_height) h = dev->_height - y;
    if (h <= 0) return;

    uint8_t buf[2] = { (uint8_t)(color >> 8), (uint8_t)(color & 0xFF) };
    CS_LOW(dev);
    set_addr_window(dev, (uint16_t)x,     (uint16_t)y,
                         (uint16_t)x, (uint16_t)(y + h - 1));
    while (h-- > 0)
        HAL_SPI_Transmit(dev->hspi, buf, 2, HAL_MAX_DELAY);
    CS_HIGH(dev);
}

void ILI9341_drawFastHLine(ILI9341_t3 *dev,
                           int16_t x, int16_t y, int16_t w, uint16_t color)
{
    if (y < 0 || y >= dev->_height || x >= dev->_width) return;
    if (x < 0) { w += x; x = 0; }
    if (x + w > dev->_width) w = dev->_width - x;
    if (w <= 0) return;

    uint8_t buf[2] = { (uint8_t)(color >> 8), (uint8_t)(color & 0xFF) };
    CS_LOW(dev);
    set_addr_window(dev, (uint16_t)x,         (uint16_t)y,
                         (uint16_t)(x + w - 1), (uint16_t)y);
    while (w-- > 0)
        HAL_SPI_Transmit(dev->hspi, buf, 2, HAL_MAX_DELAY);
    CS_HIGH(dev);
}

void ILI9341_fillScreen(ILI9341_t3 *dev, uint16_t color)
{
    uint8_t buf[2] = { (uint8_t)(color >> 8), (uint8_t)(color & 0xFF) };
    uint32_t pixel_count = (uint32_t)dev->_width * dev->_height;

    CS_LOW(dev);

    /* Set full screen window based on current dimensions */
    write_cmd(dev, ILI9341_CASET);
    DC_DAT(dev);
    uint8_t caset[4] = {
        0x00, 0x00,                           /* start col = 0 */
        (uint8_t)((dev->_width - 1) >> 8),    /* end col high byte */
        (uint8_t)((dev->_width - 1) & 0xFF)   /* end col low byte */
    };
    HAL_SPI_Transmit(dev->hspi, caset, 4, HAL_MAX_DELAY);

    write_cmd(dev, ILI9341_PASET);
    DC_DAT(dev);
    uint8_t paset[4] = {
        0x00, 0x00,                           /* start row = 0 */
        (uint8_t)((dev->_height - 1) >> 8),   /* end row high byte */
        (uint8_t)((dev->_height - 1) & 0xFF)  /* end row low byte */
    };
    HAL_SPI_Transmit(dev->hspi, paset, 4, HAL_MAX_DELAY);

    write_cmd(dev, ILI9341_RAMWR);
    DC_DAT(dev);

    /* Fill all pixels */
    for (uint32_t i = 0; i < pixel_count; i++)
        HAL_SPI_Transmit(dev->hspi, buf, 2, HAL_MAX_DELAY);

    CS_HIGH(dev);
}

void ILI9341_fillRect(ILI9341_t3 *dev,
                      int16_t x, int16_t y, int16_t w, int16_t h,
                      uint16_t color)
{
    if (x >= dev->_width || y >= dev->_height) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > dev->_width)  w = dev->_width  - x;
    if (y + h > dev->_height) h = dev->_height - y;
    if (w <= 0 || h <= 0) return;

    uint8_t buf[2] = { (uint8_t)(color >> 8), (uint8_t)(color & 0xFF) };
    CS_LOW(dev);
    set_addr_window(dev, (uint16_t)x, (uint16_t)y,
                         (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));
    for (int32_t n = (int32_t)w * h; n > 0; n--)
        HAL_SPI_Transmit(dev->hspi, buf, 2, HAL_MAX_DELAY);
    CS_HIGH(dev);
}

/* ---- display control ---- */

//#define MADCTL_MY  0x80
//#define MADCTL_MX  0x40
//#define MADCTL_MV  0x20
//#define MADCTL_BGR 0x08

#define MADCTL_MY  0x80
#define MADCTL_MX  0x40
#define MADCTL_MV  0x20
#define MADCTL_ML  0x10
#define MADCTL_BGR 0x08
#define MADCTL_MH  0x04

//void ILI9341_setRotation(ILI9341_t3 *dev, uint8_t m)
//{
//    dev->rotation = m % 4;
//    uint8_t madctl = 0;
//
//    CS_LOW(dev);
//    write_cmd(dev, ILI9341_MADCTL);
//
//    /* Adjust MADCTL based on rotation */
//    switch (dev->rotation) {
//    case 0:  /* Portrait */
//        madctl = MADCTL_MX | MADCTL_BGR;  /* 0x48 */
//        dev->_width  = ILI9341_TFTWIDTH;
//        dev->_height = ILI9341_TFTHEIGHT;
//        break;
//    case 1:  /* Landscape (rotate right 90°) */
//        madctl = MADCTL_MV | MADCTL_BGR;  /* 0x28 */
//        dev->_width  = ILI9341_TFTHEIGHT;
//        dev->_height = ILI9341_TFTWIDTH;
//        break;
//    case 2:  /* Portrait flipped */
//        madctl = MADCTL_MY | MADCTL_BGR;  /* 0x88 */
//        dev->_width  = ILI9341_TFTWIDTH;
//        dev->_height = ILI9341_TFTHEIGHT;
//        break;
//    case 3:  /* Landscape flipped (rotate left 90°) */
//        madctl = MADCTL_MV | MADCTL_MY | MADCTL_BGR;  /* 0xA8 */
//        dev->_width  = ILI9341_TFTHEIGHT;
//        dev->_height = ILI9341_TFTWIDTH;
//        break;
//    }
//
//    write_dat8(dev, madctl);
//    CS_HIGH(dev);
//
//    /* Reset cursor position */
//    dev->cursor_x = 0;
//    dev->cursor_y = 0;
//}

void ILI9341_setRotation(ILI9341_t3 *dev, uint8_t m)
{
    dev->rotation = m % 4;
    uint8_t madctl = 0;

    /* Set BGR color order (common for ILI9341) */
    madctl |= MADCTL_BGR;

    CS_LOW(dev);
    write_cmd(dev, ILI9341_MADCTL);

    switch (dev->rotation) {
    case 0:  /* Portrait 0° */
        madctl |= MADCTL_MX;  /* 0x48 - MX + BGR */
        dev->_width  = ILI9341_TFTWIDTH;
        dev->_height = ILI9341_TFTHEIGHT;
        break;

    case 1:  /* Landscape 90° (rotate right) */
        madctl |= MADCTL_MV;  /* 0x28 - MV + BGR */
        dev->_width  = ILI9341_TFTHEIGHT;
        dev->_height = ILI9341_TFTWIDTH;
        break;

    case 2:  /* Portrait 180° */
        madctl |= MADCTL_MY;  /* 0x88 - MY + BGR */
        dev->_width  = ILI9341_TFTWIDTH;
        dev->_height = ILI9341_TFTHEIGHT;
        break;

    case 3:  /* Landscape 270° (rotate left) */
        madctl |= MADCTL_MV | MADCTL_MY;  /* 0xA8 - MV + MY + BGR */
        dev->_width  = ILI9341_TFTHEIGHT;
        dev->_height = ILI9341_TFTWIDTH;
        break;
    }

    write_dat8(dev, madctl);
    CS_HIGH(dev);

    /* Reset cursor position */
    dev->cursor_x = 0;
    dev->cursor_y = 0;
}

void ILI9341_invertDisplay(ILI9341_t3 *dev, bool i)
{
    CS_LOW(dev);
    write_cmd(dev, i ? ILI9341_INVON : ILI9341_INVOFF);
    CS_HIGH(dev);
}

void ILI9341_setScroll(ILI9341_t3 *dev, uint16_t offset)
{
    CS_LOW(dev);
    write_cmd(dev, ILI9341_VSCRSADD);
    write_dat16(dev, offset);
    CS_HIGH(dev);
}

void ILI9341_sleep(ILI9341_t3 *dev, bool enable)
{
    CS_LOW(dev);
    if (enable) {
        write_cmd(dev, ILI9341_DISPOFF);
        write_cmd(dev, ILI9341_SLPIN);
    } else {
        write_cmd(dev, ILI9341_DISPON);
        write_cmd(dev, ILI9341_SLPOUT);
    }
    CS_HIGH(dev);
    if (!enable) osDelay(5);
}

/* ---- shapes ---- */

static void fill_circle_helper(ILI9341_t3 *dev,
                               int16_t x0, int16_t y0, int16_t r,
                               uint8_t corners, int16_t delta, uint16_t color)
{
    int16_t f = 1 - r, ddx = 1, ddy = -2 * r, x = 0, y = r;
    while (x < y) {
        if (f >= 0) { y--; ddy += 2; f += ddy; }
        x++; ddx += 2; f += ddx;
        if (corners & 0x1) {
            ILI9341_drawFastVLine(dev, x0+x, y0-y, 2*y+1+delta, color);
            ILI9341_drawFastVLine(dev, x0+y, y0-x, 2*x+1+delta, color);
        }
        if (corners & 0x2) {
            ILI9341_drawFastVLine(dev, x0-x, y0-y, 2*y+1+delta, color);
            ILI9341_drawFastVLine(dev, x0-y, y0-x, 2*x+1+delta, color);
        }
    }
}

static void draw_circle_helper(ILI9341_t3 *dev,
                               int16_t x0, int16_t y0,
                               int16_t r, uint8_t corners, uint16_t color)
{
    int16_t f = 1-r, ddx = 1, ddy = -2*r, x = 0, y = r, xold = 0;
    while (x < y) {
        if (f >= 0) { y--; ddy += 2; f += ddy; }
        x++; ddx += 2; f += ddx;
        if (f >= 0 || x == y) {
            if (corners & 0x4) {
                ILI9341_drawFastHLine(dev, x0+xold+1, y0+y,   x-xold, color);
                ILI9341_drawFastVLine(dev, x0+y, y0+xold+1,   x-xold, color);
            }
            if (corners & 0x2) {
                ILI9341_drawFastHLine(dev, x0+xold+1, y0-y,   x-xold, color);
                ILI9341_drawFastVLine(dev, x0+y, y0-x,        x-xold, color);
            }
            if (corners & 0x8) {
                ILI9341_drawFastVLine(dev, x0-y, y0+xold+1,   x-xold, color);
                ILI9341_drawFastHLine(dev, x0-x, y0+y,        x-xold, color);
            }
            if (corners & 0x1) {
                ILI9341_drawFastVLine(dev, x0-y, y0-x,        x-xold, color);
                ILI9341_drawFastHLine(dev, x0-x, y0-y,        x-xold, color);
            }
            xold = x;
        }
    }
}

void ILI9341_drawCircle(ILI9341_t3 *dev,
                        int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
    int16_t f = 1-r, ddx = 1, ddy = -2*r, x = 0, y = r;
    ILI9341_drawPixel(dev, x0,   y0+r, color);
    ILI9341_drawPixel(dev, x0,   y0-r, color);
    ILI9341_drawPixel(dev, x0+r, y0,   color);
    ILI9341_drawPixel(dev, x0-r, y0,   color);
    while (x < y) {
        if (f >= 0) { y--; ddy += 2; f += ddy; }
        x++; ddx += 2; f += ddx;
        ILI9341_drawPixel(dev, x0+x, y0+y, color);
        ILI9341_drawPixel(dev, x0-x, y0+y, color);
        ILI9341_drawPixel(dev, x0+x, y0-y, color);
        ILI9341_drawPixel(dev, x0-x, y0-y, color);
        ILI9341_drawPixel(dev, x0+y, y0+x, color);
        ILI9341_drawPixel(dev, x0-y, y0+x, color);
        ILI9341_drawPixel(dev, x0+y, y0-x, color);
        ILI9341_drawPixel(dev, x0-y, y0-x, color);
    }
}

void ILI9341_fillCircle(ILI9341_t3 *dev,
                        int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
    ILI9341_drawFastVLine(dev, x0, y0-r, 2*r+1, color);
    fill_circle_helper(dev, x0, y0, r, 3, 0, color);
}

void ILI9341_drawRect(ILI9341_t3 *dev,
                      int16_t x, int16_t y, int16_t w, int16_t h,
                      uint16_t color)
{
    ILI9341_drawFastHLine(dev, x,     y,     w, color);
    ILI9341_drawFastHLine(dev, x,     y+h-1, w, color);
    ILI9341_drawFastVLine(dev, x,     y,     h, color);
    ILI9341_drawFastVLine(dev, x+w-1, y,     h, color);
}

void ILI9341_drawRoundRect(ILI9341_t3 *dev,
                           int16_t x, int16_t y, int16_t w, int16_t h,
                           int16_t r, uint16_t color)
{
    ILI9341_drawFastHLine(dev, x+r,   y,     w-2*r, color);
    ILI9341_drawFastHLine(dev, x+r,   y+h-1, w-2*r, color);
    ILI9341_drawFastVLine(dev, x,     y+r,   h-2*r, color);
    ILI9341_drawFastVLine(dev, x+w-1, y+r,   h-2*r, color);
    draw_circle_helper(dev, x+r,     y+r,     r, 1, color);
    draw_circle_helper(dev, x+w-r-1, y+r,     r, 2, color);
    draw_circle_helper(dev, x+w-r-1, y+h-r-1, r, 4, color);
    draw_circle_helper(dev, x+r,     y+h-r-1, r, 8, color);
}

void ILI9341_fillRoundRect(ILI9341_t3 *dev,
                           int16_t x, int16_t y, int16_t w, int16_t h,
                           int16_t r, uint16_t color)
{
    ILI9341_fillRect(dev, x+r, y, w-2*r, h, color);
    fill_circle_helper(dev, x+w-r-1, y+r, r, 1, h-2*r-1, color);
    fill_circle_helper(dev, x+r,     y+r, r, 2, h-2*r-1, color);
}

void ILI9341_drawLine(ILI9341_t3 *dev,
                      int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      uint16_t color)
{
    if (y0 == y1) {
        if (x1 > x0) ILI9341_drawFastHLine(dev, x0, y0, x1-x0+1, color);
        else if (x1 < x0) ILI9341_drawFastHLine(dev, x1, y0, x0-x1+1, color);
        else ILI9341_drawPixel(dev, x0, y0, color);
        return;
    }
    if (x0 == x1) {
        if (y1 > y0) ILI9341_drawFastVLine(dev, x0, y0, y1-y0+1, color);
        else         ILI9341_drawFastVLine(dev, x0, y1, y0-y1+1, color);
        return;
    }
    bool steep = abs(y1-y0) > abs(x1-x0);
    if (steep)   { int16_t t; t=x0;x0=y0;y0=t; t=x1;x1=y1;y1=t; }
    if (x0 > x1) { int16_t t; t=x0;x0=x1;x1=t; t=y0;y0=y1;y1=t; }
    int16_t dx=x1-x0, dy=abs(y1-y0), err=dx/2,
            ystep=(y0<y1)?1:-1, xbegin=x0;
    for (; x0<=x1; x0++) {
        err -= dy;
        if (err < 0) {
            int16_t len = x0-xbegin;
            if (steep) {
                if (len) ILI9341_drawFastVLine(dev, y0, xbegin, len+1, color);
                else     ILI9341_drawPixel(dev, y0, x0, color);
            } else {
                if (len) ILI9341_drawFastHLine(dev, xbegin, y0, len+1, color);
                else     ILI9341_drawPixel(dev, x0, y0, color);
            }
            xbegin = x0+1; y0 += ystep; err += dx;
        }
    }
    int16_t len = x0-xbegin;
    if (len > 1) {
        if (steep) ILI9341_drawFastVLine(dev, y0, xbegin, len, color);
        else       ILI9341_drawFastHLine(dev, xbegin, y0, len, color);
    }
}

void ILI9341_drawTriangle(ILI9341_t3 *dev,
                          int16_t x0, int16_t y0,
                          int16_t x1, int16_t y1,
                          int16_t x2, int16_t y2, uint16_t color)
{
    ILI9341_drawLine(dev, x0, y0, x1, y1, color);
    ILI9341_drawLine(dev, x1, y1, x2, y2, color);
    ILI9341_drawLine(dev, x2, y2, x0, y0, color);
}

#define _SWAP(a,b) { int16_t t=(a);(a)=(b);(b)=t; }

void ILI9341_fillTriangle(ILI9341_t3 *dev,
                          int16_t x0, int16_t y0,
                          int16_t x1, int16_t y1,
                          int16_t x2, int16_t y2, uint16_t color)
{
    if (y0>y1){_SWAP(y0,y1);_SWAP(x0,x1);}
    if (y1>y2){_SWAP(y2,y1);_SWAP(x2,x1);}
    if (y0>y1){_SWAP(y0,y1);_SWAP(x0,x1);}
    if (y0==y2) {
        int16_t a=x0,b=x0;
        if(x1<a)a=x1; else if(x1>b)b=x1;
        if(x2<a)a=x2; else if(x2>b)b=x2;
        ILI9341_drawFastHLine(dev,a,y0,b-a+1,color); return;
    }
    int32_t dx01=x1-x0,dy01=y1-y0,dx02=x2-x0,dy02=y2-y0,
            dx12=x2-x1,dy12=y2-y1,sa=0,sb=0;
    int16_t last=(y1==y2)?y1:y1-1, y;
    for(y=y0;y<=last;y++){
        int16_t a2=(int16_t)(x0+sa/dy01), b2=(int16_t)(x0+sb/dy02);
        sa+=dx01; sb+=dx02;
        if(a2>b2)_SWAP(a2,b2);
        ILI9341_drawFastHLine(dev,a2,y,b2-a2+1,color);
    }
    sa=dx12*(y-y1); sb=dx02*(y-y0);
    for(;y<=y2;y++){
        int16_t a2=(int16_t)(x1+sa/dy12), b2=(int16_t)(x0+sb/dy02);
        sa+=dx12; sb+=dx02;
        if(a2>b2)_SWAP(a2,b2);
        ILI9341_drawFastHLine(dev,a2,y,b2-a2+1,color);
    }
}

void ILI9341_drawBitmap(ILI9341_t3 *dev,
                        int16_t x, int16_t y,
                        const uint8_t *bitmap, int16_t w, int16_t h,
                        uint16_t color)
{
    int16_t bw = (w+7)/8;
    for (int16_t j=0; j<h; j++)
        for (int16_t i=0; i<w; i++)
            if (bitmap[j*bw+i/8] & (128>>(i&7)))
                ILI9341_drawPixel(dev, x+i, y+j, color);
}

/* ---- gradients ---- */

void ILI9341_fillRectVGradient(ILI9341_t3 *dev,
                               int16_t x, int16_t y, int16_t w, int16_t h,
                               uint16_t color1, uint16_t color2)
{
    if (x>=dev->_width||y>=dev->_height) return;
    if (x+w>dev->_width)  w=dev->_width-x;
    if (y+h>dev->_height) h=dev->_height-y;
    if (h<=0) return;
    int16_t r1,g1,b1,r2,g2,b2,dr,dg,db,r,g,b;
    color565toRGB14(color1,&r1,&g1,&b1);
    color565toRGB14(color2,&r2,&g2,&b2);
    dr=(r2-r1)/h; dg=(g2-g1)/h; db=(b2-b1)/h;
    r=r1; g=g1; b=b1;
    CS_LOW(dev);
    set_addr_window(dev,(uint16_t)x,(uint16_t)y,
                        (uint16_t)(x+w-1),(uint16_t)(y+h-1));
    for (int16_t row=h;row>0;row--) {
        uint16_t c=RGB14tocolor565(r,g,b);
        uint8_t buf[2]={(uint8_t)(c>>8),(uint8_t)(c&0xFF)};
        for (int16_t col=w;col>0;col--)
            HAL_SPI_Transmit(dev->hspi,buf,2,HAL_MAX_DELAY);
        r+=dr; g+=dg; b+=db;
    }
    CS_HIGH(dev);
}

void ILI9341_fillRectHGradient(ILI9341_t3 *dev,
                               int16_t x, int16_t y, int16_t w, int16_t h,
                               uint16_t color1, uint16_t color2)
{
    if (x>=dev->_width||y>=dev->_height) return;
    if (x+w>dev->_width)  w=dev->_width-x;
    if (y+h>dev->_height) h=dev->_height-y;
    if (h<=0) return;
    int16_t r1,g1,b1,r2,g2,b2,dr,dg,db,r,g,b;
    color565toRGB14(color1,&r1,&g1,&b1);
    color565toRGB14(color2,&r2,&g2,&b2);
    dr=(r2-r1)/w; dg=(g2-g1)/w; db=(b2-b1)/w;
    CS_LOW(dev);
    set_addr_window(dev,(uint16_t)x,(uint16_t)y,
                        (uint16_t)(x+w-1),(uint16_t)(y+h-1));
    for (int16_t row=h;row>0;row--) {
        r=r1; g=g1; b=b1;
        for (int16_t col=w;col>0;col--) {
            uint16_t c=RGB14tocolor565(r,g,b);
            uint8_t buf[2]={(uint8_t)(c>>8),(uint8_t)(c&0xFF)};
            HAL_SPI_Transmit(dev->hspi,buf,2,HAL_MAX_DELAY);
            r+=dr; g+=dg; b+=db;
        }
    }
    CS_HIGH(dev);
}

void ILI9341_fillScreenVGradient(ILI9341_t3 *dev, uint16_t c1, uint16_t c2)
{ ILI9341_fillRectVGradient(dev,0,0,dev->_width,dev->_height,c1,c2); }

void ILI9341_fillScreenHGradient(ILI9341_t3 *dev, uint16_t c1, uint16_t c2)
{ ILI9341_fillRectHGradient(dev,0,0,dev->_width,dev->_height,c1,c2); }

/* ---- bulk pixel writers ---- */

void ILI9341_writeRect(ILI9341_t3 *dev,
                       int16_t x, int16_t y, int16_t w, int16_t h,
                       const uint16_t *pcolors)
{
    CS_LOW(dev);
    set_addr_window(dev,(uint16_t)x,(uint16_t)y,
                        (uint16_t)(x+w-1),(uint16_t)(y+h-1));
    for (int32_t n=(int32_t)w*h; n>0; n--) {
        uint8_t buf[2]={(uint8_t)(*pcolors>>8),(uint8_t)(*pcolors&0xFF)};
        pcolors++;
        HAL_SPI_Transmit(dev->hspi,buf,2,HAL_MAX_DELAY);
    }
    CS_HIGH(dev);
}

void ILI9341_writeRect8BPP(ILI9341_t3 *dev,
                           int16_t x, int16_t y, int16_t w, int16_t h,
                           const uint8_t *pixels, const uint16_t *palette)
{
    CS_LOW(dev);
    set_addr_window(dev,(uint16_t)x,(uint16_t)y,
                        (uint16_t)(x+w-1),(uint16_t)(y+h-1));
    for (int32_t n=(int32_t)w*h; n>0; n--) {
        uint16_t c=palette[*pixels++];
        uint8_t buf[2]={(uint8_t)(c>>8),(uint8_t)(c&0xFF)};
        HAL_SPI_Transmit(dev->hspi,buf,2,HAL_MAX_DELAY);
    }
    CS_HIGH(dev);
}

void ILI9341_writeRect4BPP(ILI9341_t3 *dev,
                           int16_t x, int16_t y, int16_t w, int16_t h,
                           const uint8_t *pixels, const uint16_t *palette)
{
    CS_LOW(dev);
    set_addr_window(dev,(uint16_t)x,(uint16_t)y,
                        (uint16_t)(x+w-1),(uint16_t)(y+h-1));
    for (int16_t row=h; row>0; row--)
        for (int16_t col=w; col>1; col-=2) {
            uint16_t ca=palette[(*pixels>>4)&0xF];
            uint16_t cb=palette[*pixels++&0xF];
            uint8_t ba[2]={(uint8_t)(ca>>8),(uint8_t)(ca&0xFF)};
            uint8_t bb[2]={(uint8_t)(cb>>8),(uint8_t)(cb&0xFF)};
            HAL_SPI_Transmit(dev->hspi,ba,2,HAL_MAX_DELAY);
            HAL_SPI_Transmit(dev->hspi,bb,2,HAL_MAX_DELAY);
        }
    CS_HIGH(dev);
}

void ILI9341_writeRect2BPP(ILI9341_t3 *dev,
                           int16_t x, int16_t y, int16_t w, int16_t h,
                           const uint8_t *pixels, const uint16_t *palette)
{
    CS_LOW(dev);
    set_addr_window(dev,(uint16_t)x,(uint16_t)y,
                        (uint16_t)(x+w-1),(uint16_t)(y+h-1));
    for (int16_t row=h; row>0; row--)
        for (int16_t col=w; col>0; col-=4) {
            for (int s=6; s>=0; s-=2) {
                uint16_t c=palette[(*pixels>>s)&0x3];
                uint8_t buf[2]={(uint8_t)(c>>8),(uint8_t)(c&0xFF)};
                HAL_SPI_Transmit(dev->hspi,buf,2,HAL_MAX_DELAY);
            }
            pixels++;
        }
    CS_HIGH(dev);
}

void ILI9341_writeRect1BPP(ILI9341_t3 *dev,
                           int16_t x, int16_t y, int16_t w, int16_t h,
                           const uint8_t *pixels, const uint16_t *palette)
{
    CS_LOW(dev);
    set_addr_window(dev,(uint16_t)x,(uint16_t)y,
                        (uint16_t)(x+w-1),(uint16_t)(y+h-1));
    for (int16_t row=h; row>0; row--)
        for (int16_t col=w; col>0; col-=8) {
            for (int s=7; s>=0; s--) {
                uint16_t c=palette[(*pixels>>s)&0x1];
                uint8_t buf[2]={(uint8_t)(c>>8),(uint8_t)(c&0xFF)};
                HAL_SPI_Transmit(dev->hspi,buf,2,HAL_MAX_DELAY);
            }
            pixels++;
        }
    CS_HIGH(dev);
}

/* ---- text ---- */

extern const unsigned char glcdfont[];

void ILI9341_drawChar(ILI9341_t3 *dev,
                      int16_t x, int16_t y, unsigned char c,
                      uint16_t fgcolor, uint16_t bgcolor, uint8_t size)
{
    if (x>=dev->_width||y>=dev->_height||
        (x+6*size-1)<0||(y+8*size-1)<0) return;

    if (fgcolor == bgcolor) {
        uint8_t mask=0x1;
        for (int16_t yoff=0; yoff<8; yoff++) {
            int16_t xoff=0;
            while (xoff<5) {
                while (xoff<5 && !(glcdfont[c*5+xoff]&mask)) xoff++;
                int16_t cnt=0;
                while (xoff<5 && (glcdfont[c*5+xoff]&mask)){cnt++;xoff++;}
                if (cnt) {
                    if (size==1)
                        ILI9341_drawFastHLine(dev,x+xoff-cnt,y+yoff,cnt,fgcolor);
                    else
                        ILI9341_fillRect(dev,x+(xoff-cnt)*size,y+yoff*size,
                                         cnt*size,size,fgcolor);
                }
            }
            mask<<=1;
        }
    } else {
        uint8_t fg_hi=(uint8_t)(fgcolor>>8), fg_lo=(uint8_t)(fgcolor&0xFF);
        uint8_t bg_hi=(uint8_t)(bgcolor>>8), bg_lo=(uint8_t)(bgcolor&0xFF);
        CS_LOW(dev);
        set_addr_window(dev,(uint16_t)x,(uint16_t)y,
                            (uint16_t)(x+6*size-1),(uint16_t)(y+8*size-1));
        uint8_t mask=0x01;
        for (int yr=0; yr<8; yr++) {
            for (uint8_t ys=0; ys<size; ys++) {
                for (int xr=0; xr<5; xr++) {
                    bool lit = (glcdfont[c*5+xr] & mask) != 0;
                    uint8_t buf[2] = { lit?fg_hi:bg_hi, lit?fg_lo:bg_lo };
                    for (uint8_t xs=0; xs<size; xs++)
                        HAL_SPI_Transmit(dev->hspi,buf,2,HAL_MAX_DELAY);
                }
                uint8_t bg[2]={bg_hi,bg_lo};
                for (uint8_t xs=0; xs<size; xs++)
                    HAL_SPI_Transmit(dev->hspi,bg,2,HAL_MAX_DELAY);
            }
            mask<<=1;
        }
        CS_HIGH(dev);
    }
}

void ILI9341_writeChar(ILI9341_t3 *dev, uint8_t c)
{
    if (dev->font) {
        if (c=='\n'){dev->cursor_y+=dev->font->line_space; dev->cursor_x=0;}
    } else {
        if (c=='\n') {
            dev->cursor_y+=dev->textsize*8; dev->cursor_x=0;
        } else if (c!='\r') {
            ILI9341_drawChar(dev,dev->cursor_x,dev->cursor_y,c,
                             dev->textcolor,dev->textbgcolor,dev->textsize);
            dev->cursor_x+=dev->textsize*6;
            if (dev->wrap && dev->cursor_x>(dev->_width-dev->textsize*6)){
                dev->cursor_y+=dev->textsize*8; dev->cursor_x=0;
            }
        }
    }
}

void ILI9341_writeString(ILI9341_t3 *dev, const char *str)
{ while (*str) ILI9341_writeChar(dev,(uint8_t)*str++); }

int16_t ILI9341_strPixelLen(ILI9341_t3 *dev, const char *str)
{
    if (!str) return 0;
    uint16_t len=0,maxlen=0;
    while (*str) {
        if (*str=='\n'){if(len>maxlen)maxlen=len;len=0;}
        else {len+=dev->textsize*6;if(len>maxlen)maxlen=len;}
        str++;
    }
    return (int16_t)maxlen;
}

uint16_t ILI9341_measureTextWidth(ILI9341_t3 *dev, const char *text, int num)
{
    uint16_t maxW=0,currW=0;
    int n=num?num:(int)strlen(text);
    for (int i=0;i<n;i++) {
        if (text[i]=='\n'){if(currW>maxW)maxW=currW;currW=0;}
        else currW+=dev->textsize*6;
    }
    return (maxW>currW)?maxW:currW;
}

uint16_t ILI9341_measureTextHeight(ILI9341_t3 *dev, const char *text, int num)
{
    int lines=1, n=num?num:(int)strlen(text);
    for (int i=0;i<n;i++) if(text[i]=='\n') lines++;
    uint8_t lsp=dev->font?dev->font->line_space:dev->textsize*8;
    uint8_t cap=dev->font?dev->font->cap_height:dev->textsize*8;
    return (uint16_t)((lines-1)*lsp+cap);
}

/* ---- cursor / text settings ---- */

void ILI9341_setCursor(ILI9341_t3 *dev, int16_t x, int16_t y)
{
    dev->cursor_x=(x<0)?0:(x>=dev->_width?dev->_width-1:x);
    dev->cursor_y=(y<0)?0:(y>=dev->_height?dev->_height-1:y);
}
void ILI9341_getCursor(ILI9341_t3 *dev, int16_t *x, int16_t *y)
{ *x=dev->cursor_x; *y=dev->cursor_y; }
void    ILI9341_setTextSize(ILI9341_t3 *dev, uint8_t s)   { dev->textsize=(s>0)?s:1; }
uint8_t ILI9341_getTextSize(ILI9341_t3 *dev)               { return dev->textsize; }
void    ILI9341_setTextColor(ILI9341_t3 *dev, uint16_t c)  { dev->textcolor=dev->textbgcolor=c; }
void    ILI9341_setTextColorBG(ILI9341_t3 *dev, uint16_t c, uint16_t bg)
{ dev->textcolor=c; dev->textbgcolor=bg; }
void    ILI9341_setTextWrap(ILI9341_t3 *dev, bool w)       { dev->wrap=w; }
bool    ILI9341_getTextWrap(ILI9341_t3 *dev)               { return dev->wrap; }
void    ILI9341_setFont(ILI9341_t3 *dev, const ILI9341_t3_font_t *f) { dev->font=f; }

uint8_t ILI9341_getRotation(ILI9341_t3 *dev)  { return dev->rotation; }
int16_t ILI9341_width(ILI9341_t3 *dev)        { return dev->_width; }
int16_t ILI9341_height(ILI9341_t3 *dev)       { return dev->_height; }
uint8_t ILI9341_fontCapHeight(ILI9341_t3 *dev)
{ return dev->font?dev->font->cap_height:dev->textsize*8; }
uint8_t ILI9341_fontLineSpace(ILI9341_t3 *dev)
{ return dev->font?dev->font->line_space:dev->textsize*8; }

/* ---- button ---- */

void Button_init(Adafruit_GFX_Button *btn, ILI9341_t3 *gfx,
                 int16_t x, int16_t y, uint8_t w, uint8_t h,
                 uint16_t outline, uint16_t fill, uint16_t textcolor,
                 const char *label, uint8_t textsize)
{
    btn->_gfx=gfx; btn->_x=x; btn->_y=y;
    btn->_w=w; btn->_h=h; btn->_textsize=textsize;
    btn->_outlinecolor=outline; btn->_fillcolor=fill;
    btn->_textcolor=textcolor;
    strncpy(btn->_label,label,9); btn->_label[9]='\0';
    btn->_inverted=false;
}

void Button_draw(Adafruit_GFX_Button *btn, bool inverted)
{
    uint16_t fill   = inverted?btn->_textcolor:btn->_fillcolor;
    uint16_t outline= btn->_outlinecolor;
    uint16_t text   = inverted?btn->_fillcolor:btn->_textcolor;
    uint8_t  r      = (btn->_w<btn->_h)?btn->_w/4:btn->_h/4;
    ILI9341_fillRoundRect(btn->_gfx,btn->_x-btn->_w/2,btn->_y-btn->_h/2,
                           btn->_w,btn->_h,r,fill);
    ILI9341_drawRoundRect(btn->_gfx,btn->_x-btn->_w/2,btn->_y-btn->_h/2,
                           btn->_w,btn->_h,r,outline);
    ILI9341_setCursor(btn->_gfx,
                      btn->_x-(int16_t)(strlen(btn->_label)*3*btn->_textsize),
                      btn->_y-4*btn->_textsize);
    ILI9341_setTextColor(btn->_gfx,text);
    ILI9341_setTextSize(btn->_gfx,btn->_textsize);
    ILI9341_writeString(btn->_gfx,btn->_label);
    btn->_inverted=inverted;
}

bool Button_contains(Adafruit_GFX_Button *btn, int16_t x, int16_t y)
{
    return !(x<(btn->_x-btn->_w/2)||x>(btn->_x+btn->_w/2)||
             y<(btn->_y-btn->_h/2)||y>(btn->_y+btn->_h/2));
}
