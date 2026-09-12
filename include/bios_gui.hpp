#ifndef AMD3DC_BIOS_GUI_HPP
#define AMD3DC_BIOS_GUI_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

extern "C"
{
#include "amd3dc_types.h"
}

/**
 * AMI-style BIOS GUI for AMD 3DC emulator.
 * Provides full-screen menu navigation with real settings persistence.
 */
class BiosGui
{
public:
    explicit BiosGui(AMD3DC_State& state);
    ~BiosGui() = default;

    /* Run the BIOS GUI loop. */
    void run();

private:
    /* Menu page types */
    enum class PageType : int
    {
        MAIN_MENU = 0,
        CPU_SETTINGS = 1,
        MEMORY_SETTINGS = 2,
        CACHE_SETTINGS = 3,
        GPU_SETTINGS = 4,
        QUANTUM_SETTINGS = 5,
        BOOT_SETTINGS = 6,
        SAVE_EXIT = 7,
        PAGE_COUNT = 8
    };

    /* A single menu item */
    struct MenuItem
    {
        std::string label;
        std::string value;
        bool editable = false;
        int min_val = 0;
        int max_val = 0;
        int* int_ptr = nullptr;
        bool* bool_ptr = nullptr;
    };

    AMD3DC_State& state_;
    PageType current_page_ = PageType::MAIN_MENU;
    int selected_item_ = 0;
    bool running_ = true;

    std::vector<MenuItem> menu_items_;

    /* Rendering */
    void render();
    void render_header();
    void render_menu_bar();
    void render_page();
    void render_main_menu();
    void render_cpu_settings();
    void render_memory_settings();
    void render_cache_settings();
    void render_gpu_settings();
    void render_quantum_settings();
    void render_boot_settings();
    void render_save_exit();
    void render_footer();

    /* Input handling */
    void handle_input();
    void handle_key(int key);
    void handle_enter();
    void handle_up();
    void handle_down();
    void handle_left();
    void handle_right();
    void handle_plus();
    void handle_minus();

    /* Page building */
    void build_main_menu_items();
    void build_cpu_items();
    void build_memory_items();
    void build_cache_items();
    void build_gpu_items();
    void build_quantum_items();
    void build_boot_items();
    void build_save_exit_items();

    /* Utility */
    void clear_screen();
    void print_at(int x, int y, const std::string& text);
    void print_centered(int y, const std::string& text);
    void save_settings();
    void load_settings();
};

#endif