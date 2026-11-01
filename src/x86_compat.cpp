#include "x86_compat.hpp"

#include "gui.h"
extern "C"
{
#include "igpu.h"
}

#include <cstdio>
#include <fstream>
#include <string>
#include <algorithm>
#include <cstring>
#include <utility>
#include <iostream>

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

    std::uint32_t ataCylinders(std::uint64_t totalSectors)
    {
        return static_cast<std::uint32_t>(
            (std::min<std::uint64_t>)(totalSectors / (16 * 63), 16383));
    }

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
        case AMD3DC_KEY_F5: return 0x3F00;
        case AMD3DC_KEY_F8: return 0x4200;
        case AMD3DC_KEY_F10: return 0x4400;
        default: break;
        }

        const unsigned value = static_cast<unsigned>(key);
        if (value < AMD3DC_KEY_CHAR_BASE || value > AMD3DC_KEY_CHAR_BASE + 126)
            return 0;
        const std::uint8_t ch = static_cast<std::uint8_t>(value - AMD3DC_KEY_CHAR_BASE);
        if (ch == '\r' || ch == '\n') return 0x1C0D;
        if (ch == 8)    return 0x0E08;   // Backspace
        if (ch == '\t') return 0x0F09;   // Tab
        if (ch == 27)   return 0x011B;   // Esc
        if (ch >= 1 && ch <= 26)
        {
            constexpr std::uint8_t scans[26] = {
                0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24,
                0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14,
                0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C
            };
            return static_cast<std::uint16_t>((scans[ch - 1] << 8) | ch);
        }
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
    text_rendered_ = false;
    bios_clock_epoch_ = std::chrono::steady_clock::now();
    bios_clock_epoch_ticks_ = 0;
    bios_clock_days_ = 0;
    std::memset(regs_, 0, sizeof(regs_));
    std::memset(regs_high_, 0, sizeof(regs_high_));
    std::memset(segs_, 0, sizeof(segs_));
    std::memset(state.memory.data, 0, state.memory.size);
    std::memcpy(state.memory.data + 0x7C00, disk_.data(), 512);
    if (state.memory.size >= 0xB8000 + 0x4000)
        for (std::size_t i = 0; i < 0x2000; ++i)
        {
            state.memory.data[0xB8000 + i * 2] = 0x20;
            state.memory.data[0xB8001 + i * 2] = 0x07;
        }

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
    state.memory.data[0x475] = hard_disk_.empty() ? 0 : 1; // fixed disk count
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
    for (auto& entry : instruction_trace_)
        entry = {};
    instruction_trace_position_ = 0;
    instruction_trace_count_ = 0;
    for (auto& entry : control_trace_)
        entry = {};
    control_trace_position_ = 0;
    control_trace_count_ = 0;
    disk_read_history_.clear();
    disk_read_calls_ = 0;
    disk_sectors_read_ = 0;
    disk_read_failures_ = 0;
    last_disk_lba_ = 0;
    std::memset(ata_regs_, 0, sizeof(ata_regs_));
    ataSignature();
    ata_status_ = 0x50;              // DRDY | DSC
    ata_identify_ = false;
    ata_write_command_ = false;
    ata_data_pos_ = 512;
    ata_sectors_remaining_ = 0;
    ata_lba_ = 0;
    ata_nien_ = false;
    ata_heads_ = 16;
    ata_spt_ = 63;
    return true;
}

void X86Compat::attachHardDisk(std::vector<std::uint8_t> image, const std::string& path)
{
    image.resize(image.size() & ~static_cast<std::size_t>(511)); // лише цілі сектори
    hard_disk_ = std::move(image);
    hard_disk_path_ = path;
    hard_disk_file_.close();
    if (!path.empty() && !hard_disk_.empty())
        hard_disk_file_.open(path, std::ios::binary | std::ios::in | std::ios::out);
    ata_heads_ = 16; ata_spt_ = 63;
    if (state_ != nullptr && state_->memory.data != nullptr && state_->memory.size > 0x475)
        state_->memory.data[0x475] = hard_disk_.empty() ? 0 : 1;
    if (!path.empty() && !hard_disk_.empty() && !hard_disk_file_.is_open())
        std::fprintf(stderr,
            "warning: cannot open %s for writing, HDD changes will not persist\n",
            path.c_str());
}

void X86Compat::persistHardDisk(std::uint64_t lba, std::size_t sectors)
{
    if (!hard_disk_file_.is_open() || sectors == 0) return;
    hard_disk_file_.clear();
    hard_disk_file_.seekp(static_cast<std::streamoff>(lba * 512));
    hard_disk_file_.write(
        reinterpret_cast<const char*>(hard_disk_.data() + static_cast<std::size_t>(lba) * 512),
        static_cast<std::streamsize>(sectors * 512));
    hard_disk_file_.flush();
}

void X86Compat::ataSignature()
{
    ata_regs_[1] = 0x01; ata_regs_[2] = 0x01; ata_regs_[3] = 0x01;
    ata_regs_[4] = 0x00; ata_regs_[5] = 0x00;
    ata_regs_[6] &= 0xF0;
}

void X86Compat::attachSecondFloppy(std::vector<std::uint8_t> image)
{
    if (image.size() == 1474560) second_floppy_ = std::move(image);
    if (state_ && state_->memory.data && state_->memory.size > 0x410)
        state_->memory.data[0x410] = second_floppy_.empty() ? 0x21 : 0x41;
}

