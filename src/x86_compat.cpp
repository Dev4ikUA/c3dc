#include "x86_compat.hpp"

#include "gui.h"
extern "C"
{
#include "igpu.h"
}

#include <cstdio>
#include <algorithm>
#include <cstring>
#include <utility>

namespace
{
    constexpr std::uint16_t CF = 0x0001;
    constexpr std::uint16_t PF = 0x0004;
    constexpr std::uint16_t AF = 0x0010;
    constexpr std::uint16_t ZF = 0x0040;
    constexpr std::uint16_t SF = 0x0080;
    constexpr std::uint16_t TF = 0x0100;
    constexpr std::uint16_t IF = 0x0200;
    constexpr std::uint16_t DF = 0x0400;
    constexpr std::uint16_t OF = 0x0800;
    constexpr std::uint16_t ArithmeticFlags = CF | PF | AF | ZF | SF | OF;

    std::uint16_t biosKeyCode(AMD3DC_Key key)
    {
        switch (key)
        {
        case AMD3DC_KEY_ENTER: return 0x1C0D;
        case AMD3DC_KEY_ESCAPE: return 0x011B;
        case AMD3DC_KEY_UP: return 0x4800;
        case AMD3DC_KEY_DOWN: return 0x5000;
        case AMD3DC_KEY_LEFT: return 0x4B00;
        case AMD3DC_KEY_RIGHT: return 0x4D00;
        case AMD3DC_KEY_TAB: return 0x0F09;
        case AMD3DC_KEY_F1: return 0x3B00;
        case AMD3DC_KEY_F2: return 0x3C00;
        case AMD3DC_KEY_F10: return 0x4400;
        default: break;
        }

        const unsigned value = static_cast<unsigned>(key);
        if (value < AMD3DC_KEY_CHAR_BASE || value > AMD3DC_KEY_CHAR_BASE + 126)
            return 0;
        const std::uint8_t ch = static_cast<std::uint8_t>(value - AMD3DC_KEY_CHAR_BASE);
        if (ch == '\r' || ch == '\n') return 0x1C0D;
        std::uint8_t scan = 0;
        if (ch >= 'a' && ch <= 'z')
        {
            constexpr std::uint8_t scans[26] = {
                0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24,
                0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14,
                0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C
            };
            scan = scans[ch - 'a'];
        }
        else if (ch >= 'A' && ch <= 'Z')
        {
            constexpr std::uint8_t scans[26] = {
                0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24,
                0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14,
                0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C
            };
            scan = scans[ch - 'A'];
        }
        else if (ch >= '1' && ch <= '9') scan = static_cast<std::uint8_t>(2 + ch - '1');
        else if (ch == '0') scan = 0x0B;
        else
        {
            switch (ch)
            {
            case 8: scan = 0x0E; break;
            case ' ': scan = 0x39; break;
            case '-': case '_': scan = 0x0C; break;
            case '=': case '+': scan = 0x0D; break;
            case '[': case '{': scan = 0x1A; break;
            case ']': case '}': scan = 0x1B; break;
            case ';': case ':': scan = 0x27; break;
            case '\'': case '"': scan = 0x28; break;
            case '`': case '~': scan = 0x29; break;
            case '\\': case '|': scan = 0x2B; break;
            case ',': case '<': scan = 0x33; break;
            case '.': case '>': scan = 0x34; break;
            case '/': case '?': scan = 0x35; break;
            default: break;
            }
        }
        return scan == 0 ? 0 : static_cast<std::uint16_t>((scan << 8) | ch);
    }
}

bool X86Compat::bootFloppy(
    AMD3DC_State& state,
    std::vector<std::uint8_t> image)
{
    if (state.memory.data == nullptr || state.memory.size < 0x100000 ||
        image.size() != 1474560 || image[510] != 0x55 || image[511] != 0xAA)
    {
        error_ = "expected a bootable 1.44 MB floppy image";
        return false;
    }

    state_ = &state;
    disk_ = std::move(image);
    std::memset(regs_, 0, sizeof(regs_));
    std::memset(regs_high_, 0, sizeof(regs_high_));
    std::memset(segs_, 0, sizeof(segs_));
    std::memset(state.memory.data, 0, state.memory.size);
    std::memcpy(state.memory.data + 0x7C00, disk_.data(), 512);

    // Install real-mode BIOS interrupt vectors. The host services below are
    // entered through these stubs, while DOS may replace any vector normally.
    for (std::uint16_t vector = 0; vector < 256; ++vector)
    {
        const std::uint16_t offset = static_cast<std::uint16_t>(0x0100 + vector * 4);
        write16(0, vector * 4, offset);
        write16(0, vector * 4 + 2, 0xF000);
        write8(0xF000, offset, 0xCF); // IRET
    }

    // Minimal IBM PC BIOS data area used by DOS during startup.
    state.memory.data[0x410] = 0x21; // equipment: floppy present
    state.memory.data[0x413] = 0x80; // 640 KiB conventional memory
    state.memory.data[0x414] = 0x02;
    state.memory.data[0x449] = 0x03; // 80x25 text mode
    state.memory.data[0x44A] = 80;
    state.memory.data[0x44C] = 0x00;
    state.memory.data[0x44D] = 0x10; // 4 KiB video page
    state.memory.data[0x462] = 0; // active page
    state.memory.data[0x475] = 1; // floppy count
    for (std::size_t i = 0; i < 8; ++i)
    {
        state.memory.data[0x450 + i * 2] = 0;
        state.memory.data[0x451 + i * 2] = 0;
    }

    // PC-compatible floppy boot convention: DL=0, CS:IP=0000:7C00.
    regs_[2] = 0; // DX, boot drive 0
    regs_[4] = 0x7C00; // SP
    segs_[1] = 0; // CS
    segs_[2] = 0; // SS
    segs_[3] = 0; // DS
    segs_[0] = 0; // ES
    ip_ = 0x7C00;
    flags_ = 0x0002;
    halted_ = false;
    cpuSleeping_ = false;
    a20Enabled_ = false;
    waitingForKey_ = false;
    pendingBiosKey_ = 0;
    error_.clear();
    instruction_count_ = 0;
    disk_read_history_.clear();
    disk_read_calls_ = 0;
    disk_sectors_read_ = 0;
    disk_read_failures_ = 0;
    last_disk_lba_ = 0;
    return true;
}

std::uint8_t X86Compat::fetch8()
{
    const std::uint8_t value = read8(segs_[1], ip_);
    ++ip_;
    return value;
}

std::uint16_t X86Compat::fetch16()
{
    const std::uint16_t lo = fetch8();
    return static_cast<std::uint16_t>(lo | (fetch8() << 8));
}

std::uint8_t X86Compat::read8(
    std::uint16_t segment,
    std::uint32_t offset) const
{
    if (state_ == nullptr || state_->memory.data == nullptr)
        return 0xFF;
    std::uint32_t address = (static_cast<std::uint32_t>(segment) << 4) + offset;
    if (!a20Enabled_)
        address &= 0xFFFFF;
    return address < state_->memory.size ? state_->memory.data[address] : 0xFF;
}

std::uint16_t X86Compat::read16(
    std::uint16_t segment,
    std::uint32_t offset) const
{
    const std::uint16_t lo = read8(segment, offset);
    const std::uint16_t hi = read8(segment, (offset + 1) & 0xFFFFu);
    return static_cast<std::uint16_t>(lo | (hi << 8));
}

std::uint32_t X86Compat::read32(
    std::uint16_t segment, std::uint32_t offset, bool address32) const
{
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i)
    {
        const std::uint32_t byteOffset = address32 ? offset + i : ((offset + i) & 0xFFFFu);
        value |= static_cast<std::uint32_t>(read8(segment, byteOffset)) << (i * 8);
    }
    return value;
}

void X86Compat::write8(
    std::uint16_t segment,
    std::uint32_t offset,
    std::uint8_t value)
{
    if (state_ == nullptr || state_->memory.data == nullptr)
        return;
    std::uint32_t address = (static_cast<std::uint32_t>(segment) << 4) + offset;
    if (!a20Enabled_)
        address &= 0xFFFFF;
    if (address < state_->memory.size)
        state_->memory.data[address] = value;
}

void X86Compat::write16(
    std::uint16_t segment,
    std::uint32_t offset,
    std::uint16_t value)
{
    write8(segment, offset, static_cast<std::uint8_t>(value));
    write8(segment, offset + 1,
        static_cast<std::uint8_t>(value >> 8));
}

void X86Compat::write32(
    std::uint16_t segment, std::uint32_t offset, std::uint32_t value,
    bool address32)
{
    for (unsigned i = 0; i < 4; ++i)
    {
        const std::uint32_t byteOffset = address32 ? offset + i : ((offset + i) & 0xFFFFu);
        write8(segment, byteOffset, static_cast<std::uint8_t>(value >> (i * 8)));
    }
}

std::uint16_t X86Compat::getReg16(std::uint8_t reg) const
{
    return regs_[reg & 7];
}

void X86Compat::setReg16(std::uint8_t reg, std::uint16_t value)
{
    regs_[reg & 7] = value;
}

std::uint32_t X86Compat::getReg32(std::uint8_t reg) const
{
    const std::uint8_t index = reg & 7;
    return (static_cast<std::uint32_t>(regs_high_[index]) << 16) | regs_[index];
}

