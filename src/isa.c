#include "isa.h"

#include <math.h>
#include <string.h>

#include "memory.h"

/*
 * Internal emulator representation of the separate
 * Quantum/Tensor chiplet.
 *
 * It is intentionally private to the ISA engine for now.
 * Later this can move into amd3dc_types.h as a first-class
 * architectural component of AMD3DC_State.
 */

static bool amd3dc_valid_gpr(
    uint8_t index
)
{
    return index < AMD3DC_REGISTER_COUNT;
}

static bool amd3dc_valid_vector(
    uint8_t index
)
{
    return index < AMD3DC_VECTOR_COUNT;
}

static int64_t amd3dc_sign_extend_imm32(
    uint32_t value
)
{
    return (int64_t)(int32_t)value;
}

static double amd3dc_vector_dot4(
    const AMD3DC_Vector4* a,
    const AMD3DC_Vector4* b
)
{
    return
        (double)a->x * (double)b->x +
        (double)a->y * (double)b->y +
        (double)a->z * (double)b->z +
        (double)a->w * (double)b->w;
}

static double amd3dc_vector_length4(
    const AMD3DC_Vector4* value
)
{
    return sqrt(
        (double)value->x * (double)value->x +
        (double)value->y * (double)value->y +
        (double)value->z * (double)value->z +
        (double)value->w * (double)value->w
    );
}

/* ------------------------------------------------------------------------- */
/* Encode / Decode                                                           */
/* ------------------------------------------------------------------------- */

AMD3DC_Instruction amd3dc_decode64(
    uint64_t raw
)
{
    AMD3DC_Instruction instruction;

    instruction.opcode =
        (AMD3DC_Opcode)((raw >> 56u) & 0xFFu);

    instruction.rd =
        (uint8_t)((raw >> 52u) & 0x0Fu);

    instruction.rs1 =
        (uint8_t)((raw >> 48u) & 0x0Fu);

    instruction.rs2 =
        (uint8_t)((raw >> 44u) & 0x0Fu);

    instruction.imm =
        (uint32_t)(raw & 0xFFFFFFFFu);

    return instruction;
}

uint64_t amd3dc_encode64(
    const AMD3DC_Instruction* instruction
)
{
    uint64_t raw;

    if (instruction == NULL)
        return 0;

    raw = 0;

    raw |=
        ((uint64_t)instruction->opcode & 0xFFULL)
        << 56u;

    raw |=
        ((uint64_t)instruction->rd & 0x0FULL)
        << 52u;

    raw |=
        ((uint64_t)instruction->rs1 & 0x0FULL)
        << 48u;

    raw |=
        ((uint64_t)instruction->rs2 & 0x0FULL)
        << 44u;

    raw |=
        (uint64_t)instruction->imm;

    return raw;
}

/* ------------------------------------------------------------------------- */
/* Extension classification                                                  */
/* ------------------------------------------------------------------------- */

bool amd3dc_is_extension_opcode(
    AMD3DC_Opcode opcode
)
{
    switch (opcode)
    {
    case AMD3DC_OP_CVX_VADD:
    case AMD3DC_OP_CVX_VSUB:
    case AMD3DC_OP_CVX_VMUL:
    case AMD3DC_OP_CVX_VDOT:
    case AMD3DC_OP_CVX_VZERO:

    case AMD3DC_OP_V3DREAD:
    case AMD3DC_OP_V3DWRITE:
    case AMD3DC_OP_MGRIDMAP:
    case AMD3DC_OP_ZSHIFT:

    case AMD3DC_OP_QMATMUL_P4:
    case AMD3DC_OP_QMATMUL_P8:
    case AMD3DC_OP_VECSIM_COS:
    case AMD3DC_OP_VECSIM_EUCL:
    case AMD3DC_OP_QSTATE_EMUL:

    case AMD3DC_OP_PQC_KYBER_ENC:
    case AMD3DC_OP_PQC_KYBER_DEC:
    case AMD3DC_OP_PQC_DILITHIUM:
    case AMD3DC_OP_VSECMASK:

    case AMD3DC_OP_HASH_BLOOM_3D:
    case AMD3DC_OP_MASK_ANONYM:
        return true;

    default:
        return false;
    }
}

