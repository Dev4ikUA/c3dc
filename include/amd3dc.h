#ifndef AMD3DC_H
#define AMD3DC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "amd3dc_types.h"
#include "isa.h"

bool amd3dc_execute64(
    AMD3DC_State* state,
    const AMD3DC_Instruction* instruction
);

bool amd3dc_init(
    AMD3DC_State* state,
    size_t core_count,
    size_t segment_count,
    size_t memory_size
);

void amd3dc_reset(
    AMD3DC_State* state
);

void amd3dc_destroy(
    AMD3DC_State* state
);

uint64_t amd3dc_fetch64(
    AMD3DC_State* state
);

bool amd3dc_post(
    AMD3DC_State* state
);

#endif /* AMD3DC_H */