#!/usr/bin/env python3

import argparse
import struct
import sys

IMAGE_HDR_SIZE    = 0x200
IMAGE_HDR_MAGIC   = 0xDEADC0DE
IMAGE_HDR_VERSION = 0x0100

OFFSET_CRC        = 0x10
OFFSET_DATA_SIZE  = 0x14
OFFSET_VECTOR     = 0x0C

HEADER_SIZE = 0x200


def stm32_crc32(data: bytes) -> int:
    crc = 0xFFFFFFFF

    length = len(data)
    i = 0

    while i < length:
        word = 0
        for j in range(4):
            if i + j < length:
                word |= data[i + j] << (8 * j)

        crc ^= word

        for _ in range(32):
            if crc & 0x80000000:
                crc = ((crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF
            else:
                crc = (crc << 1) & 0xFFFFFFFF

        i += 4

    return crc


def patch(input_file: str, output_file: str, base_addr: int) -> None:
    with open(input_file, 'rb') as f:
        raw = f.read()

    if len(raw) < IMAGE_HDR_SIZE:
        print("ERROR: binary smaller than header", file=sys.stderr)
        sys.exit(1)

    header = bytearray(raw[:IMAGE_HDR_SIZE])
    data   = raw[IMAGE_HDR_SIZE:]

    magic, version = struct.unpack_from('<IH', header, 0)

    if magic != IMAGE_HDR_MAGIC:
        raise RuntimeError(f"Bad magic: 0x{magic:08X}")

    if version != IMAGE_HDR_VERSION:
        raise RuntimeError(f"Bad header version: 0x{version:04X}")

    crc = stm32_crc32(data)
    size = len(data)
    vector_addr = base_addr + HEADER_SIZE

    struct.pack_into('<I', header, OFFSET_CRC, crc)
    struct.pack_into('<I', header, OFFSET_DATA_SIZE, size)
    struct.pack_into('<I', header, OFFSET_VECTOR, vector_addr)

    with open(output_file, 'wb') as f:
        f.write(header)
        f.write(data)

    print(f"Patched: {output_file}")
    print(f"  size        : {size}")
    print(f"  crc         : 0x{crc:08X}")
    print(f"  vector_addr : 0x{vector_addr:08X}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input")
    parser.add_argument("output")
    parser.add_argument("--base-addr", type=lambda x: int(x, 0), required=True)
    args = parser.parse_args()

    patch(args.input, args.output, args.base_addr)


if __name__ == "__main__":
    main()