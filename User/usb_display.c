#include "usb_display.h"
#include "ch32v30x_usbhs_device.h"
#include "crc.h"
#include "incbin.h"
#include "st7789.h"
#include "project_config.h"
#include <string.h>

INCBIN(VertexShader1, "../User/VertexShader1.hlsl");
INCBIN(PixelShader, "../User/PixelShader.hlsl");
INCBIN(EDID, "../User/EDID240x240.bin");

#define DISPLAY_DRIVER_VERSION 1U
#define DISPLAY_RANDOM_CHECK   879622951UL
#define USB_FRAME_TIMER_HZ     10000U
#define USB_FRAME_TIMEOUT_MS   20U
#define USB_FRAME_TIMEOUT_TICKS \
    ((USB_FRAME_TIMER_HZ / 1000U) * USB_FRAME_TIMEOUT_MS)

static const struct usb_display_res display_resolutions[] = {
    {LCD_WIDTH_PIXELS, LCD_HEIGHT_PIXELS, 60},
    {480, 480, 60},
    {960, 960, 60}
};

static const struct usb_display_fmt display_format = {
    RGB565_DIT, 95, LCD_WIDTH_PIXELS, LCD_HEIGHT_PIXELS
};

struct display_transform {
    float cos_theta;
    float sin_theta;
    float aspect_ratio;
    float inverse_aspect_ratio;
    float gamma;
    float reserved[3];
};

static struct display_transform transform_buffer = {
    1.0f, 0.0f, 1.0f, 1.0f, 1.0f, {0.0f, 0.0f, 0.0f}
};

static uint8_t display_edid[128];
static uint16_t shader_lengths[3];
static uint32_t driver_random[3];
static uint32_t driver_response[2];

static __attribute__((aligned(4))) uint8_t display_rx_buffer[2][USB_DISPLAY_BLOCK_SIZE];
static volatile uint16_t display_rx_length[2];
static volatile uint16_t display_fill_length;
static volatile uint8_t display_fill_index;
static volatile uint8_t display_ready_mask;
static volatile uint8_t display_rx_stalled;

static uint8_t display_active_index = 0xff;
static uint8_t display_dma_busy;
static uint8_t *display_data;
static uint32_t display_data_length;
static volatile uint8_t display_output_active;
static volatile uint8_t display_blank_pending;
static uint16_t display_last_frame_number;
static uint16_t display_last_frame_tick;
static uint8_t display_frame_tracking;

static void USB_Display_FrameWatchdogInit(void)
{
    TIM_TimeBaseInitTypeDef timer;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE);
    timer.TIM_Period = 0xffffU;
    timer.TIM_Prescaler = (uint16_t)(SystemCoreClock / USB_FRAME_TIMER_HZ - 1U);
    timer.TIM_ClockDivision = TIM_CKD_DIV1;
    timer.TIM_CounterMode = TIM_CounterMode_Up;
    timer.TIM_RepetitionCounter = 0U;
    TIM_TimeBaseInit(TIM6, &timer);
    TIM6->CNT = 0U;
    TIM_Cmd(TIM6, ENABLE);

    display_frame_tracking = 0U;
}

static uint8_t USB_Display_FrameClockAlive(void)
{
    uint16_t frame_number;
    uint16_t now;

    frame_number = (uint16_t)(USBHSD->FRAME_NO &
                              (USBHS_MICRO_FRAME_NUM | USBHS_SOF_FRAME_NUM));
    now = (uint16_t)TIM6->CNT;

    if ((display_frame_tracking == 0U) ||
        (frame_number != display_last_frame_number)) {
        display_last_frame_number = frame_number;
        display_last_frame_tick = now;
        display_frame_tracking = 1U;
        return 1U;
    }

    return ((uint16_t)(now - display_last_frame_tick) <
            USB_FRAME_TIMEOUT_TICKS) ? 1U : 0U;
}

