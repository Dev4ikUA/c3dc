#include "bios_ui.h"

#include "igpu.h"
#include "gui.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

/*
 * AMD 3DC BIOS Setup UI
 *
 * Renders directly into AMD3DC_Virtio.queue_data.
 *
 * The UI is resolution-independent and uses the actual VirtIO
 * framebuffer dimensions. The current default VM framebuffer is
 * 640x480, giving us an 80x30 text grid at 8x16 cells.
 */

 /* ------------------------------------------------------------------------- */
 /* Small 5x7 BIOS font                                                      */
 /* ------------------------------------------------------------------------- */

static const uint8_t font_digits[10][7] =
{
    {14,17,19,21,25,17,14},
    {4,12,4,4,4,4,14},
    {14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30},
    {2,6,10,18,31,2,2},
    {31,16,16,30,1,1,30},
    {6,8,16,30,17,17,14},
    {31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14},
    {14,17,17,15,1,2,12}
};

static const uint8_t font_upper[26][7] =
{
    {14,17,17,31,17,17,17}, /* A */
    {30,17,17,30,17,17,30}, /* B */
    {14,17,16,16,16,17,14}, /* C */
    {30,17,17,17,17,17,30}, /* D */
    {31,16,16,30,16,16,31}, /* E */
    {31,16,16,30,16,16,16}, /* F */
    {14,17,16,23,17,17,14}, /* G */
    {17,17,17,31,17,17,17}, /* H */
    {14,4,4,4,4,4,14},      /* I */
    {1,1,1,1,17,17,14},      /* J */
    {17,18,20,24,20,18,17}, /* K */
    {16,16,16,16,16,16,31}, /* L */
    {17,27,21,21,17,17,17}, /* M */
    {17,25,21,19,17,17,17}, /* N */
    {14,17,17,17,17,17,14}, /* O */
    {30,17,17,30,16,16,16}, /* P */
    {14,17,17,17,21,18,13}, /* Q */
    {30,17,17,30,20,18,17}, /* R */
    {15,16,16,14,1,1,30},    /* S */
    {31,4,4,4,4,4,4},       /* T */
    {17,17,17,17,17,17,14}, /* U */
    {17,17,17,17,17,10,4},  /* V */
    {17,17,17,21,21,21,10}, /* W */
    {17,17,10,4,10,17,17},  /* X */
    {17,17,10,4,4,4,4},     /* Y */
    {31,1,2,4,8,16,31}      /* Z */
};

/* ------------------------------------------------------------------------- */
/* Helpers                                                                  */
/* ------------------------------------------------------------------------- */

static uint32_t bios_ui_width(const BiosUI* ui)
{
    return ui && ui->virtio ? ui->virtio->width : 0;
}

static uint32_t bios_ui_height(const BiosUI* ui)
{
    return ui && ui->virtio ? ui->virtio->height : 0;
}

static void draw_pixel(
    BiosUI* ui,
    uint32_t x,
    uint32_t y,
    uint32_t color
)
{
    if (!ui || !ui->virtio)
        return;

    amd3dc_virtio_put_pixel(
        ui->virtio,
        x,
        y,
        color
    );
}

static void draw_rect(
    BiosUI* ui,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height,
    uint32_t color
)
{
    uint32_t yy;

    if (!ui || !ui->virtio)
        return;

    for (yy = 0; yy < height; ++yy)
    {
        uint32_t xx;

        if (y + yy >= bios_ui_height(ui))
            break;

        for (xx = 0; xx < width; ++xx)
        {
            if (x + xx >= bios_ui_width(ui))
                break;

            draw_pixel(
                ui,
                x + xx,
                y + yy,
                color
            );
        }
    }
}

