#ifndef AMD3DC_BIOS_H
#define AMD3DC_BIOS_H

#include <stdbool.h>

#include "amd3dc_types.h"

bool amd3dc_bios_init(
    AMD3DC_State* state
);

bool amd3dc_bios_post(
    AMD3DC_State* state
);

void amd3dc_bios_print(
    const AMD3DC_State* state
);

void amd3dc_bios_shutdown(
    AMD3DC_State* state
);

bool amd3dc_post(
    AMD3DC_State* state
);

#endif /* AMD3DC_BIOS_H */