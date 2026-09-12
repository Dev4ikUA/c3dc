#include "bootloader.hpp"
#include "bootloader.hpp"

#include <fstream>
#include <vector>
#include <cstring>

namespace
{
    constexpr char Magic[] = "C3BL";
    constexpr std::size_t HeaderSize = 0x20;

    std::uint32_t readBE32(
        const std::uint8_t* data
    )
    {
        return
            (static_cast<std::uint32_t>(data[0]) << 24u) |
            (static_cast<std::uint32_t>(data[1]) << 16u) |
            (static_cast<std::uint32_t>(data[2]) << 8u) |
            static_cast<std::uint32_t>(data[3]);
    }

    std::uint64_t readBE64(
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
}

bool Bootloader::loadImage(
    AMD3DC_State& state,
    const std::string& filename
)
{
    std::ifstream file(
        filename,
        std::ios::binary | std::ios::ate
    );

    if (!file)
        return false;

    const auto size =
        file.tellg();

    if (size < static_cast<std::streamoff>(HeaderSize))
        return false;

    file.seekg(0);

    std::vector<std::uint8_t> image(
        static_cast<std::size_t>(size)
    );

    if (!file.read(
        reinterpret_cast<char*>(image.data()),
        size))
    {
        return false;
    }

    if (image[0] != 'C' ||
        image[1] != '3' ||
        image[2] != 'B' ||
        image[3] != 'L')
    {
        return false;
    }

    loadAddress_ =
        readBE64(&image[0x10]);

    entryPoint_ =
        readBE64(&image[0x08]);

    imageSize_ =
        readBE64(&image[0x18]);

    if (imageSize_ < HeaderSize ||
        imageSize_ > image.size())
        return false;

    if (state.memory.data == nullptr ||
        loadAddress_ > state.memory.size ||
        imageSize_ - HeaderSize >
            state.memory.size - loadAddress_)
    {
        return false;
    }

    if (entryPoint_ < loadAddress_ ||
        entryPoint_ >= loadAddress_ + imageSize_ - HeaderSize)
    {
        return false;
    }

    std::memcpy(
        state.memory.data + loadAddress_,
        image.data() + HeaderSize,
        static_cast<std::size_t>(
            imageSize_ - HeaderSize
            )
    );

    loaded_ = true;

    return true;
}

bool Bootloader::start(
    AMD3DC_State& state
)
{
    if (!loaded_ ||
        state.cores == nullptr ||
        state.core_count == 0)
    {
        return false;
    }

    state.cores[0].pc = entryPoint_;
    state.cores[0].halted = false;

    return true;
}

std::uint64_t Bootloader::loadAddress() const noexcept
{
    return loadAddress_;
}

std::uint64_t Bootloader::entryPoint() const noexcept
{
    return entryPoint_;
}

std::uint64_t Bootloader::imageSize() const noexcept
{
    return imageSize_;
}