AMD3DC_ISAExtension amd3dc_get_extension(
    AMD3DC_Opcode opcode
)
{
    switch (opcode)
    {
    case AMD3DC_OP_CVX_VADD:
    case AMD3DC_OP_CVX_VSUB:
    case AMD3DC_OP_CVX_VMUL:
    case AMD3DC_OP_CVX_VDOT:
    case AMD3DC_OP_CVX_VZERO:
        return AMD3DC_EXT_CVX;

    case AMD3DC_OP_V3DREAD:
    case AMD3DC_OP_V3DWRITE:
    case AMD3DC_OP_MGRIDMAP:
    case AMD3DC_OP_ZSHIFT:
        return AMD3DC_EXT_CSX;

    case AMD3DC_OP_QMATMUL_P4:
    case AMD3DC_OP_QMATMUL_P8:
    case AMD3DC_OP_VECSIM_COS:
    case AMD3DC_OP_VECSIM_EUCL:
    case AMD3DC_OP_QSTATE_EMUL:
        return AMD3DC_EXT_QCX;

    case AMD3DC_OP_PQC_KYBER_ENC:
    case AMD3DC_OP_PQC_KYBER_DEC:
    case AMD3DC_OP_PQC_DILITHIUM:
    case AMD3DC_OP_VSECMASK:
        return AMD3DC_EXT_PQCX;

    case AMD3DC_OP_HASH_BLOOM_3D:
    case AMD3DC_OP_MASK_ANONYM:
        return AMD3DC_EXT_BDX;

    default:
        return AMD3DC_EXT_BASE;
    }
}

/* ------------------------------------------------------------------------- */
/* Execution                                                                 */
/* ------------------------------------------------------------------------- */

