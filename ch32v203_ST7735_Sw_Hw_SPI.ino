/**
 * CH32V203 ST7735 TFT Driver - COMPLETE GRAPHICS LIBRARY
 * Direct register access, Hardware/Software SPI, full initialization
 * Includes: Cursor, Shapes, Print overloads (int, float, hex, string)
 */

#include <Arduino.h>

// ============================================================
// DIRECT REGISTER ADDRESSES
// ============================================================
#define GPIOA_BASE  0x40010800
#define GPIOB_BASE  0x40010C00

#define GPIOA_CFGLR (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_OUTDR (*(volatile uint32_t *)(GPIOA_BASE + 0x0C))
#define GPIOA_BSHR  (*(volatile uint32_t *)(GPIOA_BASE + 0x10))
#define GPIOA_BCR   (*(volatile uint32_t *)(GPIOA_BASE + 0x14))

#define GPIOB_CFGLR (*(volatile uint32_t *)(GPIOB_BASE + 0x00))
#define GPIOB_OUTDR (*(volatile uint32_t *)(GPIOB_BASE + 0x0C))
#define GPIOB_BSHR  (*(volatile uint32_t *)(GPIOB_BASE + 0x10))
#define GPIOB_BCR   (*(volatile uint32_t *)(GPIOB_BASE + 0x14))

// ============================================================
// PIN BIT MASKS
// ============================================================
#define CS_BIT     (1 << 4)   // PA4
#define DC_BIT     (1 << 0)   // PB0
#define RST_BIT    (1 << 2)   // PB2
#define SCK_BIT    (1 << 5)   // PA5
#define MOSI_BIT   (1 << 7)   // PA7

// ============================================================
// PIN DEFINITIONS (For initialization only)
// ============================================================
#define TFT_CS_PIN    PA4
#define TFT_DC_PIN    PB0
#define TFT_RST_PIN   PB2
#define TFT_SCK_PIN   PA5
#define TFT_MOSI_PIN  PA7

// ============================================================
// ST7735 COMMANDS - COMPLETE LIST
// ============================================================
#define ST7735_SWRESET  0x01
#define ST7735_SLPOUT   0x11
#define ST7735_NORON    0x13
#define ST7735_DISPON   0x29
#define ST7735_CASET    0x2A
#define ST7735_RASET    0x2B
#define ST7735_RAMWR    0x2C
#define ST7735_COLMOD   0x3A
#define ST7735_MADCTL   0x36
#define ST7735_FRMCTR1  0xB1
#define ST7735_FRMCTR2  0xB2
#define ST7735_FRMCTR3  0xB3
#define ST7735_INVCTR   0xB4
#define ST7735_PWCTR1   0xC0
#define ST7735_PWCTR2   0xC1
#define ST7735_PWCTR3   0xC2
#define ST7735_PWCTR4   0xC3
#define ST7735_PWCTR5   0xC4
#define ST7735_VMCTR1   0xC5
#define ST7735_VMCTR2   0xC7
#define ST7735_GMCTRP1  0xE0
#define ST7735_GMCTRN1  0xE1

// ============================================================
// COLORS (RGB565)
// ============================================================
#define COLOR_BLACK   0x0000
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_WHITE   0xFFFF
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_ORANGE  0xFC00
#define COLOR_PURPLE  0x801F
#define COLOR_GRAY    0x8410

#define TFT_WIDTH  128
#define TFT_HEIGHT 160

#include "ch32v20x.h"
#include <Arduino.h>
#include "ch32v20x_spi.h"

static uint8_t currentRotation = 0;

static uint16_t screenWidth = TFT_WIDTH;
static uint16_t screenHeight = TFT_HEIGHT;
// ============================================================
// 144MHz CLOCK SETUP
// ============================================================
void setup_144mhz(void) {
    FLASH->ACTLR = 0x13;
    RCC->CTLR |= (1 << 0);
    while(!(RCC->CTLR & (1 << 1)));
    RCC->CFGR0 &= ~(0x0F << 18);
    RCC->CFGR0 |= (0x0F << 18);
    uint32_t *ext = (uint32_t *)0x40023800;
    *ext |= 0x10;
    RCC->CFGR0 &= ~(3 << 0);
    RCC->CFGR0 |= (2 << 0);
    RCC->CTLR |= (1 << 24);
    while(!(RCC->CTLR & (1 << 25)));
    SystemCoreClock = 144000000;
    ch32_systick_init_config((SystemCoreClock / 1000) - 1);
}

