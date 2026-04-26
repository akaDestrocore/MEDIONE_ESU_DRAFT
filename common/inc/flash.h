#ifndef _FLASH_H
#define _FLASH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stddef.h>

/**
 * @brief Flash Status Codes
 */
typedef enum {
    FLASH_OK                = 0,
    FLASH_ERROR             = -1,
    FLASH_INVALID_PARAM     = -2,
    FLASH_TIMEOUT           = -3,
    FLASH_ALIGNMENT_ERROR   = -4,
    FLASH_VERIFY_ERROR      = -5
} flash_Status_e;

// Core flash functions
flash_Status_e flash_unlock(void);
void flash_lock(void);
flash_Status_e flash_waitForLastOperation(void);

flash_Status_e flash_getSector(uint32_t addr, uint8_t *sector);
flash_Status_e flash_sectorErase(uint32_t sectorAddr, uint32_t *erasedBytes);
flash_Status_e flash_erase(uint32_t destination);
flash_Status_e flash_write(uint32_t addr, const uint8_t* data, size_t len);
void flash_read(uint32_t addr, uint8_t* data, size_t len);

// Get sector information
flash_Status_e flash_getSectorStart(uint8_t sector, uint32_t *address);
flash_Status_e flash_getSectorEnd(uint8_t sector, uint32_t *address);

// Write across sector boundaries
flash_Status_e flash_writeAcrossSectors(uint32_t currentAddr, uint8_t currentSector,
                                        const uint8_t* data, size_t dataLen,
                                        uint32_t* newAddr, uint8_t* newSector);

#ifdef __cplusplus
}
#endif

#endif /* _FLASH_H */