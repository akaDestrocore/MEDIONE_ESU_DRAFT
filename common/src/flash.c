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
 * @return int Returns 0 if the Flash is successfully unlocked or already unlocked, error code otherwise.
 */
int flash_unlock(void) {
    // Check if already unlocked
    if ((FLASH->CR & FLASH_CR_LOCK) == 0) {
        return 0;
    }
    
    HAL_StatusTypeDef status = HAL_FLASH_Unlock();
    return (status == HAL_OK) ? 0 : -1;
}

/**
 * @brief Locks the flash memory to prevent further write/erase operations.
 */
void flash_lock(void) {
    HAL_FLASH_Lock();
}

/**
 * @brief Waits for the last flash operation to complete and clears any errors.
 * @retval 1 if successful, 0 if timeout or error occurred.
 */
int flash_waitForLastOperation(void) {
    uint32_t timeout = HAL_GetTick() + 100; // 100ms
    
    while (__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY)) {
        if (HAL_GetTick() > timeout) {
            return 0;
        }
    }
    
    if (__HAL_FLASH_GET_FLAG(FLASH_FLAG_OPERR) || 
        __HAL_FLASH_GET_FLAG(FLASH_FLAG_WRPERR) || 
        __HAL_FLASH_GET_FLAG(FLASH_FLAG_PGAERR) || 
        __HAL_FLASH_GET_FLAG(FLASH_FLAG_PGPERR) || 
        __HAL_FLASH_GET_FLAG(FLASH_FLAG_PGSERR)) {
        
        // Clear error flags
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | 
                             FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | 
                             FLASH_FLAG_PGSERR);
        return 0;
    }
    
    return 1;
}


/**
 * @brief Determines the flash sector index for a given address.
 * @param addr The flash memory address.
 * @retval Sector index or 0xFF if address is invalid.
 */
uint8_t flash_getSector(uint32_t addr) {
    if (addr < FLASH_BASE) {
        return -1; // Invalid addr
    }
    
    uint32_t offset = addr - FLASH_BASE;
    uint32_t current_offset = 0;
    
    for (uint8_t i = 0; i < FLASH_SECTOR_COUNT; i++) {
        uint32_t sector_size_bytes = FLASH_SECTORS_KB[i] * 1024;
        if (offset >= current_offset && offset < current_offset + sector_size_bytes) {
            return i;
        }
        current_offset += sector_size_bytes;
    }
    
    return -1; // addr
}

/**
 * @brief Gets the starting address of a given sector.
 * @param sector Sector index.
 * @retval Sector start address, or -1 if invalid.
 */
uint32_t flash_getSectorStart(uint8_t sector) {
    if (sector >= FLASH_SECTOR_COUNT) {
        return -1;
    }
    
    uint32_t address = FLASH_BASE;
    for (uint8_t i = 0; i < sector; i++) {
        address += FLASH_SECTORS_KB[i] * 1024;
    }
    
    return address;
}

/**
 * @brief Gets the ending address of a given sector.
 * @param sector Sector index.
 * @retval Sector end address, or -1 if invalid.
 */
uint32_t flash_getSectorEnd(uint8_t sector) {
    if (sector >= FLASH_SECTOR_COUNT) {
        return -1;
    }
    
    uint32_t end_address = FLASH_BASE;
    for (uint8_t i = 0; i <= sector; i++) {
        end_address += FLASH_SECTORS_KB[i] * 1024;
    }
    
    return end_address - 1; // Last valid address in sector
}

/**
 * @brief Erases a single flash sector by its address.
 * @param sectorAddr Address located in the target sector.
 * @retval Number of bytes that were erased if successful, -1 otherwise.
 */
int flash_sectorErase(uint32_t sectorAddr) {
    // Check if any pending operations
    if (HAL_FLASH_GetError() != HAL_FLASH_ERROR_NONE) {
        // Clear error flags
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | 
                             FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | 
                             FLASH_FLAG_PGSERR);
        return -1;
    }
    
    // Get sector number
    uint8_t sector = flash_getSector(sectorAddr);
    if (sector == 0xFF) {
        return 0;
    }
    
    // Prepare for erase
    FLASH_EraseInitTypeDef EraseInitStruct = {0};
    uint32_t SectorError = 0;
    
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3; // 2.7V to 3.6V
    EraseInitStruct.Sector = sector;
    EraseInitStruct.NbSectors = 1;
    
    // Unlock flash
    if (!flash_unlock()) {
        return 0;
    }
    
    // Erase the sector
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);
    
    // Lock flash again
    flash_lock();
    
    if (status != HAL_OK) {
        return 0;
    }
    
    // Return the size of the erased sector in bytes
    return FLASH_SECTORS_KB[sector] * 1024;
}

/**
 * @brief Erases all flash sectors starting from a specified address.
 * @param destination Starting address for erase.
 * @retval 0 if successful, -1 otherwise.
 */
