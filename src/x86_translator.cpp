#include "x86_translator.hpp"

namespace
{
    constexpr std::uint8_t X86_NOP = 0x90;
    constexpr std::uint8_t X86_HLT = 0xF4;

    AMD3DC_Instruction make_instruction(
        AMD3DC_Opcode opcode,
        std::uint8_t rd = 0,
        std::uint8_t rs1 = 0,
        std::uint8_t rs2 = 0
    )
    {
        AMD3DC_Instruction result{};
        result.opcode = opcode;
        result.rd = rd;
        result.rs1 = rs1;
        result.rs2 = rs2;
        return result;
    }

    void emit_zero_extend16(
        X86TranslationResult& result,
        std::uint8_t reg
    )
    {
        result.instructions.push_back(
            make_instruction(AMD3DC_OP_ZERO_EXT16, reg));
    }
}

X86TranslationResult amd3dc_translate_i8086(
    const std::uint8_t* bytes,
    std::size_t size
)
{
    X86TranslationResult result;

    if (bytes == nullptr && size != 0)
    {
        result.error = "null input buffer";
        return result;
    }

    struct BranchFixup
    {
        std::size_t source_target;
        std::size_t output_instruction;
    };

    std::vector<std::size_t> source_to_output(size + 1, SIZE_MAX);
    std::vector<BranchFixup> fixups;
    std::size_t offset = 0;
    while (offset < size)
    {
        const std::size_t instruction_offset = offset;
        source_to_output[instruction_offset] = result.instructions.size();
        const std::uint8_t opcode = bytes[offset++];

        if (opcode == X86_NOP)
        {
            result.instructions.push_back(
                make_instruction(AMD3DC_OP_NOP));
        }
        else if (opcode == X86_HLT)
        {
            result.instructions.push_back(
                make_instruction(AMD3DC_OP_HALT));
        }
        else if (opcode >= 0xB8 && opcode <= 0xBF)
        {
            if (size - offset < 2)
            {
                result.error_offset = instruction_offset;
                result.error = "truncated MOV r16, imm16";
                result.instructions.clear();
                return result;
            }

            const std::uint8_t reg = opcode - 0xB8;
            const std::uint16_t immediate =
                static_cast<std::uint16_t>(bytes[offset]) |
                (static_cast<std::uint16_t>(bytes[offset + 1]) << 8);
            offset += 2;

            AMD3DC_Instruction movi =
                make_instruction(AMD3DC_OP_MOVI, reg);
            movi.imm = immediate;
            result.instructions.push_back(movi);
        }
        else if (opcode == 0xEB || opcode == 0xE9 ||
            opcode == 0x74 || opcode == 0x75)
        {
            const std::size_t displacement_size = opcode == 0xE9 ? 2 : 1;
            if (size - offset < displacement_size)
            {
                result.error_offset = instruction_offset;
                result.error = "truncated relative branch";
                result.instructions.clear();
                return result;
            }

            std::int32_t displacement;
            if (displacement_size == 1)
            {
                displacement = static_cast<std::int8_t>(bytes[offset]);
            }
            else
            {
                const std::uint16_t raw =
                    static_cast<std::uint16_t>(bytes[offset]) |
                    (static_cast<std::uint16_t>(bytes[offset + 1]) << 8);
                displacement = static_cast<std::int16_t>(raw);
            }
            offset += displacement_size;

            const std::int64_t target =
                static_cast<std::int64_t>(offset) + displacement;
            if (target < 0 || target >= static_cast<std::int64_t>(size))
            {
                result.error_offset = instruction_offset;
                result.error = "relative branch leaves translated block";
                result.instructions.clear();
                return result;
            }

            const AMD3DC_Opcode target_opcode =
                opcode == 0xEB || opcode == 0xE9
                    ? AMD3DC_OP_JMP
                    : opcode == 0x74 ? AMD3DC_OP_JZ : AMD3DC_OP_JNZ;
            const std::size_t branch_index = result.instructions.size();
            result.instructions.push_back(
                make_instruction(target_opcode));
            fixups.push_back({static_cast<std::size_t>(target), branch_index});
        }
        else
        {
            if (offset >= size)
            {
                result.error_offset = instruction_offset;
                result.error = "truncated ModRM byte";
                result.instructions.clear();
                return result;
            }

            const std::uint8_t modrm = bytes[offset++];
            const std::uint8_t mod = modrm >> 6;
            const std::uint8_t reg = (modrm >> 3) & 7;
            const std::uint8_t rm = modrm & 7;

            if (mod != 3)
            {
                result.error_offset = instruction_offset;
                result.error = "only 16-bit register-direct ModRM is supported";
                result.instructions.clear();
                return result;
            }

            AMD3DC_Instruction translated{};
            std::uint8_t written_reg = 0;
            bool writes_register = true;

            switch (opcode)
            {
            case 0x89: /* MOV r/m16, r16 */
                translated = make_instruction(AMD3DC_OP_MOV, rm, reg);
                written_reg = rm;
                break;
            case 0x8B: /* MOV r16, r/m16 */
                translated = make_instruction(AMD3DC_OP_MOV, reg, rm);
                written_reg = reg;
                break;
            case 0x01: /* ADD r/m16, r16 */
                translated = make_instruction(AMD3DC_OP_ADD, rm, rm, reg);
                written_reg = rm;
                break;
            case 0x03: /* ADD r16, r/m16 */
                translated = make_instruction(AMD3DC_OP_ADD, reg, reg, rm);
                written_reg = reg;
                break;
            case 0x29: /* SUB r/m16, r16 */
                translated = make_instruction(AMD3DC_OP_SUB, rm, rm, reg);
                written_reg = rm;
                break;
            case 0x2B: /* SUB r16, r/m16 */
                translated = make_instruction(AMD3DC_OP_SUB, reg, reg, rm);
                written_reg = reg;
                break;
            case 0x31: /* XOR r/m16, r16 */
                translated = make_instruction(AMD3DC_OP_XOR, rm, rm, reg);
                written_reg = rm;
                break;
            case 0x33: /* XOR r16, r/m16 */
                translated = make_instruction(AMD3DC_OP_XOR, reg, reg, rm);
                written_reg = reg;
                break;
            case 0x39: /* CMP r/m16, r16 */
                translated = make_instruction(AMD3DC_OP_CMP, 0, rm, reg);
                writes_register = false;
                break;
            case 0x3B: /* CMP r16, r/m16 */
                translated = make_instruction(AMD3DC_OP_CMP, 0, reg, rm);
                writes_register = false;
                break;
            default:
                result.error_offset = instruction_offset;
                result.error = "unsupported 8086 real-mode opcode";
                result.instructions.clear();
                return result;
            }

            result.instructions.push_back(translated);
            if (writes_register)
                emit_zero_extend16(result, written_reg);
        }

        result.source_bytes = offset;
    }

    source_to_output[size] = result.instructions.size();
    for (const BranchFixup& fixup : fixups)
    {
        const std::size_t target_index = source_to_output[fixup.source_target];
        if (target_index == SIZE_MAX || target_index >= result.instructions.size())
        {
            result.error_offset = fixup.source_target;
            result.error = "branch target is not an instruction boundary";
            result.instructions.clear();
            return result;
        }

        const std::int64_t delta =
            (static_cast<std::int64_t>(target_index) -
                static_cast<std::int64_t>(fixup.output_instruction) - 1) * 8;
        result.instructions[fixup.output_instruction].imm =
            static_cast<std::uint32_t>(delta);
    }

    return result;
}

X86TranslationResult amd3dc_translate_i386(
    const std::uint8_t* bytes,
    std::size_t size
)
{
    return amd3dc_translate_i8086(bytes, size);
}
