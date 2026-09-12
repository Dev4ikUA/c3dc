#include "bios_ui.h"
#include "igpu.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <conio.h>
#include <windows.h>

/* Simple 8x8 monospace font data for ASCII characters 32-126 */
static const uint8_t font_8x8[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* space */
    0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00, /* ! */
    0x66, 0x66, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, /* " */
    0x6B, 0x6B, 0xFF, 0x6B, 0xFF, 0x6B, 0x6B, 0x00, /* # */
    0x18, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x18, 0x00, /* $ */
    0x63, 0x66, 0x0C, 0x18, 0x30, 0x66, 0xC6, 0x00, /* % */
    0x1C, 0x36, 0x1C, 0x1E, 0x33, 0x33, 0x1E, 0x00, /* & */
    0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, /* ' */
    0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00, /* ( */
    0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00, /* ) */
    0x00, 0x18, 0x7E, 0x3C, 0x7E, 0x18, 0x00, 0x00, /* * */
    0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00, /* + */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30, /* , */
    0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00, /* - */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, /* . */
    0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x00, /* / */
    0x3C, 0x66, 0x6E, 0x7E, 0x76, 0x66, 0x3C, 0x00, /* 0 */
    0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00, /* 1 */
    0x3C, 0x66, 0x06, 0x0C, 0x18, 0x30, 0x7E, 0x00, /* 2 */
    0x7E, 0x06, 0x0C, 0x06, 0x06, 0x66, 0x3C, 0x00, /* 3 */
    0x0C, 0x1C, 0x3C, 0x6C, 0x7E, 0x0C, 0x0C, 0x00, /* 4 */
    0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00, /* 5 */
    0x1C, 0x30, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00, /* 6 */
    0x7E, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00, /* 7 */
    0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00, /* 8 */
    0x3C, 0x66, 0x66, 0x3E, 0x06, 0x0C, 0x38, 0x00, /* 9 */
    0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, /* : */
    0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x30, 0x00, /* ; */
    0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00, /* < */
    0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00, /* = */
    0x60, 0x30, 0x18, 0x0C, 0x18, 0x30, 0x60, 0x00, /* > */
    0x3C, 0x66, 0x0C, 0x18, 0x18, 0x00, 0x18, 0x00, /* ? */
    0x3C, 0x66, 0x6E, 0x6E, 0x60, 0x60, 0x3E, 0x00, /* @ */
    0x18, 0x3C, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00, /* A */
    0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00, /* B */
    0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00, /* C */
    0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00, /* D */
    0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x7E, 0x00, /* E */
    0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x60, 0x00, /* F */
    0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3C, 0x00, /* G */
    0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00, /* H */
    0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00, /* I */
    0x3E, 0x0C, 0x0C, 0x0C, 0x0C, 0x6C, 0x38, 0x00, /* J */
    0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00, /* K */
    0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00, /* L */
    0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00, /* M */
    0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00, /* N */
    0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00, /* O */
    0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00, /* P */
    0x3C, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x0E, 0x00, /* Q */
    0x7C, 0x66, 0x66, 0x7C, 0x78, 0x6C, 0x66, 0x00, /* R */
    0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00, /* S */
    0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, /* T */
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00, /* U */
    0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00, /* V */
    0x63, 0x63, 0x6B, 0x7F, 0x7F, 0x77, 0x63, 0x00, /* W */
    0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00, /* X */
    0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00, /* Y */
    0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00, /* Z */
    0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00, /* [ */
    0xC0, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x00, /* \ */
    0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00, /* ] */
    0x18, 0x3C, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, /* ^ */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, /* _ */
    0x30, 0x18, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, /* ` */
    0x00, 0x3C, 0x06, 0x3E, 0x66, 0x66, 0x3E, 0x00, /* a */
    0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00, /* b */
    0x00, 0x3C, 0x66, 0x60, 0x60, 0x66, 0x3C, 0x00, /* c */
    0x06, 0x06, 0x3E, 0x66, 0x66, 0x66, 0x3E, 0x00, /* d */
    0x00, 0x3C, 0x66, 0x7E, 0x60, 0x66, 0x3C, 0x00, /* e */
    0x1C, 0x30, 0x30, 0x7C, 0x30, 0x30, 0x30, 0x00, /* f */
    0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x3C, 0x00, /* g */
    0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00, /* h */
    0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00, /* i */
    0x06, 0x00, 0x06, 0x06, 0x06, 0x66, 0x3C, 0x00, /* j */
    0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x00, /* k */
    0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00, /* l */
    0x00, 0x66, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00, /* m */
    0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, /* n */
    0x00, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00, /* o */
    0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x00, /* p */
    0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x00, /* q */
    0x00, 0x7C, 0x66, 0x60, 0x60, 0x60, 0x60, 0x00, /* r */
    0x00, 0x3E, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00, /* s */
    0x30, 0x30, 0x7C, 0x30, 0x30, 0x30, 0x1C, 0x00, /* t */
    0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00, /* u */
    0x00, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00, /* v */
    0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x77, 0x63, 0x00, /* w */
    0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00, /* x */
    0x00, 0x66, 0x66, 0x66, 0x3E, 0x06, 0x3C, 0x00, /* y */
    0x00, 0x7E, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00  /* z */
};

