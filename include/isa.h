#ifndef AMD3DC_ISA_H
#define AMD3DC_ISA_H

#include <stdbool.h>
#include <stdint.h>

#include "amd3dc_types.h"

/*
 * Cellarium 3DC ISA
 *
 * Fixed-width 64-bit instruction.
 *
 *  63........56 55....52 51....48 47....44 43........32 31........0
 *  +----------+--------+--------+--------+------------+----------+
 *  |  OPCODE  |   RD   |  RS1   |  RS2   |  RESERVED  |   IMM32  |
 *  +----------+--------+--------+--------+------------+----------+
 */

typedef enum
{
    AMD3DC_EXT_BASE = 0,
    AMD3DC_EXT_CVX,
    AMD3DC_EXT_CSX,
    AMD3DC_EXT_QCX,
    AMD3DC_EXT_PQCX,
    AMD3DC_EXT_BDX
} AMD3DC_ISAExtension;

typedef enum
{
    /* Base */
    AMD3DC_OP_NOP = 0x00,
    AMD3DC_OP_MOV = 0x01,
    AMD3DC_OP_ADD = 0x02,
    AMD3DC_OP_SUB = 0x03,
    AMD3DC_OP_AND = 0x04,
    AMD3DC_OP_OR = 0x05,
    AMD3DC_OP_XOR = 0x06,
    AMD3DC_OP_SHL = 0x07,
    AMD3DC_OP_SHR = 0x08,
    AMD3DC_OP_LOAD64 = 0x09,
    AMD3DC_OP_STORE64 = 0x0A,
    AMD3DC_OP_CMP = 0x0B,
    AMD3DC_OP_JMP = 0x0C,
    AMD3DC_OP_JZ = 0x0D,
    AMD3DC_OP_ZERO_EXT32 = 0x0E,
    AMD3DC_OP_MOVI = 0x62,
    AMD3DC_OP_ZERO_EXT16 = 0x63,
    AMD3DC_OP_JNZ = 0x64,

    /* CVX */
    AMD3DC_OP_CVX_VADD = 0x10,
    AMD3DC_OP_CVX_VSUB = 0x11,
    AMD3DC_OP_CVX_VMUL = 0x12,
    AMD3DC_OP_CVX_VDOT = 0x13,
    AMD3DC_OP_CVX_VZERO = 0x14,

    /* LAE */
    AMD3DC_OP_LAE_GET = 0x20,
    AMD3DC_OP_LAE_SET = 0x21,
    AMD3DC_OP_LAE_SYNC = 0x22,

    /* CSX */
    AMD3DC_OP_V3DREAD = 0x30,
    AMD3DC_OP_V3DWRITE = 0x31,
    AMD3DC_OP_MGRIDMAP = 0x32,
    AMD3DC_OP_ZSHIFT = 0x33,

    /* QCX */
    AMD3DC_OP_QMATMUL_P4 = 0x40,
    AMD3DC_OP_QMATMUL_P8 = 0x41,
    AMD3DC_OP_VECSIM_COS = 0x42,
    AMD3DC_OP_VECSIM_EUCL = 0x43,
    AMD3DC_OP_QSTATE_EMUL = 0x44,

    /* PQCX */
    AMD3DC_OP_PQC_KYBER_ENC = 0x50,
    AMD3DC_OP_PQC_KYBER_DEC = 0x51,
    AMD3DC_OP_PQC_DILITHIUM = 0x52,
    AMD3DC_OP_VSECMASK = 0x53,

    /* BDX */
    AMD3DC_OP_HASH_BLOOM_3D = 0x60,
    AMD3DC_OP_MASK_ANONYM = 0x61,

    /* Halt */
    AMD3DC_OP_HALT = 0xFF

} AMD3DC_Opcode;

typedef struct
{
    AMD3DC_Opcode opcode;

    uint8_t rd;
    uint8_t rs1;
    uint8_t rs2;

    /*
     * Both names are supported so old code using "imm"
     * and code using "immediate" can coexist.
     */
    union
    {
        uint32_t imm;
        uint32_t immediate;
    };

} AMD3DC_Instruction;

/*
 * 64-bit instruction encoding / decoding.
 */
AMD3DC_Instruction amd3dc_decode64(
    uint64_t raw
);

uint64_t amd3dc_encode64(
    const AMD3DC_Instruction* instruction
);

/*
 * Execute one instruction on core 0.
 *
 * IMPORTANT:
 * This is the only execution API used by the emulator:
 *
 *     amd3dc_execute64(state, &instruction);
 */
bool amd3dc_execute64(
    AMD3DC_State* state,
    const AMD3DC_Instruction* instruction
);

/*
 * ISA helpers.
 */
bool amd3dc_is_extension_opcode(
    AMD3DC_Opcode opcode
);

AMD3DC_ISAExtension amd3dc_get_extension(
    AMD3DC_Opcode opcode
);

#endif /* AMD3DC_ISA_H */
