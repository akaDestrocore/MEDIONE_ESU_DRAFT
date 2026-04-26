#ifndef _IMAGE_H
#define _IMAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define IMAGE_MAGIC_APP       0xDEADC0DE
#define APP_ADDR              ((uint32_t)0x08004000U)
#define IMAGE_TYPE_APP        1U
#define IMAGE_HDR_SIZE        0x200U
#define IMAGE_HDR_VERSION     0x0100U

typedef struct {
    uint32_t image_magic;        /* Magic number — IMAGE_MAGIC_APP               */
    uint16_t image_hdr_version;  /* Header format version — IMAGE_HDR_VERSION    */
    uint8_t  image_type;         /* IMAGE_TYPE_APP                               */
    uint8_t  version_major;
    uint8_t  version_minor;
    uint8_t  version_patch;
    uint16_t _padding;           /* Alignment pad                                */
    uint32_t vector_addr;        /* Address of app vector table (patched post-build) */
    uint32_t crc;                /* STM32 HW CRC32 of image data (patched post-build) */
    uint32_t data_size;          /* Size of image data in bytes (patched post-build) */
    char     git_sha[16];        /* 8-char hex SHA + NUL, GCC compile-time constant */
    uint8_t  reserved[0x1D8];    /* Pad to exactly 0x200 bytes                   */
} __attribute__((packed)) image_hdr_t;

int image_isValid(const image_hdr_t *header);

#ifdef __cplusplus
}
#endif

#endif /* _IMAGE_H */