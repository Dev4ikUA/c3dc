#ifndef AMD3DC_X86_COMPAT_HPP
#define AMD3DC_X86_COMPAT_HPP

#include <cstddef>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

extern "C"
{
#include "amd3dc_types.h"
}

class X86Compat
{
public:
    bool bootFloppy(AMD3DC_State& state, std::vector<std::uint8_t> image);
    void attachHardDisk(std::vector<std::uint8_t> image, const std::string& path);
    void attachSecondFloppy(std::vector<std::uint8_t> image);
    void ataCommand(std::uint8_t command);
    void ataFinishWriteSector();
    void ataSignature();
    void persistHardDisk(std::uint64_t lba, std::size_t sectors);
    void pollHostInput();
    bool step();
    std::size_t run(std::size_t instructionBudget);
    void renderVideo();
    bool halted() const noexcept;
    const std::string& error() const noexcept;
    std::uint64_t instructionCount() const noexcept;
    std::string debugState() const;
    void flushOutput();

private:
    struct Operand
    {
        bool isRegister = false;
        bool highByte = false;
        bool address32 = false;
        std::uint8_t reg = 0;
        std::uint16_t segment = 0;
        std::uint32_t offset = 0;
    };

    struct InstructionTraceEntry
    {
        std::uint16_t segment = 0;
        std::uint16_t offset = 0;
        std::uint16_t nextOffset = 0;
        std::uint8_t opcode = 0;
    };

    struct ControlTransferEntry
    {
        std::uint16_t fromSegment = 0;
        std::uint16_t fromOffset = 0;
        std::uint16_t toSegment = 0;
        std::uint16_t toOffset = 0;
        std::uint8_t opcode = 0;
    };

    AMD3DC_State* state_ = nullptr;
    std::vector<std::uint8_t> disk_;
    std::uint8_t rendered_text_[80 * 25 * 2]{};
    bool text_rendered_ = false;
    std::vector<std::uint8_t> hard_disk_;
    std::vector<std::uint8_t> second_floppy_;
    std::uint8_t ata_regs_[8]{};
    std::uint8_t ata_status_ = 0x50;
    std::uint16_t ata_heads_ = 16;
    std::uint16_t ata_spt_ = 63;
    bool ata_nien_ = false;
    std::string hard_disk_path_;
    std::fstream hard_disk_file_;
    std::size_t ata_data_pos_ = 512;
    std::uint8_t ata_sector_[512]{};
    std::uint32_t ata_lba_ = 0;
    std::uint16_t ata_sectors_remaining_ = 0;
    bool ata_write_command_ = false;
    bool ata_identify_ = false;
    std::uint16_t ioRead(std::uint16_t port, bool word = false);
    void ioWrite(std::uint16_t port, std::uint8_t value);
    void ioWrite16(std::uint16_t port, std::uint16_t value);
    std::uint16_t regs_[8]{};
    std::uint16_t regs_high_[8]{};
    std::uint16_t segs_[6]{}; // ES, CS, SS, DS, FS, GS
    std::uint16_t ip_ = 0x7C00;
    std::uint16_t flags_ = 0x0002;
    bool halted_ = false;
    bool cpuSleeping_ = false;
    bool a20Enabled_ = false;
    bool waitingForKey_ = false;
    std::uint16_t pendingBiosKey_ = 0;
    std::string error_;
    std::uint64_t instruction_count_ = 0;
    InstructionTraceEntry instruction_trace_[128]{};
    std::size_t instruction_trace_position_ = 0;
    std::size_t instruction_trace_count_ = 0;
    ControlTransferEntry control_trace_[24]{};
    std::size_t control_trace_position_ = 0;
    std::size_t control_trace_count_ = 0;
    std::vector<std::string> disk_read_history_;
    std::uint64_t disk_read_calls_ = 0;
    std::uint64_t disk_sectors_read_ = 0;
    std::uint64_t disk_read_failures_ = 0;
    std::uint32_t last_disk_lba_ = 0;
    std::chrono::steady_clock::time_point bios_clock_epoch_{};
    std::uint64_t bios_clock_epoch_ticks_ = 0;
    std::uint64_t bios_clock_days_ = 0;

    std::uint8_t fetch8();
    std::uint16_t fetch16();
    std::uint32_t fetch32();
    std::uint8_t read8(std::uint16_t segment, std::uint32_t offset) const;
    std::uint16_t read16(std::uint16_t segment, std::uint32_t offset) const;
    std::uint32_t read32(std::uint16_t segment, std::uint32_t offset,
        bool address32 = false) const;
    void write8(std::uint16_t segment, std::uint32_t offset, std::uint8_t value);
    void write16(std::uint16_t segment, std::uint32_t offset, std::uint16_t value);
    void write32(std::uint16_t segment, std::uint32_t offset, std::uint32_t value,
        bool address32 = false);
    Operand decodeOperand(std::uint8_t modrm, bool word, int segmentOverride = -1,
        bool address32 = false);
    std::uint16_t getReg16(std::uint8_t reg) const;
    void setReg16(std::uint8_t reg, std::uint16_t value);
    std::uint32_t getReg32(std::uint8_t reg) const;
    void setReg32(std::uint8_t reg, std::uint32_t value);
    std::uint32_t getReg(std::uint8_t reg, bool word) const;
    void setReg(std::uint8_t reg, bool word, std::uint32_t value);
    std::uint32_t getOperand(const Operand& operand, bool word) const;
    void setOperand(const Operand& operand, bool word, std::uint32_t value);
    std::uint32_t getOperand32(const Operand& operand) const;
    void setOperand32(const Operand& operand, std::uint32_t value);
    void setSZP(std::uint32_t value, bool word);
    void setSZP32(std::uint32_t value);
    std::uint32_t alu(std::uint8_t operation, std::uint32_t lhs,
        std::uint32_t rhs, bool word);
    std::uint32_t alu32(std::uint8_t operation, std::uint32_t lhs,
        std::uint32_t rhs);
    bool condition(std::uint8_t condition_code) const;
    void interrupt(std::uint8_t number);
    void biosInterrupt(std::uint8_t number);
    void updateBiosClock();
    void pollKeyboardInput();
    bool readBiosKey(std::uint16_t& key, bool consume);
    void setError(const char* message);
};

#endif
