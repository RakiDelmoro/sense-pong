#!/usr/bin/env python3
"""
uf2conv.py - convert a raw RP2040 flash .bin into a .uf2 for BOOTSEL drag-drop.

This replaces picotool for our single purpose (bin -> uf2). It needs no host
compiler, just Python (which ESP-IDF already provides).

Usage:
    python uf2conv.py sense_pong_rp2040.bin
    -> writes sense_pong_rp2040.uf2

The .bin is treated as the flash image starting at 0x10000000 (RP2040 XIP
base). Each UF2 block carries 256 bytes of payload.
"""

import sys
import struct
from pathlib import Path

# RP2040 flash starts here in the address space (XIP_BASE).
BASE_ADDR = 0x10000000
# RP2040 family ID (used by the BOOTSEL loader to reject wrong-family files).
FAMILY_ID = 0xE48BFF56

# UF2 magic numbers.
MAGIC_START_0 = 0x0A324655
MAGIC_START_1 = 0x9E5D5157
MAGIC_END     = 0x0AB16F30
# Flag: familyID field is present and meaningful.
FLAG_FAMILY_ID_PRESENT = 0x00002000

PAYLOAD_SIZE = 256
BLOCK_SIZE   = 512


def bin_to_uf2(bin_path: Path, uf2_path: Path) -> None:
    data = bin_path.read_bytes()
    # Pad the last chunk up to a full 256-byte payload.
    if len(data) % PAYLOAD_SIZE != 0:
        data += b"\x00" * (PAYLOAD_SIZE - (len(data) % PAYLOAD_SIZE))

    num_blocks = len(data) // PAYLOAD_SIZE

    with uf2_path.open("wb") as f:
        for block_no in range(num_blocks):
            addr = BASE_ADDR + block_no * PAYLOAD_SIZE
            payload = data[block_no * PAYLOAD_SIZE:(block_no + 1) * PAYLOAD_SIZE]

            block = bytearray(BLOCK_SIZE)
            struct.pack_into(
                "<IIIIIIII",
                block, 0,
                MAGIC_START_0,
                MAGIC_START_1,
                FLAG_FAMILY_ID_PRESENT,
                addr,
                PAYLOAD_SIZE,
                block_no,
                num_blocks,
                FAMILY_ID,
            )
            block[0x20:0x20 + PAYLOAD_SIZE] = payload
            struct.pack_into("<I", block, BLOCK_SIZE - 4, MAGIC_END)
            f.write(block)

    print(f"Wrote {num_blocks} blocks ({num_blocks * PAYLOAD_SIZE} bytes) -> {uf2_path}")


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: python uf2conv.py <firmware.bin>", file=sys.stderr)
        return 1

    bin_path = Path(sys.argv[1])
    if not bin_path.is_file():
        print(f"error: {bin_path} not found", file=sys.stderr)
        return 1

    uf2_path = bin_path.with_suffix(".uf2")
    bin_to_uf2(bin_path, uf2_path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
