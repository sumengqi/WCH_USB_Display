#include "crc.h"

#define CRC32_POLYNOMIAL 0xedb88320UL

uint32_t crc32_no_comp(uint32_t crc, const uint8_t *data, uint32_t length)
{
    uint8_t bit;

    while (length != 0U) {
        crc ^= *data++;
        for (bit = 0U; bit < 8U; ++bit) {
            if ((crc & 1U) != 0U) {
                crc = (crc >> 1) ^ CRC32_POLYNOMIAL;
            } else {
                crc >>= 1;
            }
        }
        --length;
    }
    return crc;
}

uint32_t crc32(uint32_t crc, const uint8_t *data, uint32_t length)
{
    return crc32_no_comp(crc ^ 0xffffffffUL, data, length) ^ 0xffffffffUL;
}