void X86Compat::setReg32(std::uint8_t reg, std::uint32_t value)
{
    const std::uint8_t index = reg & 7;
    regs_[index] = static_cast<std::uint16_t>(value);
    regs_high_[index] = static_cast<std::uint16_t>(value >> 16);
}

std::uint32_t X86Compat::getReg(std::uint8_t reg, bool word) const
{
    if (word)
        return getReg16(reg);
    const std::uint16_t value = regs_[reg & 3];
    return reg < 4 ? value & 0xFF : value >> 8;
}

void X86Compat::setReg(std::uint8_t reg, bool word, std::uint32_t value)
{
    if (word)
    {
        setReg16(reg, static_cast<std::uint16_t>(value));
        return;
    }
    const std::uint8_t index = reg & 3;
    if (reg < 4)
        regs_[index] = static_cast<std::uint16_t>((regs_[index] & 0xFF00) | (value & 0xFF));
    else
        regs_[index] = static_cast<std::uint16_t>((regs_[index] & 0x00FF) | ((value & 0xFF) << 8));
}

X86Compat::Operand X86Compat::decodeOperand(
    std::uint8_t modrm,
    bool word,
    int segmentOverride,
    bool address32)
{
    (void)word;
    Operand operand{};
    const std::uint8_t mod = modrm >> 6;
    const std::uint8_t rm = modrm & 7;
    if (mod == 3)
    {
        operand.isRegister = true;
        operand.reg = rm;
        return operand;
    }

    std::uint32_t base = 0;
    bool usesBp = false;
    if (address32)
    {
        // Operand-size prefixes do not change the legacy register contents
        // used by DOS; address-size override still computes a 32-bit EA.
        if (rm == 4)
        {
            const std::uint8_t sib = fetch8();
            const std::uint8_t scale = sib >> 6;
            const std::uint8_t index = (sib >> 3) & 7;
            const std::uint8_t baseReg = sib & 7;
            if (index != 4) base += static_cast<std::uint32_t>(getReg16(index)) << scale;
            if (mod == 0 && baseReg == 5) base += fetch16();
            else { base += getReg16(baseReg); usesBp = (baseReg == 5 || baseReg == 4); }
        }
        else if (mod == 0 && rm == 5)
            base = fetch16();
        else
        {
            base = getReg16(rm);
            usesBp = rm == 5 || rm == 4;
        }
    }
    else
    {
        switch (rm)
        {
        case 0: base = regs_[3] + regs_[6]; break;
        case 1: base = regs_[3] + regs_[7]; break;
        case 2: base = regs_[5] + regs_[6]; usesBp = true; break;
        case 3: base = regs_[5] + regs_[7]; usesBp = true; break;
        case 4: base = regs_[6]; break;
        case 5: base = regs_[7]; break;
        case 6:
            if (mod == 0) base = fetch16();
            else { base = regs_[5]; usesBp = true; }
            break;
        case 7: base = regs_[3]; break;
        }
    }

    if (mod == 1)
        base += static_cast<std::int8_t>(fetch8());
    else if (mod == 2)
        base += static_cast<std::int16_t>(fetch16());

    operand.segment = segmentOverride >= 0
        ? segs_[segmentOverride]
        : segs_[usesBp ? 2 : 3];
    operand.offset = address32 ? base : (base & 0xFFFFu);
    operand.address32 = address32;
    return operand;
}

std::uint32_t X86Compat::getOperand(const Operand& operand, bool word) const
{
    if (operand.isRegister)
        return getReg(operand.reg, word);
    return word
        ? static_cast<std::uint16_t>(read8(operand.segment, operand.offset) |
            (static_cast<std::uint16_t>(read8(operand.segment,
                operand.address32 ? operand.offset + 1 :
                    ((operand.offset + 1) & 0xFFFFu))) << 8))
                : read8(operand.segment, operand.offset);
}

void X86Compat::setOperand(const Operand& operand, bool word, std::uint32_t value)
{
    if (operand.isRegister)
        setReg(operand.reg, word, value);
    else if (word)
    {
        write8(operand.segment, operand.offset, static_cast<std::uint8_t>(value));
        write8(operand.segment,
            operand.address32 ? operand.offset + 1 :
                ((operand.offset + 1) & 0xFFFFu),
            static_cast<std::uint8_t>(value >> 8));
    }
    else
        write8(operand.segment, operand.offset, static_cast<std::uint8_t>(value));
}

std::uint32_t X86Compat::getOperand32(const Operand& operand) const
{
    return operand.isRegister ? getReg32(operand.reg)
        : read32(operand.segment, operand.offset, operand.address32);
}

void X86Compat::setOperand32(const Operand& operand, std::uint32_t value)
{
    if (operand.isRegister)
        setReg32(operand.reg, value);
    else
        write32(operand.segment, operand.offset, value, operand.address32);
}

void X86Compat::setSZP(std::uint32_t value, bool word)
{
    const std::uint32_t mask = word ? 0xFFFF : 0xFF;
    const std::uint32_t sign = word ? 0x8000 : 0x80;
    value &= mask;
    flags_ &= static_cast<std::uint16_t>(~(SF | ZF | PF));
    if (value == 0) flags_ |= ZF;
    if (value & sign) flags_ |= SF;
    std::uint8_t low = static_cast<std::uint8_t>(value);
    low ^= low >> 4; low &= 0x0F; low ^= low >> 2; low &= 3; low ^= low >> 1;
    if ((low & 1) == 0) flags_ |= PF;
}

void X86Compat::setSZP32(std::uint32_t value)
{
    flags_ &= static_cast<std::uint16_t>(~(SF | ZF | PF));
    if (value == 0) flags_ |= ZF;
    if (value & 0x80000000u) flags_ |= SF;
    std::uint8_t low = static_cast<std::uint8_t>(value);
    low ^= low >> 4; low &= 0x0F; low ^= low >> 2; low &= 3; low ^= low >> 1;
    if ((low & 1) == 0) flags_ |= PF;
}

std::uint32_t X86Compat::alu(
    std::uint8_t operation,
    std::uint32_t lhs,
    std::uint32_t rhs,
    bool word)
{
    const std::uint32_t mask = word ? 0xFFFF : 0xFF;
    const std::uint32_t sign = word ? 0x8000 : 0x80;
    lhs &= mask; rhs &= mask;
    const bool carryIn = (operation == 2 || operation == 3) && (flags_ & CF);
    std::uint32_t result = 0;
    flags_ &= static_cast<std::uint16_t>(~ArithmeticFlags);

    switch (operation & 7)
    {
    case 0: case 2:
    {
        const std::uint32_t full = lhs + rhs + (carryIn ? 1u : 0u);
        result = full & mask;
        if (full > mask) flags_ |= CF;
        if ((~(lhs ^ rhs) & (lhs ^ result) & sign) != 0) flags_ |= OF;
        if (((lhs & 0xF) + (rhs & 0xF) + (carryIn ? 1u : 0u)) > 0xF) flags_ |= AF;
        break;
    }
    case 1: result = lhs | rhs; break;
    case 3: case 5: case 7:
    {
        const std::uint32_t subtrahend = rhs + (carryIn ? 1u : 0u);
        result = (lhs - subtrahend) & mask;
        if (lhs < subtrahend) flags_ |= CF;
        if (((lhs ^ rhs) & (lhs ^ result) & sign) != 0) flags_ |= OF;
        if ((lhs & 0xF) < ((rhs & 0xF) + (carryIn ? 1u : 0u))) flags_ |= AF;
        break;
    }
    case 4: result = lhs & rhs; break;
    case 6: result = lhs ^ rhs; break;
    }

    setSZP(result, word);
    if ((operation & 7) == 1 || (operation & 7) == 4 || (operation & 7) == 6)
        flags_ &= static_cast<std::uint16_t>(~(CF | OF | AF));
    return result;
}

std::uint32_t X86Compat::alu32(
    std::uint8_t operation, std::uint32_t lhs, std::uint32_t rhs)
{
    const bool carryIn = (operation == 2 || operation == 3) && (flags_ & CF);
    flags_ &= static_cast<std::uint16_t>(~ArithmeticFlags);
    std::uint32_t result = 0;
    if ((operation & 7) == 0 || (operation & 7) == 2)
    {
        const std::uint64_t full = static_cast<std::uint64_t>(lhs) + rhs + (carryIn ? 1u : 0u);
        result = static_cast<std::uint32_t>(full);
        if (full >> 32) flags_ |= CF;
        if ((~(lhs ^ rhs) & (lhs ^ result) & 0x80000000u) != 0) flags_ |= OF;
        if (((lhs & 0xF) + (rhs & 0xF) + (carryIn ? 1u : 0u)) > 0xF) flags_ |= AF;
    }
    else if ((operation & 7) == 1) result = lhs | rhs;
    else if ((operation & 7) == 4) result = lhs & rhs;
    else if ((operation & 7) == 6) result = lhs ^ rhs;
    else
    {
        const std::uint64_t subtrahend = static_cast<std::uint64_t>(rhs) + (carryIn ? 1u : 0u);
        result = lhs - static_cast<std::uint32_t>(subtrahend);
        if (static_cast<std::uint64_t>(lhs) < subtrahend) flags_ |= CF;
        if (((lhs ^ rhs) & (lhs ^ result) & 0x80000000u) != 0) flags_ |= OF;
        if ((lhs & 0xF) < ((rhs & 0xF) + (carryIn ? 1u : 0u))) flags_ |= AF;
    }
    setSZP32(result);
    if ((operation & 7) == 1 || (operation & 7) == 4 || (operation & 7) == 6)
        flags_ &= static_cast<std::uint16_t>(~(CF | OF | AF));
    return result;
}