static void bios_ui_draw_pixel(
    BiosUI* ui,
    uint32_t x,
    uint32_t y,
    uint32_t color)
{
    if (!ui || !ui->virtio || x >= BIOS_UI_WIDTH || y >= BIOS_UI_HEIGHT)
        return;

    amd3dc_virtio_put_pixel(ui->virtio, x, y, color);
}

static void bios_ui_draw_rect(
    BiosUI* ui,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height,
    uint32_t color)
{
    if (!ui)
        return;

    for (uint32_t row = 0; row < height; row++)
    {
        for (uint32_t col = 0; col < width; col++)
        {
            bios_ui_draw_pixel(ui, x + col, y + row, color);
        }
    }
}

static void bios_ui_draw_box(
    BiosUI* ui,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height,
    uint32_t border_color,
    uint32_t fill_color)
{
    if (!ui)
        return;

    /* Fill interior */
    bios_ui_draw_rect(ui, x + 1, y + 1, width - 2, height - 2, fill_color);

    /* Draw borders */
    bios_ui_draw_rect(ui, x, y, width, 1, border_color);
    bios_ui_draw_rect(ui, x, y + height - 1, width, 1, border_color);
    bios_ui_draw_rect(ui, x, y, 1, height, border_color);
    bios_ui_draw_rect(ui, x + width - 1, y, 1, height, border_color);
}

static void bios_ui_draw_char(
    BiosUI* ui,
    uint32_t x,
    uint32_t y,
    char ch,
    uint32_t fg_color,
    uint32_t bg_color)
{
    if (!ui)
        return;

    unsigned char c = (unsigned char)ch;

    /* Draw background */
    bios_ui_draw_rect(ui, x, y, CHAR_WIDTH, CHAR_HEIGHT, bg_color);

    /* Only render printable ASCII (32-126) */
    if (c < 32 || c > 126)
        return;

    /* Get font glyph for this character */
    const uint8_t* glyph = &font_8x8[(c - 32) * 8];

    /* Render 8x8 glyph pixel by pixel */
    for (int row = 0; row < 8; row++)
    {
        uint8_t byte = glyph[row];
        for (int col = 0; col < 8; col++)
        {
            if (byte & (0x80 >> col))
            {
                bios_ui_draw_pixel(ui, x + col, y + row, fg_color);
            }
        }
    }
}

static void bios_ui_draw_string(
    BiosUI* ui,
    uint32_t x,
    uint32_t y,
    const char* str,
    uint32_t fg_color,
    uint32_t bg_color)
{
    if (!ui || !str)
        return;

    uint32_t col = 0;
    while (*str && col < 128)
    {
        bios_ui_draw_char(ui, x + col * CHAR_WIDTH, y, *str, fg_color, bg_color);
        str++;
        col++;
    }
}

