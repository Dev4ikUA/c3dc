#include "bios_rom.hpp"

#include <cstring>
#include <fstream>

extern "C"
{
#include "amd3dc.h"
#include "isa.h"
#include "memory.h"
}

namespace
{
    constexpr std::uint8_t Magic[] =
    {
        'C', '3', 'D', 'B', 'I', 'O', 'S', '\0'
    };
}

std::uint32_t BiosRom::readBE32(
    const std::uint8_t* data
)
{
    return
        (static_cast<std::uint32_t>(data[0]) << 24u) |
        (static_cast<std::uint32_t>(data[1]) << 16u) |
        (static_cast<std::uint32_t>(data[2]) << 8u) |
        static_cast<std::uint32_t>(data[3]);
}

std::uint64_t BiosRom::readBE64(
    const std::uint8_t* data
)
{
    std::uint64_t value = 0;

    for (int i = 0; i < 8; ++i)
    {
        value <<= 8u;
        value |= data[i];
    }

    return value;
}

bool BiosRom::load(
    const std::string& filename
)
{
    clear();

    std::ifstream file(
        filename,
        std::ios::binary | std::ios::ate
    );

    if (!file)
        return false;

    const std::streamsize size =
        file.tellg();

    if (size < static_cast<std::streamsize>(HeaderSize))
        return false;

    file.seekg(0);

    image_.resize(
        static_cast<std::size_t>(size)
    );

    if (!file.read(
        reinterpret_cast<char*>(image_.data()),
        size))
    {
        clear();
        return false;
    }

    if (std::memcmp(
        image_.data(),
        Magic,
        sizeof(Magic)) != 0)
    {
        clear();
        return false;
    }

    version_ =
        readBE32(&image_[0x08]);

    flags_ =
        readBE32(&image_[0x0C]);

    entryPoint_ =
        readBE64(&image_[0x10]);

    imageSize_ =
        readBE64(&image_[0x18]);

    if (imageSize_ < HeaderSize ||
        imageSize_ > image_.size())
    {
        clear();
        return false;
    }

    if (entryPoint_ >= imageSize_)
    {
        clear();
        return false;
    }

    loaded_ = true;

    return true;
}

bool BiosRom::boot(
    AMD3DC_State& state,
    std::size_t maxInstructions
)
{
    if (!loaded_)
        return false;

    if (state.cores == nullptr ||
        state.core_count == 0)
    {
        return false;
    }

    if (state.memory.data == nullptr ||
        state.memory.size < imageSize_)
    {
        return false;
    }

    std::memcpy(
        state.memory.data,
        image_.data(),
        static_cast<std::size_t>(imageSize_)
    );

    AMD3DC_Core& core =
        state.cores[0];

    core.pc = entryPoint_;
    core.halted = false;

    for (std::size_t i = 0;
        i < maxInstructions;
        ++i)
    {
        if (core.halted)
            return true;

        if (core.pc > imageSize_ ||
            imageSize_ - core.pc < 8)
        {
            return false;
        }

        const std::uint64_t raw =
            amd3dc_fetch64(&state);

        const AMD3DC_Instruction instruction =
            amd3dc_decode64(raw);

        if (!amd3dc_execute64(
            &state,
            &instruction))
        {
            return false;
        }
    }

    return core.halted;
}

void BiosRom::clear()
{
    image_.clear();

    version_ = 0;
    flags_ = 0;
    entryPoint_ = 0;
    imageSize_ = 0;

    loaded_ = false;
}

bool BiosRom::isLoaded() const noexcept
{
    return loaded_;
}

std::uint32_t BiosRom::version() const noexcept
{
    return version_;
}

std::uint32_t BiosRom::flags() const noexcept
{
    return flags_;
}

std::uint64_t BiosRom::entryPoint() const noexcept
{
    return entryPoint_;
}

std::uint64_t BiosRom::imageSize() const noexcept
{
    return imageSize_;
}