void X86Compat::ataCommand(std::uint8_t command)
{
    ata_status_ = 0x80;
    ata_regs_[1] = 0;
    ata_identify_ = false;
    ata_write_command_ = false;
    ata_data_pos_ = 512;
    ata_sectors_remaining_ = 0;

    if ((ata_regs_[6] & 0x10) || hard_disk_.empty())
    {
        ata_regs_[1] = 0x04; ata_status_ = 0x51; return;
    }   // slave/відсутній: статус читається як 0

    const std::uint64_t total = hard_disk_.size() / 512;

    if (command == 0xEC) // IDENTIFY DEVICE
    {
        std::memset(ata_sector_, 0, sizeof(ata_sector_));
        auto word = [this](unsigned index, std::uint16_t value)
            {
                ata_sector_[index * 2] = static_cast<std::uint8_t>(value);
                ata_sector_[index * 2 + 1] = static_cast<std::uint8_t>(value >> 8);
            };
        auto text = [this](unsigned firstWord, unsigned chars, const char* s)
            {
                const std::size_t len = std::strlen(s);
                for (unsigned i = 0; i < chars; i += 2)
                {
                    ata_sector_[firstWord * 2 + i] = i + 1 < len ? s[i + 1] : ' ';
                    ata_sector_[firstWord * 2 + i + 1] = i < len ? s[i] : ' ';
                }
            };
        word(0, 0x0040);                       // фіксований диск
        word(1, static_cast<std::uint16_t>(ataCylinders(total)));
        word(3, 16); word(6, 63);
        text(10, 20, "AMD3DC0001");
        text(23, 8, "1.0");
        text(27, 40, "AMD3DC VIRTUAL PATA DISK");
        word(49, 0x0200);                      // LBA, без DMA
        word(53, 0x0001);                      // слова 54-58 валідні
        const std::uint32_t curCyls = static_cast<std::uint32_t>(
            (std::min<std::uint64_t>)(total / (ata_heads_ * ata_spt_), 65535));
        const std::uint32_t curCap = curCyls * ata_heads_ * ata_spt_;
        word(54, static_cast<std::uint16_t>(curCyls));
        word(55, ata_heads_); word(56, ata_spt_);
        word(57, static_cast<std::uint16_t>(curCap));
        word(58, static_cast<std::uint16_t>(curCap >> 16));
        const std::uint32_t lba28 = static_cast<std::uint32_t>(
            (std::min<std::uint64_t>)(total, 0x0FFFFFFF));
        word(60, static_cast<std::uint16_t>(lba28));
        word(61, static_cast<std::uint16_t>(lba28 >> 16));
        ata_identify_ = true; ata_sectors_remaining_ = 1;
        ata_data_pos_ = 0; ata_status_ = 0x58; return;
    }
    if (command == 0x90) { ataSignature(); ata_status_ = 0x50; return; }   // діагностика
    if (command == 0x91) // INITIALIZE DEVICE PARAMETERS
    {
        if (ata_regs_[2] == 0) { ata_regs_[1] = 0x04; ata_status_ = 0x51; return; }
        ata_spt_ = ata_regs_[2];
        ata_heads_ = static_cast<std::uint16_t>((ata_regs_[6] & 0x0F) + 1);
        ata_status_ = 0x50; return;
    }
    if (command == 0xE5) { ata_regs_[2] = 0xFF; ata_status_ = 0x50; return; } // CHECK POWER MODE
    if (command == 0xE0 || command == 0xE1 || command == 0xE2 || command == 0xE3 ||
        command == 0xE6 || command == 0xE7 || command == 0xEF || command == 0xC6 ||
        (command >= 0x10 && command <= 0x1F))
    {
        ata_status_ = 0x50; return;
    }   // standby/idle/sleep/flush/set features/set multiple/recalibrate

    const bool isRead = command == 0x20 || command == 0x21 || command == 0xC4;
    const bool isWrite = command == 0x30 || command == 0x31 || command == 0xC5;
    const bool isVerify = command == 0x40 || command == 0x41;
    const bool isSeek = command >= 0x70 && command <= 0x7F;
    if (!(isRead || isWrite || isVerify || isSeek))
    {
        ata_regs_[1] = 0x04; ata_status_ = 0x51; return;
    }

    std::uint64_t lba;
    if (ata_regs_[6] & 0x40)
        lba = ata_regs_[3] | (static_cast<std::uint64_t>(ata_regs_[4]) << 8) |
        (static_cast<std::uint64_t>(ata_regs_[5]) << 16) |
        (static_cast<std::uint64_t>(ata_regs_[6] & 0x0F) << 24);
    else
    {
        const std::uint32_t sector = ata_regs_[3];
        const std::uint32_t cylinder = ata_regs_[4] | (static_cast<std::uint32_t>(ata_regs_[5]) << 8);
        const std::uint32_t head = ata_regs_[6] & 0x0F;
        if (sector == 0 || sector > ata_spt_ || head >= ata_heads_)
        {
            ata_regs_[1] = 0x10; ata_status_ = 0x51; return;
        }
        lba = (static_cast<std::uint64_t>(cylinder) * ata_heads_ + head) * ata_spt_ + sector - 1;
    }
    const unsigned count = ata_regs_[2] ? ata_regs_[2] : 256;
    if (lba >= total || (!isSeek && count > total - lba))
    {
        ata_regs_[1] = 0x10; ata_status_ = 0x51; return;
    }   // IDNF

    ata_lba_ = static_cast<std::uint32_t>(lba);
    if (isSeek) { ata_status_ = 0x50; return; }
    if (isVerify) { ata_regs_[2] = 0; ata_status_ = 0x50; return; }

    ata_sectors_remaining_ = count;
    ata_write_command_ = isWrite;
    if (isRead)
        std::memcpy(ata_sector_, hard_disk_.data() + static_cast<std::size_t>(lba) * 512, 512);
    ata_data_pos_ = 0; ata_status_ = 0x58;
}
// у private-секції, біля ataCommand / ataFinishWriteSector:
void ataSignature();
void persistHardDisk(std::uint64_t lba, std::size_t sectors);

std::uint16_t X86Compat::ioRead(std::uint16_t port, bool word)
{
    const bool present = !(ata_regs_[6] & 0x10) && !hard_disk_.empty();
    if (port == 0x1F7 || port == 0x3F6) return present ? ata_status_ : 0x00;
    if (port >= 0x1F0 && port <= 0x1F7)
    {
        const unsigned reg = port - 0x1F0;
        if (reg == 0)
        {
            std::uint16_t value = 0x00FF;
            if (present && !ata_write_command_ && ata_data_pos_ < 512)
            {
                value = ata_sector_[ata_data_pos_++];
                if (word && ata_data_pos_ < 512)
                    value |= static_cast<std::uint16_t>(ata_sector_[ata_data_pos_++]) << 8;
                if (ata_data_pos_ >= 512)
                {
                    if (ata_identify_) ata_status_ = 0x50;
                    else
                    {
                        --ata_sectors_remaining_;
                        ata_regs_[2] = static_cast<std::uint8_t>(ata_sectors_remaining_);
                        if (ata_sectors_remaining_ == 0) ata_status_ = 0x50;
                        else
                        {
                            ++ata_lba_;
                            std::memcpy(ata_sector_,
                                hard_disk_.data() + static_cast<std::size_t>(ata_lba_) * 512, 512);
                            ata_data_pos_ = 0;
                        }
                    }
                }
            }
            return value;
        }
        return present ? ata_regs_[reg] : 0;
    }
    return port == 0x92 && a20Enabled_ ? 2 : 0;
}

void X86Compat::ioWrite(std::uint16_t port, std::uint8_t value)
{
    if (port >= 0x1F1 && port <= 0x1F6) ata_regs_[port - 0x1F0] = value;
    else if (port == 0x1F7) ataCommand(value);
    else if (port == 0x3F6)
    {
        ata_nien_ = (value & 0x02) != 0;
        if (value & 0x04)   // SRST: скасувати все, що в польоті
        {
            ata_status_ = 0x80; ata_identify_ = false; ata_write_command_ = false;
            ata_data_pos_ = 512; ata_sectors_remaining_ = 0;
        }
        else if (ata_status_ == 0x80) { ataSignature(); ata_status_ = 0x50; }
    }
    else if (port == 0x1F0 && ata_write_command_ && ata_data_pos_ < 512)
    {
        ata_sector_[ata_data_pos_++] = value;
        if (ata_data_pos_ == 512) ataFinishWriteSector();
    }
    else if (port == 0x92) a20Enabled_ = (value & 2) != 0;
}

void X86Compat::ataFinishWriteSector()
{
    const std::size_t offset = static_cast<std::size_t>(ata_lba_) * 512;
    if (ata_write_command_ && offset + 512 <= hard_disk_.size())
    {
        std::memcpy(hard_disk_.data() + offset, ata_sector_, 512);
        persistHardDisk(ata_lba_, 1);
    }
    if (ata_sectors_remaining_ > 0) --ata_sectors_remaining_;
    ata_regs_[2] = static_cast<std::uint8_t>(ata_sectors_remaining_);
    if (ata_sectors_remaining_ == 0) { ata_write_command_ = false; ata_status_ = 0x50; }
    else { ++ata_lba_; ata_data_pos_ = 0; ata_status_ = 0x58; }
}

void X86Compat::ioWrite16(std::uint16_t port, std::uint16_t value)
{
    if (port != 0x1F0) { ioWrite(port, static_cast<std::uint8_t>(value)); return; }
    if (ata_write_command_ && ata_data_pos_ + 1 < 512)
    {
        ata_sector_[ata_data_pos_++] = static_cast<std::uint8_t>(value);
        ata_sector_[ata_data_pos_++] = static_cast<std::uint8_t>(value >> 8);
        if (ata_data_pos_ >= 512)
        {
            ataFinishWriteSector();
        }
    }
}

std::uint8_t X86Compat::fetch8()
{
    // Instruction fetch is the hottest memory path in the interpreter.
    // Keep it inline here instead of going through read8() for every opcode
    // and prefix byte (which also repeats the generic null-state check).
    if (state_ == nullptr || state_->memory.data == nullptr)
    {
        ++ip_;
        return 0xFF;
    }
    std::uint32_t address = (static_cast<std::uint32_t>(segs_[1]) << 4) + ip_;
    if (!a20Enabled_)
        address &= 0xFFFFF;
    const std::uint8_t value = address < state_->memory.size
        ? state_->memory.data[address] : 0xFF;
    ++ip_;
    return value;
}

