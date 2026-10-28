#ifndef AMD3DC_X86_COMPAT_HPP
#define AMD3DC_X86_COMPAT_HPP

#include <cstddef>
#include <cstdint>
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
    bool step();
    void renderVideo();
    bool halted() const noexcept;
    const std::string& error() const noexcept;
    std::uint64_t instructionCount() const noexcept;
    std::string debugState() const;

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

    AMD3DC_State* state_ = nullptr;
    std::vector<std::uint8_t> disk_;
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
    std::vector<std::string> disk_read_history_;
    std::uint64_t disk_read_calls_ = 0;
    std::uint64_t disk_sectors_read_ = 0;
    std::uint64_t disk_read_failures_ = 0;
    std::uint32_t last_disk_lba_ = 0;

    std::uint8_t fetch8();
    std::uint16_t fetch16();
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
    void setError(const char* message);
};

#endif