int flash_erase(uint32_t destination) {
    // Check if any pending operations
    if (HAL_FLASH_GetError() != HAL_FLASH_ERROR_NONE) {
        return -1;
    }
    
    // Get starting sector
    uint8_t start_sector = flash_getSector(destination);
    if (start_sector == 0xFF) {
        return -1;
    }
    
    // Unlock flash
    if (!flash_unlock()) {
        return -1;
    }
    
    // Erase all sectors from start_sector to the end
    FLASH_EraseInitTypeDef EraseInitStruct = {0};
    uint32_t SectorError = 0;
    
    for (uint8_t i = start_sector; i < FLASH_SECTOR_COUNT; i++) {
        EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
        EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
        EraseInitStruct.Sector = i;
        EraseInitStruct.NbSectors = 1;
        
        if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
            flash_lock();
            return -1;
        }
    }
    
    // Lock flash
    flash_lock();
    
    return 0;
}

/**
 * @brief Writes data to flash memory.
 * @param addr Target flash address.
 * @param data Pointer to data buffer.
 * @param len Number of bytes to write.
 * @retval 0 if successful, -1 otherwise.
 */
int flash_write(uint32_t addr, const uint8_t* data, size_t len) {
    if (len == 0) {
        return -1; // Nothing to do
    }
    
    // Check alignment
    if (len % 4 != 0) {
        // Use static buffer for alignment
        static uint8_t aligned_buffer[256]; // Static buffer for alignment
        
        // Check if buffer is large enough
        size_t aligned_len = ((len + 3) / 4) * 4;
        if (aligned_len > sizeof(aligned_buffer)) {
            return -1; // Buffer too small
        }
        
        // Copy data to aligned buffer
        memcpy(aligned_buffer, data, len);
        
        // Pad with 0xFF (erased flash state)
        for (size_t i = len; i < aligned_len; i++) {
            aligned_buffer[i] = 0xFF;
        }
        
        // Call ourselves with the aligned data
        return flash_write(addr, aligned_buffer, aligned_len);
    }
    
    // Wait for any previous operations
    if (!flash_waitForLastOperation()) {
        return -1;
    }
    
    // Unlock
    if (!flash_unlock()) {
        return -1;
    }
    
    // Program flash
    for (size_t offset = 0; offset < len; offset += 4) {
        uint32_t data_word = 0;
        memcpy(&data_word, data + offset, 4);
        
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + offset, data_word) != HAL_OK) {
            flash_lock();
            return -1;
        }
        
        // Wait with timeout
        if (!flash_waitForLastOperation()) {
            flash_lock();
            return -1;
        }
        
        // Verify written data
        uint32_t read_data = *(__IO uint32_t*)(addr + offset);
        if (read_data != data_word) {
            flash_lock();
            return -1;
        }
    }
    
    // Lock flash
    flash_lock();
    
    return 0;
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
 * @retval 0 if successful, -1 otherwise.
 */
int flash_writeAcrossSectors(uint32_t currentAddr, uint8_t currentSector,
                              const uint8_t* data, size_t dataLen,
                              uint32_t* newAddr, uint8_t* newSector) {
    
    // Check if we need to cross a sector boundary
    uint32_t next_addr = currentAddr + dataLen;
    uint8_t target_sector = flash_getSector(next_addr - 1);
    
    if (target_sector != currentSector && target_sector != 0xFF) {
        // Calculate sector boundaries
        uint32_t currentSector_end = flash_getSectorEnd(currentSector);
        uint32_t next_sector_base = flash_getSectorStart(target_sector);
        
        // Calculate how much data goes in each sector
        uint32_t bytes_in_current = currentSector_end - currentAddr + 1;
        
        // Make sure it's aligned
        bytes_in_current = (bytes_in_current / 4) * 4;
        
        // Erase the next sector first
        if (!flash_sectorErase(next_sector_base)) {
            return -1;
        }
        
        // Write to current sector if needed
        if (bytes_in_current > 0 && bytes_in_current <= dataLen) {
            if (!flash_write(currentAddr, data, bytes_in_current)) {
                return -1;
            }
        }
        
        // Write to next sector
        uint32_t bytes_in_next = dataLen - bytes_in_current;
        if (bytes_in_next > 0) {
            if (!flash_write(next_sector_base, data + bytes_in_current, bytes_in_next)) {
                return -1;
            }
        }
        
        // Update output parameters
        if (newAddr) {
            *newAddr = next_sector_base + bytes_in_next;
        }
        
        if (newSector) {
            *newSector = target_sector;
        }
    } else {
        // Standard write within the same sector
        if (!flash_write(currentAddr, data, dataLen)) {
            return -1;
        }
        
        // Update output parameters
        if (newAddr) {
            *newAddr = currentAddr + dataLen;
        }
        
        if (newSector) {
            *newSector = currentSector;
        }
    }
    
    return 0;
}