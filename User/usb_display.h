#ifndef USB_DISPLAY_H_
#define USB_DISPLAY_H_

#include <stdint.h>

#define USB_DISPLAY_BLOCK_SIZE 2048U

struct usb_display_res {
    uint16_t width;
    uint16_t height;
    uint16_t fps;
} __attribute__((packed));

struct usb_display_fmt {
    uint8_t format;
    uint8_t jpeg_quality;
    uint16_t wide;
    uint16_t height;
} __attribute__((packed));

enum {
    JPEG_YUV444 = 0,
    JPEG_YUV422,
    JPEG_YUV420,
    JPEG_GRAY,
    JPEG_YUV440,
    JPEG_YUV411,
    JPEG_YUV441,
    RGB565,
    RGB565_DIT,
    RGB888,
    ARGB888
};

void USB_Display_Init(void);
void USB_Display_Task(void);
void USB_Display_Reset(void);

/* Called by the native USBHS ISR. Returns zero when EP1 must be NAKed. */
uint8_t USB_Display_RxPacketISR(const uint8_t *packet, uint16_t length,
                                uint16_t max_packet_size);

/* Endpoint-zero vendor request hooks used by the native USBHS driver. */
uint8_t USB_Display_VendorIn(uint8_t request, const uint8_t **data, uint16_t *length);
uint8_t USB_Display_VendorOut(uint8_t request, const uint8_t *data, uint16_t length);

#endif
