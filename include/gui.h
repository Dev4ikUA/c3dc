#ifndef AMD3DC_GUI_H
#define AMD3DC_GUI_H

#include <stdbool.h>

#include "amd3dc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum AMD3DC_Key
    {
        AMD3DC_KEY_NONE = 0,

        AMD3DC_KEY_UP,
        AMD3DC_KEY_DOWN,
        AMD3DC_KEY_LEFT,
        AMD3DC_KEY_RIGHT,

        AMD3DC_KEY_ENTER,
        AMD3DC_KEY_ESCAPE,
        AMD3DC_KEY_TAB,

        AMD3DC_KEY_F1,
        AMD3DC_KEY_F2,
        AMD3DC_KEY_F10,
        AMD3DC_KEY_F5,
        AMD3DC_KEY_F8,

        AMD3DC_KEY_CHAR_BASE = 256

    } AMD3DC_Key;


    /* GUI lifecycle */

    bool amd3dc_gui_init(
        AMD3DC_State* state
    );

    void amd3dc_gui_destroy(void);

    bool amd3dc_gui_render(void);

    int amd3dc_gui_run(
        AMD3DC_State* state
    );

    void amd3dc_gui_shutdown(void);


    /* Guest keyboard input */

    bool amd3dc_gui_poll_key(
        AMD3DC_Key* key
    );

#ifdef __cplusplus
}
#endif

#endif
