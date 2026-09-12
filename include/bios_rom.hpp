#ifndef AMD3DC_BIOS_ROM_HPP
#define AMD3DC_BIOS_ROM_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

extern "C"
{
#include "amd3dc_types.h"
}

class BiosRom
{
public:
    bool load(const std::string& filename);
    bool boot(AMD3DC_State& state, std::size_t maxInstructions);

    void clear();

    bool isLoaded() const noexcept;
    std::uint32_t version() const noexcept;
    std::uint32_t flags() const noexcept;
    std::uint64_t entryPoint() const noexcept;
    std::uint64_t imageSize() const noexcept;

private:
    static constexpr std::size_t HeaderSize = 0x20;

    std::vector<std::uint8_t> image_;

    std::uint32_t version_ = 0;
    std::uint32_t flags_ = 0;
    std::uint64_t entryPoint_ = 0;
    std::uint64_t imageSize_ = 0;

    bool loaded_ = false;

    static std::uint32_t readBE32(
        const std::uint8_t* data
    );

    static std::uint64_t readBE64(
        const std::uint8_t* data
    );
};

#endif