static void bios_ui_render_header(BiosUI* ui)
{
    if (!ui)
        return;

    bios_ui_draw_rect(ui, 0, 0, BIOS_UI_WIDTH, 60, COLOR_BLUE);
    bios_ui_draw_string(ui, 20, 10, "AMD 3DC BIOS Setup Utility v0.1", COLOR_WHITE, COLOR_BLUE);
    bios_ui_draw_string(ui, 20, 30, "Cellarium 3DC Firmware", COLOR_CYAN, COLOR_BLUE);
}

static void bios_ui_render_menu_bar(BiosUI* ui)
{
    if (!ui)
        return;

    const char* pages[] =
    {
        "Main",
        "CPU",
        "Memory",
        "Cache",
        "GPU",
        "Quantum",
        "Boot",
        "Save/Exit"
    };

    uint32_t x = 20;
    uint32_t y = 80;

    for (int i = 0; i < BIOS_PAGE_COUNT; i++)
    {
        if (i == (int)ui->current_page)
        {
            bios_ui_draw_box(ui, x - 5, y, 80, 30, COLOR_CYAN, COLOR_DARK_GRAY);
            bios_ui_draw_string(ui, x, y + 7, pages[i], COLOR_CYAN, COLOR_DARK_GRAY);
        }
        else
        {
            bios_ui_draw_box(ui, x - 5, y, 80, 30, COLOR_GRAY, COLOR_BLACK);
            bios_ui_draw_string(ui, x, y + 7, pages[i], COLOR_WHITE, COLOR_BLACK);
        }

        x += 90;
    }
}

static void bios_ui_render_main_page(BiosUI* ui)
{
    if (!ui || !ui->state)
        return;

    uint32_t x = 40;
    uint32_t y = 150;

    bios_ui_draw_box(ui, 30, y - 10, BIOS_UI_WIDTH - 60, 300, COLOR_CYAN, COLOR_BLACK);
    bios_ui_draw_string(ui, x, y, "System Information", COLOR_CYAN, COLOR_BLACK);

    y += 40;
    bios_ui_draw_string(ui, x, y, "CPU           : AMD 3DC Cellarium", COLOR_WHITE, COLOR_BLACK);
    y += 20;
    bios_ui_draw_string(ui, x, y, "Architecture  : 64-bit Big Endian", COLOR_WHITE, COLOR_BLACK);
    y += 20;

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "Cores         : %zu", ui->state->core_count);
    bios_ui_draw_string(ui, x, y, buffer, COLOR_WHITE, COLOR_BLACK);
    y += 20;

    snprintf(buffer, sizeof(buffer), "Segments      : %zu", ui->state->segment_count);
    bios_ui_draw_string(ui, x, y, buffer, COLOR_WHITE, COLOR_BLACK);
    y += 20;

    uint64_t hbm3_gib = ui->state->memory.capacity / (1024ULL * 1024ULL * 1024ULL);
    snprintf(buffer, sizeof(buffer), "HBM3 Memory   : %llu GiB", (unsigned long long)hbm3_gib);
    bios_ui_draw_string(ui, x, y, buffer, COLOR_WHITE, COLOR_BLACK);
    y += 20;

    const char* gpu_status = ui->state->virtio.enabled ? "Enabled" : "Disabled";
    snprintf(buffer, sizeof(buffer), "Virtio GPU    : %s", gpu_status);
    bios_ui_draw_string(ui, x, y, buffer, COLOR_WHITE, COLOR_BLACK);
}

