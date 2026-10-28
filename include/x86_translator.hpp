#ifndef AMD3DC_X86_TRANSLATOR_HPP
#define AMD3DC_X86_TRANSLATOR_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

extern "C"
{
#include "isa.h"
}

struct X86TranslationResult
{
    std::vector<AMD3DC_Instruction> instructions;
    std::size_t source_bytes = 0;
    std::size_t error_offset = 0;
    std::string error;

    explicit operator bool() const noexcept
    {
        return error.empty();
    }
};

/*
 * Translate a strict 16-bit real-mode subset into AMD 3DC instructions.
 * x86 AX..DI map to AMD 3DC R0..R7 in legacy order.
 */
X86TranslationResult amd3dc_translate_i8086(
    const std::uint8_t* bytes,
    std::size_t size
);

/* Compatibility entry point; now uses the 16-bit real-mode decoder. */
X86TranslationResult amd3dc_translate_i386(
    const std::uint8_t* bytes,
    std::size_t size
);

#endif
