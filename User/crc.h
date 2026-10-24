#ifndef USER_CRC_H_
#define USER_CRC_H_

#include <stdint.h>

uint32_t crc32(uint32_t crc, const uint8_t *data, uint32_t length);
uint32_t crc32_no_comp(uint32_t crc, const uint8_t *data, uint32_t length);

#endif