std::uint16_t X86Compat::fetch16()
{
    const std::uint16_t lo = fetch8();
    return static_cast<std::uint16_t>(lo | (fetch8() << 8));
}

std::uint32_t X86Compat::fetch32()
{
    const std::uint32_t lo = fetch16();
    return lo | (static_cast<std::uint32_t>(fetch16()) << 16);
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
            if (index != 4) base += getReg32(index) << scale;
            if (mod == 0 && baseReg == 5) base += fetch32();
            else { base += getReg32(baseReg); usesBp = (baseReg == 5 || baseReg == 4); }
        }
        else if (mod == 0 && rm == 5)
            base = fetch32();
        else
        {
            base = getReg32(rm);
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
        base += address32 ? static_cast<std::int32_t>(fetch32())
                          : static_cast<std::int16_t>(fetch16());

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

void X86Compat::pollHostInput()
{
    pollKeyboardInput();
    updateBiosClock();
}

void X86Compat::updateBiosClock()
{
    if (state_ == nullptr || state_->memory.data == nullptr || state_->memory.size <= 0x470)
        return;

    constexpr std::uint64_t ticksPerDay = 0x1800B0;
    constexpr double ticksPerSecond = 1193182.0 / 65536.0;
    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - bios_clock_epoch_).count();
    const std::uint64_t totalTicks = bios_clock_epoch_ticks_ +
        static_cast<std::uint64_t>(elapsed * ticksPerSecond);
    const std::uint64_t days = totalTicks / ticksPerDay;
    const std::uint32_t ticks = static_cast<std::uint32_t>(totalTicks % ticksPerDay);

    state_->memory.data[0x46C] = static_cast<std::uint8_t>(ticks);
    state_->memory.data[0x46D] = static_cast<std::uint8_t>(ticks >> 8);
    state_->memory.data[0x46E] = static_cast<std::uint8_t>(ticks >> 16);
    state_->memory.data[0x46F] = static_cast<std::uint8_t>(ticks >> 24);
    if (days > bios_clock_days_)
        state_->memory.data[0x470] = 1;
    bios_clock_days_ = days;
}

std::string X86Compat::debugState() const
{
    char text[1024];
    const std::uint32_t biosTicks = state_ != nullptr && state_->memory.data != nullptr &&
        state_->memory.size > 0x46F
        ? (static_cast<std::uint32_t>(state_->memory.data[0x46F]) << 24) |
          (static_cast<std::uint32_t>(state_->memory.data[0x46E]) << 16) |
          (static_cast<std::uint32_t>(state_->memory.data[0x46D]) << 8) |
          state_->memory.data[0x46C]
        : 0;
    std::snprintf(text, sizeof(text),
        "ES:CS:SS:DS=%04X:%04X:%04X:%04X IP=%04X AX=%04X BX=%04X CX=%04X DX=%04X SP=%04X BP=%04X SI=%04X DI=%04X FLAGS=%04X INS=%llu TICKS=%u DISK=%llu/%llu FAIL=%llu LBA=%u READS=",
        segs_[0], segs_[1], segs_[2], segs_[3], ip_, regs_[0], regs_[3], regs_[1], regs_[2],
        regs_[4], regs_[5], regs_[6], regs_[7], flags_,
        static_cast<unsigned long long>(instruction_count_),
        biosTicks,
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
        const std::uint8_t* text = state_->memory.data + 0xB8000;
        if (text_rendered_ && std::memcmp(rendered_text_, text, sizeof(rendered_text_)) == 0)
            return;
        std::memcpy(rendered_text_, text, sizeof(rendered_text_));
        text_rendered_ = true;
        static int dumps = 0;
        if (dumps < 5)
        {
            ++dumps;
            std::fprintf(stderr, "---- VRAM dump %d ----\n", dumps);
            for (int row = 0; row < 6; ++row)
            {
                for (int col = 0; col < 40; ++col)
                {
                    const std::uint8_t ch = text[(row * 80 + col) * 2];
                    std::fputc(ch >= 32 && ch < 127 ? ch : '.', stderr);
                }
                std::fputs("  attr:", stderr);
                for (int col = 0; col < 12; ++col)
                    std::fprintf(stderr, " %02X", text[(row * 80 + col) * 2 + 1]);
                std::fputc('\n', stderr);
            }
        }
        amd3dc_virtio_render_text_mode(
            &state_->virtio,
            text);
    }
}

