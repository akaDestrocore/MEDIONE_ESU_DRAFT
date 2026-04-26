#include "crc.h"

/**
 * @brief  Initializes the CRC peripheral clock.
 * @note   This function is a wrapper around the HAL function to enable the CRC clock.
 * @note   Must be called before performing any CRC operations.
 */
void crc_init(void) {
    __HAL_RCC_CRC_CLK_ENABLE();
}

/**
 * @brief  Resets the CRC calculation unit.
 * @note   This clears the internal CRC calculation register and prepares it for a new computation.
 */
void crc_reset(void) {
    CRC->CR = CRC_CR_RESET;
}


/**
 * @brief  Calculates the CRC for a given data buffer.
 * @param  pData: [in] Pointer to the data buffer.
 * @param  len: [in] Length of the buffer in bytes.
 * @return The calculated CRC value.
 * @note   This function processes data as 32-bit words, and handles trailing bytes manually.
 */
uint32_t crc_calculate(const uint8_t* pData, size_t len) {

    if (NULL == pData || 0 == len) {
        return 0;
    }

    crc_reset();
    
    // Process data word by word
    for (size_t i = 0; i < len / 4; i++) {
        CRC->DR = ((uint32_t*)pData)[i];
    }
    
    // Handle remaining bytes
    uint32_t remaining = len % 4;
    if (remaining > 0) {
        uint32_t lastWord  = 0;
        size_t offset = len - remaining;
        
        for (size_t i = 0; i < remaining; i++) {
            lastWord  |= (uint32_t)pData[offset + i] << (i * 8);
        }
        
        CRC->DR = lastWord;
    }
    
    return CRC->DR;
}

/**
 * @brief  Calculates the CRC of a memory region starting at a given address.
 * @param  addr: [in] Start address of the memory region.
 * @param  size: [in] Size of the memory region in bytes.
 * @return The computed CRC value.
 * @note   Memory is read as 32-bit words; remaining bytes are padded and processed.
 */
uint32_t crc_calculateMemory(uint32_t addr, uint32_t size) {

    if (addr < 0x08000000U || addr >= FLASH_END || size == 0) {
        return 0;
    }

    if ((addr + size) > FLASH_END || (addr + size) < addr) {
        return 0;
    }

    crc_init();
    crc_reset();
    
    // Process data word by word
    for (uint32_t i = 0; i < size / 4; i++) {
        uint32_t data = *((uint32_t*)(addr + i * 4));
        CRC->DR = data;
    }
    
    // Handle remaining bytes
    uint32_t remaining = size % 4;
    if (remaining > 0) {
        uint32_t lastWord  = 0;
        uint32_t offset = addr + size - remaining;
        
        for (uint32_t i = 0; i < remaining; i++) {
            lastWord  |= (uint32_t)(*((uint8_t*)(offset + i))) << (i * 8);
        }
        
        CRC->DR = lastWord;
    }
    
    return CRC->DR;
}

/**
 * @brief  Verifies the CRC of a firmware image.
 * @param  addr: [in] Start address of the firmware image in flash.
 * @param  headerSize: [in] Size of the image header in bytes.
 * @return CRC_OK if CRC matches, error code otherwise.
 * @note   This function reads the header to get the expected CRC and data size, then calculates and compares.
 */
crc_Status_e crc_verifyFirmware(uint32_t addr, uint32_t headerSize) {
    
    const image_hdr_t *header = (const image_hdr_t *)addr;
    uint32_t firmwareAddr = addr + headerSize;

    if (0 == header->data_size || firmwareAddr + header->data_size > FLASH_END)  {
        return CRC_INVALID_SIZE;
    }
    
    // Calculate CRC for the image
    uint32_t calculatedCrc = crc_calculateMemory(firmwareAddr, header->data_size);
    
    // Compare with stored CRC
    return (calculatedCrc == header->crc) ? CRC_OK : CRC_MISMATCH;
}

crc_Status_e crc_invalidateFirmware(uint32_t addr) {
    
    uint32_t erasedBytes;
    flash_Status_e flashStatus = flash_sectorErase(addr, &erasedBytes);

    if (flashStatus != FLASH_OK) {
        return CRC_INVALID_SIZE;
    }

    return CRC_OK;
}