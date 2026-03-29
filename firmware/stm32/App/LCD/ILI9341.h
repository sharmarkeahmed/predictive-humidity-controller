#ifndef ILI9341_STM32_H
#define ILI9341_STM32_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

/* --------------------------------------------------------------------------
 * ILI9341_STM32.h  –  ILI9341 TFT driver for STM32F4 + STM32 HAL
 *
 * Ported from ILI9341_t3 (Teensy/Kinetik) to STM32 HAL (CubeMX).
 * All Teensy-specific SPI FIFO / KINETISK / IMXRT register access has been
 * replaced with HAL_SPI_Transmit / HAL_GPIO_WritePin calls.
 *
 * Usage
 * -----
 *  1. Create an SPI_HandleTypeDef in CubeMX (SPI1 or SPI2, full-duplex,
 *     8-bit, CPOL=0, CPHA=0, MSB first).
 *  2. Leave CS and DC as ordinary GPIO outputs (configured in CubeMX).
 *  3. Construct the driver:
 *       ILI9341_t3 tft;
 *       ILI9341_init(&tft, &hspi1,
 *                    CS_GPIO_Port,  CS_Pin,
 *                    DC_GPIO_Port,  DC_Pin,
 *                    RST_GPIO_Port, RST_Pin);   // pass NULL / 0 if no RST
 *  4. Call ILI9341_begin(&tft) once, then draw normally.
 * -------------------------------------------------------------------------- */

/* ---- ILI9341 command bytes ---- */
#define ILI9341_NOP        0x00
#define ILI9341_SWRESET    0x01
#define ILI9341_RDDID      0x04
#define ILI9341_RDDST      0x09
#define ILI9341_SLPIN      0x10
#define ILI9341_SLPOUT     0x11
#define ILI9341_PTLON      0x12
#define ILI9341_NORON      0x13
#define ILI9341_RDMODE     0x0A
#define ILI9341_RDMADCTL   0x0B
#define ILI9341_RDPIXFMT   0x0C
#define ILI9341_RDIMGFMT   0x0D
#define ILI9341_RDSELFDIAG 0x0F
#define ILI9341_INVOFF     0x20
#define ILI9341_INVON      0x21
#define ILI9341_GAMMASET   0x26
#define ILI9341_DISPOFF    0x28
#define ILI9341_DISPON     0x29
#define ILI9341_CASET      0x2A
#define ILI9341_PASET      0x2B
#define ILI9341_RAMWR      0x2C
#define ILI9341_RAMRD      0x2E
#define ILI9341_PTLAR      0x30
#define ILI9341_VSCRSADD   0x37
#define ILI9341_MADCTL     0x36
#define ILI9341_PIXFMT     0x3A
#define ILI9341_FRMCTR1    0xB1
#define ILI9341_FRMCTR2    0xB2
#define ILI9341_FRMCTR3    0xB3
#define ILI9341_INVCTR     0xB4
#define ILI9341_DFUNCTR    0xB6
#define ILI9341_PWCTR1     0xC0
#define ILI9341_PWCTR2     0xC1
#define ILI9341_PWCTR3     0xC2
#define ILI9341_PWCTR4     0xC3
#define ILI9341_PWCTR5     0xC4
#define ILI9341_VMCTR1     0xC5
#define ILI9341_VMCTR2     0xC7
#define ILI9341_GMCTRP1    0xE0
#define ILI9341_GMCTRN1    0xE1

/* ---- display dimensions ---- */
#define ILI9341_TFTWIDTH   240
#define ILI9341_TFTHEIGHT  320

/* ---- common 16-bit colour helpers ---- */
#define ILI9341_BLACK       0x0001
#define ILI9341_NAVY        0x000F
#define ILI9341_DARKGREEN   0x03E0
#define ILI9341_DARKCYAN    0x03EF
#define ILI9341_MAROON      0x7800
#define ILI9341_PURPLE      0x780F
#define ILI9341_OLIVE       0x7BE0
#define ILI9341_LIGHTGREY   0xC618
#define ILI9341_DARKGREY    0x7BEF
#define ILI9341_BLUE        0x001F
#define ILI9341_GREEN       0x07E0
#define ILI9341_CYAN        0x07FF
#define ILI9341_RED         0xF800
#define ILI9341_MAGENTA     0xF81F
#define ILI9341_YELLOW      0xFFE0
#define ILI9341_WHITE       0xFFFF
#define ILI9341_ORANGE      0xFD20
#define ILI9341_GREENYELLOW 0xAFE5
#define ILI9341_PINK        0xF81F

/* colour conversion helpers */
static inline uint16_t color565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static inline void color565toRGB(uint16_t color, uint8_t *r, uint8_t *g, uint8_t *b)
{
    *r = (color >> 8) & 0xF8;
    *g = (color >> 3) & 0xFC;
    *b = (color << 3) & 0xF8;
}