// ============================================================
// SPI - Hardware or Software (Select with #if)
// ============================================================
#if (1)  // 1 = HW SPI, 0 = Soft SPI

void spi_init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef  SPI_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1 | RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIOA_BSHR = CS_BIT;
    GPIOB_BSHR = DC_BIT | RST_BIT;
    SPI_Cmd(SPI1, DISABLE);
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI1, &SPI_InitStructure);
    SPI_Cmd(SPI1, ENABLE);
}

void spi_write_byte(uint8_t data) {
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SPI1, data);
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
    (void)SPI_I2S_ReceiveData(SPI1);
}

void spi_write_16(uint16_t data) {
    spi_write_byte(data >> 8);
    spi_write_byte(data & 0xFF);
}

#else

void spi_init(void) {
    pinMode(TFT_CS_PIN, OUTPUT);
    pinMode(TFT_DC_PIN, OUTPUT);
    pinMode(TFT_RST_PIN, OUTPUT);
    pinMode(TFT_SCK_PIN, OUTPUT);
    pinMode(TFT_MOSI_PIN, OUTPUT);
    GPIOA_BSHR = CS_BIT | SCK_BIT | MOSI_BIT;
    GPIOB_BSHR = DC_BIT | RST_BIT;
}

void spi_write_byte(uint8_t data) {
    for(int i = 7; i >= 0; i--) {
        if(data & (1 << i)) GPIOA_BSHR = MOSI_BIT;
        else GPIOA_BCR = MOSI_BIT;
        GPIOA_BCR = SCK_BIT;
        GPIOA_BSHR = SCK_BIT;
    }
}

void spi_write_16(uint16_t data) {
    spi_write_byte(data >> 8);
    spi_write_byte(data & 0xFF);
}
#endif

// ============================================================
// TFT CORE FUNCTIONS
// ============================================================
void tft_send_command(uint8_t cmd) {
    GPIOB_BCR = DC_BIT;
    GPIOA_BCR = CS_BIT;
    spi_write_byte(cmd);
    GPIOA_BSHR = CS_BIT;
}

void tft_send_data(uint8_t data) {
    GPIOB_BSHR = DC_BIT;
    GPIOA_BCR = CS_BIT;
    spi_write_byte(data);
    GPIOA_BSHR = CS_BIT;
}

void tft_send_data16(uint16_t data) {
    GPIOB_BSHR = DC_BIT;
    GPIOA_BCR = CS_BIT;
    spi_write_16(data);
    GPIOA_BSHR = CS_BIT;
}

void tft_set_addr_window1(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    if(x0 > TFT_WIDTH - 1) x0 = TFT_WIDTH - 1;
    if(y0 > TFT_HEIGHT - 1) y0 = TFT_HEIGHT - 1;
    if(x1 > TFT_WIDTH - 1) x1 = TFT_WIDTH - 1;
    if(y1 > TFT_HEIGHT - 1) y1 = TFT_HEIGHT - 1;
    if(x0 > x1) { uint16_t t = x0; x0 = x1; x1 = t; }
    if(y0 > y1) { uint16_t t = y0; y0 = y1; y1 = t; }
    tft_send_command(ST7735_CASET);
    tft_send_data16(x0);
    tft_send_data16(x1);
    tft_send_command(ST7735_RASET);
    tft_send_data16(y0);
    tft_send_data16(y1);
    tft_send_command(ST7735_RAMWR);
}


