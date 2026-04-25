/**
 * @file   image_header.c
 * @brief  Places the firmware image header at the start of the application
 *         flash region (FLASH_HDR, address 0x08004000).
 *
 * The fields crc, data_size, and vector_addr are initially zero and are
 * patched in-place by scripts/patch_image_header.py after linking.
 *
 * IMAGE_HDR_SIZE (0x200 = 512 bytes) is enforced by the linker script:
 * the .image_hdr section is padded/aligned to 0x200.
 */

#include "image.h"

/* GIT_COMMIT_HASH is injected by CMake as a compile definition string.
   It is 8 hex characters + null terminator (9 bytes), well within [16]. */
const image_hdr_t __attribute__((section(".image_hdr"))) __attribute__((used))
image_header = {
    .image_magic       = IMAGE_MAGIC_APP,
    .image_hdr_version = IMAGE_HDR_VERSION,
    .image_type        = IMAGE_TYPE_APP,
    .version_major     = 1,
    .version_minor     = 0,
    .version_patch     = 0,
    ._padding          = 0,
    .vector_addr       = 0,
    .crc               = 0,
    .data_size         = 0,
    .git_sha           = GIT_COMMIT_HASH,
    /* .reserved is zero-initialised by default */
};