/* 14-bit RGB helpers used by gradient functions */
static inline void color565toRGB14(uint16_t color,
                                   int16_t *r, int16_t *g, int16_t *b)
{
    *r = (color >> 7) & 0x1F8;   /* 8-bit red   << 1   */
    *g = (color >> 2) & 0x1FC;   /* 8-bit green << 1   */
    *b = (color << 3) & 0xF8;    /* 5-bit blue         */
    /* shift up by 1 more so math has headroom */
    *r <<= 1; *g <<= 1; *b <<= 1;
}

static inline uint16_t RGB14tocolor565(int16_t r, int16_t g, int16_t b)
{
    return color565((uint8_t)(r >> 2), (uint8_t)(g >> 2), (uint8_t)(b >> 2));
}

/* ---- font descriptor (unchanged from ILI9341_t3) ---- */
typedef struct {
    const uint8_t  *index;
    const uint8_t  *unicode;
    const uint8_t  *data;
    uint8_t  version;
    uint8_t  reserved;
    uint8_t  index1_first;
    uint8_t  index1_last;
    uint8_t  index2_first;
    uint8_t  index2_last;
    uint8_t  bits_index;
    uint8_t  bits_width;
    uint8_t  bits_height;
    uint8_t  bits_xoffset;
    uint8_t  bits_yoffset;
    uint8_t  bits_delta;
    uint8_t  line_space;
    uint8_t  cap_height;
} ILI9341_t3_font_t;

/* ---- driver state ---- */
typedef struct {
    SPI_HandleTypeDef *hspi;

    GPIO_TypeDef *cs_port;
    uint16_t      cs_pin;
    GPIO_TypeDef *dc_port;
    uint16_t      dc_pin;
    GPIO_TypeDef *rst_port;   /* NULL = no reset pin */
    uint16_t      rst_pin;

    int16_t  _width, _height;
    uint8_t  rotation;
    int16_t  cursor_x, cursor_y;
    uint8_t  textsize;
    uint16_t textcolor, textbgcolor;
    bool     wrap;
    const ILI9341_t3_font_t *font;
} ILI9341_t3;

/* ---- button helper ---- */
typedef struct {
    ILI9341_t3 *_gfx;
    int16_t  _x, _y;
    uint8_t  _w, _h;
    uint8_t  _textsize;
    uint16_t _outlinecolor, _fillcolor, _textcolor;
    char     _label[10];
    bool     _inverted;
} Adafruit_GFX_Button;

