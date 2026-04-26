#ifndef _FLASH_H
#define _FLASH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stddef.h>

// Core flash functions
int flash_unlock(void);
void flash_lock(void);
int flash_waitForLastOperation(void);
uint8_t flash_getSector(uint32_t address);
int flash_sectorErase(uint32_t sectorAddr);
int flash_erase(uint32_t destination);
int flash_write(uint32_t address, const uint8_t* data, size_t len);
void flash_read(uint32_t address, uint8_t* data, size_t len);

// Get sector information
uint32_t flash_getSectorStart(uint8_t sector);
uint32_t flash_getSectorEnd(uint8_t sector);

// Write across sector boundaries
int flash_writeAcrossSectors(uint32_t currentAddr, uint8_t currentSector,
                              const uint8_t* data, size_t dataLen,
                              uint32_t* newAddr, uint8_t* newSector);

#ifdef __cplusplus
}
#endif

#endif /* _FLASH_H */