static void draw_char(
    BiosUI* ui,
    uint32_t x,
    uint32_t y,
    char character,
    uint32_t color
)
{
    const uint8_t* glyph = NULL;
    uint8_t temporary[7];
    uint32_t row;

    if (!ui)
        return;

    memset(temporary, 0, sizeof(temporary));

    if (character >= '0' && character <= '9')
    {
        glyph = font_digits[character - '0'];
    }
    else
    {
        char upper = character;

        if (upper >= 'a' && upper <= 'z')
            upper = (char)(upper - 'a' + 'A');

        if (upper >= 'A' && upper <= 'Z')
            glyph = font_upper[upper - 'A'];
    }

    if (glyph)
    {
        for (row = 0; row < 7; ++row)
        {
            uint32_t bit;

            for (bit = 0; bit < 5; ++bit)
            {
                if (glyph[row] & (1u << (4u - bit)))
                {
                    draw_pixel(
                        ui,
                        x + bit,
                        y + row,
                        color
                    );
                }
            }
        }

        return;
    }

    /*
     * Punctuation and simple BIOS symbols.
     */
    switch (character)
    {
    case ' ':
        return;

    case '.':
        draw_rect(ui, x + 2, y + 6, 2, 1, color);
        return;

    case ':':
        draw_rect(ui, x + 2, y + 1, 1, 1, color);
        draw_rect(ui, x + 2, y + 5, 1, 1, color);
        return;

    case '-':
        draw_rect(ui, x + 1, y + 3, 4, 1, color);
        return;

    case '_':
        draw_rect(ui, x, y + 7, 5, 1, color);
        return;

    case '/':
        draw_pixel(ui, x + 4, y, color);
        draw_pixel(ui, x + 3, y + 1, color);
        draw_pixel(ui, x + 2, y + 2, color);
        draw_pixel(ui, x + 1, y + 3, color);
        draw_pixel(ui, x, y + 4, color);
        return;

    case '\\':
        draw_pixel(ui, x, y, color);
        draw_pixel(ui, x + 1, y + 1, color);
        draw_pixel(ui, x + 2, y + 2, color);
        draw_pixel(ui, x + 3, y + 3, color);
        draw_pixel(ui, x + 4, y + 4, color);
        return;

    case '=':
        draw_rect(ui, x, y + 2, 5, 1, color);
        draw_rect(ui, x, y + 5, 5, 1, color);
        return;

    case '+':
        draw_rect(ui, x + 2, y + 1, 1, 5, color);
        draw_rect(ui, x, y + 3, 5, 1, color);
        return;

    case '%':
        draw_pixel(ui, x, y + 1, color);
        draw_pixel(ui, x + 1, y, color);
        draw_pixel(ui, x + 3, y + 6, color);
        draw_pixel(ui, x + 4, y + 5, color);
        draw_pixel(ui, x + 1, y + 5, color);
        draw_pixel(ui, x + 3, y + 1, color);
        return;

    case '[':
        draw_rect(ui, x + 1, y, 3, 1, color);
        draw_rect(ui, x, y, 1, 7, color);
        draw_rect(ui, x + 1, y + 6, 3, 1, color);
        return;

    case ']':
        draw_rect(ui, x + 1, y, 3, 1, color);
        draw_rect(ui, x + 4, y, 1, 7, color);
        draw_rect(ui, x + 1, y + 6, 3, 1, color);
        return;

    case '(':
        draw_pixel(ui, x + 2, y, color);
        draw_pixel(ui, x + 1, y + 1, color);
        draw_pixel(ui, x, y + 2, color);
        draw_pixel(ui, x, y + 4, color);
        draw_pixel(ui, x + 1, y + 5, color);
        draw_pixel(ui, x + 2, y + 6, color);
        return;

    case ')':
        draw_pixel(ui, x + 2, y, color);
        draw_pixel(ui, x + 3, y + 1, color);
        draw_pixel(ui, x + 4, y + 2, color);
        draw_pixel(ui, x + 4, y + 4, color);
        draw_pixel(ui, x + 3, y + 5, color);
        draw_pixel(ui, x + 2, y + 6, color);
        return;

    case '>':
        draw_pixel(ui, x, y + 1, color);
        draw_pixel(ui, x + 1, y + 2, color);
        draw_pixel(ui, x + 2, y + 3, color);
        draw_pixel(ui, x + 1, y + 4, color);
        draw_pixel(ui, x, y + 5, color);
        return;

    case '<':
        draw_pixel(ui, x + 2, y + 1, color);
        draw_pixel(ui, x + 1, y + 2, color);
        draw_pixel(ui, x, y + 3, color);
        draw_pixel(ui, x + 1, y + 4, color);
        draw_pixel(ui, x + 2, y + 5, color);
        return;

    case '#':
        draw_rect(ui, x + 1, y, 1, 7, color);
        draw_rect(ui, x + 3, y, 1, 7, color);
        draw_rect(ui, x, y + 2, 5, 1, color);
        draw_rect(ui, x, y + 5, 5, 1, color);
        return;

    default:
        break;
    }

    /*
     * Unknown characters are rendered as a small placeholder.
     */
    draw_rect(ui, x, y, 5, 1, color);
    draw_rect(ui, x, y + 6, 5, 1, color);
    draw_rect(ui, x, y, 1, 7, color);
    draw_rect(ui, x + 4, y, 1, 7, color);
}

