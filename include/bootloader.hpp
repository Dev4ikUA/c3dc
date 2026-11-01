#ifndef AMD3DC_BOOTLOADER_HPP
#define AMD3DC_BOOTLOADER_HPP

#include <cstddef>
#include <cstdint>
#include <string>

#include "x86_compat.hpp"

extern "C"
{
#include "amd3dc_types.h"
}

class Bootloader
{
public:
    bool loadImage(
        AMD3DC_State& state,
        const std::string& filename
    );
    bool loadHardDisk(const std::string& filename);
    bool loadSecondFloppy(const std::string& filename);

    bool start(
        AMD3DC_State& state
    );

    bool isRealModeImage() const noexcept;
    bool stepRealMode();
    std::size_t runRealModeBatch(std::size_t instructionBudget);
    void pollRealModeInput();
    void renderRealModeDisplay();
    const std::string& realModeError() const noexcept;

    std::uint64_t loadAddress() const noexcept;
    std::uint64_t entryPoint() const noexcept;
    std::uint64_t imageSize() const noexcept;

    std::string realModeDebugState() const { return x86_.debugState(); }

private:
    std::uint64_t loadAddress_ = 0;
    std::uint64_t entryPoint_ = 0;
    std::uint64_t imageSize_ = 0;
    std::uint64_t payloadSize_ = 0;

    bool loaded_ = false;
    bool realModeImage_ = false;
    X86Compat x86_;
};

#endif
