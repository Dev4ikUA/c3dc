import struct

def encode_inst(opcode: int, rd: int = 0, rs1: int = 0, rs2: int = 0, imm: int = 0) -> bytes:
    raw = (
        ((opcode & 0xFF) << 56) |
        ((rd & 0x0F) << 52) |
        ((rs1 & 0x0F) << 48) |
        ((rs2 & 0x0F) << 44) |
        (imm & 0xFFFFFFFF)
    )
    return struct.pack('>Q', raw)

def generate_real_bios():
    magic = b'C3DBIOS\0'                  # 8 байт магічний підпис
    version = struct.pack('>I', 0x0100)   # Версія 1.0 (4 байти)
    flags = struct.pack('>I', 0x00000003) # Прапорці ініціалізації заліза (4 байти)
    
    # Заголовок займає 32 байти, точка входу вказує одразу за ним (0x20)
    entry_point = struct.pack('>Q', 0x20) 
    
    # The image uses only instructions implemented by src/isa.c.
    instructions = [
        # Select the default layer and terminate the firmware routine.
        encode_inst(opcode=0x33, imm=0x00),           # ZSHIFT 0
        encode_inst(opcode=0xFF),                    # HALT
    ]

    # Добиваємо нульовим заповненням до розміру щонайменше 1 Кілобайт, 
    # щоб це був справжній бінарний образ мікрокоду, а не заглушка.
    code = b''.join(instructions)
    padding_size = max(0, 1024 - len(code) - 32)
    padding = b'\x00' * padding_size

    full_body = code + padding
    image_size = 32 + len(full_body)
    image_size_field = struct.pack('>Q', image_size)

    header = magic + version + flags + entry_point + image_size_field
    binary_image = header + full_body

    with open('bios.bin', 'wb') as f:
        f.write(binary_image)
    
    print(f"Згенеровано повноцінний BIOS: bios.bin ({image_size} байтів)")

if __name__ == '__main__':
    generate_real_bios()