static void draw_string(
    BiosUI* ui,
    uint32_t x,
    uint32_t y,
    const char* text,
    uint32_t color
)
{
    if (!ui || !text)
        return;

    while (*text)
    {
        if (x + 5 >= bios_ui_width(ui))
            break;

        draw_char(
            ui,
            x,
            y,
            *text,
            color
        );

        x += CHAR_WIDTH;
        ++text;
    }
}

static void clear_screen(
    BiosUI* ui
)
{
    draw_rect(
        ui,
        0,
        0,
        bios_ui_width(ui),
        bios_ui_height(ui),
        COLOR_BLACK
    );
}

static void draw_box(
    BiosUI* ui,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height,
    uint32_t color
)
{
    if (width < 2 || height < 2)
        return;

    draw_rect(ui, x, y, width, 1, color);
    draw_rect(ui, x, y + height - 1, width, 1, color);
    draw_rect(ui, x, y, 1, height, color);
    draw_rect(ui, x + width - 1, y, 1, height, color);
}

static void draw_separator(
    BiosUI* ui,
    uint32_t y
)
{
    draw_rect(
        ui,
        8,
        y,
        bios_ui_width(ui) > 16 ? bios_ui_width(ui) - 16 : 0,
        1,
        COLOR_DARK_GRAY
    );
}

/* ------------------------------------------------------------------------- */
/* Header / footer                                                           */
/* ------------------------------------------------------------------------- */

static void draw_header(
    BiosUI* ui
)
{
    const uint32_t width = bios_ui_width(ui);

    draw_rect(
        ui,
        0,
        0,
        width,
        48,
        COLOR_BLUE
    );

    draw_string(
        ui,
        16,
        8,
        "AMD 3DC BIOS SETUP UTILITY V0.1",
        COLOR_WHITE
    );

    draw_string(
        ui,
        16,
        28,
        "CELLARIUM 3DC FIRMWARE",
        COLOR_CYAN
    );
}

static void draw_tabs(
    BiosUI* ui
)
{
    static const char* tabs[] =
    {
        "MAIN",
        "CPU",
        "MEMORY",
        "CACHE",
        "GPU",
        "QUANTUM",
        "BOOT",
        "SAVE/EXIT"
    };

    static const BiosPageType pages[] =
    {
        BIOS_PAGE_MAIN,
        BIOS_PAGE_CPU,
        BIOS_PAGE_MEMORY,
        BIOS_PAGE_CACHE,
        BIOS_PAGE_GPU,
        BIOS_PAGE_QUANTUM,
        BIOS_PAGE_BOOT,
        BIOS_PAGE_SAVE_EXIT
    };

    uint32_t x = 8;
    size_t i;

    for (i = 0; i < sizeof(tabs) / sizeof(tabs[0]); ++i)
    {
        uint32_t width =
            (uint32_t)(strlen(tabs[i]) * CHAR_WIDTH + 12);

        if (x + width >= bios_ui_width(ui))
            break;

        if (ui->current_page == pages[i])
        {
            draw_rect(
                ui,
                x,
                52,
                width,
                20,
                COLOR_CYAN
            );

            draw_string(
                ui,
                x + 6,
                58,
                tabs[i],
                COLOR_BLACK
            );
        }
        else
        {
            draw_string(
                ui,
                x + 6,
                58,
                tabs[i],
                COLOR_GRAY
            );
        }

        x += width + 2;
    }
}

