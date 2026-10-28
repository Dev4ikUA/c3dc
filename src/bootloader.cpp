#include "bootloader.hpp"
#include "x86_translator.hpp"

#include <fstream>
#include <vector>
#include <cstring>

namespace
{
    constexpr std::uint64_t RawImageLoadAddress = 0x1000;

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
    loaded_ = false;
    loadAddress_ = 0;
    entryPoint_ = 0;
    imageSize_ = 0;
    payloadSize_ = 0;
    realModeImage_ = false;

    std::ifstream file(
        filename,
        std::ios::binary | std::ios::ate
    );

    if (!file)
        return false;

    const auto size =
        file.tellg();

    if (size <= 0)
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

    const bool hasHeader =
        image.size() >= 4 &&
        image[0] == 'C' && image[1] == '3' &&
        image[2] == 'B' && image[3] == 'L';

    const bool isBootFloppy =
        image.size() == 1474560 &&
        image[510] == 0x55 && image[511] == 0xAA &&
        image[11] == 0x00 && image[12] == 0x02;

    if (isBootFloppy)
    {
        if (!x86_.bootFloppy(state, std::move(image)))
            return false;

        loadAddress_ = 0x7C00;
        entryPoint_ = 0x7C00;
        imageSize_ = 1474560;
        payloadSize_ = 512;
        realModeImage_ = true;
        loaded_ = true;
        return true;
    }

    if (hasHeader)
    {
        if (image.size() < 0x20)
            return false;

        loadAddress_ = readBE64(&image[0x10]);
        entryPoint_ = readBE64(&image[0x08]);
        imageSize_ = readBE64(&image[0x18]);

        if (imageSize_ < 0x20 || imageSize_ > image.size())
            return false;

        payloadSize_ = imageSize_ - 0x20;
        if (entryPoint_ < loadAddress_ ||
            entryPoint_ - loadAddress_ >= payloadSize_)
            return false;
    }
    else
    {
        const X86TranslationResult translated =
            amd3dc_translate_i8086(image.data(), image.size());

        if (!translated)
            return false;

        loadAddress_ = RawImageLoadAddress;
        entryPoint_ = RawImageLoadAddress;
        payloadSize_ = translated.instructions.size() * sizeof(std::uint64_t);
        imageSize_ = payloadSize_;

        if (state.memory.data == nullptr ||
            loadAddress_ > state.memory.size ||
            payloadSize_ > state.memory.size - loadAddress_)
        {
            return false;
        }

        for (std::size_t i = 0; i < translated.instructions.size(); ++i)
        {
            const std::uint64_t raw =
                amd3dc_encode64(&translated.instructions[i]);
            const std::size_t address =
                static_cast<std::size_t>(loadAddress_) +
                i * sizeof(raw);

            for (std::size_t byte = 0; byte < sizeof(raw); ++byte)
            {
                state.memory.data[address + byte] =
                    static_cast<std::uint8_t>(
                        raw >> ((sizeof(raw) - 1 - byte) * 8));
            }
        }

        loaded_ = true;
        return true;
    }

    if (state.memory.data == nullptr ||
        loadAddress_ > state.memory.size ||
        payloadSize_ >
            state.memory.size - loadAddress_)
    {
        return false;
    }

    std::memcpy(
        state.memory.data + loadAddress_,
        image.data() + (hasHeader ? 0x20 : 0),
        static_cast<std::size_t>(payloadSize_)
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
    state.cores[0].halted = realModeImage_;

    if (realModeImage_)
        return true;

    state.cores[0].halted = false;

    return true;
}

bool Bootloader::isRealModeImage() const noexcept
{
    return realModeImage_;
}

bool Bootloader::stepRealMode()
{
    return realModeImage_ && x86_.step();
}

void Bootloader::renderRealModeDisplay()
{
    if (realModeImage_)
        x86_.renderVideo();
}

const std::string& Bootloader::realModeError() const noexcept
{
    return x86_.error();
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
