#include "bios_gui.hpp"

#include <cstdio>
#include <conio.h>
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <string>

BiosGui::BiosGui(AMD3DC_State& state)
    : state_(state)
{
    load_settings();
}

void BiosGui::run()
{
    running_ = true;

    while (running_)
    {
        render();
        handle_input();
    }

    save_settings();
}

void BiosGui::clear_screen()
{
    std::system("cls");
}

void BiosGui::print_at(int x, int y, const std::string& text)
{
    /* Position cursor and print */
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdOut != INVALID_HANDLE_VALUE)
    {
        COORD coord = { static_cast<SHORT>(x), static_cast<SHORT>(y) };
        SetConsoleCursorPosition(hStdOut, coord);
        std::printf("%s", text.c_str());
    }
}

void BiosGui::print_centered(int y, const std::string& text)
{
    const int width = 80;
    const int x = (width - static_cast<int>(text.length())) / 2;
    print_at((std::max)(0, x), y, text);
}

void BiosGui::render()
{
    clear_screen();
    render_header();
    render_menu_bar();
    render_page();
    render_footer();
}

void BiosGui::render_header()
{
    print_centered(0, "╔════════════════════════════════════════════════════════════════════════════════╗");
    print_centered(1, "║                       AMD 3DC BIOS Setup Utility v0.1                          ║");
    print_centered(2, "║                          Cellarium 3DC Firmware                                ║");
    print_centered(3, "╚════════════════════════════════════════════════════════════════════════════════╝");
}

void BiosGui::render_menu_bar()
{
    const char* pages[] =
    {
        " Main ",
        " CPU ",
        " Memory ",
        " Cache ",
        " GPU ",
        " Quantum ",
        " Boot ",
        " Save/Exit "
    };

    std::string bar = "┌";
    for (int i = 0; i < static_cast<int>(BiosGui::PageType::PAGE_COUNT); ++i)
    {
        if (i == static_cast<int>(current_page_))
            bar += "─[";
        else
            bar += "─";

        bar += pages[i];

        if (i == static_cast<int>(current_page_))
            bar += "]─";
        else
            bar += "─";
    }
    bar += "┐";

    print_at(0, 5, bar);
}

void BiosGui::render_page()
{
    switch (current_page_)
    {
    case BiosGui::PageType::MAIN_MENU:
        render_main_menu();
        break;
    case BiosGui::PageType::CPU_SETTINGS:
        render_cpu_settings();
        break;
    case BiosGui::PageType::MEMORY_SETTINGS:
        render_memory_settings();
        break;
    case BiosGui::PageType::CACHE_SETTINGS:
        render_cache_settings();
        break;
    case BiosGui::PageType::GPU_SETTINGS:
        render_gpu_settings();
        break;
    case BiosGui::PageType::QUANTUM_SETTINGS:
        render_quantum_settings();
        break;
    case BiosGui::PageType::BOOT_SETTINGS:
        render_boot_settings();
        break;
    case BiosGui::PageType::SAVE_EXIT:
        render_save_exit();
        break;
    default:
        break;
    }
}

void BiosGui::render_main_menu()
{
    print_at(2, 7, "┌──────────────────────────────────────────────────────────────────────────┐");
    print_at(2, 8, "│  System Information                                                      │");
    print_at(2, 9, "├──────────────────────────────────────────────────────────────────────────┤");

    char info[256];

    snprintf(info, sizeof(info), "│  CPU           : AMD 3DC Cellarium                                    │");
    print_at(2, 10, info);

    snprintf(info, sizeof(info), "│  Architecture  : 64-bit Big Endian                                    │");
    print_at(2, 11, info);

    snprintf(info, sizeof(info), "│  Cores         : %-2zu                                                   │",
        state_.core_count);
    print_at(2, 12, info);

    snprintf(info, sizeof(info), "│  Segments      : %-2zu                                                   │",
        state_.segment_count);
    print_at(2, 13, info);

    snprintf(info, sizeof(info), "│  HBM3 Memory   : 24 GiB                                                 │");
    print_at(2, 14, info);

    snprintf(info, sizeof(info), "│  Virtio GPU    : %s                                              │",
        state_.virtio.enabled ? "Enabled " : "Disabled");
    print_at(2, 15, info);

    print_at(2, 16, "├──────────────────────────────────────────────────────────────────────────┤");
    print_at(2, 17, "│  Press TAB to navigate pages, or arrow keys for menu items              │");
    print_at(2, 18, "└──────────────────────────────────────────────────────────────────────────┘");
}

