# AMD 3DC

Boot media can be configured in `BOOT.LD`: `IMAGE=` selects the boot image,
`HDD=` attaches a sector-aligned disk image as the primary PATA master and
BIOS drive `0x80`, and `FLOPPY2=` attaches a 1.44 MB floppy image as BIOS
drive `0x01`. The real-mode machine exposes a basic primary-channel ATA PIO
device at ports `0x1F0`–`0x1F7` and alternate status at `0x3F6`.
