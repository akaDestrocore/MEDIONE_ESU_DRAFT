#ifndef _IMAGE_H
#define _IMAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define IMAGE_MAGIC_APP       0xDEADC0DE
#define IMAGE_MAGIC_UPDATER   0xC0FFEE00
#define UPDATER_ADDR          ((uint32_t)0x08004000U)
#define APP_ADDR              ((uint32_t)0x08010000U)
#define IMAGE_TYPE_APP        2U
#define IMAGE_TYPE_UPDATER    1U
#define IMAGE_HDR_SIZE        0x200U
#define IMAGE_HDR_VERSION     0x0100U

typedef struct {
    uint32_t image_magic;        // IMAGE_MAGIC_APP
    uint16_t image_hdr_version;  // IMAGE_HDR_VERSION
    uint8_t  image_type;         // IMAGE_TYPE_APP
    uint8_t  version_major;
    uint8_t  version_minor;
    uint8_t  version_patch;
    uint16_t _padding;
    uint32_t vector_addr;        // Patched post-build
    uint32_t crc;                // Patched post-build
    uint32_t data_size;          // Patched post-build
    char     git_sha[16];        // 8-char hex SHA + NUL
    uint8_t  reserved[0x1D8];    // Pad to exactly IMAGE_HDR_SIZE bytes
} __attribute__((packed)) image_hdr_t;

int image_isValid(const image_hdr_t *header);

#ifdef __cplusplus
}
#endif

#endif /* _IMAGE_H */