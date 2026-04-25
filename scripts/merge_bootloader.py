#!/usr/bin/env python3

import argparse
import os
import sys


def merge(boot_path, app_path, output_path, app_offset):
    with open(boot_path, "rb") as f:
        bl = f.read()

    with open(app_path, "rb") as f:
        app = f.read()

    if len(bl) > app_offset:
        raise RuntimeError("Bootloader too large")

    padding = b'\xFF' * (app_offset - len(bl))

    merged = bl + padding + app

    with open(output_path, "wb") as f:
        f.write(merged)

    print(f"MERGED IMAGE: {output_path}")
    print(f"  BOOTLOADER : {len(bl)} bytes")
    print(f"  APP        : {len(app)} bytes")
    print(f"  TOTAL      : {len(merged)} bytes")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("bootloader")
    parser.add_argument("app")
    parser.add_argument("output")
    parser.add_argument("--offset", type=lambda x: int(x, 0), required=True)
    args = parser.parse_args()

    merge(args.bootloader, args.app, args.output, args.offset)


if __name__ == "__main__":
    main()