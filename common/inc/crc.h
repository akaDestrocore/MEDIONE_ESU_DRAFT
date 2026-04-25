#ifndef _CRC_H
#define _CRC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

// Initialize CRC hardware unit
void crc_init(void);

// Reset CRC calculation
void crc_reset(void);

// Calculate CRC for a buffer
uint32_t crc_calculate(const uint8_t* data, size_t len);

// Calculate CRC for data of selected size in flash memory
uint32_t crc_calculateMemory(uint32_t addr, uint32_t size);

// Verify image CRC
int crc_verifyFirmware(uint32_t addr, uint32_t headerSize);

// Invalidate image by corrupting the header
int crc_invalidateFirmware(uint32_t addr);

#ifdef __cplusplus
}
#endif

#endif /* _CRC_H */