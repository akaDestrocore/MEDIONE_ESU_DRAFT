#ifndef _IMAGE_H
#define _IMAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "crc.h"
#include <stdint.h>
#include <string.h>

#define IMAGE_MAGIC_APP       0xDEADC0DE
#define APP_ADDR              ((uint32_t)0x08004000U)
#define IMAGE_TYPE_APP        1

#define IMAGE_VERSION_CURRENT 0x0100

typedef struct __attribute__((packed)) {
    uint32_t image_magic;        // Magic number (component-specific)
    uint16_t image_hdr_version;  // Header version
    uint8_t  image_type;         // Type of image
    uint8_t  is_patch;           // Flag indicating if this is a delta patch or full image
    uint8_t  version_major;      // Major version number
    uint8_t  version_minor;      // Minor version number
    uint8_t  version_patch;      // Patch version number
    uint8_t  _padding;           // Padding for alignment
    uint32_t vector_addr;        // Address of the vector table
    uint32_t crc;                // CRC of the image (excluding header)
    uint32_t data_size;          // Size of the image data
    char git_sha[16];            // Git SHA
    uint8_t  reserved[0x1D0];    // Reserved space to make header 0x200 bytes
} __attribute__((packed)) image_hdr_t;

// Functions for header operations
int image_isValid(const image_hdr_t* header);

#ifdef __cplusplus
}
#endif

#endif /* _IMAGE_H */