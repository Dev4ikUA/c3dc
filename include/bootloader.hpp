#ifndef AMD3DC_BOOTLOADER_HPP
#define AMD3DC_BOOTLOADER_HPP

#include <cstddef>
#include <cstdint>
#include <string>

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

    bool start(
        AMD3DC_State& state
    );

    std::uint64_t loadAddress() const noexcept;
    std::uint64_t entryPoint() const noexcept;
    std::uint64_t imageSize() const noexcept;

private:
    std::uint64_t loadAddress_ = 0;
    std::uint64_t entryPoint_ = 0;
    std::uint64_t imageSize_ = 0;

    bool loaded_ = false;
};

#endif