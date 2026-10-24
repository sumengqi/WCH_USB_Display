#ifndef USER_USB_AUDIO_H_
#define USER_USB_AUDIO_H_

#include "debug.h"

#define USB_AUDIO_CONTROL_INTERFACE     1U
#define USB_AUDIO_STREAM_INTERFACE      2U
#define USB_AUDIO_OUT_ENDPOINT          0x03U
#define USB_AUDIO_MAX_PACKET_SIZE       800U

void USB_Audio_Init(void);
void USB_Audio_Reset(void);

uint8_t USB_Audio_SetInterface(uint8_t interface_number,
                               uint8_t alternate_setting);
uint8_t USB_Audio_GetInterface(uint8_t interface_number,
                               uint8_t *alternate_setting);

uint8_t USB_Audio_ClassIn(uint8_t request, uint16_t value, uint16_t index,
                          const uint8_t **data, uint16_t *length);
uint8_t USB_Audio_ClassOutSetup(uint8_t request, uint16_t value,
                                uint16_t index, uint16_t length);
uint8_t USB_Audio_ClassOutData(uint8_t request, uint16_t value,
                               uint16_t index, const uint8_t *data,
                               uint16_t length);

uint8_t USB_Audio_RxPacketISR(const uint8_t *packet, uint16_t length);

#endif
