#ifndef _CRC_H
#define _CRC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "image.h"
#include "flash.h"

/**
 * @brief CRC Error Codes
 */
typedef enum {
    CRC_OK              = 0,
    CRC_MISMATCH        = -1,
    CRC_INVALID_SIZE    = -2
} crc_Status_e;

// Initialize CRC hardware unit
void crc_init(void);

// Reset CRC calculation
void crc_reset(void);

// Calculate CRC for a buffer
uint32_t crc_calculate(const uint8_t* data, size_t len);

// Calculate CRC for data of selected size in flash memory
uint32_t crc_calculateMemory(uint32_t addr, uint32_t size);

// Verify image CRC
crc_Status_e crc_verifyFirmware(uint32_t addr, uint32_t headerSize);

// Invalidate image by corrupting the header
crc_Status_e crc_invalidateFirmware(uint32_t addr);

#ifdef __cplusplus
}
#endif

#endif /* _CRC_H */