static void USB_Display_SetEdidSerial(uint32_t serial)
{
    uint8_t checksum = 0;
    uint8_t i;

    memcpy(display_edid, gEDIDData, sizeof(display_edid));
    display_edid[12] = (uint8_t)serial;
    display_edid[13] = (uint8_t)(serial >> 8);
    display_edid[14] = (uint8_t)(serial >> 16);
    display_edid[15] = (uint8_t)(serial >> 24);
    for (i = 0; i < 127; ++i) {
        checksum = (uint8_t)(checksum + display_edid[i]);
    }
    display_edid[127] = (uint8_t)(0U - checksum);
}

void USB_Display_Reset(void)
{
    if (display_output_active != 0U) {
        display_blank_pending = 1U;
    }
    DMA1_Channel5->CFGR &= ~DMA_CFGR1_EN;
    DMA1->INTFCR = DMA1_FLAG_TC5;
    display_fill_length = 0;
    display_fill_index = 0;
    display_ready_mask = 0;
    display_rx_stalled = 0;
    display_active_index = 0xff;
    display_dma_busy = 0;
    display_data = 0;
    display_data_length = 0;
}

void USB_Display_Init(void)
{
    USB_Display_SetEdidSerial(USB_DISPLAY_EDID_SERIAL);
    shader_lengths[0] = (uint16_t)gVertexShader1Size;
    shader_lengths[1] = (uint16_t)gPixelShaderSize;
    shader_lengths[2] = (uint16_t)sizeof(transform_buffer);
    display_output_active = 0U;
    display_blank_pending = 0U;
    USB_Display_FrameWatchdogInit();
    USB_Display_Reset();
}

uint8_t USB_Display_RxPacketISR(const uint8_t *packet, uint16_t length,
                                uint16_t max_packet_size)
{
    uint8_t index = display_fill_index;
    uint16_t room;

    if ((display_ready_mask & (uint8_t)(1U << index)) != 0U) {
        display_rx_stalled = 1;
        return 0;
    }

    room = (uint16_t)(USB_DISPLAY_BLOCK_SIZE - display_fill_length);
    if (length > room) {
        length = room;
    }
    if (length != 0U) {
        memcpy(&display_rx_buffer[index][display_fill_length], packet, length);
        display_fill_length = (uint16_t)(display_fill_length + length);
    }

    if ((length < max_packet_size) || (display_fill_length == USB_DISPLAY_BLOCK_SIZE)) {
        display_rx_length[index] = display_fill_length;
        display_ready_mask |= (uint8_t)(1U << index);
        display_fill_length = 0;
        display_fill_index ^= 1U;

        if ((display_ready_mask & (uint8_t)(1U << display_fill_index)) != 0U) {
            display_rx_stalled = 1;
            return 0;
        }
    }
    return 1;
}

static void USB_Display_ReleaseActive(void)
{
    uint8_t index = display_active_index;
    uint8_t resume = 0;

    if (index == 0xffU) {
        return;
    }

    NVIC_DisableIRQ(USBHS_IRQn);
    display_ready_mask &= (uint8_t)~(1U << index);
    display_rx_length[index] = 0;
    display_active_index = 0xff;
    if (display_rx_stalled != 0U) {
        display_fill_index = index;
        display_fill_length = 0;
        display_rx_stalled = 0;
        resume = 1;
    }
    if (resume != 0U) {
        USBHS_Display_RxResume();
    }
    NVIC_EnableIRQ(USBHS_IRQn);
}