// Updated tft_set_addr_window - uses screen dimensions for clamping
void tft_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    // Clamp values to valid display range using current screen dimensions
    if(x0 > screenWidth - 1) x0 = screenWidth - 1;
    if(y0 > screenHeight - 1) y0 = screenHeight - 1;
    if(x1 > screenWidth - 1) x1 = screenWidth - 1;
    if(y1 > screenHeight - 1) y1 = screenHeight - 1;
    
    // Ensure x0 <= x1 and y0 <= y1 (swap if needed)
    if(x0 > x1) { uint16_t temp = x0; x0 = x1; x1 = temp; }
    if(y0 > y1) { uint16_t temp = y0; y0 = y1; y1 = temp; }
    
    tft_send_command(ST7735_CASET);
    tft_send_data16(x0);
    tft_send_data16(x1);
    
    tft_send_command(ST7735_RASET);
    tft_send_data16(y0);
    tft_send_data16(y1);
    
    tft_send_command(ST7735_RAMWR);
}
// ============================================================
// TFT INITIALIZATION
// ============================================================
void tft_init(void) {
    Serial.println("========================================");
    Serial.println("  CH32V203 ST7735 - COMPLETE INIT");
    Serial.println("========================================");
    spi_init();
    Serial.println("[OK] SPI initialized");
    GPIOB_BCR = RST_BIT;
    delay(20);
    GPIOB_BSHR = RST_BIT;
    delay(150);
    tft_send_command(ST7735_SWRESET);
    delay(150);
    tft_send_command(ST7735_SLPOUT);
    delay(120);
    tft_send_command(ST7735_FRMCTR1);
    tft_send_data(0x01);
    tft_send_data(0x2C);
    tft_send_data(0x2D);
    tft_send_command(ST7735_INVCTR);
    tft_send_data(0x07);
    tft_send_command(ST7735_PWCTR1);
    tft_send_data(0xA2);
    tft_send_data(0x02);
    tft_send_data(0x84);
    tft_send_command(ST7735_PWCTR2);
    tft_send_data(0xC5);
    tft_send_command(ST7735_PWCTR3);
    tft_send_data(0x0A);
    tft_send_data(0x00);
    tft_send_command(ST7735_PWCTR4);
    tft_send_data(0x8A);
    tft_send_data(0x2A);
    tft_send_command(ST7735_PWCTR5);
    tft_send_data(0x8A);
    tft_send_data(0xEE);
    tft_send_command(ST7735_VMCTR1);
    tft_send_data(0x0E);
    tft_send_command(ST7735_COLMOD);
    tft_send_data(0x05);
    delay(10);
    tft_send_command(ST7735_GMCTRP1);
    tft_send_data(0x02);
    tft_send_data(0x1C);
    tft_send_data(0x07);
    tft_send_data(0x12);
    tft_send_data(0x37);
    tft_send_data(0x32);
    tft_send_data(0x29);
    tft_send_data(0x2D);
    tft_send_data(0x29);
    tft_send_data(0x25);
    tft_send_data(0x2B);
    tft_send_data(0x39);
    tft_send_data(0x00);
    tft_send_data(0x01);
    tft_send_data(0x03);
    tft_send_data(0x10);
    tft_send_command(ST7735_GMCTRN1);
    tft_send_data(0x03);
    tft_send_data(0x1D);
    tft_send_data(0x07);
    tft_send_data(0x06);
    tft_send_data(0x2E);
    tft_send_data(0x2C);
    tft_send_data(0x29);
    tft_send_data(0x2D);
    tft_send_data(0x2E);
    tft_send_data(0x2E);
    tft_send_data(0x37);
    tft_send_data(0x3F);
    tft_send_data(0x00);
    tft_send_data(0x00);
    tft_send_data(0x02);
    tft_send_data(0x10);
    tft_send_command(ST7735_MADCTL);
    tft_send_data(0xC0);
    tft_send_command(ST7735_CASET);
    tft_send_data16(0);
    tft_send_data16(127);
    tft_send_command(ST7735_RASET);
    tft_send_data16(0);
    tft_send_data16(127);
    tft_send_command(ST7735_NORON);
    delay(10);
    tft_send_command(ST7735_DISPON);
    delay(10);
    Serial.println("[OK] Display initialized");
    tft_fill_screen(COLOR_BLACK);
}