void BiosGui::render_cpu_settings()
{
    print_at(2, 7, "┌──────────────────────────────────────────────────────────────────────────┐");
    print_at(2, 8, "│  CPU Settings                                                            │");
    print_at(2, 9, "├──────────────────────────────────────────────────────────────────────────┤");

    if (state_.cores == nullptr || state_.core_count == 0)
    {
        print_at(2, 11, "│  (No cores initialized)                                                  │");
        print_at(2, 12, "└──────────────────────────────────────────────────────────────────────────┘");
        return;
    }

    AMD3DC_Core& core = state_.cores[0];

    char line[256];
    int y = 10;

    snprintf(line, sizeof(line), "│  %s Core 0 Enabled              [%s]                                │",
        selected_item_ == 0 ? ">" : " ",
        core.enabled ? "X" : " ");
    print_at(2, y++, line);

    snprintf(line, sizeof(line), "│  %s Core 0 Type                 [%s]                         │",
        selected_item_ == 1 ? ">" : " ",
        core.type == AMD3DC_CORE_BROADWELL ? "BROADWELL" : "ALPHA    ");
    print_at(2, y++, line);

    snprintf(line, sizeof(line), "│  %s PC (Program Counter)        [0x%016llX]                 │",
        selected_item_ == 2 ? ">" : " ",
        (unsigned long long)core.pc);
    print_at(2, y++, line);

    snprintf(line, sizeof(line), "│  %s Layer (LAE Context)          [%-2llu]                                 │",
        selected_item_ == 3 ? ">" : " ",
        (unsigned long long)core.layer);
    print_at(2, y++, line);

    print_at(2, y++, "├──────────────────────────────────────────────────────────────────────────┤");
    print_at(2, y, "└──────────────────────────────────────────────────────────────────────────┘");
}

void BiosGui::render_memory_settings()
{
    print_at(2, 7, "┌──────────────────────────────────────────────────────────────────────────┐");
    print_at(2, 8, "│  Memory Settings                                                         │");
    print_at(2, 9, "├──────────────────────────────────────────────────────────────────────────┤");

    char line[256];
    int y = 10;

    snprintf(line, sizeof(line), "│  Memory Size                   [%zu MB]                           │",
        state_.memory.size / (1024 * 1024));
    print_at(2, y++, line);

    print_at(2, y++, "├──────────────────────────────────────────────────────────────────────────┤");
    print_at(2, y, "└──────────────────────────────────────────────────────────────────────────┘");
}

void BiosGui::render_cache_settings()
{
    print_at(2, 7, "┌──────────────────────────────────────────────────────────────────────────┐");
    print_at(2, 8, "│  Cache Settings                                                          │");
    print_at(2, 9, "├──────────────────────────────────────────────────────────────────────────┤");
    print_at(2, 10, "└──────────────────────────────────────────────────────────────────────────┘");
}

void BiosGui::render_gpu_settings()
{
    print_at(2, 7, "┌──────────────────────────────────────────────────────────────────────────┐");
    print_at(2, 8, "│  Virtio GPU Settings                                                     │");
    print_at(2, 9, "├──────────────────────────────────────────────────────────────────────────┤");
    print_at(2, 10, "└──────────────────────────────────────────────────────────────────────────┘");
}

void BiosGui::render_quantum_settings()
{
    print_at(2, 7, "┌──────────────────────────────────────────────────────────────────────────┐");
    print_at(2, 8, "│  Quantum Settings                                                        │");
    print_at(2, 9, "├──────────────────────────────────────────────────────────────────────────┤");
    print_at(2, 10, "└──────────────────────────────────────────────────────────────────────────┘");
}

void BiosGui::render_boot_settings()
{
    print_at(2, 7, "┌──────────────────────────────────────────────────────────────────────────┐");
    print_at(2, 8, "│  Boot Settings                                                           │");
    print_at(2, 9, "├──────────────────────────────────────────────────────────────────────────┤");
    print_at(2, 10, "└──────────────────────────────────────────────────────────────────────────┘");
}

void BiosGui::render_save_exit()
{
    print_at(2, 7, "┌──────────────────────────────────────────────────────────────────────────┐");
    print_at(2, 8, "│  Save and Exit                                                           │");
    print_at(2, 9, "├──────────────────────────────────────────────────────────────────────────┤");
    print_at(2, 10, "│  Press ENTER to save and exit, or ESC to cancel                        │");
    print_at(2, 11, "└──────────────────────────────────────────────────────────────────────────┘");
}

void BiosGui::render_footer()
{
    print_at(0, 24, "F1=Help  F2=Continue  ESC=Exit");
}

void BiosGui::handle_input()
{
    if (_kbhit())
    {
        int key = _getch();
        handle_key(key);
    }
}

void BiosGui::handle_key(int key)
{
    switch (key)
    {
    case 27:  /* ESC */
        running_ = false;
        break;
    case 9:   /* TAB */
        current_page_ = static_cast<PageType>((static_cast<int>(current_page_) + 1) % static_cast<int>(PageType::PAGE_COUNT));
        break;
    case 13:  /* ENTER */
        handle_enter();
        break;
    case 72:  /* UP arrow */
        handle_up();
        break;
    case 80:  /* DOWN arrow */
        handle_down();
        break;
    case 75:  /* LEFT arrow */
        handle_left();
        break;
    case 77:  /* RIGHT arrow */
        handle_right();
        break;
    case 43:  /* + */
        handle_plus();
        break;
    case 45:  /* - */
        handle_minus();
        break;
    }
}

void BiosGui::handle_enter()
{
    /* Handle enter key */
}

void BiosGui::handle_up()
{
    if (selected_item_ > 0)
        selected_item_--;
}

void BiosGui::handle_down()
{
    selected_item_++;
}

void BiosGui::handle_left()
{
    /* Handle left arrow */
}

void BiosGui::handle_right()
{
    /* Handle right arrow */
}

void BiosGui::handle_plus()
{
    /* Handle + key */
}

void BiosGui::handle_minus()
{
    /* Handle - key */
}

void BiosGui::save_settings()
{
    /* Save settings to file or state */
}

void BiosGui::load_settings()
{
    /* Load settings from file or state */
}