bool X86Compat::condition(std::uint8_t code) const
{
    const bool cf = flags_ & CF, pf = flags_ & PF, zf = flags_ & ZF;
    const bool sf = flags_ & SF, of = flags_ & OF;
    switch (code & 0xF)
    {
    case 0: return of; case 1: return !of;
    case 2: return cf; case 3: return !cf;
    case 4: return zf; case 5: return !zf;
    case 6: return cf || zf; case 7: return !cf && !zf;
    case 8: return sf; case 9: return !sf;
    case 10: return pf; case 11: return !pf;
    case 12: return sf != of; case 13: return sf == of;
    case 14: return zf || (sf != of); default: return !zf && (sf == of);
    }
}

void X86Compat::setError(const char* message)
{
    error_ = message;
    halted_ = true;
}

bool X86Compat::halted() const noexcept { return halted_; }
const std::string& X86Compat::error() const noexcept { return error_; }
std::uint64_t X86Compat::instructionCount() const noexcept { return instruction_count_; }

std::string X86Compat::debugState() const
{
    char text[1024];
    std::snprintf(text, sizeof(text),
        "ES:CS:SS:DS=%04X:%04X:%04X:%04X IP=%04X AX=%04X BX=%04X CX=%04X DX=%04X SP=%04X BP=%04X SI=%04X DI=%04X FLAGS=%04X DISK=%llu/%llu FAIL=%llu LBA=%u READS=",
        segs_[0], segs_[1], segs_[2], segs_[3], ip_, regs_[0], regs_[3], regs_[1], regs_[2],
        regs_[4], regs_[5], regs_[6], regs_[7], flags_,
        static_cast<unsigned long long>(disk_read_calls_),
        static_cast<unsigned long long>(disk_sectors_read_),
        static_cast<unsigned long long>(disk_read_failures_),
        last_disk_lba_);
    std::string result(text);
    for (std::size_t i = 0; i < disk_read_history_.size() && i < 24; ++i)
        result += (i == 0 ? "" : ",") + disk_read_history_[i];
    return result;
}

void X86Compat::renderVideo()
{
    if (state_ != nullptr && state_->memory.data != nullptr &&
        state_->memory.size >= 0xB8000 + 80 * 25 * 2)
    {
        amd3dc_virtio_render_text_mode(
            &state_->virtio,
            state_->memory.data + 0xB8000);
    }
}

void X86Compat::interrupt(std::uint8_t number)
{
    const std::uint16_t vectorOffset = read16(0, number * 4);
    const std::uint16_t vectorSegment = read16(0, number * 4 + 2);
    const std::uint16_t biosOffset = static_cast<std::uint16_t>(0x0100 + number * 4);
    if (vectorSegment != 0xF000 || vectorOffset != biosOffset)
    {
        regs_[4] -= 2;
        write16(segs_[2], regs_[4], flags_);
        regs_[4] -= 2;
        write16(segs_[2], regs_[4], segs_[1]);
        regs_[4] -= 2;
        write16(segs_[2], regs_[4], ip_);
        flags_ &= static_cast<std::uint16_t>(~(IF | TF));
        segs_[1] = vectorSegment;
        ip_ = vectorOffset;
        return;
    }
    biosInterrupt(number);
}