/* ========== public API ========== */
#ifdef __cplusplus
extern "C" {
#endif

/* Initialise the driver struct (does NOT touch the hardware). */
void ILI9341_init(ILI9341_t3 *dev, SPI_HandleTypeDef *hspi,
                  GPIO_TypeDef *cs_port,  uint16_t cs_pin,
                  GPIO_TypeDef *dc_port,  uint16_t dc_pin,
                  GPIO_TypeDef *rst_port, uint16_t rst_pin);

/* Reset + send init sequence. Call once after ILI9341_init(). */
void ILI9341_begin(ILI9341_t3 *dev);

/* Basic drawing */
void ILI9341_setAddrWindow(ILI9341_t3 *dev,
                           uint16_t x0, uint16_t y0,
                           uint16_t x1, uint16_t y1);
void ILI9341_pushColor(ILI9341_t3 *dev, uint16_t color);
void ILI9341_drawPixel(ILI9341_t3 *dev, int16_t x, int16_t y, uint16_t color);
void ILI9341_drawFastVLine(ILI9341_t3 *dev, int16_t x, int16_t y, int16_t h, uint16_t color);
void ILI9341_drawFastHLine(ILI9341_t3 *dev, int16_t x, int16_t y, int16_t w, uint16_t color);
void ILI9341_fillScreen(ILI9341_t3 *dev, uint16_t color);
void ILI9341_fillRect(ILI9341_t3 *dev, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

/* Gradient fills */
void ILI9341_fillRectVGradient(ILI9341_t3 *dev, int16_t x, int16_t y, int16_t w, int16_t h,
                               uint16_t color1, uint16_t color2);
void ILI9341_fillRectHGradient(ILI9341_t3 *dev, int16_t x, int16_t y, int16_t w, int16_t h,
                               uint16_t color1, uint16_t color2);
void ILI9341_fillScreenVGradient(ILI9341_t3 *dev, uint16_t color1, uint16_t color2);
void ILI9341_fillScreenHGradient(ILI9341_t3 *dev, uint16_t color1, uint16_t color2);

/* Display control */
void ILI9341_setRotation(ILI9341_t3 *dev, uint8_t m);
void ILI9341_setScroll(ILI9341_t3 *dev, uint16_t offset);
void ILI9341_invertDisplay(ILI9341_t3 *dev, bool i);
void ILI9341_sleep(ILI9341_t3 *dev, bool enable);

/* Shapes */
void ILI9341_drawLine(ILI9341_t3 *dev, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void ILI9341_drawRect(ILI9341_t3 *dev, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void ILI9341_fillRect2(ILI9341_t3 *dev, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void ILI9341_drawCircle(ILI9341_t3 *dev, int16_t x0, int16_t y0, int16_t r, uint16_t color);
void ILI9341_fillCircle(ILI9341_t3 *dev, int16_t x0, int16_t y0, int16_t r, uint16_t color);
void ILI9341_drawRoundRect(ILI9341_t3 *dev, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
void ILI9341_fillRoundRect(ILI9341_t3 *dev, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
void ILI9341_drawTriangle(ILI9341_t3 *dev, int16_t x0, int16_t y0,
                          int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
void ILI9341_fillTriangle(ILI9341_t3 *dev, int16_t x0, int16_t y0,
                          int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
void ILI9341_drawBitmap(ILI9341_t3 *dev, int16_t x, int16_t y,
                        const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color);

/* Bulk pixel write */
void ILI9341_writeRect(ILI9341_t3 *dev, int16_t x, int16_t y, int16_t w, int16_t h,
                       const uint16_t *pcolors);
void ILI9341_writeRect8BPP(ILI9341_t3 *dev, int16_t x, int16_t y, int16_t w, int16_t h,
                           const uint8_t *pixels, const uint16_t *palette);
void ILI9341_writeRect4BPP(ILI9341_t3 *dev, int16_t x, int16_t y, int16_t w, int16_t h,
                           const uint8_t *pixels, const uint16_t *palette);
void ILI9341_writeRect2BPP(ILI9341_t3 *dev, int16_t x, int16_t y, int16_t w, int16_t h,
                           const uint8_t *pixels, const uint16_t *palette);
void ILI9341_writeRect1BPP(ILI9341_t3 *dev, int16_t x, int16_t y, int16_t w, int16_t h,
                           const uint8_t *pixels, const uint16_t *palette);

/* Text */
void ILI9341_setCursor(ILI9341_t3 *dev, int16_t x, int16_t y);
void ILI9341_getCursor(ILI9341_t3 *dev, int16_t *x, int16_t *y);
void ILI9341_setTextSize(ILI9341_t3 *dev, uint8_t s);
uint8_t ILI9341_getTextSize(ILI9341_t3 *dev);
void ILI9341_setTextColor(ILI9341_t3 *dev, uint16_t c);
void ILI9341_setTextColorBG(ILI9341_t3 *dev, uint16_t c, uint16_t bg);
void ILI9341_setTextWrap(ILI9341_t3 *dev, bool w);
bool ILI9341_getTextWrap(ILI9341_t3 *dev);
void ILI9341_setFont(ILI9341_t3 *dev, const ILI9341_t3_font_t *f);
void ILI9341_writeChar(ILI9341_t3 *dev, uint8_t c);
void ILI9341_writeString(ILI9341_t3 *dev, const char *str);
void ILI9341_drawChar(ILI9341_t3 *dev, int16_t x, int16_t y, unsigned char c,
                      uint16_t fgcolor, uint16_t bgcolor, uint8_t size);
int16_t  ILI9341_strPixelLen(ILI9341_t3 *dev, const char *str);
uint16_t ILI9341_measureTextWidth(ILI9341_t3 *dev, const char *text, int num);
uint16_t ILI9341_measureTextHeight(ILI9341_t3 *dev, const char *text, int num);

/* Display geometry */
uint8_t  ILI9341_getRotation(ILI9341_t3 *dev);
int16_t  ILI9341_width(ILI9341_t3 *dev);
int16_t  ILI9341_height(ILI9341_t3 *dev);
uint8_t  ILI9341_fontCapHeight(ILI9341_t3 *dev);
uint8_t  ILI9341_fontLineSpace(ILI9341_t3 *dev);

/* Button helpers */
void Button_init(Adafruit_GFX_Button *btn, ILI9341_t3 *gfx,
                 int16_t x, int16_t y, uint8_t w, uint8_t h,
                 uint16_t outline, uint16_t fill, uint16_t textcolor,
                 const char *label, uint8_t textsize);
void Button_draw(Adafruit_GFX_Button *btn, bool inverted);
bool Button_contains(Adafruit_GFX_Button *btn, int16_t x, int16_t y);

#ifdef __cplusplus
}
#endif

#endif /* ILI9341_STM32_H */
