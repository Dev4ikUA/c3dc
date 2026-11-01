#include "igpu.h"
#include "igpu.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define AMD3DC_FONT_WIDTH       5u
#define AMD3DC_FONT_HEIGHT      7u
#define AMD3DC_FONT_SCALE       2u
#define AMD3DC_FONT_ADVANCE    12u
#define AMD3DC_FONT_LINE_HEIGHT 16u

#define AMD3DC_SCREEN_BG        0x00101818u
#define AMD3DC_SCREEN_GREEN     0x0000FF66u
#define AMD3DC_SCREEN_WHITE     0x00E0E0E0u
#define AMD3DC_SCREEN_DIM       0x00608080u
#define AMD3DC_SCREEN_CYAN      0x0000FFFFu

bool amd3dc_virtio_init(
    AMD3DC_Virtio* virtio,
    uint32_t width,
    uint32_t height)
{
    size_t pixel_count;

    if (!virtio || width == 0 || height == 0)
        return false;

    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / 4u)
        return false;

    memset(virtio, 0, sizeof(*virtio));

    virtio->width = width;
    virtio->height = height;
    virtio->queue_data_size = pixel_count * 4u;
    virtio->queue_data = (uint8_t*)malloc(virtio->queue_data_size);

    if (!virtio->queue_data)
        return false;

    memset(virtio->queue_data, 0, virtio->queue_data_size);

    virtio->initialized = true;
    virtio->enabled = true;

    return true;
}

void amd3dc_virtio_destroy(
    AMD3DC_Virtio* virtio)
{
    if (!virtio)
        return;

    if (virtio->queue_data)
    {
        free(virtio->queue_data);
        virtio->queue_data = NULL;
    }

    virtio->queue_data_size = 0;
    virtio->initialized = false;
    virtio->enabled = false;
}

void amd3dc_virtio_render_system_screen(
    AMD3DC_Virtio* virtio)
{
    if (!virtio || !virtio->enabled || !virtio->queue_data)
        return;

    /* Render implementation - fill framebuffer or update display */
    /* This is a placeholder for the actual rendering logic */
}