// ============================================================
// BASIC DRAWING FUNCTIONS
// ============================================================
void tft_fill_screen1(uint16_t color) {
    tft_set_addr_window(0, 0, TFT_WIDTH-1, TFT_HEIGHT-1);
    for(uint32_t i = 0; i < (TFT_WIDTH * TFT_HEIGHT); i++) {
        tft_send_data16(color);
    }
}

void tft_draw_pixel1(uint16_t x, uint16_t y, uint16_t color) {
    if(x >= TFT_WIDTH || y >= TFT_HEIGHT) return;
    tft_set_addr_window(x, y, x, y);
    tft_send_data16(color);
}

// ============================================================
// GRAPHICS: LINES
// ============================================================
void drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
    int16_t dx = abs(x1 - x0);
    int16_t dy = abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    while(true) {
        tft_draw_pixel(x0, y0, color);
        if(x0 == x1 && y0 == y1) break;
        int16_t e2 = 2 * err;
        if(e2 > -dy) { err -= dy; x0 += sx; }
        if(e2 < dx) { err += dx; y0 += sy; }
    }
}

void drawFastHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color) {
    if(y >= TFT_HEIGHT || x >= TFT_WIDTH || w == 0) return;
    if(x + w > TFT_WIDTH) w = TFT_WIDTH - x;
    tft_set_addr_window(x, y, x + w - 1, y);
    for(uint16_t i = 0; i < w; i++) tft_send_data16(color);
}

void drawFastVLine(uint16_t x, uint16_t y, uint16_t h, uint16_t color) {
    if(x >= TFT_WIDTH || y >= TFT_HEIGHT || h == 0) return;
    if(y + h > TFT_HEIGHT) h = TFT_HEIGHT - y;
    for(uint16_t i = 0; i < h; i++) tft_draw_pixel(x, y + i, color);
}

// ============================================================
// GRAPHICS: RECTANGLES
// ============================================================
void drawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    drawFastHLine(x, y, w, color);
    drawFastHLine(x, y + h - 1, w, color);
    drawFastVLine(x, y, h, color);
    drawFastVLine(x + w - 1, y, h, color);
}

void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if(x >= TFT_WIDTH || y >= TFT_HEIGHT || w == 0 || h == 0) return;
    if(x + w > TFT_WIDTH) w = TFT_WIDTH - x;
    if(y + h > TFT_HEIGHT) h = TFT_HEIGHT - y;
    tft_set_addr_window(x, y, x + w - 1, y + h - 1);
    for(uint32_t i = 0; i < (uint32_t)w * h; i++) tft_send_data16(color);
}

// ============================================================
// GRAPHICS: ROUNDED RECTANGLES
// ============================================================
void drawRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r, uint16_t color) {
    if(r > w/2) r = w/2;
    if(r > h/2) r = h/2;
    if(r == 0) { drawRect(x, y, w, h, color); return; }
    drawFastHLine(x + r, y, w - 2*r, color);
    drawFastHLine(x + r, y + h - 1, w - 2*r, color);
    drawFastVLine(x, y + r, h - 2*r, color);
    drawFastVLine(x + w - 1, y + r, h - 2*r, color);
    for(int16_t i = 0; i <= r; i++) {
        int16_t j = sqrt(r*r - i*i);
        tft_draw_pixel(x + r - i, y + r - j, color);
        tft_draw_pixel(x + r - j, y + r - i, color);
        tft_draw_pixel(x + w - r + i, y + r - j, color);
        tft_draw_pixel(x + w - r + j, y + r - i, color);
        tft_draw_pixel(x + r - i, y + h - r + j, color);
        tft_draw_pixel(x + r - j, y + h - r + i, color);
        tft_draw_pixel(x + w - r + i, y + h - r + j, color);
        tft_draw_pixel(x + w - r + j, y + h - r + i, color);
    }
}

void fillRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r, uint16_t color) {
    if(r > w/2) r = w/2;
    if(r > h/2) r = h/2;
    if(r == 0) { fillRect(x, y, w, h, color); return; }
    fillRect(x + r, y, w - 2*r, h, color);
    for(int16_t i = 0; i < r; i++) {
        int16_t j = sqrt(r*r - i*i);
        fillRect(x + r - j, y + r - i, j, i + 1, color);
        fillRect(x + w - r, y + r - i, j, i + 1, color);
        fillRect(x + r - j, y + h - r + i - 1, j, i + 1, color);
        fillRect(x + w - r, y + h - r + i - 1, j, i + 1, color);
    }
}