bool amd3dc_execute64(
    AMD3DC_State* state,
    const AMD3DC_Instruction* instruction
)
{
    AMD3DC_Core* core;
    AMD3DC_Memory* memory;

    if (state == NULL ||
        instruction == NULL ||
        state->cores == NULL ||
        state->core_count == 0)
    {
        return false;
    }

    /*
     * Current architectural execution target is core 0.
     * Multi-core scheduling will be added later.
     */
    core = &state->cores[0];

    if (!core->enabled ||
        core->faulty ||
        core->halted)
    {
        return false;
    }

    memory = &state->memory;

    switch (instruction->opcode)
    {
        /* ================================================================ */
        /* BASE ISA                                                         */
        /* ================================================================ */

    case AMD3DC_OP_NOP:
        return true;

    case AMD3DC_OP_MOVI:
        if (!amd3dc_valid_gpr(instruction->rd))
            return false;
        core->registers[instruction->rd] = instruction->imm;
        return true;

    case AMD3DC_OP_MOV:

        if (!amd3dc_valid_gpr(instruction->rd) ||
            !amd3dc_valid_gpr(instruction->rs1))
        {
            return false;
        }

        core->registers[instruction->rd] =
            core->registers[instruction->rs1];

        return true;

    case AMD3DC_OP_ADD:

        if (!amd3dc_valid_gpr(instruction->rd) ||
            !amd3dc_valid_gpr(instruction->rs1) ||
            !amd3dc_valid_gpr(instruction->rs2))
        {
            return false;
        }

        core->registers[instruction->rd] =
            core->registers[instruction->rs1] +
            core->registers[instruction->rs2];

        return true;

    case AMD3DC_OP_SUB:

        if (!amd3dc_valid_gpr(instruction->rd) ||
            !amd3dc_valid_gpr(instruction->rs1) ||
            !amd3dc_valid_gpr(instruction->rs2))
        {
            return false;
        }

        core->registers[instruction->rd] =
            core->registers[instruction->rs1] -
            core->registers[instruction->rs2];

        return true;

    case AMD3DC_OP_AND:

        if (!amd3dc_valid_gpr(instruction->rd) ||
            !amd3dc_valid_gpr(instruction->rs1) ||
            !amd3dc_valid_gpr(instruction->rs2))
        {
            return false;
        }

        core->registers[instruction->rd] =
            core->registers[instruction->rs1] &
            core->registers[instruction->rs2];

        return true;

    case AMD3DC_OP_OR:

        if (!amd3dc_valid_gpr(instruction->rd) ||
            !amd3dc_valid_gpr(instruction->rs1) ||
            !amd3dc_valid_gpr(instruction->rs2))
        {
            return false;
        }

        core->registers[instruction->rd] =
            core->registers[instruction->rs1] |
            core->registers[instruction->rs2];

        return true;

    case AMD3DC_OP_XOR:

        if (!amd3dc_valid_gpr(instruction->rd) ||
            !amd3dc_valid_gpr(instruction->rs1) ||
            !amd3dc_valid_gpr(instruction->rs2))
        {
            return false;
        }

        core->registers[instruction->rd] =
            core->registers[instruction->rs1] ^
            core->registers[instruction->rs2];

        return true;

    case AMD3DC_OP_SHL:

        if (!amd3dc_valid_gpr(instruction->rd) ||
            !amd3dc_valid_gpr(instruction->rs1))
        {
            return false;
        }

        core->registers[instruction->rd] =
            core->registers[instruction->rs1] <<
            (instruction->imm & 63u);

        return true;

    case AMD3DC_OP_SHR:

        if (!amd3dc_valid_gpr(instruction->rd) ||
            !amd3dc_valid_gpr(instruction->rs1))
        {
            return false;
        }

        core->registers[instruction->rd] =
            core->registers[instruction->rs1] >>
            (instruction->imm & 63u);

        return true;

    case AMD3DC_OP_LOAD64:
    {
        int64_t offset;
        uint64_t address;

        if (!amd3dc_valid_gpr(instruction->rd) ||
            !amd3dc_valid_gpr(instruction->rs1))
        {
            return false;
        }

        offset =
            amd3dc_sign_extend_imm32(instruction->imm);

        address =
            core->registers[instruction->rs1] +
            (uint64_t)offset;

        core->registers[instruction->rd] =
            amd3dc_memory_read64(
                memory,
                (size_t)address
            );

        return true;
    }

    case AMD3DC_OP_STORE64:
    {
        int64_t offset;
        uint64_t address;

        if (!amd3dc_valid_gpr(instruction->rd) ||
            !amd3dc_valid_gpr(instruction->rs1))
        {
            return false;
        }

        offset =
            amd3dc_sign_extend_imm32(instruction->imm);

        address =
            core->registers[instruction->rs1] +
            (uint64_t)offset;

        return amd3dc_memory_write64(
            memory,
            (size_t)address,
            core->registers[instruction->rd]
        );
    }

    case AMD3DC_OP_CMP:

        if (!amd3dc_valid_gpr(instruction->rs1) ||
            !amd3dc_valid_gpr(instruction->rs2))
        {
            return false;
        }

        core->flags = 0;

        if (core->registers[instruction->rs1] ==
            core->registers[instruction->rs2])
        {
            core->flags |= 1ULL;
        }

        if (core->registers[instruction->rs1] <
            core->registers[instruction->rs2])
        {
            core->flags |= 2ULL;
        }

        if (core->registers[instruction->rs1] >
            core->registers[instruction->rs2])
        {
            core->flags |= 4ULL;
        }

        return true;

    case AMD3DC_OP_JMP:

        core->pc +=
            (uint64_t)amd3dc_sign_extend_imm32(
                instruction->imm
            );

        return true;

    case AMD3DC_OP_JZ:

        if ((core->flags & 1ULL) != 0)
        {
            core->pc +=
                (uint64_t)amd3dc_sign_extend_imm32(
                    instruction->imm
                );
        }

        return true;

    case AMD3DC_OP_JNZ:
        if ((core->flags & 1ULL) == 0)
        {
            core->pc +=
                (uint64_t)amd3dc_sign_extend_imm32(
                    instruction->imm
                );
        }
        return true;

    case AMD3DC_OP_ZERO_EXT32:
        if (!amd3dc_valid_gpr(instruction->rd))
            return false;

        core->registers[instruction->rd] &= 0xFFFFFFFFULL;
        return true;

    case AMD3DC_OP_ZERO_EXT16:
        if (!amd3dc_valid_gpr(instruction->rd))
            return false;

        core->registers[instruction->rd] &= 0xFFFFULL;
        return true;

        /* ================================================================ */
        /* CVX                                                               */
        /* ================================================================ */

    case AMD3DC_OP_CVX_VADD:

        if (!amd3dc_valid_vector(instruction->rd) ||
            !amd3dc_valid_vector(instruction->rs1) ||
            !amd3dc_valid_vector(instruction->rs2))
        {
            return false;
        }

        core->vector_registers[instruction->rd].x =
            core->vector_registers[instruction->rs1].x +
            core->vector_registers[instruction->rs2].x;

        core->vector_registers[instruction->rd].y =
            core->vector_registers[instruction->rs1].y +
            core->vector_registers[instruction->rs2].y;

        core->vector_registers[instruction->rd].z =
            core->vector_registers[instruction->rs1].z +
            core->vector_registers[instruction->rs2].z;

        core->vector_registers[instruction->rd].w =
            core->vector_registers[instruction->rs1].w +
            core->vector_registers[instruction->rs2].w;

        return true;

    case AMD3DC_OP_CVX_VSUB:

        if (!amd3dc_valid_vector(instruction->rd) ||
            !amd3dc_valid_vector(instruction->rs1) ||
            !amd3dc_valid_vector(instruction->rs2))
        {
            return false;
        }

        core->vector_registers[instruction->rd].x =
            core->vector_registers[instruction->rs1].x -
            core->vector_registers[instruction->rs2].x;

        core->vector_registers[instruction->rd].y =
            core->vector_registers[instruction->rs1].y -
            core->vector_registers[instruction->rs2].y;

        core->vector_registers[instruction->rd].z =
            core->vector_registers[instruction->rs1].z -
            core->vector_registers[instruction->rs2].z;

        core->vector_registers[instruction->rd].w =
            core->vector_registers[instruction->rs1].w -
            core->vector_registers[instruction->rs2].w;

        return true;

    case AMD3DC_OP_CVX_VMUL:
    {
        float scalar;

        if (!amd3dc_valid_vector(instruction->rd) ||
            !amd3dc_valid_vector(instruction->rs1))
        {
            return false;
        }

        scalar =
            (float)(int32_t)instruction->imm;

        core->vector_registers[instruction->rd].x =
            core->vector_registers[instruction->rs1].x *
            scalar;

        core->vector_registers[instruction->rd].y =
            core->vector_registers[instruction->rs1].y *
            scalar;

        core->vector_registers[instruction->rd].z =
            core->vector_registers[instruction->rs1].z *
            scalar;

        core->vector_registers[instruction->rd].w =
            core->vector_registers[instruction->rs1].w *
            scalar;

        return true;
    }

    case AMD3DC_OP_CVX_VDOT:
    {
        double result;

        if (!amd3dc_valid_gpr(instruction->rd) ||
            !amd3dc_valid_vector(instruction->rs1) ||
            !amd3dc_valid_vector(instruction->rs2))
        {
            return false;
        }

        result =
            amd3dc_vector_dot4(
                &core->vector_registers[instruction->rs1],
                &core->vector_registers[instruction->rs2]
            );

        /*
         * Integerized first implementation.
         * Floating-point vector result support can be added later.
         */
        core->registers[instruction->rd] =
            (uint64_t)(int64_t)result;

        return true;
    }

    case AMD3DC_OP_CVX_VZERO:

        if (!amd3dc_valid_vector(instruction->rd))
            return false;

        memset(
            &core->vector_registers[instruction->rd],
            0,
            sizeof(core->vector_registers[instruction->rd])
        );

        return true;

        /* ================================================================ */
        /* LAE                                                               */
        /* ================================================================ */

    case AMD3DC_OP_LAE_GET:

        if (!amd3dc_valid_gpr(instruction->rd))
            return false;

        core->registers[instruction->rd] =
            core->layer;

        return true;

    case AMD3DC_OP_LAE_SET:

        if (!amd3dc_valid_gpr(instruction->rs1))
            return false;

        core->layer =
            core->registers[instruction->rs1];

        return true;

    case AMD3DC_OP_LAE_SYNC:

        return true;

        /* ================================================================ */
        /* CSX                                                               */
        /* ================================================================ */

    case AMD3DC_OP_V3DREAD:
    {
        int64_t offset;
        uint64_t address;

        if (!amd3dc_valid_gpr(instruction->rd) ||
            !amd3dc_valid_gpr(instruction->rs1))
        {
            return false;
        }

        offset =
            amd3dc_sign_extend_imm32(instruction->imm);

        address =
            core->registers[instruction->rs1] +
            (uint64_t)offset;

        core->registers[instruction->rd] =
            amd3dc_memory_read64(
                memory,
                (size_t)address
            );

        return true;
    }

    case AMD3DC_OP_V3DWRITE:
    {
        int64_t offset;
        uint64_t address;

        if (!amd3dc_valid_gpr(instruction->rd) ||
            !amd3dc_valid_gpr(instruction->rs1))
        {
            return false;
        }

        offset =
            amd3dc_sign_extend_imm32(instruction->imm);

        address =
            core->registers[instruction->rs1] +
            (uint64_t)offset;

        return amd3dc_memory_write64(
            memory,
            (size_t)address,
            core->registers[instruction->rd]
        );
    }

    case AMD3DC_OP_MGRIDMAP:

        /*
         * IMM32 is currently a compact grid descriptor.
         * R0 acts as the architectural mapping status register.
         */
        core->registers[0] =
            (uint64_t)instruction->imm;

        return true;

    case AMD3DC_OP_ZSHIFT:

        /*
         * Current emulator represents active Z context directly
         * in the core layer field.
         */
        core->layer =
            (uint64_t)instruction->imm;

        return true;

        /* ================================================================ */
        /* QCX -- Quantum/Tensor chiplet                                     */
        /* ================================================================ */

    case AMD3DC_OP_QMATMUL_P4:
    case AMD3DC_OP_QMATMUL_P8:
    {
        double result;

        if (!amd3dc_valid_gpr(instruction->rd) ||
            !amd3dc_valid_vector(instruction->rs1) ||
            !amd3dc_valid_vector(instruction->rs2))
        {
            return false;
        }

        /*
         * First tensor model:
         * one Vector4 pair represents a tiny tile and a dot-product
         * represents the reduction stage.
         */
        result =
            amd3dc_vector_dot4(
                &core->vector_registers[instruction->rs1],
                &core->vector_registers[instruction->rs2]
            );

        core->registers[instruction->rd] =
            (uint64_t)(int64_t)result;


        return true;
    }

    case AMD3DC_OP_VECSIM_COS:
    {
        double dot;
        double length_a;
        double length_b;
        double similarity;

        if (!amd3dc_valid_gpr(instruction->rd) ||
            !amd3dc_valid_vector(instruction->rs1) ||
            !amd3dc_valid_vector(instruction->rs2))
        {
            return false;
        }

        dot =
            amd3dc_vector_dot4(
                &core->vector_registers[instruction->rs1],
                &core->vector_registers[instruction->rs2]
            );

        length_a =
            amd3dc_vector_length4(
                &core->vector_registers[instruction->rs1]
            );

        length_b =
            amd3dc_vector_length4(
                &core->vector_registers[instruction->rs2]
            );

        if (length_a == 0.0 || length_b == 0.0)
        {
            similarity = 0.0;
        }
        else
        {
            similarity =
                dot / (length_a * length_b);
        }

        /*
         * Store exact IEEE-754 double bits into the GPR.
         */
        memcpy(
            &core->registers[instruction->rd],
            &similarity,
            sizeof(similarity)
        );

        return true;
    }

    case AMD3DC_OP_VECSIM_EUCL:
    {
        double dx;
        double dy;
        double dz;
        double dw;
        double distance;

        if (!amd3dc_valid_gpr(instruction->rd) ||
            !amd3dc_valid_vector(instruction->rs1) ||
            !amd3dc_valid_vector(instruction->rs2))
        {
            return false;
        }

        dx =
            (double)core->vector_registers[instruction->rs1].x -
            (double)core->vector_registers[instruction->rs2].x;

        dy =
            (double)core->vector_registers[instruction->rs1].y -
            (double)core->vector_registers[instruction->rs2].y;

        dz =
            (double)core->vector_registers[instruction->rs1].z -
            (double)core->vector_registers[instruction->rs2].z;

        dw =
            (double)core->vector_registers[instruction->rs1].w -
            (double)core->vector_registers[instruction->rs2].w;

        distance =
            sqrt(
                dx * dx +
                dy * dy +
                dz * dz +
                dw * dw
            );

        memcpy(
            &core->registers[instruction->rd],
            &distance,
            sizeof(distance)
        );


        return true;
    }

    case AMD3DC_OP_QSTATE_EMUL:

        if (!amd3dc_valid_gpr(instruction->rd))
            return false;

        /*
         * IMM[3:0] = number of qubits in the first emulator model.
         * Result = number of computational basis states.
         */
        core->registers[instruction->rd] =
            1ULL << (instruction->imm & 0x0Fu);

        return true;

        /* ================================================================ */
        /* PQCX                                                               */
        /* ================================================================ */

    case AMD3DC_OP_PQC_KYBER_ENC:
    case AMD3DC_OP_PQC_KYBER_DEC:
    case AMD3DC_OP_PQC_DILITHIUM:

        if (!amd3dc_valid_gpr(instruction->rd))
            return false;

        /*
         * Architectural dispatch stub.
         *
         * Dedicated NTT/polynomial engines will replace this later.
         */
        core->registers[instruction->rd] =
            ((uint64_t)instruction->opcode << 32u) |
            (uint64_t)instruction->imm;

        return true;

    case AMD3DC_OP_VSECMASK:

        if (!amd3dc_valid_gpr(instruction->rd) ||
            !amd3dc_valid_gpr(instruction->rs1))
        {
            return false;
        }

        /*
         * Emulator abstraction of layer-distributed secret masking.
         */
        core->registers[instruction->rd] =
            core->registers[instruction->rs1] ^
            (core->layer << 32u);

        return true;

        /* ================================================================ */
        /* BDX                                                               */
        /* ================================================================ */

    case AMD3DC_OP_HASH_BLOOM_3D:

        if (!amd3dc_valid_gpr(instruction->rd) ||
            !amd3dc_valid_gpr(instruction->rs1))
        {
            return false;
        }

        /*
         * First architectural placeholder for a 3D Bloom engine.
         */
        core->registers[instruction->rd] =
            core->registers[instruction->rs1] ^
            (uint64_t)instruction->imm;

        return true;

    case AMD3DC_OP_MASK_ANONYM:

        if (!amd3dc_valid_gpr(instruction->rd) ||
            !amd3dc_valid_gpr(instruction->rs1))
        {
            return false;
        }

        /*
         * First hardware abstraction:
         * clear the upper 16 bits.
         */
        core->registers[instruction->rd] =
            core->registers[instruction->rs1] &
            0x0000FFFFFFFFFFFFULL;

        return true;

        /* ================================================================ */
        /* HALT                                                              */
        /* ================================================================ */

    case AMD3DC_OP_HALT:

        core->halted = true;
        return true;

    default:
        return false;
    }
}