void X86Compat::interrupt(std::uint8_t number)
{
    const std::uint16_t vectorOffset = read16(0, number * 4);
    const std::uint16_t vectorSegment = read16(0, number * 4 + 2);
    const std::uint16_t biosOffset = static_cast<std::uint16_t>(0x0100 + number * 4);

    if (number == 0x13 && (regs_[0] >> 8) == 0x41)
    {
        std::fprintf(stderr,
            "INT 13h/AH=41 route: DL=%02X BX=%04X vector=%04X:%04X\n",
            static_cast<unsigned>(regs_[2] & 0xFF),
            static_cast<unsigned>(regs_[3]),
            static_cast<unsigned>(vectorSegment),
            static_cast<unsigned>(vectorOffset));
    }

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
        auto scroll = [&](bool down, std::uint8_t page, std::uint8_t top, std::uint8_t left,
            std::uint8_t bottom, std::uint8_t right, std::size_t lines, std::uint8_t fill)
            {
                if (top > bottom || left > right || bottom >= 25 || right >= 80)
                    return;
                const std::size_t height = bottom - top + 1;
                lines = lines == 0 ? height : (std::min<std::size_t>)(lines, height);
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
                            state_->memory.data[dst + 1] = fill;
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
                            state_->memory.data[dst + 1] = fill;
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
                    scroll(false, page, 0, 0, 24, 79, 1, 0x07);
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
        else if (ah == 0x06 || ah == 0x07)
        {
            if (state_->memory.size >= 0xB8000 + 0x8000)
                scroll(ah == 0x07, state_->memory.data[0x462] & 7,
                    static_cast<std::uint8_t>(regs_[1] >> 8), static_cast<std::uint8_t>(regs_[1]),
                    static_cast<std::uint8_t>(regs_[2] >> 8), static_cast<std::uint8_t>(regs_[2]),
                    al, static_cast<std::uint8_t>(regs_[3] >> 8));
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
            const std::size_t cursor = 0x450 + page * 2;
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
        const bool hardDrive = drive == 0x80 && !hard_disk_.empty();
        const bool floppy1 = drive == 1 && !second_floppy_.empty();
        std::vector<std::uint8_t>& media = hardDrive ? hard_disk_ : (floppy1 ? second_floppy_ : disk_);
        bool ok = drive == 0 || floppy1 || hardDrive;
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
            // Some boot sectors keep DL=00 after loading from floppy, then
            // issue hard-disk CHS requests. Only redirect CHS values that
            // cannot exist on a 1.44 MiB floppy.
            const bool floppyChs = sector != 0 && sector <= 18 &&
                head < 2 && cylinder < 80;
            const bool redirectedToHardDisk = drive == 0 && !floppyChs &&
                !hard_disk_.empty();
            const bool useHardDisk = hardDrive || redirectedToHardDisk;
            const bool useSecondFloppy = floppy1 && !useHardDisk;
            const std::vector<std::uint8_t>& media =
                useHardDisk ? hard_disk_ : (useSecondFloppy ? second_floppy_ : disk_);
            const unsigned sectorsPerTrack = useHardDisk ? 63 : 18;
            const unsigned heads = useHardDisk ? 16 : 2;
            const std::size_t cylinders = media.size() / 512 /
                (sectorsPerTrack * heads);
            if (sector == 0 || sector > sectorsPerTrack || head >= heads ||
                cylinder >= cylinders)
                ok = false;
            const std::size_t lba =
                (static_cast<std::size_t>(cylinder) * heads + head) * sectorsPerTrack + sector - 1;
            last_disk_lba_ = static_cast<std::uint32_t>(lba);
            const std::size_t bytes = static_cast<std::size_t>(count) * 512;
            const std::size_t dest =
                ((static_cast<std::size_t>(segs_[0]) << 4) + regs_[3]) & 0xFFFFF;
            if (disk_read_history_.size() == 24)
                disk_read_history_.erase(disk_read_history_.begin());
            char readText[64];
            std::snprintf(readText, sizeof(readText), "%02X>%02X/%02X:%u:%u@%05X",
                static_cast<unsigned>(drive),
                static_cast<unsigned>(useHardDisk ? 0x80 : drive),
                static_cast<unsigned>(ah),
                static_cast<unsigned>(lba), count,
                static_cast<unsigned>(dest));
            disk_read_history_.emplace_back(readText);
            if (lba * 512 > media.size() || bytes > media.size() - lba * 512 ||
                dest > state_->memory.size || bytes > state_->memory.size - dest)
                ok = false;
            if (ok)
            {
                std::memcpy(state_->memory.data + dest, media.data() + lba * 512, bytes);
                disk_sectors_read_ += count;
            }
            else
                ++disk_read_failures_;
            regs_[0] = static_cast<std::uint16_t>((regs_[0] & 0x00FF) | (ok ? 0 : 0x0100));
        }
        else if (ah == 0x03) // CHS write
        {
            const std::uint8_t count = regs_[0] & 0xFF;
            const std::uint8_t sector = regs_[1] & 0x3F;
            const std::uint16_t cylinder = static_cast<std::uint16_t>(
                ((regs_[1] >> 8) & 0xFF) | ((regs_[1] & 0xC0) << 2));
            const std::uint8_t head = regs_[2] >> 8;
            const bool floppyChs = sector != 0 && sector <= 18 &&
                head < 2 && cylinder < 80;
            const bool redirectedToHardDisk = drive == 0 && !floppyChs &&
                !hard_disk_.empty();
            const bool useHardDisk = hardDrive || redirectedToHardDisk;
            std::vector<std::uint8_t>& target = useHardDisk ? hard_disk_ : media;
            const unsigned sectorsPerTrack = useHardDisk ? 63 : 18;
            const unsigned heads = useHardDisk ? 16 : 2;
            const std::size_t lba =
                (static_cast<std::size_t>(cylinder) * heads + head) * sectorsPerTrack +
                (sector ? sector - 1 : 0);
            const std::size_t bytes = static_cast<std::size_t>(count) * 512;
            const std::size_t source =
                ((static_cast<std::size_t>(segs_[0]) << 4) + regs_[3]) & 0xFFFFF;
            if (count == 0 || sector == 0 || sector > sectorsPerTrack ||
                head >= heads || lba > target.size() / 512 ||
                count > target.size() / 512 - lba ||
                source > state_->memory.size || bytes > state_->memory.size - source)
                ok = false;
            if (ok)
            {
                std::memcpy(target.data() + lba * 512,
                    state_->memory.data + source, bytes);
                if (useHardDisk) persistHardDisk(lba, count);
            }
            regs_[0] = static_cast<std::uint16_t>((regs_[0] & 0x00FF) | (ok ? 0 : 0x0100));
        }
        else if (ah == 0x08) // drive parameters
        {
            if (hardDrive)
            {
                constexpr std::size_t sectorsPerCylinder = 16 * 63;
                const std::size_t totalSectors = media.size() / 512;
                const std::uint32_t cylinders = static_cast<std::uint32_t>(
                    (std::min<std::size_t>)(
                        1024,
                        (totalSectors + sectorsPerCylinder - 1) /
                            sectorsPerCylinder));
                const std::uint32_t maxCylinder = cylinders - 1;
                regs_[1] = static_cast<std::uint16_t>(
                    ((maxCylinder & 0xFF) << 8) |
                    ((maxCylinder >> 2) & 0xC0) |
                    63);
                regs_[2] = 0x0F01;
            }
            else
            {
                regs_[1] = static_cast<std::uint16_t>(0x4F00 | 18);
                regs_[2] = 0x0101;
            }
            regs_[0] &= 0x00FF;
        }
        else if (ah == 0x15) // get disk type
        {
            if (hardDrive)
            {
                const auto sectors = static_cast<std::uint32_t>(
                    hard_disk_.size() / 512);
                regs_[0] = 0x0300; // fixed disk
                regs_[1] = static_cast<std::uint16_t>(sectors >> 16);
                regs_[2] = static_cast<std::uint16_t>(sectors);
            }
            else if (drive == 0 || floppy1)
            {
                regs_[0] = 0x0100; // floppy
            }
            else
            {
                ok = false;
            }
        }
        else if (ah == 0x41) // EDD installation check
        {
            const bool accepted = (drive == 0 || hardDrive) && regs_[3] == 0x55AA;
            if (accepted)
            {
                regs_[3] = 0xAA55;
                regs_[1] = 0x0005;
                regs_[0] = 0x3000;
            }
            else
            {
                ok = false;
            }

            std::fprintf(stderr,
                "INT 13h/AH=41 BIOS: accepted=%u DL=%02X AX=%04X BX=%04X CX=%04X\n",
                accepted ? 1u : 0u,
                static_cast<unsigned>(drive),
                static_cast<unsigned>(regs_[0]),
                static_cast<unsigned>(regs_[3]),
                static_cast<unsigned>(regs_[1]));
        }
        else if (ah == 0x48) // EDD get drive parameters
        {
            const std::uint16_t bufferSegment = segs_[3];
            const std::uint16_t bufferOffset = regs_[6];
            const std::size_t bufferAddress =
                ((static_cast<std::size_t>(bufferSegment) << 4) + bufferOffset) & 0xFFFFF;
            if (!hardDrive || bufferAddress > state_->memory.size ||
                state_->memory.size - bufferAddress < 0x1E)
            {
                ok = false;
            }
            else
            {
                const std::uint64_t totalSectors = hard_disk_.size() / 512;
                const std::uint64_t sectorsPerCylinder = 16 * 63;
                write16(bufferSegment, bufferOffset, 0x001E);
                write16(bufferSegment, bufferOffset + 2, 0);
                write32(bufferSegment, bufferOffset + 4,
                    static_cast<std::uint32_t>((totalSectors + sectorsPerCylinder - 1) / sectorsPerCylinder));
                write32(bufferSegment, bufferOffset + 8, 16);
                write32(bufferSegment, bufferOffset + 12, 63);
                write32(bufferSegment, bufferOffset + 16, static_cast<std::uint32_t>(totalSectors));
                write32(bufferSegment, bufferOffset + 20, static_cast<std::uint32_t>(totalSectors >> 32));
                write16(bufferSegment, bufferOffset + 24, 512);
            }
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
            if (disk_read_history_.size() == 24)
                disk_read_history_.erase(disk_read_history_.begin());
            char readText[64];
            std::snprintf(readText, sizeof(readText), "%02X/%02X:%u:%u@%05X",
                static_cast<unsigned>(drive),
                static_cast<unsigned>(ah),
                static_cast<unsigned>(lba), count,
                static_cast<unsigned>(dest));
            disk_read_history_.emplace_back(readText);
            if (!(drive == 0 || hardDrive) || lba > media.size() / 512 || count == 0 ||
                count > media.size() / 512 - lba ||
                dest > state_->memory.size || bytes > state_->memory.size - dest)
                ok = false;
            if (ok)
            {
                std::memcpy(state_->memory.data + dest,
                    media.data() + static_cast<std::size_t>(lba) * 512, bytes);
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
        std::uint16_t biosKey = 0;

        if (function == 0x00 || function == 0x10)
        {
            if (readBiosKey(biosKey, true))
            {
                regs_[0] = biosKey;
                waitingForKey_ = false;
                flags_ &= static_cast<std::uint16_t>(~ZF);
            }
            else
            {
                waitingForKey_ = true;
            }
        }
        else if (function == 0x01 || function == 0x11)
        {
            if (readBiosKey(biosKey, false))
            {
                regs_[0] = biosKey;
                flags_ &= static_cast<std::uint16_t>(~ZF);
            }
            else
            {
                flags_ |= ZF;
            }
        }
        else if (function == 0x02)
        {
            setReg(0, false, read8(0, 0x417));
        }
        else
        {
            flags_ |= CF;
        }

        return;
    }

    if (number == 0x17)
    {
        const std::uint8_t function = regs_[0] >> 8;
        if (function <= 0x02)
        {
            // No parallel port is attached. Keep the BIOS call well-formed
            // and report an idle, selected printer so DOS startup probes can
            // complete without treating the unimplemented vector as a fault.
            regs_[0] = static_cast<std::uint16_t>((regs_[0] & 0x00FF) | 0x9000);
            flags_ &= static_cast<std::uint16_t>(~CF);
        }
        else
        {
            regs_[0] = static_cast<std::uint16_t>((regs_[0] & 0x00FF) | 0x0100);
            flags_ |= CF;
        }
        return;
    }

    if (number == 0x11) { regs_[0] = 0x0021; return; }
    if (number == 0x12) { regs_[0] = 640; return; }
    if (number == 0x1A)
    {
        updateBiosClock();
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
            setReg(0, false, state_->memory.data[0x470]);
            state_->memory.data[0x470] = 0;
            flags_ &= static_cast<std::uint16_t>(~CF);
        }
        else if (function == 0x01)
        {
            const std::uint32_t ticks =
                (static_cast<std::uint32_t>(regs_[1]) << 16) | regs_[2];
            bios_clock_epoch_ticks_ = ticks;
            bios_clock_days_ = 0;
            bios_clock_epoch_ = std::chrono::steady_clock::now();
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
            else if (subfunction == 0x03) { regs_[3] = 3; regs_[0] &= 0x00FF; } // BX = keyboard + fast A20, AH = 0
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
        std::uint16_t key = 0;
        if (!readBiosKey(key, false))
            return true;  // Still waiting for key

        cpuSleeping_ = false;
    }

    if (waitingForKey_)
    {
        std::uint16_t biosKey = 0;
        if (!readBiosKey(biosKey, true))
            return true;  // Still waiting for key

        regs_[0] = biosKey;
        flags_ &= static_cast<std::uint16_t>(~ZF);
        waitingForKey_ = false;
        return true;
    }

    const std::uint16_t instructionStart = ip_;
    int segmentOverride = -1;
    std::uint8_t repeat = 0;
    bool address32 = false;
    bool operand32 = false;
    bool opcodeFound = false;
    std::uint8_t opcode = 0;
    for (unsigned prefixCount = 0; prefixCount < 15; ++prefixCount)
    {
        opcode = fetch8();
        if (opcode == 0x26) segmentOverride = 0;
        else if (opcode == 0x2E) segmentOverride = 1;
        else if (opcode == 0x36) segmentOverride = 2;
        else if (opcode == 0x3E) segmentOverride = 3;
        else if (opcode == 0xF0) {} // LOCK is unnecessary in this single-CPU emulator.
        else if (opcode == 0x64) segmentOverride = 4;
        else if (opcode == 0x65) segmentOverride = 5;
        else if (opcode == 0x67) address32 = true;
        else if (opcode == 0x66) operand32 = true;
        else if (opcode == 0xF2 || opcode == 0xF3) repeat = opcode;
        else { opcodeFound = true; break; }
    }
    auto& traceEntry = instruction_trace_[instruction_trace_position_];
    traceEntry = {segs_[1], instructionStart, ip_, opcode};
    struct InstructionTraceFinalizer
    {
        std::uint16_t& ip;
        std::uint16_t& cs;
        InstructionTraceEntry& entry;
        ControlTransferEntry* controlTrace;
        std::size_t& controlPosition;
        std::size_t& controlCount;
        static bool isControlOpcode(std::uint8_t op)
        {
            return (op >= 0x70 && op <= 0x7F) ||
                (op >= 0xE0 && op <= 0xE3) || op == 0x9A ||
                op == 0xC2 || op == 0xC3 || op == 0xCA || op == 0xCB ||
                op == 0x0F || op == 0xCC || op == 0xCD || op == 0xCE || op == 0xCF ||
                op == 0xE8 || op == 0xE9 || op == 0xEA || op == 0xEB ||
                op == 0xFF;
        }
        ~InstructionTraceFinalizer()
        {
            entry.nextOffset = ip;
            if (!isControlOpcode(entry.opcode)) return;
            constexpr std::size_t capacity = 24;
            controlTrace[controlPosition] = {
                entry.segment, entry.offset, cs, ip, entry.opcode
            };
            controlPosition = (controlPosition + 1) % capacity;
            controlCount = (std::min)(controlCount + 1, capacity);
        }
    } traceFinalizer{ip_, segs_[1], traceEntry, control_trace_,
        control_trace_position_, control_trace_count_};
    instruction_trace_position_ = (instruction_trace_position_ + 1) %
        (sizeof(instruction_trace_) / sizeof(instruction_trace_[0]));
    instruction_trace_count_ = (std::min)(instruction_trace_count_ + 1,
        sizeof(instruction_trace_) / sizeof(instruction_trace_[0]));
    ++instruction_count_;
    // DOS reads the BIOS Data Area timer directly while waiting during boot.
    // Keep it advancing even when the guest never calls INT 1Ah; otherwise
    // early DOS versions spin forever with the timer stuck at zero.
    if ((instruction_count_ & 0xFFFFu) == 0)
        updateBiosClock();

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

    if (!opcodeFound)
        return fail("x86 instruction exceeds the 15-byte maximum");

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
                setReg32(3, 0x756E6547); // "Genu"
                setReg32(2, 0x49656E69); // EDX: "ineI"
                setReg32(1, 0x6C65746E); // ECX: "ntel"
                break;
            case 1:
                setReg32(0, 0x00000400); // Intel 486, family 4/model 0
                setReg32(3, 0);
                setReg32(2, 0x00000001); // EDX: integrated x87 FPU
                setReg32(1, 0); // ECX: no later-generation extensions
                break;
            default:
                setReg32(0, 0); setReg32(1, 0); setReg32(2, 0); setReg32(3, 0);
                break;
            }
            return true;
        }
        if (extension == 0x08 || extension == 0x09)
        {
            // INVD/WBINVD: the emulator has no separately modelled CPU cache.
            return true;
        }
        if (extension >= 0xC8 && extension <= 0xCF) // BSWAP r32 (486)
        {
            const std::uint8_t reg = extension & 7;
            const std::uint32_t value = getReg32(reg);
            setReg32(reg, (value << 24) | ((value << 8) & 0x00FF0000u) |
                ((value >> 8) & 0x0000FF00u) | (value >> 24));
            return true;
        }
        if (extension == 0xB0 || extension == 0xB1) // CMPXCHG (486)
        {
            const bool byte = extension == 0xB0;
            const bool word = !byte;
            const bool dword = word && operand32;
            const std::uint8_t modrm = fetch8();
            const std::uint8_t sourceReg = (modrm >> 3) & 7;
            const Operand destination = decodeOperand(modrm, word, segmentOverride, address32);
            const std::uint32_t accumulator = dword ? getReg32(0) : getReg(0, word);
            const std::uint32_t oldDestination = dword
                ? getOperand32(destination) : getOperand(destination, word);
            const std::uint32_t difference = dword
                ? alu32(7, accumulator, oldDestination)
                : alu(7, accumulator, oldDestination, word);
            (void)difference;
            if (flags_ & ZF)
            {
                if (dword) setOperand32(destination, getReg32(sourceReg));
                else setOperand(destination, word, getReg(sourceReg, word));
            }
            else if (dword) setReg32(0, oldDestination);
            else setReg(0, word, oldDestination);
            return true;
        }
        if (extension == 0xC0 || extension == 0xC1) // XADD (486)
        {
            const bool word = extension == 0xC1;
            const bool dword = word && operand32;
            const std::uint8_t modrm = fetch8();
            const std::uint8_t sourceReg = (modrm >> 3) & 7;
            const Operand destination = decodeOperand(modrm, word, segmentOverride, address32);
            const std::uint32_t oldDestination = dword
                ? getOperand32(destination) : getOperand(destination, word);
            const std::uint32_t source = dword ? getReg32(sourceReg) : getReg(sourceReg, word);
            const std::uint32_t sum = dword
                ? alu32(0, oldDestination, source)
                : alu(0, oldDestination, source, word);
            if (dword)
            {
                setOperand32(destination, sum);
                setReg32(sourceReg, oldDestination);
            }
            else
            {
                setOperand(destination, word, sum);
                setReg(sourceReg, word, oldDestination);
            }
            return true;
        }
        if (extension >= 0x90 && extension <= 0x9F) // SETcc
        {
            const std::uint8_t modrm = fetch8();
            const Operand destination = decodeOperand(modrm, false, segmentOverride, address32);
            setOperand(destination, false, condition(extension & 0x0F) ? 1 : 0);
            return true;
        }
        if (extension == 0xA3 || extension == 0xAB || extension == 0xB3 ||
            extension == 0xBB || extension == 0xBA) // BT/BTS/BTR/BTC
        {
            const std::uint8_t modrm = fetch8();
            const std::uint8_t operation = (modrm >> 3) & 7;
            if (extension == 0xBA && operation < 4)
                return fail("invalid 0F BA bit-test group operation");
            const bool word = true;
            Operand destination = decodeOperand(modrm, word, segmentOverride, address32);
            const bool dword = operand32;
            const unsigned width = dword ? 32 : 16;
            const std::int32_t rawIndex = extension == 0xBA
                ? fetch8()
                : dword ? static_cast<std::int32_t>(getReg32(operation))
                        : static_cast<std::int16_t>(getReg16(operation));
            const std::int64_t signedIndex = rawIndex;
            const std::int64_t element = destination.isRegister ? 0
                : signedIndex >= 0 ? signedIndex / width
                : -((-signedIndex + width - 1) / width);
            const std::uint32_t bitIndex = destination.isRegister
                ? (static_cast<std::uint32_t>(rawIndex) & (width - 1))
                : static_cast<std::uint32_t>(signedIndex - element * width);
            if (!destination.isRegister)
            {
                destination.offset = static_cast<std::uint32_t>(
                    destination.offset + element * (width / 8));
            }
            const std::uint32_t value = dword
                ? getOperand32(destination) : getOperand(destination, true);
            const std::uint32_t bit = 1u << bitIndex;
            flags_ = static_cast<std::uint16_t>((flags_ & ~CF) | ((value & bit) ? CF : 0));
            const bool set = extension == 0xAB || (extension == 0xBA && operation == 5);
            const bool clear = extension == 0xB3 || (extension == 0xBA && operation == 6);
            const bool toggle = extension == 0xBB || (extension == 0xBA && operation == 7);
            if (set || clear || toggle)
            {
                std::uint32_t result = value;
                if (set) result |= bit;
                else if (clear) result &= ~bit;
                else result ^= bit;
                if (dword) setOperand32(destination, result);
                else setOperand(destination, true, result);
            }
            return true;
        }
        if (extension == 0xAF) // IMUL r16/32, r/m16/32
        {
            const std::uint8_t modrm = fetch8();
            const std::uint8_t reg = (modrm >> 3) & 7;
            const Operand source = decodeOperand(modrm, true, segmentOverride, address32);
            bool overflow = false;
            if (operand32)
            {
                const std::int64_t product = static_cast<std::int64_t>(
                    static_cast<std::int32_t>(getReg32(reg))) *
                    static_cast<std::int32_t>(getOperand32(source));
                const std::int32_t low = static_cast<std::int32_t>(product);
                overflow = product != low;
                setReg32(reg, static_cast<std::uint32_t>(low));
            }
            else
            {
                const std::int32_t product = static_cast<std::int16_t>(getReg16(reg)) *
                    static_cast<std::int16_t>(getOperand(source, true));
                const std::int16_t low = static_cast<std::int16_t>(product);
                overflow = product != low;
                setReg16(reg, static_cast<std::uint16_t>(low));
            }
            flags_ = static_cast<std::uint16_t>((flags_ & ~(CF | OF)) |
                (overflow ? (CF | OF) : 0));
            return true;
        }
        if (extension == 0xB2 || extension == 0xB4 || extension == 0xB5)
        {
            const std::uint8_t modrm = fetch8();
            const std::uint8_t reg = (modrm >> 3) & 7;
            const Operand source = decodeOperand(modrm, true, segmentOverride, address32);
            if (source.isRegister)
                return fail("LSS/LFS/LGS requires a memory operand");
            const std::uint32_t offset = operand32
                ? read32(source.segment, source.offset, source.address32)
                : read16(source.segment, source.offset);
            const std::uint32_t segmentOffset = source.offset + (operand32 ? 4u : 2u);
            const std::uint16_t segment = static_cast<std::uint16_t>(
                read8(source.segment, segmentOffset) |
                (static_cast<std::uint16_t>(read8(source.segment, segmentOffset + 1)) << 8));
            if (operand32) setReg32(reg, offset);
            else setReg16(reg, static_cast<std::uint16_t>(offset));
            segs_[extension == 0xB2 ? 2 : extension == 0xB4 ? 4 : 5] = segment;
            return true;
        }
        if (extension == 0xA4 || extension == 0xA5 || extension == 0xAC || extension == 0xAD)
        {
            const bool right = extension >= 0xAC;   // SHRD
            const std::uint8_t modrm = fetch8();
            const std::uint8_t reg = (modrm >> 3) & 7;
            const Operand destination = decodeOperand(modrm, true, segmentOverride, address32);
            const unsigned count = ((extension == 0xA4 || extension == 0xAC)
                ? fetch8() : getReg(1, false)) & 0x1F;
            if (count == 0) return true;

            const bool dword = operand32;
            const unsigned width = dword ? 32 : 16;
            const std::uint64_t mask = dword ? 0xFFFFFFFFull : 0xFFFFull;
            const std::uint64_t dst = (dword ? getOperand32(destination) : getOperand(destination, true)) & mask;
            const std::uint64_t src = (dword ? getReg32(reg) : getReg16(reg)) & mask;

            std::uint64_t result;
            bool carry;
            if (!right)
            {
                const std::uint64_t combined = (dst << width) | src;
                result = ((combined << count) >> width) & mask;
                carry = ((combined >> (2 * width - count)) & 1) != 0;
            }
            else
            {
                const std::uint64_t combined = (src << width) | dst;
                result = (combined >> count) & mask;
                carry = ((combined >> (count - 1)) & 1) != 0;
            }
            flags_ = static_cast<std::uint16_t>((flags_ & ~CF) | (carry ? CF : 0));
            if (count == 1)
            {
                const bool changed = (((dst ^ result) >> (width - 1)) & 1) != 0;
                flags_ = static_cast<std::uint16_t>((flags_ & ~OF) | (changed ? OF : 0));
            }
            if (dword)
            {
                setOperand32(destination, static_cast<std::uint32_t>(result));
                setSZP32(static_cast<std::uint32_t>(result));
            }
            else
            {
                setOperand(destination, true, static_cast<std::uint32_t>(result));
                setSZP(static_cast<std::uint32_t>(result), true);
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
            std::uint32_t value = getOperand(rm, sourceWord);
            if (extension == 0xBE) value = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value)));
            if (extension == 0xBF) value = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value)));
            if (operand32) setReg32(reg, value);
            else setReg16(reg, static_cast<std::uint16_t>(value));
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
        const bool word = (opcode & 1) != 0;
        const std::uint16_t value = ioRead(port, word);
        if (opcode & 1) regs_[0] = value;
        else setReg(0, false, value);
        return true;
    }
    case 0xE6: case 0xE7: case 0xEE: case 0xEF:
        {
            const std::uint16_t port = opcode < 0xEE ? fetch8() : regs_[2];
            const bool word = opcode == 0xE7 || opcode == 0xEF;
            if (word) ioWrite16(port, regs_[0]);
            else ioWrite(port, static_cast<std::uint8_t>(regs_[0]));
        }
        return true;
    case 0x6C: case 0x6D: // INS byte/word from an emulated I/O port
    {
        const bool word = opcode == 0x6D;
        const std::uint32_t count = repeat ? regs_[1] : 1;
        const std::int16_t delta = (flags_ & DF) ? -static_cast<std::int16_t>(word ? 2 : 1)
                                                 : static_cast<std::int16_t>(word ? 2 : 1);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            const std::uint16_t value = ioRead(regs_[2], word);
            if (word) write16(segs_[0], regs_[7], value);
            else write8(segs_[0], regs_[7], static_cast<std::uint8_t>(value));
            regs_[7] = static_cast<std::uint16_t>(regs_[7] + delta);
        }
        if (repeat) regs_[1] = 0;
        return true;
    }
    case 0x6E: case 0x6F: // OUTS byte/word to an emulated I/O port
    {
        const bool word = opcode == 0x6F;
        const std::uint32_t count = repeat ? regs_[1] : 1;
        const std::int16_t delta = (flags_ & DF) ? -static_cast<std::int16_t>(word ? 2 : 1)
                                                 : static_cast<std::int16_t>(word ? 2 : 1);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            const std::uint16_t value = word ? read16(segs_[3], regs_[6]) : read8(segs_[3], regs_[6]);
            if (word) ioWrite16(regs_[2], value);
            else ioWrite(regs_[2], static_cast<std::uint8_t>(value));
            regs_[6] = static_cast<std::uint16_t>(regs_[6] + delta);
        }
        if (repeat) regs_[1] = 0;
        return true;
    }
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
        const auto modrm = fetch8(); const auto reg = (modrm >> 3) & 7;
        if (reg > 5) return fail("invalid segment register in MOV Sreg");
        const auto rm = decodeOperand(modrm, true, segmentOverride, address32);
        if (opcode == 0x8C) setOperand(rm, true, segs_[reg]);
        else
        {
            if (reg == 1) return fail("MOV into CS is invalid");
            segs_[reg] = static_cast<std::uint16_t>(getOperand(rm, true));
        }
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
        const bool word = opcode & 1;
        const bool dword = operand32 && word;
        const auto modrm = fetch8();
        const auto operation = (modrm >> 3) & 7;
        const auto rm = decodeOperand(modrm, word, segmentOverride, address32);
        const unsigned count = ((opcode == 0xC0 || opcode == 0xC1) ? fetch8() :
            (opcode == 0xD0 || opcode == 0xD1) ? 1u : getReg(1, false)) & 0x1F;
        if (count == 0) return true;   // flags stay untouched

        const unsigned bits = dword ? 32 : (word ? 16 : 8);
        const std::uint64_t mask = bits == 32 ? 0xFFFFFFFFull : ((1ull << bits) - 1);
        const std::uint64_t msb = 1ull << (bits - 1);
        const std::uint64_t value = (dword ? getOperand32(rm) : getOperand(rm, word)) & mask;
        std::uint64_t result = value;
        bool cf = (flags_ & CF) != 0;
        bool of = (flags_ & OF) != 0;
        bool setsSZP = false;

        if (operation == 0)        // ROL
        {
            const unsigned n = count % bits;
            result = n ? ((value << n) | (value >> (bits - n))) & mask : value;
            cf = (result & 1) != 0;
            if (count == 1) of = ((result & msb) != 0) != cf;
        }
        else if (operation == 1)   // ROR
        {
            const unsigned n = count % bits;
            result = n ? ((value >> n) | (value << (bits - n))) & mask : value;
            cf = (result & msb) != 0;
            if (count == 1) of = cf != ((result & (msb >> 1)) != 0);
        }
        else if (operation == 2 || operation == 3)   // RCL / RCR
        {
            for (unsigned i = 0; i < count; ++i)
            {
                if (operation == 2)
                {
                    const bool out = (result & msb) != 0;
                    result = ((result << 1) | (cf ? 1u : 0u)) & mask;
                    cf = out;
                }
                else
                {
                    const bool out = (result & 1) != 0;
                    result = (result >> 1) | (cf ? msb : 0);
                    cf = out;
                }
            }
            if (count == 1)
                of = operation == 2
                ? ((result & msb) != 0) != cf
                : ((result & msb) != 0) != ((result & (msb >> 1)) != 0);
        }
        else if (operation == 4 || operation == 6)   // SHL / SAL
        {
            setsSZP = true;
            cf = count <= bits && ((value >> (bits - count)) & 1) != 0;
            result = count >= bits ? 0 : (value << count) & mask;
            if (count == 1) of = ((result & msb) != 0) != cf;
        }
        else if (operation == 5)   // SHR
        {
            setsSZP = true;
            cf = count <= bits && ((value >> (count - 1)) & 1) != 0;
            result = count >= bits ? 0 : value >> count;
            if (count == 1) of = (value & msb) != 0;
        }
        else   // SAR
        {
            setsSZP = true;
            const std::int64_t signedValue = bits == 32 ? static_cast<std::int32_t>(value)
                : bits == 16 ? static_cast<std::int16_t>(value) : static_cast<std::int8_t>(value);
            cf = count >= bits ? (value & msb) != 0 : ((signedValue >> (count - 1)) & 1) != 0;
            result = static_cast<std::uint64_t>(signedValue >> (count >= bits ? bits - 1 : count)) & mask;
            if (count == 1) of = false;
        }

        if (setsSZP)
        {
            flags_ &= static_cast<std::uint16_t>(~AF);
            if (dword) setSZP32(static_cast<std::uint32_t>(result));
            else setSZP(static_cast<std::uint32_t>(result), word);
        }
        flags_ = static_cast<std::uint16_t>((flags_ & ~(CF | OF)) | (cf ? CF : 0) | (of ? OF : 0));
        if (dword) setOperand32(rm, static_cast<std::uint32_t>(result));
        else setOperand(rm, word, static_cast<std::uint32_t>(result));
        return true;
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
        const bool word = opcode == 0xFF;
        const bool dword = word && operand32;
        const auto modrm = fetch8();
        const auto operation = (modrm >> 3) & 7;
        const unsigned mod = modrm >> 6;
        traceEntry.nextOffset = ip_;
        const bool validOperation = opcode == 0xFE
            ? operation <= 1
            : operation <= 6 && !((operation == 3 || operation == 5) && mod == 3);
        if (!validOperation)
        {
            const auto vectorOffset = read16(0, 6 * 4);
            const auto vectorSegment = read16(0, 6 * 4 + 2);
            const auto biosOffset = static_cast<std::uint16_t>(0x0100 + 6 * 4);
            if (vectorSegment == 0xF000 && vectorOffset == biosOffset)
            {
                char message[8192];
                int length = std::snprintf(message, sizeof(message),
                    "#UD invalid opcode %02X /%u ModRM %02X at %04X:%04X "
                    "AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X BP=%04X SP=%04X "
                    "DS=%04X ES=%04X SS=%04X FLAGS=%04X bytes[-8..+7]=",
                    opcode, operation, modrm, segs_[1], instructionStart,
                    regs_[0], regs_[3], regs_[1], regs_[2], regs_[6], regs_[7],
                    regs_[5], regs_[4], segs_[3], segs_[0], segs_[2], flags_);
                for (int delta = -8; delta < 8 && length > 0 &&
                    static_cast<std::size_t>(length) < sizeof(message); ++delta)
                {
                    const auto offset = static_cast<std::uint16_t>(instructionStart + delta);
                    length += std::snprintf(message + length, sizeof(message) - length,
                        "%s%02X", delta == -8 ? "" : " ",
                        read8(segs_[1], offset));
                }
                if (length > 0 && static_cast<std::size_t>(length) < sizeof(message))
                    length += std::snprintf(message + length, sizeof(message) - length,
                        " trace=");
                const std::size_t traceStart =
                    (instruction_trace_position_ +
                        (sizeof(instruction_trace_) / sizeof(instruction_trace_[0])) -
                        instruction_trace_count_) %
                    (sizeof(instruction_trace_) / sizeof(instruction_trace_[0]));
                // Keep enough pre-fault history to expose a bad jump/return that
                // entered a run of data or uninitialized memory before #UD.
                for (std::size_t i = 0; i < instruction_trace_count_ && length > 0 &&
                    static_cast<std::size_t>(length) < sizeof(message); ++i)
                {
                    const auto& entry = instruction_trace_[
                        (traceStart + i) %
                        (sizeof(instruction_trace_) / sizeof(instruction_trace_[0]))];
                    const int written = std::snprintf(message + length, sizeof(message) - static_cast<std::size_t>(length),
                        "%s%04X:%04X=%02X>%04X", i == 0 ? "" : ",",
                        entry.segment, entry.offset, entry.opcode, entry.nextOffset);
                    if (written < 0) break;
                    length = written >= static_cast<int>(sizeof(message) - static_cast<std::size_t>(length))
                        ? static_cast<int>(sizeof(message) - 1) : length + written;
                }
                const std::size_t transferStart =
                    (control_trace_position_ + 24 - control_trace_count_) % 24;
                std::fprintf(stderr, "\nRecent x86 control transfers (%zu):\n", control_trace_count_);
                for (std::size_t i = 0; i < control_trace_count_; ++i)
                {
                    const auto& transfer = control_trace_[(transferStart + i) % 24];
                    std::fprintf(stderr, "%04X:%04X=%02X>%04X:%04X\n",
                        transfer.fromSegment, transfer.fromOffset, transfer.opcode,
                        transfer.toSegment, transfer.toOffset);
                }
                return fail(message);
            }

            // The 486 reports the address of the offending instruction (including
            // any prefixes) when delivering the invalid-opcode exception.
            regs_[4] -= 2;
            write16(segs_[2], regs_[4], flags_);
            regs_[4] -= 2;
            write16(segs_[2], regs_[4], segs_[1]);
            regs_[4] -= 2;
            write16(segs_[2], regs_[4], instructionStart);
            flags_ &= static_cast<std::uint16_t>(~(IF | TF));
            segs_[1] = vectorSegment;
            ip_ = vectorOffset;
            return true;
        }

        const auto rm = decodeOperand(modrm, word, segmentOverride, address32);
        const std::uint32_t value = dword
            ? getOperand32(rm) : getOperand(rm, word);
        if (operation == 0 || operation == 1)
        {
            const auto carry = flags_ & CF;
            const auto result = dword
                ? alu32(operation == 0 ? 0 : 5, value, 1)
                : alu(operation == 0 ? 0 : 5, value, 1, word);
            flags_ = static_cast<std::uint16_t>((flags_ & ~CF) | carry);
            if (dword) setOperand32(rm, result);
            else setOperand(rm, word, result);
            return true;
        }
        if (operation == 2)
        {
            if (dword)
            {
                if (value > 0xFFFF) return fail("real-mode near CALL target exceeds CS limit");
                push32(ip_);
                ip_ = static_cast<std::uint16_t>(value);
            }
            else { push(ip_); ip_ = static_cast<std::uint16_t>(value); }
            return true;
        }
        if (operation == 4)
        {
            if (dword && value > 0xFFFF)
                return fail("real-mode near JMP target exceeds CS limit");
            ip_ = static_cast<std::uint16_t>(value);
            return true;
        }
        if ((operation == 3 || operation == 5) && !rm.isRegister)
        {
            const auto pointerOffset = [address32](std::uint32_t offset, unsigned amount)
            {
                return address32 ? offset + amount : (offset + amount) & 0xFFFFu;
            };
            const std::uint32_t target = dword
                ? read32(rm.segment, rm.offset, rm.address32)
                : read16(rm.segment, rm.offset);
            const std::uint16_t segment = read16(rm.segment,
                pointerOffset(rm.offset, dword ? 4 : 2));
            if (dword && target > 0xFFFF)
                return fail("real-mode far control target exceeds CS limit");
            if (operation == 3)
            {
                push(segs_[1]);
                if (dword) push32(ip_); else push(ip_);
            }
            ip_ = static_cast<std::uint16_t>(target);
            segs_[1] = segment;
            return true;
        }
        if (operation == 6)
        {
            if (dword) push32(value); else push(static_cast<std::uint16_t>(value));
            return true;
        }
        return fail("internal FF/FE decoder inconsistency");
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
                if (quotient < static_cast<int16_t>(-32768) || quotient > static_cast<int16_t>(32767))
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

