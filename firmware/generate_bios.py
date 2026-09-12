#!/usr/bin/env python3
"""
Generate the AMD 3DC BIOS binary image.

This creates a minimal bootable BIOS that:
1. Has a valid C3DBIOS header
2. Executes MOV r0, 1 (requests BIOS setup)
3. Executes HALT
"""

import struct
import os

def main():
    # Create firmware directory if it doesn't exist
    os.makedirs(os.path.dirname(__file__), exist_ok=True)

    # BIOS header: 0x20 bytes
    # Magic: "C3DBIOS\0" (8 bytes)
    magic = b'C3DBIOS\0'

    # Version: 0x00000001 (big-endian)
    version = struct.pack('>I', 1)

    # Flags: 0x00000000
    flags = struct.pack('>I', 0)

    # Entry point: 0x20 (offset where code starts, after header)
    entry_point = struct.pack('>Q', 0x20)

    # Image size: 0x30 (48 bytes = header 0x20 + two instructions 0x10)
    image_size = struct.pack('>Q', 0x30)

    # Instruction 1: MOV r0, 1
    # Encoding: 0x01 (opcode) 0x0 (rd=0) 0x0 (rs1=0) 0x0 (rs2=0) 0x00000001 (imm=1)
    # Raw: 0x0100000000000001
    mov_r0_1 = struct.pack('>Q', 0x0100000000000001)

    # Instruction 2: HALT
    # Encoding: 0xFF (opcode) rest zeros
    # Raw: 0xFF00000000000000
    halt = struct.pack('>Q', 0xFF00000000000000)

    # Assemble the complete image
    bios_image = magic + version + flags + entry_point + image_size + mov_r0_1 + halt

    # Write to file
    bios_path = os.path.join(os.path.dirname(__file__), 'bios.bin')
    with open(bios_path, 'wb') as f:
        f.write(bios_image)

    print(f"Generated BIOS binary: {bios_path}")
    print(f"Size: {len(bios_image)} bytes (0x{len(bios_image):02x})")
    print(f"Hex dump:")
    for i in range(0, len(bios_image), 16):
        chunk = bios_image[i:i+16]
        hex_str = ' '.join(f'{b:02X}' for b in chunk)
        print(f"  {i:04X}: {hex_str}")

if __name__ == '__main__':
    main()