static const uint8_t*
amd3dc_font_glyph(
    char character)
{
    static const uint8_t blank[7] =
    {
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00
    };

    static const uint8_t glyph_0[7] =
    {
        0x0E,
        0x11,
        0x13,
        0x15,
        0x19,
        0x11,
        0x0E
    };

    static const uint8_t glyph_1[7] =
    {
        0x04,
        0x0C,
        0x04,
        0x04,
        0x04,
        0x04,
        0x0E
    };

    static const uint8_t glyph_2[7] =
    {
        0x0E,
        0x11,
        0x01,
        0x02,
        0x04,
        0x08,
        0x1F
    };

    static const uint8_t glyph_3[7] =
    {
        0x1E,
        0x01,
        0x01,
        0x0E,
        0x01,
        0x01,
        0x1E
    };

    static const uint8_t glyph_4[7] =
    {
        0x02,
        0x06,
        0x0A,
        0x12,
        0x1F,
        0x02,
        0x02
    };

    static const uint8_t glyph_5[7] =
    {
        0x1F,
        0x10,
        0x10,
        0x1E,
        0x01,
        0x01,
        0x1E
    };

    static const uint8_t glyph_6[7] =
    {
        0x06,
        0x08,
        0x10,
        0x1E,
        0x11,
        0x11,
        0x0E
    };

    static const uint8_t glyph_7[7] =
    {
        0x1F,
        0x01,
        0x02,
        0x04,
        0x08,
        0x08,
        0x08
    };

    static const uint8_t glyph_8[7] =
    {
        0x0E,
        0x11,
        0x11,
        0x0E,
        0x11,
        0x11,
        0x0E
    };

    static const uint8_t glyph_9[7] =
    {
        0x0E,
        0x11,
        0x11,
        0x0F,
        0x01,
        0x02,
        0x0C
    };

    static const uint8_t glyph_a[7] =
    {
        0x0E,
        0x11,
        0x11,
        0x1F,
        0x11,
        0x11,
        0x11
    };

    static const uint8_t glyph_b[7] =
    {
        0x1E,
        0x11,
        0x11,
        0x1E,
        0x11,
        0x11,
        0x1E
    };

    static const uint8_t glyph_c[7] =
    {
        0x0E,
        0x11,
        0x10,
        0x10,
        0x10,
        0x11,
        0x0E
    };

    static const uint8_t glyph_d[7] =
    {
        0x1E,
        0x11,
        0x11,
        0x11,
        0x11,
        0x11,
        0x1E
    };

    static const uint8_t glyph_e[7] =
    {
        0x1F,
        0x10,
        0x10,
        0x1E,
        0x10,
        0x10,
        0x1F
    };

    static const uint8_t glyph_f[7] =
    {
        0x1F,
        0x10,
        0x10,
        0x1E,
        0x10,
        0x10,
        0x10
    };

    static const uint8_t glyph_g[7] =
    {
        0x0E,
        0x11,
        0x10,
        0x17,
        0x11,
        0x11,
        0x0F
    };

    static const uint8_t glyph_h[7] =
    {
        0x11,
        0x11,
        0x11,
        0x1F,
        0x11,
        0x11,
        0x11
    };

    static const uint8_t glyph_i[7] =
    {
        0x0E,
        0x04,
        0x04,
        0x04,
        0x04,
        0x04,
        0x0E
    };

    static const uint8_t glyph_j[7] =
    {
        0x01,
        0x01,
        0x01,
        0x01,
        0x11,
        0x11,
        0x0E
    };

    static const uint8_t glyph_k[7] =
    {
        0x11,
        0x12,
        0x14,
        0x18,
        0x14,
        0x12,
        0x11
    };

    static const uint8_t glyph_l[7] =
    {
        0x10,
        0x10,
        0x10,
        0x10,
        0x10,
        0x10,
        0x1F
    };

    static const uint8_t glyph_m[7] =
    {
        0x11,
        0x1B,
        0x15,
        0x15,
        0x11,
        0x11,
        0x11
    };

    static const uint8_t glyph_n[7] =
    {
        0x11,
        0x19,
        0x15,
        0x13,
        0x11,
        0x11,
        0x11
    };

    static const uint8_t glyph_o[7] =
    {
        0x0E,
        0x11,
        0x11,
        0x11,
        0x11,
        0x11,
        0x0E
    };

    static const uint8_t glyph_p[7] =
    {
        0x1E,
        0x11,
        0x11,
        0x1E,
        0x10,
        0x10,
        0x10
    };

    static const uint8_t glyph_q[7] =
    {
        0x0E,
        0x11,
        0x11,
        0x11,
        0x15,
        0x12,
        0x0D
    };

    static const uint8_t glyph_r[7] =
    {
        0x1E,
        0x11,
        0x11,
        0x1E,
        0x14,
        0x12,
        0x11
    };

    static const uint8_t glyph_s[7] =
    {
        0x0F,
        0x10,
        0x10,
        0x0E,
        0x01,
        0x01,
        0x1E
    };

    static const uint8_t glyph_t[7] =
    {
        0x1F,
        0x04,
        0x04,
        0x04,
        0x04,
        0x04,
        0x04
    };

    static const uint8_t glyph_u[7] =
    {
        0x11,
        0x11,
        0x11,
        0x11,
        0x11,
        0x11,
        0x0E
    };

    static const uint8_t glyph_v[7] =
    {
        0x11,
        0x11,
        0x11,
        0x11,
        0x11,
        0x0A,
        0x04
    };

    static const uint8_t glyph_w[7] =
    {
        0x11,
        0x11,
        0x11,
        0x15,
        0x15,
        0x1B,
        0x11
    };

    static const uint8_t glyph_x[7] =
    {
        0x11,
        0x11,
        0x0A,
        0x04,
        0x0A,
        0x11,
        0x11
    };

    static const uint8_t glyph_y[7] =
    {
        0x11,
        0x11,
        0x0A,
        0x04,
        0x04,
        0x04,
        0x04
    };

    static const uint8_t glyph_z[7] =
    {
        0x1F,
        0x01,
        0x02,
        0x04,
        0x08,
        0x10,
        0x1F
    };

    static const uint8_t glyph_colon[7] =
    {
        0x00,
        0x04,
        0x04,
        0x00,
        0x04,
        0x04,
        0x00
    };

    static const uint8_t glyph_dot[7] =
    {
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x0C,
        0x0C
    };

    static const uint8_t glyph_dash[7] =
    {
        0x00,
        0x00,
        0x00,
        0x1F,
        0x00,
        0x00,
        0x00
    };

    static const uint8_t glyph_slash[7] =
    {
        0x01,
        0x02,
        0x02,
        0x04,
        0x08,
        0x08,
        0x10
    };

    static const uint8_t glyph_equal[7] =
    {
        0x00,
        0x1F,
        0x00,
        0x1F,
        0x00,
        0x00,
        0x00
    };

    static const uint8_t glyph_bracket_left[7] =
    {
        0x0E,
        0x08,
        0x08,
        0x08,
        0x08,
        0x08,
        0x0E
    };

    static const uint8_t glyph_bracket_right[7] =
    {
        0x0E,
        0x02,
        0x02,
        0x02,
        0x02,
        0x02,
        0x0E
    };

    static const uint8_t glyph_percent[7] =
    {
        0x19,
        0x19,
        0x02,
        0x04,
        0x08,
        0x13,
        0x13
    };

    static const uint8_t glyph_hash[7] =
    {
        0x0A,
        0x1F,
        0x0A,
        0x0A,
        0x1F,
        0x0A,
        0x00
    };

    static const struct { char ch; uint8_t rows[7]; } extra[] =
    {
        { '\\', { 0x10, 0x08, 0x08, 0x04, 0x02, 0x02, 0x01 } },
        { '>',  { 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08 } },
        { '<',  { 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02 } },
        { '_',  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F } },
        { '(',  { 0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02 } },
        { ')',  { 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08 } },
        { ',',  { 0x00, 0x00, 0x00, 0x00, 0x0C, 0x04, 0x08 } },
        { '\'', { 0x0C, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00 } },
        { '"',  { 0x0A, 0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00 } },
        { '!',  { 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04 } },
        { '?',  { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04 } },
        { ';',  { 0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x04, 0x08 } },
        { '+',  { 0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00 } },
        { '*',  { 0x00, 0x15, 0x0E, 0x1F, 0x0E, 0x15, 0x00 } },
        { '$',  { 0x04, 0x0F, 0x14, 0x0E, 0x05, 0x1E, 0x04 } },
        { '@',  { 0x0E, 0x11, 0x17, 0x15, 0x17, 0x10, 0x0E } },
        { '&',  { 0x0C, 0x12, 0x14, 0x08, 0x15, 0x12, 0x0D } },
        { '|',  { 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } },
        { '{',  { 0x02, 0x04, 0x04, 0x08, 0x04, 0x04, 0x02 } },
        { '}',  { 0x08, 0x04, 0x04, 0x02, 0x04, 0x04, 0x08 } },
        { '~',  { 0x00, 0x00, 0x08, 0x15, 0x02, 0x00, 0x00 } },
        { '^',  { 0x04, 0x0A, 0x11, 0x00, 0x00, 0x00, 0x00 } },
        { '`',  { 0x08, 0x04, 0x02, 0x00, 0x00, 0x00, 0x00 } },
    };

    switch (character)
    {
    case '0': return glyph_0;
    case '1': return glyph_1;
    case '2': return glyph_2;
    case '3': return glyph_3;
    case '4': return glyph_4;
    case '5': return glyph_5;
    case '6': return glyph_6;
    case '7': return glyph_7;
    case '8': return glyph_8;
    case '9': return glyph_9;

    case 'A':
    case 'a': return glyph_a;

    case 'B':
    case 'b': return glyph_b;

    case 'C':
    case 'c': return glyph_c;

    case 'D':
    case 'd': return glyph_d;

    case 'E':
    case 'e': return glyph_e;

    case 'F':
    case 'f': return glyph_f;

    case 'G':
    case 'g': return glyph_g;

    case 'H':
    case 'h': return glyph_h;

    case 'I':
    case 'i': return glyph_i;

    case 'J':
    case 'j': return glyph_j;

    case 'K':
    case 'k': return glyph_k;

    case 'L':
    case 'l': return glyph_l;

    case 'M':
    case 'm': return glyph_m;

    case 'N':
    case 'n': return glyph_n;

    case 'O':
    case 'o': return glyph_o;

    case 'P':
    case 'p': return glyph_p;

    case 'Q':
    case 'q': return glyph_q;

    case 'R':
    case 'r': return glyph_r;

    case 'S':
    case 's': return glyph_s;

    case 'T':
    case 't': return glyph_t;

    case 'U':
    case 'u': return glyph_u;

    case 'V':
    case 'v': return glyph_v;

    case 'W':
    case 'w': return glyph_w;

    case 'X':
    case 'x': return glyph_x;

    case 'Y':
    case 'y': return glyph_y;

    case 'Z':
    case 'z': return glyph_z;

    case ':': return glyph_colon;
    case '.': return glyph_dot;
    case '-': return glyph_dash;
    case '/': return glyph_slash;
    case '=': return glyph_equal;
    case '[': return glyph_bracket_left;
    case ']': return glyph_bracket_right;
    case '%': return glyph_percent;
    case '#': return glyph_hash;

    default:
        for (size_t i = 0; i < sizeof(extra) / sizeof(extra[0]); ++i)
            if (extra[i].ch == character)
                return extra[i].rows;
        return blank;
    }
}

