"""Initialize the blank 64 MiB hdd.img as an active FAT16 data disk."""

from pathlib import Path
import struct


image_path = Path(__file__).with_name("hdd.img")
image = bytearray(image_path.read_bytes())
if len(image) != 64 * 1024 * 1024 or any(image):
    raise SystemExit("Refusing to overwrite: hdd.img is not the expected blank 64 MiB image")

total_sectors = len(image) // 512
partition_start = 2048
partition_sectors = total_sectors - partition_start
sectors_per_cluster = 2
fat_sectors = 252
root_entries = 512

# MBR with one active FAT16 partition. Boot code stays empty: this is a data
# disk intended to be accessed from DOS booted from the existing floppy image.
mbr = memoryview(image)[:512]
mbr[446] = 0x80
mbr[450] = 0x06
struct.pack_into("<II", mbr, 454, partition_start, partition_sectors)
mbr[510:512] = b"\x55\xaa"

# FAT16 volume boot record.
volume_offset = partition_start * 512
boot = memoryview(image)[volume_offset : volume_offset + 512]
boot[0:3] = b"\xeb\x3c\x90"
boot[3:11] = b"MSWIN4.1"
struct.pack_into(
    "<HBHBHHBHHHII",
    boot,
    11,
    512,                 # bytes per sector
    sectors_per_cluster,
    1,                   # reserved sectors
    2,                   # FAT copies
    root_entries,
    0,                   # 16-bit total sector count (use 32-bit field below)
    0xF8,
    fat_sectors,
    63,
    16,
    partition_start,
    partition_sectors,
)
boot[36] = 0x80
boot[38] = 0x29
struct.pack_into("<I", boot, 39, 0x3DC2026)
boot[43:54] = b"AMD3DC HDD "
boot[54:62] = b"FAT16   "
boot[510:512] = b"\x55\xaa"

# Initialize the first two reserved entries in both FAT copies.
fat1_sector = partition_start + 1
fat2_sector = fat1_sector + fat_sectors
struct.pack_into("<HH", image, fat1_sector * 512, 0xFFF8, 0xFFFF)
struct.pack_into("<HH", image, fat2_sector * 512, 0xFFF8, 0xFFFF)

image_path.write_bytes(image)
print(
    f"Initialized {len(image) // (1024 * 1024)} MiB HDD as FAT16; "
    f"active partition starts at LBA {partition_start}"
)
