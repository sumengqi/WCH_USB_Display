#ifndef USER_ST7789_H_
#define USER_ST7789_H_

#include <stdint.h>

#define LCD_WIDTH_PIXELS   240U
#define LCD_HEIGHT_PIXELS  240U

void LCD_Init(void);
void LCD_SetLight(uint8_t duty);
void LCD_DMA_Transfer(uint16_t *source, uint32_t pixel_count);
void LCD_WR_DATA_FAST(uint16_t data);
void LCD_ResetWindows(void);
void LCD_Clear(uint16_t color);

#endif