// ============================================================
// GRAPHICS: CIRCLES (FIXED)
// ============================================================
void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
    if(r < 0) return;
    if(r == 0) {
        tft_draw_pixel(x0, y0, color);
        return;
    }
    
    int16_t x = 0;
    int16_t y = r;
    int16_t d = 3 - 2 * r;  // Decision parameter
    
    while(y >= x) {
        // Draw 8 symmetric points
        tft_draw_pixel(x0 + x, y0 + y, color);
        tft_draw_pixel(x0 - x, y0 + y, color);
        tft_draw_pixel(x0 + x, y0 - y, color);
        tft_draw_pixel(x0 - x, y0 - y, color);
        tft_draw_pixel(x0 + y, y0 + x, color);
        tft_draw_pixel(x0 - y, y0 + x, color);
        tft_draw_pixel(x0 + y, y0 - x, color);
        tft_draw_pixel(x0 - y, y0 - x, color);
        
        x++;
        if(d < 0) {
            d = d + 4 * x + 6;
        } else {
            y--;
            d = d + 4 * (x - y) + 10;
        }
    }
}

void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
    if(r < 0) return;
    if(r == 0) {
        tft_draw_pixel(x0, y0, color);
        return;
    }
    
    // Draw filled circles using horizontal lines
    for(int16_t y = -r; y <= r; y++) {
        int16_t x = sqrt(r * r - y * y);
        drawFastHLine(x0 - x, y0 + y, 2 * x + 1, color);
    }
}
// ============================================================
// GRAPHICS: TRIANGLES
// ============================================================
template <typename T> void swap(T &a, T &b) { T t = a; a = b; b = t; }

void drawTriangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
    drawLine(x0, y0, x1, y1, color);
    drawLine(x1, y1, x2, y2, color);
    drawLine(x2, y2, x0, y0, color);
}

void fillTriangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
    if(y0 > y1) { swap(x0, x1); swap(y0, y1); }
    if(y0 > y2) { swap(x0, x2); swap(y0, y2); }
    if(y1 > y2) { swap(x1, x2); swap(y1, y2); }
    int16_t dx1 = x1 - x0, dy1 = y1 - y0;
    int16_t dx2 = x2 - x0, dy2 = y2 - y0;
    int16_t ax = 0, bx = 0;
    if(dy1 != 0) ax = dx1 * 65536 / dy1;
    if(dy2 != 0) bx = dx2 * 65536 / dy2;
    if(y0 == y1) {
        int16_t ay = 0, by = 0;
        if(dy2 != 0) ay = dx2 * 65536 / dy2;
        if(y1 == y2) return;
        for(int16_t y = y0; y <= y2; y++)
            drawFastHLine(x0 + ax * (y - y0), y, x0 + bx * (y - y0) - x0, color);
        return;
    }
    if(y1 == y2) {
        int16_t ay = 0, by = 0;
        if(dy1 != 0) ay = dx1 * 65536 / dy1;
        for(int16_t y = y0; y <= y2; y++)
            drawFastHLine(x0 + ax * (y - y0), y, x0 + ay * (y - y0) - x0, color);
        return;
    }
    for(int16_t y = y0; y <= y1; y++)
        drawFastHLine(x0 + ax * (y - y0), y, x0 + bx * (y - y0) - x0, color);
    for(int16_t y = y1; y <= y2; y++)
        drawFastHLine(x1 + dx2 * (y - y1) / dy2, y, x0 + bx * (y - y0) - x0, color);
}
// ============================================================
// SCREEN ROTATION
// ============================================================
// Rotation values: 0=0°, 1=90°, 2=180°, 3=270°


