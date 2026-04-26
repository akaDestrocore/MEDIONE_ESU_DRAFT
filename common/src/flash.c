#include "flash.h"
#include <string.h>

static const uint32_t FLASH_SECTORS_KB[] = {
    16,  // sector 0
    16,  // sector 1
    16,  // sector 2
    16,  // sector 3
    64,  // sector 4
    128, // sector 5
    128, // sector 6
    128, // sector 7
    128, // sector 8
    128, // sector 9
    128, // sector 10
    128  // sector 11
};

static const uint8_t FLASH_SECTOR_COUNT = sizeof(FLASH_SECTORS_KB) / sizeof(FLASH_SECTORS_KB[0]);

/**
 * @brief Unlocks the Flash memory for write/erase operations.
 * @return Returns FLASH_OK if the Flash is successfully unlocked or already unlocked, error code otherwise.
 */
flash_Status_e flash_unlock(void) {

    if (0 == (FLASH->CR & FLASH_CR_LOCK)) {
        return FLASH_OK;
    }

    return (HAL_OK == HAL_FLASH_Unlock()) ? FLASH_OK : FLASH_ERROR;
}

/**
 * @brief Locks the flash memory to prevent further write/erase operations.
 */
void flash_lock(void) {
    HAL_FLASH_Lock();
}

/**
 * @brief Waits for the last flash operation to complete and clears any errors.
 * @retval FLASH_OK if successful, error code otherwise.
 */
flash_Status_e flash_waitForLastOperation(void) {
    uint32_t timeout = HAL_GetTick() + 100;

    while (__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY)) {
        if (HAL_GetTick() > timeout) {
            return FLASH_TIMEOUT;
        }
    }

    if (__HAL_FLASH_GET_FLAG(FLASH_FLAG_OPERR) ||
        __HAL_FLASH_GET_FLAG(FLASH_FLAG_WRPERR) ||
        __HAL_FLASH_GET_FLAG(FLASH_FLAG_PGAERR) ||
        __HAL_FLASH_GET_FLAG(FLASH_FLAG_PGPERR) ||
        __HAL_FLASH_GET_FLAG(FLASH_FLAG_PGSERR)) {
        
        // Clear error flags
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPERR |
                              FLASH_FLAG_WRPERR |
                              FLASH_FLAG_PGAERR |
                              FLASH_FLAG_PGPERR |
                              FLASH_FLAG_PGSERR);

        return FLASH_ERROR;
    }

    return FLASH_OK;
}


/**
 * @brief Determines the flash sector index for a given address.
 * @param addr The flash memory address.
 * @param sector Returns sector ID by pointer
 * @retval FLASH_OK or 0xFF if address is invalid.
 */
flash_Status_e flash_getSector(uint32_t addr, uint8_t *sector) {
    
    if (addr < FLASH_BASE || NULL == sector) {
        return FLASH_INVALID_PARAM;
    }

    uint32_t offset = addr - FLASH_BASE;
    uint32_t current_offset = 0;

    for (uint8_t i = 0; i < FLASH_SECTOR_COUNT; i++) {
        uint32_t size = FLASH_SECTORS_KB[i] * 1024;

        if (offset < current_offset + size) {
            *sector = i;
            return FLASH_OK;
        }

        current_offset += size;
    }

    return FLASH_INVALID_PARAM;
}

/**
 * @brief Gets the starting address of a given sector.
 * @param sector Sector index
 * @param address Returns the starting address by pointer.
 * @retval FLASH_OK if successfull or error code if invalid.
 */
flash_Status_e flash_getSectorStart(uint8_t sector, uint32_t *address)
{
    if (address == NULL) {
        return FLASH_INVALID_PARAM;
    }

    if (sector >= FLASH_SECTOR_COUNT) {
        return FLASH_INVALID_PARAM;
    }

    uint32_t addr = FLASH_BASE;

    for (uint8_t i = 0U; i < sector; i++) {
        addr += FLASH_SECTORS_KB[i] * 1024U;
    }

    *address = addr;
    return FLASH_OK;
}

/**
 * @brief Gets the ending address of a given sector.
 * @param sector Sector index.
 * @param address Returns the ending address by pointer.
 * @retval FLASH_OK on successfull or error code if invalid.
 */
