#ifndef CRC16_H
#define CRC16_H

#include <stddef.h>
#include <stdint.h>

/* CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no final XOR.
 * Check value: crc16_ccitt("123456789", 9) == 0x29B1. */
#define CRC16_INIT 0xFFFFu

uint16_t crc16_update(uint16_t crc, uint8_t byte);
uint16_t crc16_ccitt(const uint8_t *data, size_t len);

#endif