static void bios_ui_render_cpu_page(BiosUI* ui)
{
    if (!ui || !ui->state)
        return;

    uint32_t x = 40;
    uint32_t y = 150;

    bios_ui_draw_box(ui, 30, y - 10, BIOS_UI_WIDTH - 60, 300, COLOR_CYAN, COLOR_BLACK);
    bios_ui_draw_string(ui, x, y, "CPU Settings", COLOR_CYAN, COLOR_BLACK);

    y += 40;
    bios_ui_draw_string(ui, x, y, "CPU Type      : AMD 3DC", COLOR_WHITE, COLOR_BLACK);
    y += 20;

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "Number of Cores : %zu", ui->state->core_count);
    bios_ui_draw_string(ui, x, y, buffer, COLOR_WHITE, COLOR_BLACK);
    y += 20;

    bios_ui_draw_string(ui, x, y, "Cache Enabled  : ", COLOR_WHITE, COLOR_BLACK);
    y += 20;

    snprintf(buffer, sizeof(buffer), "  L1: %s", ui->state->l1.enabled ? "ON" : "OFF");
    bios_ui_draw_string(ui, x, y, buffer, COLOR_GREEN, COLOR_BLACK);
    y += 20;

    snprintf(buffer, sizeof(buffer), "  L2: %s", ui->state->l2.enabled ? "ON" : "OFF");
    bios_ui_draw_string(ui, x, y, buffer, COLOR_GREEN, COLOR_BLACK);
}

static void bios_ui_render_memory_page(BiosUI* ui)
{
    if (!ui || !ui->state)
        return;

    uint32_t x = 40;
    uint32_t y = 150;

    bios_ui_draw_box(ui, 30, y - 10, BIOS_UI_WIDTH - 60, 300, COLOR_CYAN, COLOR_BLACK);
    bios_ui_draw_string(ui, x, y, "Memory Settings", COLOR_CYAN, COLOR_BLACK);

    y += 40;

    char buffer[128];
    uint64_t total_mb = ui->state->memory.capacity / (1024ULL * 1024ULL);
    snprintf(buffer, sizeof(buffer), "Total Memory  : %llu MB", (unsigned long long)total_mb);
    bios_ui_draw_string(ui, x, y, buffer, COLOR_WHITE, COLOR_BLACK);
    y += 20;

    uint64_t used_mb = ui->state->memory.size / (1024ULL * 1024ULL);
    snprintf(buffer, sizeof(buffer), "Used Memory   : %llu MB", (unsigned long long)used_mb);
    bios_ui_draw_string(ui, x, y, buffer, COLOR_WHITE, COLOR_BLACK);
    y += 20;

    bios_ui_draw_string(ui, x, y, "Memory Type   : HBM3", COLOR_WHITE, COLOR_BLACK);
}

static void bios_ui_render_cache_page(BiosUI* ui)
{
    if (!ui || !ui->state)
        return;

    uint32_t x = 40;
    uint32_t y = 150;

    bios_ui_draw_box(ui, 30, y - 10, BIOS_UI_WIDTH - 60, 300, COLOR_CYAN, COLOR_BLACK);
    bios_ui_draw_string(ui, x, y, "Cache Settings", COLOR_CYAN, COLOR_BLACK);

    y += 40;

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "L1 Cache      : %s (%llu KB)",
        ui->state->l1.enabled ? "Enabled" : "Disabled",
        (unsigned long long)(ui->state->l1.size / 1024));
    bios_ui_draw_string(ui, x, y, buffer, COLOR_WHITE, COLOR_BLACK);
    y += 20;

    snprintf(buffer, sizeof(buffer), "L2 Cache      : %s (%llu KB)",
        ui->state->l2.enabled ? "Enabled" : "Disabled",
        (unsigned long long)(ui->state->l2.size / 1024));
    bios_ui_draw_string(ui, x, y, buffer, COLOR_WHITE, COLOR_BLACK);
    y += 20;

    snprintf(buffer, sizeof(buffer), "L3 Cache      : %s (%llu KB)",
        ui->state->l3.enabled ? "Enabled" : "Disabled",
        (unsigned long long)(ui->state->l3.size / 1024));
    bios_ui_draw_string(ui, x, y, buffer, COLOR_WHITE, COLOR_BLACK);
    y += 20;

    snprintf(buffer, sizeof(buffer), "L4 Cache      : %s (%llu KB)",
        ui->state->l4.enabled ? "Enabled" : "Disabled",
        (unsigned long long)(ui->state->l4.size / 1024));
    bios_ui_draw_string(ui, x, y, buffer, COLOR_WHITE, COLOR_BLACK);
}

