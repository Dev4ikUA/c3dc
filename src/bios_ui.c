#include "bios_ui.h"
#include "igpu.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <conio.h>
#include <windows.h>

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

    /* Draw background */
    bios_ui_draw_rect(ui, x, y, CHAR_WIDTH, CHAR_HEIGHT, bg_color);

    /* Simple character rendering */
    if (ch >= 32 && ch <= 126)
    {
        bios_ui_draw_rect(ui, x + 2, y + 2, CHAR_WIDTH - 4, CHAR_HEIGHT - 4, fg_color);
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

        default:
            break;
        }
    }
}

void bios_ui_next_page(BiosUI* ui)
{
    if (!ui)
        return;

    ui->current_page = (BiosPageType)((ui->current_page + 1) % BIOS_PAGE_COUNT);
}

void bios_ui_prev_page(BiosUI* ui)
{
    if (!ui)
        return;

    if (ui->current_page == 0)
        ui->current_page = (BiosPageType)(BIOS_PAGE_COUNT - 1);
    else
        ui->current_page = (BiosPageType)(ui->current_page - 1);
}