void X86Compat::biosInterrupt(std::uint8_t number)
{
    if (number == 0x10)
    {
        const std::uint8_t ah = regs_[0] >> 8;
        const std::uint8_t al = regs_[0] & 0xFF;
        auto cellAddress = [](std::uint8_t page, std::uint8_t row, std::uint8_t column)
        {
            return static_cast<std::size_t>(0xB8000) +
                static_cast<std::size_t>(page & 7) * 0x1000 +
                (static_cast<std::size_t>(row) * 80 + column) * 2;
        };
        auto scroll = [&](bool down)
        {
            const std::uint8_t top = static_cast<std::uint8_t>(regs_[1] >> 8);
            const std::uint8_t left = static_cast<std::uint8_t>(regs_[1]);
            const std::uint8_t bottom = static_cast<std::uint8_t>(regs_[2] >> 8);
            const std::uint8_t right = static_cast<std::uint8_t>(regs_[2]);
            const std::uint8_t page = static_cast<std::uint8_t>(regs_[3] >> 8) & 7;
            if (top > bottom || left > right || bottom >= 25 || right >= 80)
                return;
            const std::size_t height = bottom - top + 1;
            const std::size_t lines = al == 0 ? height : (std::min<std::size_t>)(al, height);
            if (!down)
            {
                for (std::size_t row = top; row + lines <= bottom; ++row)
                    for (std::size_t col = left; col <= right; ++col)
                    {
                        const std::size_t dst = cellAddress(page, row, col);
                        const std::size_t src = cellAddress(page, row + lines, col);
                        state_->memory.data[dst] = state_->memory.data[src];
                        state_->memory.data[dst + 1] = state_->memory.data[src + 1];
                    }
                for (std::size_t row = bottom - lines + 1; row <= bottom; ++row)
                    for (std::size_t col = left; col <= right; ++col)
                    {
                        const std::size_t dst = cellAddress(page, row, col);
                        state_->memory.data[dst] = 0x20;
                        state_->memory.data[dst + 1] = static_cast<std::uint8_t>(regs_[3]);
                    }
            }
            else
            {
                for (std::size_t row = bottom + 1; row-- > top + lines;)
                    for (std::size_t col = left; col <= right; ++col)
                    {
                        const std::size_t dst = cellAddress(page, row, col);
                        const std::size_t src = cellAddress(page, row - lines, col);
                        state_->memory.data[dst] = state_->memory.data[src];
                        state_->memory.data[dst + 1] = state_->memory.data[src + 1];
                    }
                for (std::size_t row = top; row < top + lines; ++row)
                    for (std::size_t col = left; col <= right; ++col)
                    {
                        const std::size_t dst = cellAddress(page, row, col);
                        state_->memory.data[dst] = 0x20;
                        state_->memory.data[dst + 1] = static_cast<std::uint8_t>(regs_[3]);
                    }
            }
        };
        if (ah == 0x0E)
        {
            std::putchar(al);
            if (state_->memory.size >= 0xB8000 + 4000)
            {
                const std::uint8_t page = static_cast<std::uint8_t>(regs_[3] >> 8) & 7;
                const std::size_t cursor = 0x450 + page * 2;
                std::uint8_t column = state_->memory.data[cursor];
                std::uint8_t row = state_->memory.data[cursor + 1];
                if (al == '\r') column = 0;
                else if (al == '\n') { ++row; }
                else if (al == '\b') { if (column != 0) --column; }
                else if (al == '\t') column = static_cast<std::uint8_t>((column + 8) & ~7u);
                else
                {
                    const std::size_t cell = cellAddress(page, row, column);
                    state_->memory.data[cell] = al;
                    state_->memory.data[cell + 1] = 0x07;
                    if (++column >= 80) { column = 0; ++row; }
                }
                if (row >= 25)
                {
                    const std::uint16_t savedDx = regs_[2];
                    const std::uint16_t savedCx = regs_[1];
                    const std::uint16_t savedAx = regs_[0];
                    const std::uint16_t savedBx = regs_[3];
                    regs_[1] = 0;
                    regs_[2] = static_cast<std::uint16_t>((24 << 8) | 79);
                    regs_[0] = 1 << 8;
                    regs_[3] = static_cast<std::uint16_t>((page << 8) | 0x07);
                    scroll(false);
                    regs_[2] = savedDx; regs_[1] = savedCx;
                    regs_[0] = savedAx; regs_[3] = savedBx;
                    row = 24;
                }
                state_->memory.data[cursor] = column;
                state_->memory.data[cursor + 1] = row;
            }
        }
        else if (ah == 0x00)
        {
            if (state_->memory.size >= 0xB8000 + 4000)
            {
                for (std::size_t i = 0; i < 2000; ++i)
                {
                    state_->memory.data[0xB8000 + i * 2] = 0x20;
                    state_->memory.data[0xB8001 + i * 2] = 0x07;
                }
                state_->memory.data[0x449] = al;
                for (std::size_t i = 0; i < 8; ++i)
                {
                    state_->memory.data[0x450 + i * 2] = 0;
                    state_->memory.data[0x451 + i * 2] = 0;
                }
            }
        }
        else if (ah == 0x02)
        {
            const std::uint8_t page = static_cast<std::uint8_t>(regs_[3] >> 8) & 7;
            state_->memory.data[0x450 + page * 2] = static_cast<std::uint8_t>(regs_[2]);
            state_->memory.data[0x451 + page * 2] = static_cast<std::uint8_t>(regs_[2] >> 8);
        }
        else if (ah == 0x03)
        {
            const std::uint8_t page = static_cast<std::uint8_t>(regs_[3] >> 8) & 7;
            regs_[2] = static_cast<std::uint16_t>(
                (state_->memory.data[0x451 + page * 2] << 8) |
                state_->memory.data[0x450 + page * 2]);
        }
        else if (ah == 0x06 || ah == 0x07)
        {
            if (state_->memory.size >= 0xB8000 + 0x8000)
                scroll(ah == 0x07);
        }
        else if (ah == 0x08)
        {
            const std::uint8_t page = static_cast<std::uint8_t>(regs_[3] >> 8) & 7;
            const std::size_t cursor = 0x450 + page * 2;
            const std::size_t cell = cellAddress(page,
                state_->memory.data[cursor + 1], state_->memory.data[cursor]);
            regs_[0] = static_cast<std::uint16_t>(state_->memory.data[cell] |
                (static_cast<std::uint16_t>(state_->memory.data[cell + 1]) << 8));
        }
        else if (ah == 0x09 || ah == 0x0A)
        {
            const std::uint8_t page = static_cast<std::uint8_t>(regs_[3] >> 8) & 7;
            const std::uint8_t cursor = 0x450 + page * 2;
            const std::uint8_t row = state_->memory.data[cursor + 1];
            std::uint8_t column = state_->memory.data[cursor];
            const std::uint8_t attribute = static_cast<std::uint8_t>(regs_[3]);
            const std::uint16_t count = regs_[1];
            for (std::uint16_t i = 0; i < count && row < 25; ++i)
            {
                const std::size_t cell = cellAddress(page, row, column);
                state_->memory.data[cell] = al;
                if (ah == 0x09) state_->memory.data[cell + 1] = attribute;
                if (++column >= 80) { column = 0; break; }
            }
        }
        else if (ah == 0x0F)
        {
            regs_[0] = static_cast<std::uint16_t>((regs_[0] & 0xFF00) | 0x03);
            regs_[0] = static_cast<std::uint16_t>((regs_[0] & 0x00FF) | (80u << 8));
            regs_[3] &= 0x00FF;
        }
        else if (ah == 0x13)
        {
            std::uint8_t row = static_cast<std::uint8_t>(regs_[2] >> 8);
            std::uint8_t column = static_cast<std::uint8_t>(regs_[2]);
            const std::uint8_t page = static_cast<std::uint8_t>(regs_[3] >> 8) & 7;
            const std::uint8_t attribute = static_cast<std::uint8_t>(regs_[3]);
            const bool updateCursor = (al & 1) != 0;
            const bool attributesInString = (al & 2) != 0;
            std::uint16_t offset = regs_[5]; // BP
            for (std::uint16_t i = 0; i < regs_[1] && row < 25; ++i)
            {
                const std::uint8_t ch = read8(segs_[0], offset++);
                const std::uint8_t cellAttribute = attributesInString
                    ? read8(segs_[0], offset++) : attribute;
                const std::size_t cell = cellAddress(page, row, column);
                state_->memory.data[cell] = ch;
                state_->memory.data[cell + 1] = cellAttribute;
                if (++column >= 80) { column = 0; ++row; }
            }
            if (updateCursor)
            {
                const std::uint8_t cursor = 0x450 + page * 2;
                state_->memory.data[cursor] = column;
                state_->memory.data[cursor + 1] = row;
            }
        }
        flags_ &= static_cast<std::uint16_t>(~CF);
        return;
    }

    if (number == 0x13)
    {
        const std::uint8_t ah = regs_[0] >> 8;
        const std::uint8_t drive = regs_[2] & 0xFF;
        bool ok = drive == 0;
        if (ah == 0x00) // reset disk system
        {
            regs_[0] &= 0x00FF;
        }
        else if (ah == 0x02) // CHS read
        {
            ++disk_read_calls_;
            const std::uint8_t count = regs_[0] & 0xFF;
            const std::uint8_t sector = regs_[1] & 0x3F;
            const std::uint16_t cylinder =
                static_cast<std::uint16_t>(((regs_[1] >> 8) & 0xFF) |
                    ((regs_[1] & 0xC0) << 2));
            const std::uint8_t head = regs_[2] >> 8;
            if (sector == 0 || sector > 18 || head > 1 || cylinder > 79)
                ok = false;
            const std::size_t lba =
                (static_cast<std::size_t>(cylinder) * 2 + head) * 18 + sector - 1;
            last_disk_lba_ = static_cast<std::uint32_t>(lba);
            const std::size_t bytes = static_cast<std::size_t>(count) * 512;
            const std::size_t dest =
                ((static_cast<std::size_t>(segs_[0]) << 4) + regs_[3]) & 0xFFFFF;
            if (disk_read_history_.size() < 24)
            {
                char readText[48];
                std::snprintf(readText, sizeof(readText), "%u:%u@%05X",
                    static_cast<unsigned>(lba), count,
                    static_cast<unsigned>(dest));
                disk_read_history_.emplace_back(readText);
            }
            if (lba * 512 > disk_.size() || bytes > disk_.size() - lba * 512 ||
                dest > state_->memory.size || bytes > state_->memory.size - dest)
                ok = false;
            if (ok)
            {
                std::memcpy(state_->memory.data + dest, disk_.data() + lba * 512, bytes);
                disk_sectors_read_ += count;
            }
            else
                ++disk_read_failures_;
            regs_[0] = static_cast<std::uint16_t>((regs_[0] & 0x00FF) | (ok ? 0 : 0x0100));
        }
        else if (ah == 0x08) // drive parameters
        {
            regs_[1] = static_cast<std::uint16_t>(0x4F00 | 18);
            regs_[2] = 0x0101;
            regs_[0] &= 0x00FF;
        }
        else if (ah == 0x41) // EDD installation check
        {
            if (drive == 0 && (regs_[3] & 0xFFFF) == 0x55AA)
            {
                regs_[3] = 0xAA55;
                regs_[1] = 0x0001; // extended disk access functions
                regs_[0] = 0x3000;
            }
            else
                ok = false;
        }
        else if (ah == 0x42) // EDD extended read
        {
            ++disk_read_calls_;
            const std::uint16_t dapOffset = regs_[6];
            const std::uint16_t dapSegment = segs_[3];
            const std::uint16_t count = read16(dapSegment, dapOffset + 2);
            const std::uint16_t bufferOffset = read16(dapSegment, dapOffset + 4);
            const std::uint16_t bufferSegment = read16(dapSegment, dapOffset + 6);
            const std::uint64_t lba =
                static_cast<std::uint64_t>(read16(dapSegment, dapOffset + 10)) << 16 |
                read16(dapSegment, dapOffset + 8);
            last_disk_lba_ = static_cast<std::uint32_t>(lba);
            const std::size_t bytes = static_cast<std::size_t>(count) * 512;
            const std::size_t dest =
                ((static_cast<std::size_t>(bufferSegment) << 4) + bufferOffset) & 0xFFFFF;
            if (disk_read_history_.size() < 24)
            {
                char readText[48];
                std::snprintf(readText, sizeof(readText), "%u:%u@%05X",
                    static_cast<unsigned>(lba), count,
                    static_cast<unsigned>(dest));
                disk_read_history_.emplace_back(readText);
            }
            if (drive != 0 || lba > disk_.size() / 512 || count == 0 ||
                count > disk_.size() / 512 - lba ||
                dest > state_->memory.size || bytes > state_->memory.size - dest)
                ok = false;
            if (ok)
            {
                std::memcpy(state_->memory.data + dest,
                    disk_.data() + static_cast<std::size_t>(lba) * 512, bytes);
                disk_sectors_read_ += count;
            }
            else
                ++disk_read_failures_;
        }
        else
            ok = false;

        if (ok) flags_ &= static_cast<std::uint16_t>(~CF);
        else { flags_ |= CF; regs_[0] = static_cast<std::uint16_t>((regs_[0] & 0x00FF) | 0x0100); }
        return;
    }

    if (number == 0x16)
    {
        const std::uint8_t function = regs_[0] >> 8;
        AMD3DC_Key key = AMD3DC_KEY_NONE;
        if (pendingBiosKey_ == 0 && amd3dc_gui_poll_key(&key))
            pendingBiosKey_ = biosKeyCode(key);
        const bool hasKey = pendingBiosKey_ != 0;
        const std::uint16_t biosKey = pendingBiosKey_;
        if (function == 0x00 || function == 0x10)
        {
            if (hasKey)
            {
                regs_[0] = biosKey;
                pendingBiosKey_ = 0;
                flags_ &= static_cast<std::uint16_t>(~ZF);
            }
            else
                waitingForKey_ = true;
        }
        else if (function == 0x01 || function == 0x11)
        {
            if (hasKey)
            {
                regs_[0] = biosKey;
                flags_ &= static_cast<std::uint16_t>(~ZF);
            }
            else
                flags_ |= ZF;
        }
        else if (function == 0x02)
            setReg(0, false, 0);
        else
            flags_ |= CF;
        return;
    }

    if (number == 0x11) { regs_[0] = 0x0021; return; }
    if (number == 0x12) { regs_[0] = 640; return; }
    if (number == 0x1A)
    {
        const std::uint8_t function = regs_[0] >> 8;
        if (function == 0x00)
        {
            const std::uint32_t ticks =
                (static_cast<std::uint32_t>(state_->memory.data[0x46F]) << 24) |
                (static_cast<std::uint32_t>(state_->memory.data[0x46E]) << 16) |
                (static_cast<std::uint32_t>(state_->memory.data[0x46D]) << 8) |
                state_->memory.data[0x46C];
            regs_[1] = static_cast<std::uint16_t>(ticks >> 16);
            regs_[2] = static_cast<std::uint16_t>(ticks);
            setReg(0, false, 0);
            flags_ &= static_cast<std::uint16_t>(~CF);
        }
        else if (function == 0x01)
        {
            const std::uint32_t ticks =
                (static_cast<std::uint32_t>(regs_[1]) << 16) | regs_[2];
            instruction_count_ = static_cast<std::uint64_t>(ticks) * 65536u;
            state_->memory.data[0x46C] = static_cast<std::uint8_t>(ticks);
            state_->memory.data[0x46D] = static_cast<std::uint8_t>(ticks >> 8);
            state_->memory.data[0x46E] = static_cast<std::uint8_t>(ticks >> 16);
            state_->memory.data[0x46F] = static_cast<std::uint8_t>(ticks >> 24);
            flags_ &= static_cast<std::uint16_t>(~CF);
        }
        else flags_ |= CF;
        return;
    }
    if (number == 0x15)
    {
        const std::uint8_t function = regs_[0] >> 8;
        if (function == 0x88) // extended memory size in KiB above 1 MiB
        {
            const std::size_t extendedBytes = state_->memory.size > 0x100000
                ? state_->memory.size - 0x100000 : 0;
            regs_[0] = static_cast<std::uint16_t>(
                (std::min)(extendedBytes / 1024, static_cast<std::size_t>(0xFFFF)));
            flags_ &= static_cast<std::uint16_t>(~CF);
        }
        else if (function == 0x24)
        {
            const std::uint8_t subfunction = regs_[0] & 0xFF;
            if (subfunction == 0x00) a20Enabled_ = false;
            else if (subfunction == 0x01) a20Enabled_ = true;
            else if (subfunction == 0x02) regs_[0] = a20Enabled_ ? 1 : 0;
            else if (subfunction == 0x03) regs_[0] = 3; // keyboard and fast A20
            else { flags_ |= CF; return; }
            flags_ &= static_cast<std::uint16_t>(~CF);
        }
        else if (function == 0x86)
            flags_ &= static_cast<std::uint16_t>(~CF); // wait request completed
        else
        {
            regs_[0] &= 0x00FF;
            flags_ |= CF;
        }
        return;
    }
    if (number == 0x19) { segs_[1] = 0; ip_ = 0x7C00; return; }
    if (number == 0x20) { halted_ = true; return; }

    flags_ |= CF;
}

