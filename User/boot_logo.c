#include "boot_logo.h"
#include "debug.h"
#include "incbin.h"
#include "st7789.h"

INCBIN(BootLogo, "../User/boot_logo.rle565");

#define BOOT_LOGO_PIXELS      (LCD_WIDTH_PIXELS * LCD_HEIGHT_PIXELS)
#define LCD_CS_SET()          (GPIOB->BSHR = GPIO_BSHR_BS12)

/* Decode count/RGB565 records directly from Flash to the LCD.  The compressed
   logo needs no framebuffer in RAM. */
void LCD_ShowBootLogo(void)
{
    const uint8_t *data = gBootLogoData;
    const uint8_t *end = data + gBootLogoSize;
    uint32_t pixels_written = 0U;

    LCD_ResetWindows();
    while (((uint32_t)(end - data) >= 3U) &&
           (pixels_written < BOOT_LOGO_PIXELS)) {
        uint16_t count = *data++;
        uint16_t color = ((uint16_t)data[0] << 8) | data[1];
        uint32_t remaining = BOOT_LOGO_PIXELS - pixels_written;
        data += 2;

        if (count > remaining) {
            count = (uint16_t)remaining;
        }
        pixels_written += count;
        while (count != 0U) {
            LCD_WR_DATA_FAST(color);
            --count;
        }
    }
    while ((SPI2->STATR & SPI_STATR_BSY) != 0U) {
    }
    LCD_CS_SET();
}