static void bios_ui_render_gpu_page(BiosUI* ui)
{
    if (!ui || !ui->state)
        return;

    uint32_t x = 40;
    uint32_t y = 150;

    bios_ui_draw_box(ui, 30, y - 10, BIOS_UI_WIDTH - 60, 300, COLOR_CYAN, COLOR_BLACK);
    bios_ui_draw_string(ui, x, y, "GPU Settings", COLOR_CYAN, COLOR_BLACK);

    y += 40;

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "GPU Status    : %s",
        ui->state->virtio.enabled ? "Enabled" : "Disabled");
    bios_ui_draw_string(ui, x, y, buffer, COLOR_WHITE, COLOR_BLACK);
    y += 20;

    snprintf(buffer, sizeof(buffer), "Resolution    : %ux%u",
        ui->state->virtio.width,
        ui->state->virtio.height);
    bios_ui_draw_string(ui, x, y, buffer, COLOR_WHITE, COLOR_BLACK);
    y += 20;

    snprintf(buffer, sizeof(buffer), "Device ID     : 0x%04X", ui->state->virtio.device_id);
    bios_ui_draw_string(ui, x, y, buffer, COLOR_WHITE, COLOR_BLACK);
    y += 20;

    snprintf(buffer, sizeof(buffer), "Vendor ID     : 0x%04X", ui->state->virtio.vendor_id);
    bios_ui_draw_string(ui, x, y, buffer, COLOR_WHITE, COLOR_BLACK);
}

static void bios_ui_render_quantum_page(BiosUI* ui)
{
    if (!ui || !ui->state)
        return;

    uint32_t x = 40;
    uint32_t y = 150;

    bios_ui_draw_box(ui, 30, y - 10, BIOS_UI_WIDTH - 60, 300, COLOR_CYAN, COLOR_BLACK);
    bios_ui_draw_string(ui, x, y, "Quantum/Tensor Settings", COLOR_CYAN, COLOR_BLACK);

    y += 40;

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "Status        : %s",
        ui->state->quantum_die.enabled ? "Enabled" : "Disabled");
    bios_ui_draw_string(ui, x, y, buffer, COLOR_WHITE, COLOR_BLACK);
    y += 20;

    snprintf(buffer, sizeof(buffer), "Tensor Cores  : %u", ui->state->quantum_die.tensor_core_count);
    bios_ui_draw_string(ui, x, y, buffer, COLOR_WHITE, COLOR_BLACK);
    y += 20;

    snprintf(buffer, sizeof(buffer), "Quantum Cores : %u", ui->state->quantum_die.quantum_core_count);
    bios_ui_draw_string(ui, x, y, buffer, COLOR_WHITE, COLOR_BLACK);
    y += 20;

    snprintf(buffer, sizeof(buffer), "PQC Engines   : %u", ui->state->quantum_die.pqc_engine_count);
    bios_ui_draw_string(ui, x, y, buffer, COLOR_WHITE, COLOR_BLACK);
}

static void bios_ui_render_boot_page(BiosUI* ui)
{
    if (!ui)
        return;

    uint32_t x = 40;
    uint32_t y = 150;

    bios_ui_draw_box(ui, 30, y - 10, BIOS_UI_WIDTH - 60, 300, COLOR_CYAN, COLOR_BLACK);
    bios_ui_draw_string(ui, x, y, "Boot Settings", COLOR_CYAN, COLOR_BLACK);

    y += 40;
    bios_ui_draw_string(ui, x, y, "Boot Device   : Internal HDD", COLOR_WHITE, COLOR_BLACK);
    y += 20;
    bios_ui_draw_string(ui, x, y, "Boot Mode     : UEFI", COLOR_WHITE, COLOR_BLACK);
    y += 20;
    bios_ui_draw_string(ui, x, y, "Secure Boot   : Enabled", COLOR_WHITE, COLOR_BLACK);
}