flash_Status_e flash_getSectorEnd(uint8_t sector, uint32_t *address)
{
    if (address == NULL) {
        return FLASH_INVALID_PARAM;
    }

    if (sector >= FLASH_SECTOR_COUNT) {
        return FLASH_INVALID_PARAM;
    }

    uint32_t addr = FLASH_BASE;

    for (uint8_t i = 0U; i <= sector; i++) {
        addr += FLASH_SECTORS_KB[i] * 1024U;
    }

    *address = addr - 1U;  // last valid address
    return FLASH_OK;
}

/**
 * @brief Erases a single flash sector by its address.
 * @param sectorAddr Address located in the target sector.
 * @param erasedBytes Pointer to store the number of erased bytes.
 * @retval FLASH_OK if successful, error code otherwise.
 */
flash_Status_e flash_sectorErase(uint32_t sectorAddr, uint32_t *erasedBytes)
{
    flash_Status_e status;
    uint8_t sector;

    // Validate and resolve sector
    status = flash_getSector(sectorAddr, &sector);
    if (status != FLASH_OK) {
        return status;
    }

    // Clear any previous error flags
    if (HAL_FLASH_GetError() != HAL_FLASH_ERROR_NONE) {
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPERR |
                               FLASH_FLAG_WRPERR |
                               FLASH_FLAG_PGAERR |
                               FLASH_FLAG_PGPERR |
                               FLASH_FLAG_PGSERR);
    }

    // Prepare erase structure
    FLASH_EraseInitTypeDef eraseInit = {0};
    uint32_t sectorError = 0U;

    eraseInit.TypeErase    = FLASH_TYPEERASE_SECTORS;
    eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    eraseInit.Sector       = sector;
    eraseInit.NbSectors    = 1U;

    // Unlock
    status = flash_unlock();
    if (status != FLASH_OK) {
        return status;
    }

    // Execute erase
    HAL_StatusTypeDef halStatus = HAL_FLASHEx_Erase(&eraseInit, &sectorError);

    // Lock immediately after operation
    flash_lock();

    if (halStatus != HAL_OK) {
        return FLASH_ERROR;
    }

    // Return erased size if requested
    if (erasedBytes != NULL) {
        *erasedBytes = FLASH_SECTORS_KB[sector] * 1024U;
    }

    return FLASH_OK;
}

/**
 * @brief Erases all flash sectors starting from a specified address.
 * @param destination Starting address for erase.
 * @retval FLASH_OK if successful, error code otherwise.
 */
flash_Status_e flash_erase(uint32_t destination)
{
    flash_Status_e status;
    uint8_t start_sector;

    // Resolve starting sector
    status = flash_getSector(destination, &start_sector);
    if (status != FLASH_OK) {
        return status;
    }

    // Clear previous errors if any
    if (HAL_FLASH_GetError() != HAL_FLASH_ERROR_NONE) {
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPERR |
                               FLASH_FLAG_WRPERR |
                               FLASH_FLAG_PGAERR |
                               FLASH_FLAG_PGPERR |
                               FLASH_FLAG_PGSERR);
    }

    // Unlock flash
    status = flash_unlock();
    if (status != FLASH_OK) {
        return status;
    }

    FLASH_EraseInitTypeDef eraseInit = {0};
    uint32_t sectorError = 0U;

    eraseInit.TypeErase    = FLASH_TYPEERASE_SECTORS;
    eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    eraseInit.NbSectors    = 1U;

    for (uint8_t i = start_sector; i < FLASH_SECTOR_COUNT; i++) {

        eraseInit.Sector = i;

        if (HAL_FLASHEx_Erase(&eraseInit, &sectorError) != HAL_OK) {
            flash_lock();
            return FLASH_ERROR;
        }

        // Optional but safer: wait & validate after each sector
        status = flash_waitForLastOperation();
        if (status != FLASH_OK) {
            flash_lock();
            return status;
        }
    }

    flash_lock();
    return FLASH_OK;
}

/**
 * @brief Writes data to flash memory.
 * @param addr Target flash address.
 * @param data Pointer to data buffer.
 * @param len Number of bytes to write.
 * @retval FLASH_OK if successful, error code otherwise.
 */