std::size_t X86Compat::run(std::size_t instructionBudget)
{
    std::size_t executed = 0;
    static bool fired = false;
    while (executed < instructionBudget && step())
    {
        ++executed;
        if (fired || state_ == nullptr || state_->memory.size < 0xB8FA0)
            continue;
        for (unsigned r = 0; r < 25; ++r)
        {
            const std::size_t a = 0xB8000 + r * 160;
            if (state_->memory.data[a + 1] != 0x10)
                continue;
            fired = true;
            constexpr std::size_t cap =
                sizeof(instruction_trace_) / sizeof(instruction_trace_[0]);
            const auto& e = instruction_trace_[
                (instruction_trace_position_ + cap - 1) % cap];
            std::fprintf(stderr,
                "WATCH row=%u char=%02X after %04X:%04X op=%02X next=%02X "
                "AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X ES=%04X DS=%04X\n",
                r, state_->memory.data[a], e.segment, e.offset, e.opcode,
                read8(e.segment, e.offset + 1),
                regs_[0], regs_[3], regs_[1], regs_[2],
                regs_[6], regs_[7], segs_[0], segs_[3]);
            break;
        }
    }
    return executed;
}

void X86Compat::pollKeyboardInput()
{
    if (state_ == nullptr || state_->memory.data == nullptr ||
        state_->memory.size < 0x440)
    {
        return;
    }

    const auto validOffset = [](std::uint16_t offset)
    {
        return offset >= 0x001E && offset < 0x003E &&
            ((offset - 0x001E) % 2) == 0;
    };

    std::uint16_t head = read16(0, 0x41A);
    std::uint16_t tail = read16(0, 0x41C);

    if (!validOffset(head) || !validOffset(tail))
    {
        head = 0x001E;
        tail = 0x001E;
        write16(0, 0x41A, head);
        write16(0, 0x41C, tail);
    }

    AMD3DC_Key key = AMD3DC_KEY_NONE;
    while (amd3dc_gui_poll_key(&key))
    {
        const std::uint16_t biosKey = biosKeyCode(key);
        if (biosKey == 0)
            continue;

        head = read16(0, 0x41A);
        tail = read16(0, 0x41C);

        std::uint16_t nextTail =
            static_cast<std::uint16_t>(tail + 2);

        if (nextTail >= 0x003E)
            nextTail = 0x001E;

        if (nextTail == head)
            continue; // BIOS keyboard buffer is full.

        write16(0, 0x0400 + tail, biosKey);
        write16(0, 0x41C, nextTail);
    }
}

bool X86Compat::readBiosKey(std::uint16_t& key, bool consume)
{
    if (state_ == nullptr || state_->memory.data == nullptr ||
        state_->memory.size < 0x440)
    {
        return false;
    }

    const std::uint16_t head = read16(0, 0x41A);
    const std::uint16_t tail = read16(0, 0x41C);

    const auto validOffset = [](std::uint16_t offset)
    {
        return offset >= 0x001E && offset < 0x003E &&
            ((offset - 0x001E) % 2) == 0;
    };

    if (!validOffset(head) || !validOffset(tail) || head == tail)
        return false;

    key = read16(0, 0x0400 + head);

    if (consume)
    {
        std::uint16_t nextHead =
            static_cast<std::uint16_t>(head + 2);

        if (nextHead >= 0x003E)
            nextHead = 0x001E;

        write16(0, 0x41A, nextHead);
    }

    return true;
}