static void bios_ui_render_save_exit_page(BiosUI* ui)
{
    if (!ui)
        return;

    uint32_t x = 40;
    uint32_t y = 150;

    bios_ui_draw_box(ui, 30, y - 10, BIOS_UI_WIDTH - 60, 200, COLOR_CYAN, COLOR_BLACK);
    bios_ui_draw_string(ui, x, y, "Save and Exit", COLOR_CYAN, COLOR_BLACK);

    y += 60;
    bios_ui_draw_string(ui, x, y, "[ ] Save and Exit Setup", COLOR_WHITE, COLOR_BLACK);
    y += 30;
    bios_ui_draw_string(ui, x, y, "[ ] Exit Setup (Discard Changes)", COLOR_WHITE, COLOR_BLACK);
    y += 30;
    bios_ui_draw_string(ui, x, y, "[ ] Load Default Settings", COLOR_WHITE, COLOR_BLACK);
}

static void bios_ui_render_footer(BiosUI* ui)
{
    if (!ui)
        return;

    uint32_t y = BIOS_UI_HEIGHT - 30;

    bios_ui_draw_rect(ui, 0, y, BIOS_UI_WIDTH, 30, COLOR_DARK_GRAY);
    bios_ui_draw_string(ui, 20, y + 7, "F1=Help  F2=Continue  TAB=Navigate  ESC=Exit", COLOR_CYAN, COLOR_DARK_GRAY);
}

bool bios_ui_init(BiosUI* ui, AMD3DC_State* state)
{
    if (!ui || !state)
        return false;

    memset(ui, 0, sizeof(*ui));
    ui->state = state;
    ui->virtio = &state->virtio;
    ui->current_page = BIOS_PAGE_MAIN;
    ui->selected_item = 0;
    ui->frame_counter = 0;

    if (!ui->virtio->initialized || !ui->virtio->enabled)
        return false;

    ui->running = true;
    return true;
}

void bios_ui_destroy(BiosUI* ui)
{
    if (!ui)
        return;

    ui->running = false;
}

void bios_ui_render(BiosUI* ui)
{
    if (!ui || !ui->virtio)
        return;

    /* Clear screen */
    bios_ui_draw_rect(ui, 0, 0, BIOS_UI_WIDTH, BIOS_UI_HEIGHT, COLOR_BLACK);

    /* Render UI components */
    bios_ui_render_header(ui);
    bios_ui_render_menu_bar(ui);

    /* Render current page */
    switch (ui->current_page)
    {
    case BIOS_PAGE_MAIN:
        bios_ui_render_main_page(ui);
        break;
    case BIOS_PAGE_CPU:
        bios_ui_render_cpu_page(ui);
        break;
    case BIOS_PAGE_MEMORY:
        bios_ui_render_memory_page(ui);
        break;
    case BIOS_PAGE_CACHE:
        bios_ui_render_cache_page(ui);
        break;
    case BIOS_PAGE_GPU:
        bios_ui_render_gpu_page(ui);
        break;
    case BIOS_PAGE_QUANTUM:
        bios_ui_render_quantum_page(ui);
        break;
    case BIOS_PAGE_BOOT:
        bios_ui_render_boot_page(ui);
        break;
    case BIOS_PAGE_SAVE_EXIT:
        bios_ui_render_save_exit_page(ui);
        break;
    default:
        break;
    }

    bios_ui_render_footer(ui);

    ui->frame_counter++;
}

void bios_ui_handle_input(BiosUI* ui)
{
    if (!ui)
        return;

    if (_kbhit())
    {
        int key = _getch();

        switch (key)
        {
        case 27: /* ESC */
            ui->running = false;
            break;

        case 9: /* TAB - Next page */
            bios_ui_next_page(ui);
            break;

        case 0: /* Extended key */
            if (_kbhit())
            {
                int ext = _getch();
                if (ext == 75) /* Left arrow */
                    bios_ui_prev_page(ui);
                else if (ext == 77) /* Right arrow */
                    bios_ui_next_page(ui);
            }
            break;
        }
    }
}

void bios_ui_next_page(BiosUI* ui)
{
    if (!ui)
        return;

    ui->current_page = (ui->current_page + 1) % BIOS_PAGE_COUNT;
}

void bios_ui_prev_page(BiosUI* ui)
{
    if (!ui)
        return;

    ui->current_page = (ui->current_page == 0) ? (BIOS_PAGE_COUNT - 1) : (ui->current_page - 1);
}