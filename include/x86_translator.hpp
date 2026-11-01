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
 * Legacy static translator for a small 8086 subset. This does not implement an
 * x86 CPU and is not used by the floppy boot path; real-mode images execute in
 * X86Compat, the real-mode x86 execution core being extended toward 486
 * compatibility.
 */
X86TranslationResult amd3dc_translate_i8086(
    const std::uint8_t* bytes,
    std::size_t size
);

/* Historical name retained for callers; this still translates only 8086 code. */
X86TranslationResult amd3dc_translate_i386(
    const std::uint8_t* bytes,
    std::size_t size
);

#endif