flash_Status_e flash_write(uint32_t addr, const uint8_t* data, size_t len) {
    
    if (NULL == data || 0 == len) {
        return FLASH_INVALID_PARAM;
    }

    if (len % 4 != 0) {
        return FLASH_ALIGNMENT_ERROR;
    }

    if (flash_waitForLastOperation() != FLASH_OK) {
        return FLASH_ERROR;
    }

    if (flash_unlock() != FLASH_OK) {
        return FLASH_ERROR;
    }

    for (size_t offset = 0; offset < len; offset += 4) {
        
        uint32_t word;
        memcpy(&word, data + offset, 4);

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + offset, word) != HAL_OK) {
            flash_lock();
            return FLASH_ERROR;
        }

        if (flash_waitForLastOperation() != FLASH_OK) {
            flash_lock();
            return FLASH_TIMEOUT;
        }

        if (*(__IO uint32_t*)(addr + offset) != word) {
            flash_lock();
            return FLASH_VERIFY_ERROR;
        }
    }

    flash_lock();
    return FLASH_OK;
}

/**
 * @brief Reads data from flash memory.
 * @param addr Source flash address.
 * @param data Pointer to destination buffer.
 * @param len Number of bytes to read.
 */
void flash_read(uint32_t addr, uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        data[i] = *(__IO uint8_t*)(addr + i);
    }
}


/**
 * @brief Writes data across sector boundaries, handling erasure and alignment.
 * @param currentAddr Current write address.
 * @param currentSector Current sector index.
 * @param data Data to write.
 * @param dataLen Length of data.
 * @param newAddr Optional output pointer to receive next write address.
 * @param newSector Optional output pointer to receive new sector index.
 * @retval FLASH_OK if successful, error code otherwise.
 */
flash_Status_e flash_writeAcrossSectors(uint32_t currentAddr, uint8_t currentSector,
                                        const uint8_t* data, size_t dataLen,
                                        uint32_t* newAddr, uint8_t* newSector)
{
    if (data == NULL || dataLen == 0) {
        return FLASH_INVALID_PARAM;
    }

    flash_Status_e status;
    uint8_t target_sector;
    uint32_t next_addr = currentAddr + dataLen;

    // Determine target sector
    status = flash_getSector(next_addr - 1U, &target_sector);
    if (status != FLASH_OK) {
        return status;
    }

    // Crossing sector boundary
    if (target_sector != currentSector) {

        uint32_t currentSector_end;
        uint32_t next_sector_base;

        status = flash_getSectorEnd(currentSector, &currentSector_end);
        if (status != FLASH_OK) {
            return status;
        }

        status = flash_getSectorStart(target_sector, &next_sector_base);
        if (status != FLASH_OK) {
            return status;
        }

        // Bytes remaining in current sector
        uint32_t bytes_in_current = currentSector_end - currentAddr + 1U;

        // Align to 4 bytes
        bytes_in_current = (bytes_in_current / 4U) * 4U;

        if (bytes_in_current > dataLen) {
            bytes_in_current = dataLen;
        }

        // Erase next sector before writing
        uint32_t erasedBytes;
        status = flash_sectorErase(next_sector_base, &erasedBytes);
        if (status != FLASH_OK) {
            return status;
        }

        // Write portion in current sector
        if (bytes_in_current > 0U) {
            status = flash_write(currentAddr, data, bytes_in_current);
            if (status != FLASH_OK) {
                return status;
            }
        }

        // Write remaining data into next sector
        uint32_t bytes_in_next = dataLen - bytes_in_current;

        if (bytes_in_next > 0U) {
            status = flash_write(next_sector_base,
                                 data + bytes_in_current,
                                 bytes_in_next);
            if (status != FLASH_OK) {
                return status;
            }
        }

        // Outputs
        if (newAddr != NULL) {
            *newAddr = next_sector_base + bytes_in_next;
        }

        if (newSector != NULL) {
            *newSector = target_sector;
        }
    }
    else {
        // Single-sector write
        status = flash_write(currentAddr, data, dataLen);
        if (status != FLASH_OK) {
            return status;
        }

        if (newAddr != NULL) {
            *newAddr = currentAddr + dataLen;
        }

        if (newSector != NULL) {
            *newSector = currentSector;
        }
    }

    return FLASH_OK;
}