void USB_Display_Task(void)
{
    uint32_t display_command;
    uint32_t pixel_bytes;
    uint8_t usb_link_ready;

    if (display_dma_busy != 0U) {
        if ((DMA1->INTFR & DMA1_FLAG_TC5) == 0U) {
            return;
        }
        DMA1->INTFCR = DMA1_FLAG_TC5;
        display_dma_busy = 0;
    }

    usb_link_ready = (uint8_t)((USBHS_DevConfig != 0U) &&
                               ((USBHS_DevSleepStatus & 0x02U) == 0U) &&
                               ((USBHSD->MIS_ST & USBHS_UMS_SUSPEND) == 0U));
    if (usb_link_ready != 0U) {
        usb_link_ready = USB_Display_FrameClockAlive();
    } else {
        display_frame_tracking = 0U;
    }
    if (usb_link_ready == 0U) {
        if ((display_output_active != 0U) || (display_blank_pending != 0U)) {
            LCD_SetLight(0U);
            LCD_Clear(0U);
            display_output_active = 0U;
            display_blank_pending = 0U;
        }
        return;
    }

    if (display_blank_pending != 0U) {
        LCD_SetLight(0U);
        LCD_Clear(0U);
        display_output_active = 0U;
        display_blank_pending = 0U;
    }

    if (display_active_index == 0xffU) {
        uint8_t ready = display_ready_mask;
        if (ready == 0U) {
            return;
        }
        display_active_index = (ready & 0x01U) ? 0U : 1U;
        display_data = display_rx_buffer[display_active_index];
        display_data_length = display_rx_length[display_active_index];
    }

    if (display_data_length < 4U) {
        USB_Display_ReleaseActive();
        return;
    }

    memcpy(&display_command, display_data, sizeof(display_command));
    display_data += 4;
    display_data_length -= 4;

    if (display_command == 0x80U) {
        LCD_SetLight(0U);
        LCD_Clear(0U);
        display_output_active = 0U;
    } else if ((display_command & 0x01U) != 0U) {
        LCD_ResetWindows();
        LCD_SetLight(100U);
        display_output_active = 1U;
    }

    pixel_bytes = (display_data_length > 508U) ? 508U : display_data_length;
    pixel_bytes &= ~1UL;
    if (pixel_bytes != 0U) {
        LCD_DMA_Transfer((uint16_t *)display_data, pixel_bytes / 2U);
        display_data += pixel_bytes;
        display_data_length -= pixel_bytes;
        display_dma_busy = 1;
    } else {
        USB_Display_ReleaseActive();
    }
}

uint8_t USB_Display_VendorIn(uint8_t request, const uint8_t **data, uint16_t *length)
{
    switch (request) {
        case 0:
            *data = (const uint8_t *)display_resolutions;
            *length = (uint16_t)sizeof(display_resolutions);
            return 1;
        case 1:
            *data = display_edid;
            *length = (uint16_t)sizeof(display_edid);
            return 1;
        case 2:
            *data = (const uint8_t *)&display_format;
            *length = (uint16_t)sizeof(display_format);
            return 1;
        case 4:
            *data = (const uint8_t *)driver_response;
            *length = (uint16_t)sizeof(driver_response);
            return 1;
        case 5:
            *data = (const uint8_t *)shader_lengths;
            *length = (uint16_t)sizeof(shader_lengths);
            return 1;
        case 6:
            *data = gVertexShader1Data;
            *length = (uint16_t)gVertexShader1Size;
            return 1;
        case 7:
            *data = gPixelShaderData;
            *length = (uint16_t)gPixelShaderSize;
            return 1;
        case 8:
            *data = (const uint8_t *)&transform_buffer;
            *length = (uint16_t)sizeof(transform_buffer);
            return 1;
        default:
            return 0;
    }
}

uint8_t USB_Display_VendorOut(uint8_t request, const uint8_t *data, uint16_t length)
{
    if ((request == 3U) && (length == sizeof(driver_random))) {
        memcpy(driver_random, data, sizeof(driver_random));
        if (((driver_random[0] ^ driver_random[1]) == DISPLAY_RANDOM_CHECK) &&
            ((driver_random[2] ^ driver_random[1]) == DISPLAY_DRIVER_VERSION)) {
            driver_response[0] = crc32(0, (const uint8_t *)driver_random,
                                       sizeof(driver_random));
            driver_response[1] = crc32(DISPLAY_RANDOM_CHECK,
                                       (const uint8_t *)driver_random,
                                       sizeof(driver_random));
        } else {
            driver_response[0] = 0;
            driver_response[1] = 0;
        }
        return 1;
    }
    if ((request == 9U) && (length == 1U)) {
        return 1;
    }
    return 0;
}