void setRotation(uint8_t rotation) {
    currentRotation = rotation % 4;  // Only 0-3 are valid
    
    tft_send_command(ST7735_MADCTL);
    
    switch(currentRotation) {
        case 0:  // 0° - Normal (Portrait)
            tft_send_data(0xC0);  // MX=1, MY=1
            screenWidth = TFT_WIDTH;
            screenHeight = TFT_HEIGHT;
            break;
            
        case 1:  // 90° - Landscape (swap X/Y)
            tft_send_data(0x60);  // MX=1, MV=1
            screenWidth = TFT_HEIGHT;
            screenHeight = TFT_WIDTH;
            break;
            
        case 2:  // 180° - Upside down
            tft_send_data(0x00);  // Normal orientation
            screenWidth = TFT_WIDTH;
            screenHeight = TFT_HEIGHT;
            break;
            
        case 3:  // 270° - Landscape inverted
            tft_send_data(0xA0);  // MV=1, MY=1
            screenWidth = TFT_HEIGHT;
            screenHeight = TFT_WIDTH;
            break;
    }
}

uint8_t getRotation(void) {
    return currentRotation;
}

uint16_t getWidth(void) {
    return screenWidth;
}

uint16_t getHeight(void) {
    return screenHeight;
}
// Updated tft_fill_screen - uses current screen dimensions
void tft_fill_screen(uint16_t color) {
    tft_set_addr_window(0, 0, screenWidth - 1, screenHeight - 1);
    for(uint32_t i = 0; i < ((uint32_t)screenWidth * screenHeight); i++) {
        tft_send_data16(color);
    }
}

// Updated tft_draw_pixel - bounds check uses current dimensions
void tft_draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
    if(x >= screenWidth || y >= screenHeight) return;
    tft_set_addr_window(x, y, x, y);
    tft_send_data16(color);
}
// ============================================================
// FONT TABLE (5x7 ASCII 32-126)
// ============================================================
static const uint8_t font5x7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},
    {0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},{0x00,0x1C,0x22,0x41,0x00},
    {0x00,0x41,0x22,0x1C,0x00},{0x14,0x08,0x3E,0x08,0x14},{0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},
    {0x20,0x10,0x08,0x04,0x02},{0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},
    {0x00,0x56,0x36,0x00,0x00},{0x08,0x14,0x22,0x41,0x00},{0x14,0x14,0x14,0x14,0x14},
    {0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},{0x32,0x49,0x79,0x41,0x3E},
    {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x3A},{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},{0x00,0x7F,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x7F,0x00},{0x04,0x02,0x01,0x02,0x04},
    {0x40,0x40,0x40,0x40,0x40},{0x00,0x01,0x02,0x04,0x00},{0x20,0x54,0x54,0x54,0x78},
    {0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},{0x38,0x44,0x44,0x48,0x7F},
    {0x38,0x54,0x54,0x54,0x18},{0x08,0x7E,0x09,0x01,0x02},{0x18,0xA4,0xA4,0xA4,0x7C},
    {0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x40,0x80,0x84,0x7D,0x00},
    {0x7F,0x10,0x28,0x44,0x00},{0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x18,0x04,0x78},
    {0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},{0xFC,0x24,0x24,0x24,0x18},
    {0x18,0x24,0x24,0x24,0xFC},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},
    {0x3C,0x40,0x30,0x40,0x3C},{0x44,0x28,0x10,0x28,0x44},{0x1C,0xA0,0xA0,0xA0,0x7C},
    {0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},{0x00,0x00,0x7F,0x00,0x00},
    {0x00,0x41,0x36,0x08,0x00},{0x02,0x01,0x02,0x04,0x02}
};

// ============================================================
// CURSOR AND TEXT STATE
// ============================================================
static uint16_t cursorX = 0;
static uint16_t cursorY = 0;
static uint16_t textColor = COLOR_WHITE;
static uint16_t textBgColor = COLOR_BLACK;
static uint8_t textSize = 1;

// ============================================================
// CURSOR CONTROL
// ============================================================
void setCursor(uint16_t x, uint16_t y) { cursorX = x; cursorY = y; }
void setTextColor(uint16_t color) { textColor = color; }
void setTextColorBg(uint16_t color, uint16_t bg) { textColor = color; textBgColor = bg; }
void setTextSize(uint8_t size) { textSize = (size < 1) ? 1 : (size > 5 ? 5 : size); }