static void draw_footer(
    BiosUI* ui
)
{
    const uint32_t height = bios_ui_height(ui);

    if (height < 24)
        return;

    draw_separator(ui, height - 28);

    draw_string(
        ui,
        8,
        height - 20,
        "TAB/NEXT  LEFT/RIGHT  UP/DOWN  ENTER  ESC/F10 EXIT",
        COLOR_GRAY
    );
}

/* ------------------------------------------------------------------------- */
/* Pages                                                                     */
/* ------------------------------------------------------------------------- */

static void draw_main_page(
    BiosUI* ui
)
{
    AMD3DC_State* state = ui->state;

    draw_box(
        ui,
        8,
        82,
        bios_ui_width(ui) - 16,
        250,
        COLOR_GRAY
    );

    draw_string(
        ui,
        20,
        94,
        "SYSTEM INFORMATION",
        COLOR_CYAN
    );

    draw_separator(ui, 112);

    draw_string(ui, 20, 126, "CPU          : AMD 3DC CELLARIUM", COLOR_WHITE);
    draw_string(ui, 20, 150, "ARCHITECTURE : 64-BIT BIG ENDIAN", COLOR_WHITE);

    if (state)
    {
        char buffer[64];

        snprintf(
            buffer,
            sizeof(buffer),
            "CORES        : %u",
            state->core_count
        );
        draw_string(ui, 20, 174, buffer, COLOR_WHITE);

        snprintf(
            buffer,
            sizeof(buffer),
            "SEGMENTS     : %u",
            state->segment_count
        );
        draw_string(ui, 20, 198, buffer, COLOR_WHITE);

        draw_string(
            ui,
            20,
            222,
            "HBM3 MEMORY   : 24 GIB",
            COLOR_WHITE
        );

        draw_string(
            ui,
            20,
            246,
            "VIRTIO GPU    : ENABLED",
            COLOR_GREEN
        );
    }

    draw_box(
        ui,
        8,
        348,
        bios_ui_width(ui) - 16,
        56,
        COLOR_DARK_GRAY
    );

    draw_string(
        ui,
        20,
        366,
        "AMD 3DC CELLARIUM FIRMWARE",
        COLOR_WHITE
    );

    draw_string(
        ui,
        20,
        386,
        "BIOS SETUP READY",
        COLOR_GREEN
    );
}

static void draw_cpu_page(
    BiosUI* ui
)
{
    AMD3DC_State* state = ui->state;
    char buffer[64];

    draw_box(ui, 8, 82, bios_ui_width(ui) - 16, 300, COLOR_GRAY);

    draw_string(ui, 20, 94, "CPU CONFIGURATION", COLOR_CYAN);
    draw_separator(ui, 112);

    draw_string(ui, 20, 128, "PROCESSOR : AMD 3DC CELLARIUM", COLOR_WHITE);
    draw_string(ui, 20, 152, "MODE      : 64-BIT BIG ENDIAN", COLOR_WHITE);

    if (state)
    {
        snprintf(
            buffer,
            sizeof(buffer),
            "CORES     : %u",
            state->core_count
        );

        draw_string(ui, 20, 176, buffer, COLOR_WHITE);
    }

    draw_string(ui, 20, 200, "L1 CACHE  : ENABLED", COLOR_GREEN);
    draw_string(ui, 20, 224, "L2 CACHE  : ENABLED", COLOR_GREEN);
    draw_string(ui, 20, 248, "L3 CACHE  : ENABLED", COLOR_GREEN);
    draw_string(ui, 20, 272, "L4 CACHE  : ENABLED", COLOR_GREEN);
}

