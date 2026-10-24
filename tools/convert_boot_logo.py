#!/usr/bin/env python3
"""Convert a 240x240 RGB image to the firmware's RGB565 RLE stream.

Each record is three bytes: run length, RGB565 high byte, RGB565 low byte.
Runs longer than 255 pixels are split.  The stream is decoded directly to the
LCD, so it consumes Flash but no framebuffer RAM.
"""

import argparse
from pathlib import Path

from PIL import Image


WIDTH = 240
HEIGHT = 240


def to_rgb565(rgb: tuple[int, int, int]) -> int:
    red, green, blue = rgb
    return ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3)


def encode(pixels: list[int]) -> bytes:
    output = bytearray()
    run_color = pixels[0]
    run_length = 0

    for color in pixels:
        if color == run_color and run_length < 255:
            run_length += 1
            continue
        output.extend((run_length, run_color >> 8, run_color & 0xFF))
        run_color = color
        run_length = 1

    output.extend((run_length, run_color >> 8, run_color & 0xFF))
    return bytes(output)


def verify(encoded: bytes, expected: list[int]) -> None:
    decoded: list[int] = []
    if len(encoded) % 3:
        raise ValueError("RLE stream size is not divisible by three")
    for offset in range(0, len(encoded), 3):
        count = encoded[offset]
        color = (encoded[offset + 1] << 8) | encoded[offset + 2]
        decoded.extend([color] * count)
    if decoded != expected:
        raise ValueError("RLE verification failed")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    image = Image.open(args.input).convert("RGB")
    if image.size != (WIDTH, HEIGHT):
        raise ValueError(f"expected {WIDTH}x{HEIGHT}, got {image.size}")

    pixels = [to_rgb565(pixel) for pixel in image.get_flattened_data()]
    encoded = encode(pixels)
    verify(encoded, pixels)
    args.output.write_bytes(encoded)

    raw_size = WIDTH * HEIGHT * 2
    print(
        f"{args.input.name}: RGB565 {raw_size} bytes -> "
        f"RLE {len(encoded)} bytes ({len(encoded) / raw_size:.1%})"
    )


if __name__ == "__main__":
    main()