bool X86Compat::step()
{
    if (halted_ || state_ == nullptr)
        return false;

    if (cpuSleeping_)
    {
        AMD3DC_Key key = AMD3DC_KEY_NONE;
        if (!amd3dc_gui_poll_key(&key))
            return true;
        pendingBiosKey_ = biosKeyCode(key);
        cpuSleeping_ = false;
    }

    if (segs_[1] == 0xF000 && ip_ >= 0x0100 && ip_ < 0x0500 &&
        ((ip_ - 0x0100) % 4) == 0)
    {
        const std::uint8_t number = static_cast<std::uint8_t>((ip_ - 0x0100) / 4);
        const std::uint16_t oldCs = segs_[1];
        const std::uint16_t oldIp = ip_;
        biosInterrupt(number);
        if (!halted_ && segs_[1] == oldCs && ip_ == oldIp)
        {
            ip_ = read16(segs_[2], regs_[4]); regs_[4] += 2;
            segs_[1] = read16(segs_[2], regs_[4]); regs_[4] += 2;
            flags_ = static_cast<std::uint16_t>(read16(segs_[2], regs_[4]) | 2);
            regs_[4] += 2;
        }
        ++instruction_count_;
        return !halted_;
    }

    if (waitingForKey_)
    {
        AMD3DC_Key key = AMD3DC_KEY_NONE;
        if (!amd3dc_gui_poll_key(&key))
            return true;
        const std::uint16_t biosKey = biosKeyCode(key);
        regs_[0] = biosKey;
        flags_ &= static_cast<std::uint16_t>(~ZF);
        waitingForKey_ = false;
        return true;
    }

    int segmentOverride = -1;
    std::uint8_t repeat = 0;
    bool address32 = false;
    bool operand32 = false;
    std::uint8_t opcode = 0;
    for (unsigned prefixCount = 0; prefixCount < 8; ++prefixCount)
    {
        opcode = fetch8();
        if (opcode == 0x26) segmentOverride = 0;
        else if (opcode == 0x2E) segmentOverride = 1;
        else if (opcode == 0x36) segmentOverride = 2;
        else if (opcode == 0x3E) segmentOverride = 3;
        else if (opcode == 0x64) segmentOverride = 4;
        else if (opcode == 0x65) segmentOverride = 5;
        else if (opcode == 0x67) address32 = true;
        else if (opcode == 0x66) operand32 = true;
        else if (opcode == 0xF2 || opcode == 0xF3) repeat = opcode;
        else break;
    }
    ++instruction_count_;
    if (state_->memory.size > 0x46F)
    {
        constexpr std::uint32_t TicksPerDay = 0x1800B0;
        const std::uint32_t ticks =
            static_cast<std::uint32_t>((instruction_count_ / 65536u) % TicksPerDay);
        state_->memory.data[0x46C] = static_cast<std::uint8_t>(ticks);
        state_->memory.data[0x46D] = static_cast<std::uint8_t>(ticks >> 8);
        state_->memory.data[0x46E] = static_cast<std::uint8_t>(ticks >> 16);
        state_->memory.data[0x46F] = static_cast<std::uint8_t>(ticks >> 24);
    }

    auto push = [this](std::uint16_t value)
    {
        regs_[4] = static_cast<std::uint16_t>(regs_[4] - 2);
        write16(segs_[2], regs_[4], value);
    };
    auto pop = [this]()
    {
        const std::uint16_t value = read16(segs_[2], regs_[4]);
        regs_[4] = static_cast<std::uint16_t>(regs_[4] + 2);
        return value;
    };
    auto push32 = [this](std::uint32_t value)
    {
        regs_[4] = static_cast<std::uint16_t>(regs_[4] - 4);
        write32(segs_[2], regs_[4], value);
    };
    auto pop32 = [this]()
    {
        const std::uint32_t value = read32(segs_[2], regs_[4]);
        regs_[4] = static_cast<std::uint16_t>(regs_[4] + 4);
        return value;
    };
    auto fail = [this](const char* text)
    {
        setError(text);
        return false;
    };
    auto logicFlags = [this](std::uint32_t value, bool word)
    {
        flags_ &= static_cast<std::uint16_t>(~(CF | OF | AF));
        setSZP(value, word);
    };

    if ((opcode <= 0x3D) && ((opcode & 7) <= 5) && ((opcode >> 3) <= 7))
    {
        const std::uint8_t operation = (opcode >> 3) & 7;
        const std::uint8_t form = opcode & 7;
        if (form <= 3)
        {
            const bool word = (form & 1) != 0;
            const bool dword = operand32 && word;
            const std::uint8_t modrm = fetch8();
            const std::uint8_t reg = (modrm >> 3) & 7;
            const Operand rm = decodeOperand(modrm, word, segmentOverride, address32);
            const bool toReg = (form & 2) != 0;
            const std::uint32_t lhs = dword
                ? (toReg ? getReg32(reg) : getOperand32(rm))
                : (toReg ? getReg(reg, word) : getOperand(rm, word));
            const std::uint32_t rhs = dword
                ? (toReg ? getOperand32(rm) : getReg32(reg))
                : (toReg ? getOperand(rm, word) : getReg(reg, word));
            const std::uint32_t value = dword
                ? alu32(operation, lhs, rhs) : alu(operation, lhs, rhs, word);
            if (operation != 7)
            {
                if (dword)
                {
                    if (toReg) setReg32(reg, value);
                    else setOperand32(rm, value);
                }
                else if (toReg) setReg(reg, word, value);
                else setOperand(rm, word, value);
            }
            return true;
        }

        const bool word = (form & 1) != 0;
        const bool dword = operand32 && word;
        const std::uint32_t immediate = dword ? read32(segs_[1], ip_) :
            word ? fetch16() : fetch8();
        if (dword) ip_ += 4;
        const std::uint32_t lhs = dword ? getReg32(0) : getReg(0, word);
        const std::uint32_t value = dword
            ? alu32(operation, lhs, immediate) : alu(operation, lhs, immediate, word);
        if (operation != 7)
        {
            if (dword) setReg32(0, value);
            else setReg(0, word, value);
        }
        return true;
    }

    if (opcode >= 0x40 && opcode <= 0x47)
    {
        const std::uint16_t oldCarry = flags_ & CF;
        if (operand32) setReg32(opcode & 7, alu32(0, getReg32(opcode & 7), 1));
        else setReg16(opcode & 7, alu(0, getReg16(opcode & 7), 1, true));
        flags_ = static_cast<std::uint16_t>((flags_ & ~CF) | oldCarry);
        return true;
    }
    if (opcode >= 0x48 && opcode <= 0x4F)
    {
        const std::uint16_t oldCarry = flags_ & CF;
        if (operand32) setReg32(opcode & 7, alu32(5, getReg32(opcode & 7), 1));
        else setReg16(opcode & 7, alu(5, getReg16(opcode & 7), 1, true));
        flags_ = static_cast<std::uint16_t>((flags_ & ~CF) | oldCarry);
        return true;
    }
    if (opcode >= 0x50 && opcode <= 0x57)
    {
        if (operand32) push32(getReg32(opcode & 7)); else push(getReg16(opcode & 7));
        return true;
    }
    if (opcode >= 0x58 && opcode <= 0x5F)
    {
        if (operand32) setReg32(opcode & 7, pop32()); else setReg16(opcode & 7, pop());
        return true;
    }
    if (opcode >= 0x70 && opcode <= 0x7F)
    {
        const auto displacement = static_cast<std::int8_t>(fetch8());
        if (condition(opcode & 0xF)) ip_ = static_cast<std::uint16_t>(ip_ + displacement);
        return true;
    }
    if (opcode >= 0xB0 && opcode <= 0xB7) { setReg(opcode & 7, false, fetch8()); return true; }
    if (opcode >= 0xB8 && opcode <= 0xBF)
    {
        if (operand32)
        {
            const std::uint32_t value = read32(segs_[1], ip_); ip_ += 4;
            setReg32(opcode & 7, value);
        }
        else setReg16(opcode & 7, fetch16());
        return true;
    }
    if (opcode >= 0x91 && opcode <= 0x97)
    {
        if (operand32)
        {
            const auto value = getReg32(0);
            setReg32(0, getReg32(opcode & 7)); setReg32(opcode & 7, value);
        }
        else { const auto value = regs_[0]; regs_[0] = regs_[opcode & 7]; regs_[opcode & 7] = value; }
        return true;
    }

    switch (opcode)
    {
    case 0x0F:
    {
        const auto extension = fetch8();
        if (extension == 0xA0) { push(segs_[4]); return true; }
        if (extension == 0xA1) { segs_[4] = pop(); return true; }
        if (extension == 0xA8) { push(segs_[5]); return true; }
        if (extension == 0xA9) { segs_[5] = pop(); return true; }
        if (extension == 0xA2) // CPUID
        {
            switch (getReg32(0))
            {
            case 0:
                setReg32(0, 1);
                setReg32(3, 0x68747541); // "Auth"
                setReg32(2, 0x69746E65); // "enti"
                setReg32(1, 0x444D4163); // "cAMD"
                break;
            case 1:
                setReg32(0, 0x00000500); // Pentium class, family 5
                setReg32(3, 0);
                setReg32(2, 0);
                setReg32(1, 0x00000131); // FPU, TSC, MSR, CX8, CMOV
                break;
            default:
                setReg32(0, 0); setReg32(1, 0); setReg32(2, 0); setReg32(3, 0);
                break;
            }
            return true;
        }
        if (extension >= 0x80 && extension <= 0x8F)
        {
            const auto displacement = static_cast<std::int16_t>(fetch16());
            if (condition(extension & 0xF)) ip_ = static_cast<std::uint16_t>(ip_ + displacement);
            return true;
        }
        if (extension == 0xB6 || extension == 0xB7 ||
            extension == 0xBE || extension == 0xBF)
        {
            const auto modrm = fetch8();
            const auto reg = (modrm >> 3) & 7;
            const bool sourceWord = extension == 0xB7 || extension == 0xBF;
            const auto rm = decodeOperand(modrm, sourceWord, segmentOverride, address32);
            std::uint16_t value = static_cast<std::uint16_t>(getOperand(rm, sourceWord));
            if (extension == 0xBE) value = static_cast<std::uint16_t>(static_cast<std::int8_t>(value));
            if (extension == 0xBF) value = static_cast<std::uint16_t>(static_cast<std::int16_t>(value));
            setReg16(reg, value);
            return true;
        }
        char message[96];
        std::snprintf(message, sizeof(message),
            "unsupported 386 escape 0F %02X at %04X:%04X",
            extension, segs_[1], static_cast<std::uint16_t>(ip_ - 2));
        return fail(message);
    }
    case 0x90: return true;
    case 0xF4:
        // Real-mode DOS commonly idles with STI; HLT while waiting for IRQs.
        // Poll host input while asleep, then resume with the BIOS key queued.
        cpuSleeping_ = true;
        return true;
    case 0xFA: flags_ &= static_cast<std::uint16_t>(~IF); return true;
    case 0xFB: flags_ |= IF; return true;
    case 0xFC: flags_ &= static_cast<std::uint16_t>(~DF); return true;
    case 0xFD: flags_ |= DF; return true;
    case 0xF8: flags_ &= static_cast<std::uint16_t>(~CF); return true;
    case 0xF9: flags_ |= CF; return true;
    case 0xF5: flags_ ^= CF; return true;
    case 0xD6: setReg(0, false, (flags_ & CF) ? 0xFF : 0); return true; // SALC
    case 0x27: case 0x2F:
    {
        const bool subtract = opcode == 0x2F;
        const std::uint8_t oldAl = static_cast<std::uint8_t>(regs_[0]);
        const bool oldCarry = (flags_ & CF) != 0;
        std::uint8_t al = oldAl;
        bool carry = oldCarry;
        if ((al & 0x0F) > 9 || (flags_ & AF))
        {
            al = static_cast<std::uint8_t>(subtract ? al - 6 : al + 6);
            flags_ |= AF;
        }
        else flags_ &= static_cast<std::uint16_t>(~AF);
        if (oldAl > 0x99 || oldCarry)
        {
            al = static_cast<std::uint8_t>(subtract ? al - 0x60 : al + 0x60);
            carry = true;
        }
        else carry = false;
        flags_ = static_cast<std::uint16_t>((flags_ & ~CF) | (carry ? CF : 0));
        setReg(0, false, al); setSZP(al, false); return true;
    }
    case 0x37: case 0x3F:
    {
        const bool subtract = opcode == 0x3F;
        const std::uint8_t al = static_cast<std::uint8_t>(regs_[0]);
        if ((al & 0x0F) > 9 || (flags_ & AF))
        {
            regs_[0] = static_cast<std::uint16_t>(subtract ? regs_[0] - 0x0106 : regs_[0] + 0x0106);
            flags_ |= AF | CF;
        }
        else flags_ &= static_cast<std::uint16_t>(~(AF | CF));
        setReg(0, false, regs_[0] & 0x0F); return true;
    }
    case 0xD4:
    {
        const std::uint8_t base = fetch8();
        if (base == 0) return fail("AAM divide by zero");
        const std::uint8_t al = static_cast<std::uint8_t>(regs_[0]);
        setReg(4, false, al / base); setReg(0, false, al % base);
        setSZP(getReg(0, false), false); return true;
    }
    case 0xD5:
    {
        const std::uint8_t base = fetch8();
        const std::uint8_t value = static_cast<std::uint8_t>(getReg(4, false) * base + getReg(0, false));
        regs_[0] = value; setSZP(value, false); return true;
    }
    case 0x9C: push(flags_); return true;
    case 0x9D: flags_ = static_cast<std::uint16_t>(pop() | 2); return true;
    case 0x60:
    {
        const auto originalSp = regs_[4];
        push(regs_[0]); push(regs_[1]); push(regs_[2]); push(regs_[3]);
        push(originalSp); push(regs_[5]); push(regs_[6]); push(regs_[7]);
        return true;
    }
    case 0x61:
    {
        regs_[7] = pop(); regs_[6] = pop(); regs_[5] = pop(); (void)pop();
        regs_[3] = pop(); regs_[2] = pop(); regs_[1] = pop(); regs_[0] = pop();
        return true;
    }
    case 0x06: case 0x0E: case 0x16: case 0x1E:
        push(segs_[(opcode >> 3) & 3]); return true;
    case 0x07: case 0x17: case 0x1F:
        segs_[(opcode >> 3) & 3] = pop(); return true;
    case 0x68:
        if (operand32) { const auto value = read32(segs_[1], ip_); ip_ += 4; push32(value); }
        else push(fetch16());
        return true;
    case 0x6A:
        if (operand32) push32(static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(fetch8()))));
        else push(static_cast<std::uint16_t>(static_cast<std::int8_t>(fetch8())));
        return true;
    case 0xC3: ip_ = operand32 ? static_cast<std::uint16_t>(pop32()) : pop(); return true;
    case 0xC2:
    {
        const auto adjust = fetch16();
        ip_ = operand32 ? static_cast<std::uint16_t>(pop32()) : pop();
        regs_[4] += adjust; return true;
    }
    case 0xCB: { ip_ = operand32 ? static_cast<std::uint16_t>(pop32()) : pop(); segs_[1] = pop(); return true; }
    case 0xCA: { const auto adjust = fetch16(); ip_ = operand32 ? static_cast<std::uint16_t>(pop32()) : pop(); segs_[1] = pop(); regs_[4] += adjust; return true; }
    case 0xE8:
        if (operand32)
        {
            const std::int32_t rel = static_cast<std::int32_t>(read32(segs_[1], ip_)); ip_ += 4;
            push32(ip_); ip_ = static_cast<std::uint16_t>(ip_ + rel);
        }
        else { const auto rel = static_cast<std::int16_t>(fetch16()); push(ip_); ip_ += rel; }
        return true;
    case 0xE9:
    {
        const std::int32_t displacement = operand32
            ? static_cast<std::int32_t>(read32(segs_[1], ip_))
            : static_cast<std::int16_t>(fetch16());
        if (operand32) ip_ += 4;
        ip_ = static_cast<std::uint16_t>(ip_ + displacement);
        return true;
    }
    case 0xEB:
    {
        const auto displacement = static_cast<std::int8_t>(fetch8());
        ip_ = static_cast<std::uint16_t>(ip_ + displacement);
        return true;
    }
    case 0xEA:
    {
        const std::uint32_t target = operand32 ? read32(segs_[1], ip_) : fetch16();
        if (operand32) ip_ += 4;
        const auto segment = fetch16(); ip_ = static_cast<std::uint16_t>(target);
        segs_[1] = segment; return true;
    }
    case 0x9A:
    {
        const std::uint32_t target = operand32 ? read32(segs_[1], ip_) : fetch16();
        if (operand32) ip_ += 4;
        const auto segment = fetch16();
        push(segs_[1]);
        if (operand32) push32(ip_); else push(ip_);
        ip_ = static_cast<std::uint16_t>(target); segs_[1] = segment; return true;
    }
    case 0xCD: interrupt(fetch8()); return true;
    case 0xCC: interrupt(3); return true;
    case 0xCF: ip_ = pop(); segs_[1] = pop(); flags_ = static_cast<std::uint16_t>(pop() | 2); return true;
    case 0xE4: case 0xE5: case 0xEC: case 0xED:
    {
        const std::uint16_t port = opcode < 0xEC ? fetch8() : regs_[2];
        const std::uint8_t value = port == 0x92 && a20Enabled_ ? 2 : 0;
        if (opcode & 1) regs_[0] = static_cast<std::uint16_t>((regs_[0] & 0xFF00) | value);
        else setReg(0, false, value);
        return true;
    }
    case 0xE6: case 0xE7: case 0xEE: case 0xEF:
        {
            const std::uint16_t port = opcode < 0xEE ? fetch8() : regs_[2];
            const std::uint8_t value = static_cast<std::uint8_t>(regs_[0]);
            if (port == 0x92)
                a20Enabled_ = (value & 2) != 0;
        }
        return true;
    case 0xA0: case 0xA1: case 0xA2: case 0xA3:
    {
        const std::uint16_t offset = fetch16();
        const bool word = opcode & 1;
        const bool storeAccumulator = opcode & 2;
        const std::uint16_t segment = segmentOverride >= 0 ? segs_[segmentOverride] : segs_[3];
        Operand operand{}; operand.segment = segment; operand.offset = offset;
        if (storeAccumulator) setOperand(operand, word, getReg(0, word));
        else setReg(0, word, getOperand(operand, word));
        return true;
    }
    case 0x88: case 0x89: case 0x8A: case 0x8B:
    {
        const bool word = opcode & 1;
        const auto modrm = fetch8(); const auto reg = (modrm >> 3) & 7;
        const auto rm = decodeOperand(modrm, word, segmentOverride, address32);
        const bool dword = operand32 && word;
        if (dword)
        {
            if (opcode & 2) setReg32(reg, getOperand32(rm));
            else setOperand32(rm, getReg32(reg));
        }
        else if (opcode & 2) setReg(reg, word, getOperand(rm, word));
        else setOperand(rm, word, getReg(reg, word));
        return true;
    }
    case 0x8C: case 0x8E:
    {
        const auto modrm = fetch8(); const auto reg = (modrm >> 3) & 3;
        const auto rm = decodeOperand(modrm, true, segmentOverride, address32);
        if (opcode == 0x8C) setOperand(rm, true, segs_[reg]);
        else { if (reg == 1) return fail("MOV into CS is invalid"); segs_[reg] = static_cast<std::uint16_t>(getOperand(rm, true)); }
        return true;
    }
    case 0x8D:
    {
        const auto modrm = fetch8(); const auto reg = (modrm >> 3) & 7;
        const auto rm = decodeOperand(modrm, true, segmentOverride, address32);
        if (rm.isRegister) return fail("LEA requires memory operand");
        if (operand32) setReg32(reg, rm.offset); else setReg16(reg, static_cast<std::uint16_t>(rm.offset));
        return true;
    }
    case 0xC4: case 0xC5:
    {
        const auto modrm = fetch8();
        const auto reg = (modrm >> 3) & 7;
        const auto rm = decodeOperand(modrm, true, segmentOverride, address32);
        if (rm.isRegister) return fail("LES/LDS requires memory operand");
        setReg16(reg, read16(rm.segment, rm.offset));
        segs_[opcode == 0xC4 ? 0 : 3] =
            read16(rm.segment, static_cast<std::uint16_t>(rm.offset + 2));
        return true;
    }
    case 0x8F:
    {
        const auto modrm = fetch8();
        if (((modrm >> 3) & 7) != 0)
            return fail("invalid POP r/m group operation");
        const auto destination = decodeOperand(modrm, true, segmentOverride, address32);
        setOperand(destination, true, pop());
        return true;
    }
    case 0x86: case 0x87:
    {
        const bool word = opcode & 1; const auto modrm = fetch8();
        const auto reg = (modrm >> 3) & 7; const auto rm = decodeOperand(modrm, word, segmentOverride, address32);
        const auto lhs = getReg(reg, word), rhs = getOperand(rm, word);
        setReg(reg, word, rhs); setOperand(rm, word, lhs); return true;
    }
    case 0xC6: case 0xC7:
    {
        const bool word = opcode & 1; const auto modrm = fetch8();
        if (((modrm >> 3) & 7) != 0) return fail("invalid MOV immediate group");
        const auto rm = decodeOperand(modrm, word, segmentOverride, address32);
        if (operand32 && word)
        {
            const std::uint32_t immediate = read32(segs_[1], ip_); ip_ += 4;
            setOperand32(rm, immediate); return true;
        }
        setOperand(rm, word, word ? fetch16() : fetch8()); return true;
    }
    case 0x80: case 0x81: case 0x82: case 0x83:
    {
        const bool word = opcode == 0x81 || opcode == 0x83; const auto modrm = fetch8();
        const auto operation = (modrm >> 3) & 7; const auto rm = decodeOperand(modrm, word, segmentOverride, address32);
        if (operand32 && word)
        {
            const std::uint32_t immediate = opcode == 0x81
                ? read32(segs_[1], ip_)
                : static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(fetch8())));
            if (opcode == 0x81) ip_ += 4;
            const auto value = alu32(operation, getOperand32(rm), immediate);
            if (operation != 7) setOperand32(rm, value);
            return true;
        }
        const std::uint32_t immediate = opcode == 0x81 ? fetch16() :
            opcode == 0x83 ? static_cast<std::uint16_t>(static_cast<std::int8_t>(fetch8())) : fetch8();
        const auto value = alu(operation, getOperand(rm, word), immediate, word);
        if (operation != 7) setOperand(rm, word, value);
        return true;
    }
    case 0x84: case 0x85:
    {
        const bool word = opcode & 1; const auto modrm = fetch8();
        const auto reg = (modrm >> 3) & 7; const auto rm = decodeOperand(modrm, word, segmentOverride, address32);
        logicFlags(getReg(reg, word) & getOperand(rm, word), word); return true;
    }
    case 0xA8: case 0xA9:
    {
        const bool word = opcode & 1; const auto immediate = word ? fetch16() : fetch8();
        logicFlags(getReg(0, word) & immediate, word); return true;
    }
    case 0xC0: case 0xC1: case 0xD0: case 0xD1: case 0xD2: case 0xD3:
    {
        const bool word = opcode & 1; const auto modrm = fetch8();
        const auto operation = (modrm >> 3) & 7; const auto rm = decodeOperand(modrm, word, segmentOverride, address32);
        const unsigned count = (opcode == 0xC0 || opcode == 0xC1) ? fetch8() :
            (opcode == 0xD0 || opcode == 0xD1) ? 1 : getReg(1, false);
        std::uint32_t value = getOperand(rm, word);
        const unsigned bits = word ? 16 : 8;
        for (unsigned i = 0; i < (count & 0x1F); ++i)
        {
            if (operation == 0)
            {
                const auto top = (value >> (bits - 1)) & 1;
                value = ((value << 1) | top) & (word ? 0xFFFF : 0xFF);
                flags_ = static_cast<std::uint16_t>((flags_ & ~CF) | top);
            }
            else if (operation == 1)
            {
                const auto bottom = value & 1;
                value = (value >> 1) | (bottom << (bits - 1));
                flags_ = static_cast<std::uint16_t>((flags_ & ~CF) | bottom);
            }
            else if (operation == 2 || operation == 3)
            {
                const auto carry = flags_ & CF;
                const auto nextCarry = operation == 2
                    ? (value >> (bits - 1)) & 1 : value & 1;
                value = operation == 2
                    ? ((value << 1) | carry) & (word ? 0xFFFF : 0xFF)
                    : (value >> 1) | (carry << (bits - 1));
                flags_ = static_cast<std::uint16_t>((flags_ & ~CF) | nextCarry);
            }
            else if (operation == 4 || operation == 6) { flags_ = static_cast<std::uint16_t>((flags_ & ~CF) | ((value >> (bits - 1)) & 1)); value = (value << 1) & (word ? 0xFFFF : 0xFF); }
            else if (operation == 5) { flags_ = static_cast<std::uint16_t>((flags_ & ~CF) | (value & 1)); value >>= 1; }
            else if (operation == 7) { flags_ = static_cast<std::uint16_t>((flags_ & ~CF) | (value & 1)); value = static_cast<std::uint32_t>(static_cast<std::int32_t>(value) >> 1); }
            else return fail("unsupported shift/rotate operation");
        }
        setSZP(value, word); setOperand(rm, word, value); return true;
    }
    case 0xA4: case 0xA5: case 0xA6: case 0xA7: case 0xAA: case 0xAB: case 0xAC: case 0xAD: case 0xAE: case 0xAF:
    {
        const bool word = opcode & 1; const unsigned width = word ? 2 : 1;
        unsigned count = repeat ? regs_[1] : 1;
        if (count > 0x10000) count = 0x10000;
        for (unsigned i = 0; i < count; ++i)
        {
            const int delta = (flags_ & DF) ? -static_cast<int>(width) : static_cast<int>(width);
            const std::uint16_t srcSeg = segmentOverride >= 0 ? segs_[segmentOverride] : segs_[3];
            const std::uint16_t src = word ? read16(srcSeg, regs_[6]) : read8(srcSeg, regs_[6]);
            const std::uint16_t dst = word ? read16(segs_[0], regs_[7]) : read8(segs_[0], regs_[7]);
            switch (opcode & 0xFE)
            {
            case 0xA4: if (word) write16(segs_[0], regs_[7], src); else write8(segs_[0], regs_[7], static_cast<std::uint8_t>(src)); regs_[6] += delta; regs_[7] += delta; break;
            case 0xA6: (void)alu(7, src, dst, word); regs_[6] += delta; regs_[7] += delta; break;
            case 0xAA: if (word) write16(segs_[0], regs_[7], regs_[0]); else write8(segs_[0], regs_[7], static_cast<std::uint8_t>(regs_[0])); regs_[7] += delta; break;
            case 0xAC: if (word) regs_[0] = src; else setReg(0, false, src); regs_[6] += delta; break;
            case 0xAE: (void)alu(7, getReg(0, word), dst, word); regs_[7] += delta; break;
            }
            if (repeat) --regs_[1];
            if ((opcode == 0xA6 || opcode == 0xA7 || opcode == 0xAE || opcode == 0xAF) && repeat &&
                ((repeat == 0xF3 && !(flags_ & ZF)) || (repeat == 0xF2 && (flags_ & ZF))))
                break;
        }
        return true;
    }
    case 0xE0: case 0xE1: case 0xE2:
    {
        const auto rel = static_cast<std::int8_t>(fetch8()); --regs_[1];
        if (regs_[1] != 0 && (opcode == 0xE2 || (opcode == 0xE1 && (flags_ & ZF)) || (opcode == 0xE0 && !(flags_ & ZF)))) ip_ += rel;
        return true;
    }
    case 0xE3: { const auto rel = static_cast<std::int8_t>(fetch8()); if (regs_[1] == 0) ip_ += rel; return true; }
    case 0x98: regs_[0] = static_cast<std::uint16_t>(static_cast<std::int16_t>(static_cast<std::int8_t>(regs_[0]))); return true;
    case 0x99: regs_[2] = (regs_[0] & 0x8000) ? 0xFFFF : 0; return true;
    case 0x9E: flags_ = static_cast<std::uint16_t>((flags_ & 0xFF00) | (regs_[0] & 0xD5) | 2); return true;
    case 0x9F: regs_[0] = static_cast<std::uint16_t>((regs_[0] & 0xFF00) | (flags_ & 0xD5)); return true;
    case 0xC9: regs_[4] = regs_[5]; regs_[5] = pop(); return true;
    case 0xC8:
    {
        const std::uint16_t allocation = fetch16();
        const std::uint8_t nesting = fetch8() & 0x1F;
        push(regs_[5]);
        const std::uint16_t frame = regs_[4];
        for (std::uint8_t level = 1; level < nesting; ++level)
        {
            regs_[5] = static_cast<std::uint16_t>(regs_[5] - 2);
            push(read16(segs_[2], regs_[5]));
        }
        if (nesting != 0) push(frame);
        regs_[5] = frame;
        regs_[4] = static_cast<std::uint16_t>(regs_[4] - allocation);
        return true;
    }
    case 0xF1: return fail("ICEBP is unsupported");
    default: break;
    }

    if (opcode == 0xFE || opcode == 0xFF)
    {
        const bool word = opcode == 0xFF; const auto modrm = fetch8();
        const auto operation = (modrm >> 3) & 7; const auto rm = decodeOperand(modrm, word, segmentOverride, address32);
        const auto value = getOperand(rm, word);
        if (operation == 0 || operation == 1)
        {
            const auto carry = flags_ & CF;
            const auto result = alu(operation == 0 ? 0 : 5, value, 1, word);
            flags_ = static_cast<std::uint16_t>((flags_ & ~CF) | carry);
            setOperand(rm, word, result); return true;
        }
        if (operation == 2) { push(ip_); ip_ = static_cast<std::uint16_t>(value); return true; }
        if (operation == 4) { ip_ = static_cast<std::uint16_t>(value); return true; }
        if ((operation == 3 || operation == 5) && !rm.isRegister)
        {
            const std::uint16_t target = read16(rm.segment, rm.offset);
            const std::uint16_t segment = read16(rm.segment,
                static_cast<std::uint16_t>(rm.offset + 2));
            if (operation == 3) { push(segs_[1]); push(ip_); }
            ip_ = target;
            segs_[1] = segment;
            return true;
        }
        if (operation == 6) { push(static_cast<std::uint16_t>(value)); return true; }
        return fail("unsupported FF/FE group operation");
    }

    if (opcode == 0xF6 || opcode == 0xF7)
    {
        const bool word = opcode == 0xF7; const auto modrm = fetch8();
        const auto operation = (modrm >> 3) & 7; const auto rm = decodeOperand(modrm, word, segmentOverride);
        const std::uint32_t value = getOperand(rm, word);
        const std::uint32_t mask = word ? 0xFFFF : 0xFF;
        if (operation <= 1) { const auto imm = word ? fetch16() : fetch8(); logicFlags(value & imm, word); return true; }
        if (operation == 2) { setOperand(rm, word, (~value) & mask); return true; }
        if (operation == 3) { setOperand(rm, word, alu(5, 0, value, word)); return true; }
        if (operation == 4)
        {
            if (word) { const std::uint32_t result = regs_[0] * value; regs_[0] = result; regs_[2] = result >> 16; flags_ = static_cast<std::uint16_t>((flags_ & ~(CF|OF)) | ((regs_[2] != 0) ? (CF|OF) : 0)); }
            else { const std::uint16_t result = static_cast<std::uint8_t>(regs_[0]) * value; regs_[0] = result; flags_ = static_cast<std::uint16_t>((flags_ & ~(CF|OF)) | ((result > 0xFF) ? (CF|OF) : 0)); }
            return true;
        }
        if (operation == 5)
        {
            if (word)
            {
                const std::int32_t product =
                    static_cast<std::int16_t>(regs_[0]) *
                    static_cast<std::int16_t>(value);
                regs_[0] = static_cast<std::uint16_t>(product);
                regs_[2] = static_cast<std::uint16_t>(product >> 16);
                const bool overflow = product < -32768 || product > 32767;
                flags_ = static_cast<std::uint16_t>((flags_ & ~(CF | OF)) |
                    (overflow ? CF | OF : 0));
            }
            else
            {
                const std::int16_t product = static_cast<std::int8_t>(regs_[0]) *
                    static_cast<std::int8_t>(value);
                regs_[0] = static_cast<std::uint16_t>(product);
                const bool overflow = product < -128 || product > 127;
                flags_ = static_cast<std::uint16_t>((flags_ & ~(CF | OF)) |
                    (overflow ? CF | OF : 0));
            }
            return true;
        }
        if (operation == 6 && value != 0)
        {
            if (word) { const std::uint32_t dividend = (static_cast<std::uint32_t>(regs_[2]) << 16) | regs_[0]; const auto quotient = dividend / value; if (quotient > 0xFFFF) return fail("DIV quotient overflow"); regs_[0] = quotient; regs_[2] = dividend % value; }
            else { const std::uint16_t dividend = regs_[0]; const auto quotient = dividend / static_cast<std::uint8_t>(value); if (quotient > 0xFF) return fail("DIV quotient overflow"); setReg(0, false, quotient); setReg(4, false, dividend % static_cast<std::uint8_t>(value)); }
            return true;
        }
        if (operation == 7 && value != 0)
        {
            if (word)
            {
                const std::int32_t dividend =
                    static_cast<std::int32_t>((static_cast<std::uint32_t>(regs_[2]) << 16) | regs_[0]);
                const std::int32_t divisor = static_cast<std::int16_t>(value);
                if (divisor == 0 || (dividend == INT32_MIN && divisor == -1))
                    return fail("IDIV overflow");
                const std::int32_t quotient = dividend / divisor;
                if (quotient < -32768 || quotient > 32767)
                    return fail("IDIV quotient overflow");
                regs_[0] = static_cast<std::uint16_t>(quotient);
                regs_[2] = static_cast<std::uint16_t>(dividend % divisor);
            }
            else
            {
                const std::int16_t dividend = static_cast<std::int16_t>(regs_[0]);
                const std::int16_t divisor = static_cast<std::int8_t>(value);
                if (divisor == 0 || (dividend == INT16_MIN && divisor == -1))
                    return fail("IDIV overflow");
                const std::int16_t quotient = dividend / divisor;
                if (quotient < -128 || quotient > 127)
                    return fail("IDIV quotient overflow");
                setReg(0, false, static_cast<std::uint8_t>(quotient));
                setReg(4, false, static_cast<std::uint8_t>(dividend % divisor));
            }
            return true;
        }
        return fail("unsupported F6/F7 group operation");
    }

    // The real-mode FreeDOS kernel may contain x87 escape opcodes. The
    // virtual machine has no floating-point unit, so consume their operand
    // encoding while leaving the integer machine state untouched.
    if (opcode >= 0xD8 && opcode <= 0xDF)
    {
        const auto modrm = fetch8();
        (void)decodeOperand(modrm, true, segmentOverride, address32);
        return true;
    }

    char message[96];
    std::snprintf(message, sizeof(message),
        "unsupported 8086 opcode %02X at %04X:%04X",
        opcode, segs_[1], static_cast<std::uint16_t>(ip_ - 1));
    setError(message);
    return false;
}