static void draw_memory_page(
    BiosUI* ui
)
{
    AMD3DC_State* state = ui->state;
    char buffer[64];

    draw_box(ui, 8, 82, bios_ui_width(ui) - 16, 300, COLOR_GRAY);

    draw_string(ui, 20, 94, "MEMORY CONFIGURATION", COLOR_CYAN);
    draw_separator(ui, 112);

    draw_string(ui, 20, 128, "MEMORY TYPE : HBM3", COLOR_WHITE);
    draw_string(ui, 20, 152, "CAPACITY    : 24 GIB", COLOR_WHITE);

    if (state)
    {
        uint64_t capacity =
            (uint64_t)state->memory.capacity;

        snprintf(
            buffer,
            sizeof(buffer),
            "CAPACITY BYTES : %llu",
            (unsigned long long)capacity
        );

        draw_string(ui, 20, 176, buffer, COLOR_GRAY);
    }

    draw_string(ui, 20, 208, "CONTROLLER   : ENABLED", COLOR_GREEN);
    draw_string(ui, 20, 232, "ECC          : ENABLED", COLOR_GREEN);
    draw_string(ui, 20, 256, "INTERLEAVING  : ENABLED", COLOR_GREEN);
}

static void draw_cache_page(
    BiosUI* ui
)
{
    draw_box(ui, 8, 82, bios_ui_width(ui) - 16, 300, COLOR_GRAY);

    draw_string(ui, 20, 94, "CACHE CONFIGURATION", COLOR_CYAN);
    draw_separator(ui, 112);

    draw_string(ui, 20, 128, "L1 CACHE : ENABLED", COLOR_GREEN);
    draw_string(ui, 20, 152, "L2 CACHE : ENABLED", COLOR_GREEN);
    draw_string(ui, 20, 176, "L3 CACHE : ENABLED", COLOR_GREEN);
    draw_string(ui, 20, 200, "L4 CACHE : ENABLED", COLOR_GREEN);

    draw_string(ui, 20, 240, "CACHE COHERENCY : ENABLED", COLOR_WHITE);
}

static void draw_gpu_page(
    BiosUI* ui
)
{
    AMD3DC_Virtio* virtio = ui->virtio;
    char buffer[64];

    draw_box(ui, 8, 82, bios_ui_width(ui) - 16, 300, COLOR_GRAY);

    draw_string(ui, 20, 94, "VIRTIO GPU CONFIGURATION", COLOR_CYAN);
    draw_separator(ui, 112);

    draw_string(ui, 20, 128, "STATUS     : ENABLED", COLOR_GREEN);

    if (virtio)
    {
        snprintf(
            buffer,
            sizeof(buffer),
            "RESOLUTION : %ux%u",
            virtio->width,
            virtio->height
        );

        draw_string(ui, 20, 152, buffer, COLOR_WHITE);

        snprintf(
            buffer,
            sizeof(buffer),
            "DEVICE ID  : %u",
            virtio->device_id
        );

        draw_string(ui, 20, 176, buffer, COLOR_WHITE);

        snprintf(
            buffer,
            sizeof(buffer),
            "VENDOR ID  : %u",
            virtio->vendor_id
        );

        draw_string(ui, 20, 200, buffer, COLOR_WHITE);

        snprintf(
            buffer,
            sizeof(buffer),
            "FRAMEBUFFER: %llu KB",
            (unsigned long long)(
                virtio->queue_data_size / 1024u
                )
        );

        draw_string(ui, 20, 224, buffer, COLOR_WHITE);
    }
}

static void draw_quantum_page(
    BiosUI* ui
)
{
    draw_box(ui, 8, 82, bios_ui_width(ui) - 16, 300, COLOR_GRAY);

    draw_string(ui, 20, 94, "QUANTUM / TENSOR SUBSYSTEM", COLOR_CYAN);
    draw_separator(ui, 112);

    draw_string(ui, 20, 128, "QUANTUM DIE   : PRESENT", COLOR_GREEN);
    draw_string(ui, 20, 152, "TENSOR CORES  : ENABLED", COLOR_GREEN);
    draw_string(ui, 20, 176, "QUANTUM CORES  : ENABLED", COLOR_GREEN);
    draw_string(ui, 20, 200, "PQC ENGINES    : ENABLED", COLOR_GREEN);
    draw_string(ui, 20, 224, "QMATMUL        : AVAILABLE", COLOR_WHITE);
}