void amd3dc_virtio_render_text_mode(
    AMD3DC_Virtio* virtio,
    const uint8_t* video_memory)
{
    static const uint32_t palette[16] =
    {
        0xFF000000u, 0xFF0000AAu, 0xFF00AA00u, 0xFF00AAAAu,
        0xFFAA0000u, 0xFFAA00AAu, 0xFFAA5500u, 0xFFAAAAAAu,
        0xFF555555u, 0xFF5555FFu, 0xFF55FF55u, 0xFF55FFFFu,
        0xFFFF5555u, 0xFFFF55FFu, 0xFFFFFF55u, 0xFFFFFFFFu
    };

    if (virtio == NULL || video_memory == NULL ||
        !virtio->enabled || virtio->queue_data == NULL)
        return;

    memset(virtio->queue_data, 0, virtio->queue_data_size);

    for (uint32_t row = 0; row < 25; ++row)
    {
        for (uint32_t column = 0; column < 80; ++column)
        {
            const size_t cell = (size_t)(row * 80 + column) * 2;
            const uint8_t character = video_memory[cell];
            const uint8_t attribute = video_memory[cell + 1];
            const uint32_t foreground = palette[attribute & 0x0F];
            const uint32_t background = palette[(attribute >> 4) & 0x0F];
            const uint8_t* glyph = amd3dc_font_glyph(
                character >= 0x20 && character < 0x7F
                ? (char)character : ' ');

            for (uint32_t y = 0; y < 16; ++y)
            {
                const int glyph_row = (int)y - 4;
                const uint8_t row_bits =
                    glyph_row >= 0 && glyph_row < 7 ? glyph[glyph_row] : 0;
                for (uint32_t x = 0; x < 8; ++x)
                {
                    const uint32_t color =
                        x >= 1 && x <= 5 &&
                        (row_bits & (1u << (5 - x)))
                            ? foreground : background;
                    amd3dc_virtio_put_pixel(
                        virtio,
                        column * 8 + x,
                        row * 16 + y,
                        color);
                }
            }
        }
    }
}
