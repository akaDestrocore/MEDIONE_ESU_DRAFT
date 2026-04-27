#include "image.h"


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
    // .reserved is zero-initialised by default
};