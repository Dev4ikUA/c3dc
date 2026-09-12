#ifndef BIOS_UI_H
#define BIOS_UI_H

#include "amd3dc_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Display dimensions */
#define BIOS_UI_WIDTH   1024
#define BIOS_UI_HEIGHT  768

/* Colors (ARGB) */
#define COLOR_BLACK     0xFF000000
#define COLOR_WHITE     0xFFFFFFFF
#define COLOR_BLUE      0xFF0066CC
#define COLOR_CYAN      0xFF00FFFF
#define COLOR_GREEN     0xFF00FF00
#define COLOR_GRAY      0xFF808080
#define COLOR_DARK_GRAY 0xFF404040

/* Font dimensions (monospace) */
#define CHAR_WIDTH      8
#define CHAR_HEIGHT     16

typedef enum
{
    BIOS_PAGE_MAIN,
    BIOS_PAGE_CPU,
    BIOS_PAGE_MEMORY,
    BIOS_PAGE_CACHE,
    BIOS_PAGE_GPU,
    BIOS_PAGE_QUANTUM,
    BIOS_PAGE_BOOT,
    BIOS_PAGE_SAVE_EXIT,
    BIOS_PAGE_COUNT
} BiosPageType;

typedef struct
{
    AMD3DC_State* state;
    AMD3DC_Virtio* virtio;
    BiosPageType current_page;
    int selected_item;
    bool running;
    uint32_t frame_counter;
} BiosUI;

/* Initialization and cleanup */
bool bios_ui_init(BiosUI* ui, AMD3DC_State* state);
void bios_ui_destroy(BiosUI* ui);

/* Main update loop */
void bios_ui_render(BiosUI* ui);
void bios_ui_handle_input(BiosUI* ui);

/* Navigation */
void bios_ui_next_page(BiosUI* ui);
void bios_ui_prev_page(BiosUI* ui);

#ifdef __cplusplus
}
#endif

#endif /* BIOS_UI_H */