static void draw_boot_page(
    BiosUI* ui
)
{
    draw_box(ui, 8, 82, bios_ui_width(ui) - 16, 300, COLOR_GRAY);

    draw_string(ui, 20, 94, "BOOT CONFIGURATION", COLOR_CYAN);
    draw_separator(ui, 112);

    draw_string(ui, 20, 128, "BOOT DEVICE : INTERNAL HDD", COLOR_WHITE);
    draw_string(ui, 20, 152, "BOOT MODE   : UEFI", COLOR_WHITE);
    draw_string(ui, 20, 176, "SECURE BOOT : ENABLED", COLOR_GREEN);

    draw_string(ui, 20, 224, "BOOT ORDER", COLOR_CYAN);
    draw_string(ui, 28, 248, "1. INTERNAL HDD", COLOR_WHITE);
    draw_string(ui, 28, 272, "2. NETWORK", COLOR_GRAY);
    draw_string(ui, 28, 296, "3. REMOVABLE MEDIA", COLOR_GRAY);
}

static void draw_save_exit_page(
    BiosUI* ui
)
{
    static const char* options[] =
    {
        "SAVE CHANGES AND EXIT",
        "DISCARD CHANGES AND EXIT",
        "LOAD DEFAULT SETTINGS"
    };

    int i;

    draw_box(ui, 8, 82, bios_ui_width(ui) - 16, 250, COLOR_GRAY);

    draw_string(ui, 20, 94, "SAVE / EXIT", COLOR_CYAN);
    draw_separator(ui, 112);

    for (i = 0; i < 3; ++i)
    {
        uint32_t y =
            136u + (uint32_t)i * 40u;

        if (i == ui->selected_item)
        {
            draw_rect(
                ui,
                20,
                y - 4,
                bios_ui_width(ui) - 40,
                24,
                COLOR_CYAN
            );

            draw_string(
                ui,
                28,
                y,
                options[i],
                COLOR_BLACK
            );
        }
        else
        {
            draw_string(
                ui,
                28,
                y,
                options[i],
                COLOR_WHITE
            );
        }
    }

    draw_string(
        ui,
        20,
        286,
        "ENTER = SELECT",
        COLOR_GRAY
    );
}

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

bool bios_ui_init(
    BiosUI* ui,
    AMD3DC_State* state
)
{
    if (!ui || !state)
        return false;

    memset(
        ui,
        0,
        sizeof(*ui)
    );

    ui->state = state;
    ui->virtio = &state->virtio;

    /*
     * IMPORTANT:
     *
     * Do NOT require 1024x768 here.
     * The actual VM framebuffer is currently 640x480.
     */
    if (!ui->virtio->initialized ||
        !ui->virtio->enabled ||
        !ui->virtio->queue_data ||
        ui->virtio->queue_data_size == 0 ||
        ui->virtio->width == 0 ||
        ui->virtio->height == 0)
    {
        return false;
    }

    ui->current_page = BIOS_PAGE_MAIN;
    ui->selected_item = 0;
    ui->frame_counter = 0;
    ui->running = true;

    return true;
}

void bios_ui_destroy(
    BiosUI* ui
)
{
    if (!ui)
        return;

    ui->running = false;
}

void bios_ui_render(
    BiosUI* ui
)
{
    if (!ui ||
        !ui->running ||
        !ui->virtio ||
        !ui->virtio->queue_data)
    {
        return;
    }

    clear_screen(ui);

    draw_header(ui);
    draw_tabs(ui);

    switch (ui->current_page)
    {
    case BIOS_PAGE_MAIN:
        draw_main_page(ui);
        break;

    case BIOS_PAGE_CPU:
        draw_cpu_page(ui);
        break;

    case BIOS_PAGE_MEMORY:
        draw_memory_page(ui);
        break;

    case BIOS_PAGE_CACHE:
        draw_cache_page(ui);
        break;

    case BIOS_PAGE_GPU:
        draw_gpu_page(ui);
        break;

    case BIOS_PAGE_QUANTUM:
        draw_quantum_page(ui);
        break;

    case BIOS_PAGE_BOOT:
        draw_boot_page(ui);
        break;

    case BIOS_PAGE_SAVE_EXIT:
        draw_save_exit_page(ui);
        break;

    default:
        break;
    }

    draw_footer(ui);

    ++ui->frame_counter;
}

