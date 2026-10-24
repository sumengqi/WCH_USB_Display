/********************************** (C) COPYRIGHT *******************************
 * USB display + adaptive USB Audio, using the WCH native USBHS driver.
 *******************************************************************************/
#include "debug.h"
#include "ch32v30x_usbhs_device.h"
#include "usb_display.h"
#include "usb_audio.h"
#include "usb_desc.h"
#include "boot_logo.h"
#include "project_config.h"
#include "st7789.h"

#define BOOT_FADE_STEPS       100U
#define BOOT_FADE_STEP_MS     10U
#define BOOT_HOLD_MS          1000U

static void ShowBootScreen(void)
{
    uint8_t brightness;

    LCD_ShowBootLogo();
    for (brightness = 1U; brightness <= BOOT_FADE_STEPS; ++brightness) {
        LCD_SetLight(brightness);
        Delay_Ms(BOOT_FADE_STEP_MS);
    }
    Delay_Ms(BOOT_HOLD_MS);

    /* Leave the panel black before USB enumeration starts. */
    LCD_SetLight(0U);
    LCD_Clear(0U);
}

int main(void)
{
    SystemCoreClockUpdate();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    Delay_Init();

    LCD_Init();
    ShowBootScreen();
    USB_Display_SetSerial(USB_DISPLAY_SERIAL_NUMBER);
    USB_Display_Init();
    USB_Audio_Init();

    USBHS_RCC_Init();
    USBHS_Device_Init(ENABLE);

    while (1) {
        USB_Display_Task();
    }
}
