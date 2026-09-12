#ifndef AMD3DC_GUI_H
#define AMD3DC_GUI_H

#include <stdbool.h>

#include "amd3dc_types.h"

bool amd3dc_gui_init(
    AMD3DC_State* state
);

int amd3dc_gui_run(
    AMD3DC_State* state
);

void amd3dc_gui_shutdown(void);

#endif /* AMD3DC_GUI_H */