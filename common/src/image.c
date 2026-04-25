#include <image.h>

/**
 * @brief Validates an image header based on its magic value.
 * @param header Pointer to the packet image header.
 * @return 0 if valid, error code otherwise.
 */
int image_isValid(const ImageHeader_Packet_t* header) {

    return !(header->image_magic == IMAGE_MAGIC_APP);
}