void bios_ui_next_page(
    BiosUI* ui
)
{
    if (!ui)
        return;

    switch (ui->current_page)
    {
    case BIOS_PAGE_MAIN:
        ui->current_page = BIOS_PAGE_CPU;
        break;

    case BIOS_PAGE_CPU:
        ui->current_page = BIOS_PAGE_MEMORY;
        break;

    case BIOS_PAGE_MEMORY:
        ui->current_page = BIOS_PAGE_CACHE;
        break;

    case BIOS_PAGE_CACHE:
        ui->current_page = BIOS_PAGE_GPU;
        break;

    case BIOS_PAGE_GPU:
        ui->current_page = BIOS_PAGE_QUANTUM;
        break;

    case BIOS_PAGE_QUANTUM:
        ui->current_page = BIOS_PAGE_BOOT;
        break;

    case BIOS_PAGE_BOOT:
        ui->current_page = BIOS_PAGE_SAVE_EXIT;
        break;

    case BIOS_PAGE_SAVE_EXIT:
    default:
        ui->current_page = BIOS_PAGE_MAIN;
        break;
    }

    ui->selected_item = 0;
}

void bios_ui_prev_page(
    BiosUI* ui
)
{
    if (!ui)
        return;

    switch (ui->current_page)
    {
    case BIOS_PAGE_MAIN:
        ui->current_page = BIOS_PAGE_SAVE_EXIT;
        break;

    case BIOS_PAGE_CPU:
        ui->current_page = BIOS_PAGE_MAIN;
        break;

    case BIOS_PAGE_MEMORY:
        ui->current_page = BIOS_PAGE_CPU;
        break;

    case BIOS_PAGE_CACHE:
        ui->current_page = BIOS_PAGE_MEMORY;
        break;

    case BIOS_PAGE_GPU:
        ui->current_page = BIOS_PAGE_CACHE;
        break;

    case BIOS_PAGE_QUANTUM:
        ui->current_page = BIOS_PAGE_GPU;
        break;

    case BIOS_PAGE_BOOT:
        ui->current_page = BIOS_PAGE_QUANTUM;
        break;

    case BIOS_PAGE_SAVE_EXIT:
    default:
        ui->current_page = BIOS_PAGE_BOOT;
        break;
    }

    ui->selected_item = 0;
}

void bios_ui_handle_input(
    BiosUI* ui
)
{
    AMD3DC_Key key;

    if (!ui || !ui->running)
        return;

    while (amd3dc_gui_poll_key(&key))
    {
        switch (key)
        {
        case AMD3DC_KEY_ESCAPE:
        case AMD3DC_KEY_F10:
            ui->running = false;
            break;

        case AMD3DC_KEY_TAB:
        case AMD3DC_KEY_RIGHT:
            bios_ui_next_page(ui);
            break;

        case AMD3DC_KEY_LEFT:
            bios_ui_prev_page(ui);
            break;

        case AMD3DC_KEY_UP:
            if (ui->current_page == BIOS_PAGE_SAVE_EXIT)
            {
                if (ui->selected_item > 0)
                    --ui->selected_item;
            }
            break;

        case AMD3DC_KEY_DOWN:
            if (ui->current_page == BIOS_PAGE_SAVE_EXIT)
            {
                if (ui->selected_item < 2)
                    ++ui->selected_item;
            }
            break;

        case AMD3DC_KEY_ENTER:
            if (ui->current_page == BIOS_PAGE_SAVE_EXIT)
            {
                if (ui->selected_item == 0 ||
                    ui->selected_item == 1)
                {
                    ui->running = false;
                }
                else if (ui->selected_item == 2)
                {
                    ui->selected_item = 0;
                }
            }
            break;

        default:
            break;
        }
    }
}