// ============================================================
// LOW-LEVEL CHARACTER DRAWING
// ============================================================
void tft_draw_char(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg) {
    if(c < 32 || c > 126) return;
    uint8_t idx = c - 32;
    for(uint8_t col = 0; col < 5; col++) {
        uint8_t line = font5x7[idx][col];
        for(uint8_t row = 0; row < 7; row++) {
            uint16_t px = x + col * textSize;
            uint16_t py = y + row * textSize;
            uint16_t clr = (line & (1 << row)) ? color : bg;
            for(uint8_t sx = 0; sx < textSize; sx++)
                for(uint8_t sy = 0; sy < textSize; sy++)
                    tft_draw_pixel(px + sx, py + sy, clr);
        }
    }
}

// ============================================================
// HELPER: Convert integer to string
// ============================================================
void intToStr(int32_t num, char* buf, uint8_t base) {
    if(num == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    
    bool negative = false;
    if(num < 0 && base == 10) {
        negative = true;
        num = -num;
    }
    
    int i = 0;
    while(num > 0) {
        uint8_t digit = num % base;
        buf[i++] = (digit < 10) ? '0' + digit : 'A' + digit - 10;
        num /= base;
    }
    
    if(negative) buf[i++] = '-';
    buf[i] = '\0';
    
    // Reverse string
    for(int j = 0; j < i / 2; j++) {
        char temp = buf[j];
        buf[j] = buf[i - 1 - j];
        buf[i - 1 - j] = temp;
    }
}

// ============================================================
// OVERLOADED PRINT FUNCTIONS
// ============================================================
size_t print(const char* str) {
    size_t n = 0;
    while(*str) {
        if(*str == '\n') {
            cursorY += 7 * textSize;
            cursorX = 0;
        } else if(*str == '\r') {
            cursorX = 0;
        } else if(*str == '\t') {
            cursorX += 6 * textSize * 4;
        } else {
            tft_draw_char(cursorX, cursorY, *str, textColor, textBgColor);
            cursorX += 6 * textSize;
            n++;
        }
        str++;
    }
    return n;
}

size_t print(char c) { char buf[2] = {c, 0}; return print(buf); }
size_t print(int n) { char buf[16]; intToStr(n, buf, 10); return print(buf); }
size_t print(unsigned int n) { char buf[16]; intToStr(n, buf, 10); return print(buf); }
size_t print(long n) { char buf[16]; intToStr(n, buf, 10); return print(buf); }
size_t print(unsigned long n) { char buf[16]; intToStr(n, buf, 10); return print(buf); }

size_t print(float n, int digits = 2) {
    if(n < 0) { print('-'); n = -n; }
    int intPart = (int)n;
    float fracPart = n - intPart;
    print(intPart);
    print('.');
    for(int i = 0; i < digits; i++) {
        fracPart *= 10;
        int digit = (int)fracPart;
        print((char)('0' + digit));
        fracPart -= digit;
    }
    return 0;
}
size_t print(double n, int digits = 2) { return print((float)n, digits); }

// Print with newline
size_t println(const char* str) { size_t n = print(str); print('\n'); return n + 1; }
size_t println(char c) { return print(c) + print('\n'); }
size_t println(int n) { return print(n) + print('\n'); }
size_t println(unsigned int n) { return print(n) + print('\n'); }
size_t println(long n) { return print(n) + print('\n'); }
size_t println(unsigned long n) { return print(n) + print('\n'); }
size_t println(float n, int digits = 2) { return print(n, digits) + print('\n'); }
size_t println(double n, int digits = 2) { return print(n, digits) + print('\n'); }

// ============================================================
// PRINT HEXADECIMAL
// ============================================================
void printHex(uint32_t num) {
    print("0x");
    for(int i = 7; i >= 0; i--) {
        uint8_t digit = (num >> (i * 4)) & 0xF;
        print((char)(digit < 10 ? '0' + digit : 'A' + digit - 10));
    }
}

void printlnHex(uint32_t num) { printHex(num); print('\n'); }


// ============================================================
// DEMO: GRAPHICS TEST PATTERN
// ============================================================
void drawGraphicsDemo(void) {
    tft_fill_screen(COLOR_BLACK);
    
    // Title
    setCursor(10, 5);
    setTextColor(COLOR_WHITE);
    setTextSize(1);
    print("Graphics Demo");
    
    // Lines
    drawLine(5, 25, 60, 25, COLOR_RED);
    drawLine(5, 35, 60, 35, COLOR_GREEN);
    drawLine(5, 45, 60, 45, COLOR_BLUE);
    delay(500);
    // Rectangles
    drawRect(70, 25, 50, 25, COLOR_YELLOW);
    fillRect(70, 55, 50, 25, COLOR_MAGENTA);
      delay(500);
    // Circles
    drawCircle(40, 100, 18, COLOR_CYAN);
    fillCircle(90, 100, 18, COLOR_RED);
     delay(500);
    // Triangle
    drawTriangle(20, 140, 60, 120, 100, 140, COLOR_WHITE);
    fillTriangle(25, 135, 60, 125, 95, 135, COLOR_ORANGE);
      delay(500);
    // Text with cursor
    setCursor(5, 120);
    setTextColor(COLOR_WHITE);
    print("Print Test:");
      delay(500);
    setCursor(5, 130);
    setTextColor(COLOR_YELLOW);
    print("Int: ");
    print(-12345);
      delay(500);
    setCursor(5, 140);
    setTextColor(COLOR_CYAN);
    print("Hex: ");
    printHex(0xDEADBEEF);
      delay(500);
    setCursor(5, 150);
    setTextColor(COLOR_GREEN);
    print("Float: ");
    print(3.14159, 4);
}

// ============================================================
// SETUP
// ============================================================
void setup() {
    setup_144mhz();
    Serial.begin(115200);
    delay(2000);
    
    tft_init();
    
    // Test pattern - color fill
    tft_fill_screen(COLOR_RED);
    delay(300);
    tft_fill_screen(COLOR_GREEN);
    delay(300);
    tft_fill_screen(COLOR_BLUE);
    delay(300);
    tft_fill_screen(COLOR_BLACK);
    delay(300);
    
   
    
    Serial.println("========================================");
    Serial.println("  CH32V203 ST7735 - COMPLETE GRAPHICS");
    Serial.println("========================================");
    Serial.println("[OK] Setup complete!");
    Serial.println("Features: Lines, Rect, Circles, Triangles");
    Serial.println("          Cursor, Print overloads");
}

// ============================================================
// LOOP
// ============================================================
void loop() { 
    static byte r;
    // Draw graphics demo
    setRotation(r++);
    drawGraphicsDemo();
    // Test cursor-based printing
    tft_fill_screen(COLOR_BLACK);
    
    setCursor(10, 10);
    setTextColor(COLOR_WHITE);
    setTextSize(2);
    print("CH32V203");
      delay(500);
    setCursor(10, 35);
    setTextSize(1);
    setTextColor(COLOR_YELLOW);
    print("144MHz TFT");
      delay(500);
    setCursor(10, 50);
    setTextColor(COLOR_CYAN);
    print("Graphics: ");
    print("Lines, Circles");
      delay(500);
    setCursor(10, 65);
    setTextColor(COLOR_GREEN);
    print("Print: ");
    print(12345);
    print(" ");
    printHex(0xABCD);
      delay(500);
    setCursor(10, 80);
    setTextColor(COLOR_MAGENTA);
    print("Float: ");
    print(3.14159, 3);

    setCursor(10, 95);
    setTextColor(COLOR_ORANGE);
    print("Milliseconds: ");
    print(millis());
      delay(500);
    // Draw some shapes 
    drawCircle(110, 30, 15, COLOR_RED);
    drawRect(95, 110, 30, 30, COLOR_YELLOW);
    drawTriangle(70, 140, 110, 130, 130, 145, COLOR_GREEN);
    fillCircle(100, 70, 12, COLOR_BLUE);
     
    delay(1000);
}