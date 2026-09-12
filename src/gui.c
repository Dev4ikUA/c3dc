#include "gui.h"
#include "igpu.h"
#include "amd3dc_types.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define AMD3DC_GUI_CLASS_NAME L"AMD3DCVirtualDisplay"
#define AMD3DC_GUI_TITLE      L"AMD 3DC Virtual Machine"

#define AMD3DC_GUI_TIMER_ID  1
#define AMD3DC_GUI_REFRESH   250

#define AMD3DC_GUI_BG_COLOR RGB(0, 0, 0)

static AMD3DC_State* g_state = NULL;
static HWND g_window = NULL;

static uint32_t* g_dib_pixels = NULL;
static size_t g_dib_pixel_count = 0;

static BITMAPINFO g_bitmap_info;

static bool amd3dc_gui_resize_buffer(void)
{
    if (g_state == NULL)
        return false;

    const AMD3DC_Virtio* virtio = &g_state->virtio;

    if (!virtio->initialized || !virtio->enabled || 
        virtio->width == 0 || virtio->height == 0)
    {
        return false;
    }

    const size_t pixel_count =
        (size_t)virtio->width *
        (size_t)virtio->height;

    if (pixel_count > SIZE_MAX / sizeof(uint32_t))
        return false;

    if (pixel_count == g_dib_pixel_count &&
        g_dib_pixels != NULL)
    {
        return true;
    }

    if (g_dib_pixels != NULL)
    {
        free(g_dib_pixels);
        g_dib_pixels = NULL;
    }

    g_dib_pixels = (uint32_t*)malloc(pixel_count * sizeof(uint32_t));
    if (g_dib_pixels == NULL)
    {
        g_dib_pixel_count = 0;
        return false;
    }

    g_dib_pixel_count = pixel_count;

    memset(
        &g_bitmap_info,
        0,
        sizeof(g_bitmap_info)
    );

    g_bitmap_info.bmiHeader.biSize =
        sizeof(BITMAPINFOHEADER);

    g_bitmap_info.bmiHeader.biWidth =
        (LONG)virtio->width;

    g_bitmap_info.bmiHeader.biHeight =
        -(LONG)virtio->height;

    g_bitmap_info.bmiHeader.biPlanes =
        1;

    g_bitmap_info.bmiHeader.biBitCount =
        32;

    g_bitmap_info.bmiHeader.biCompression =
        BI_RGB;

    return true;
}

static bool amd3dc_gui_update_framebuffer(void)
{
    if (g_state == NULL)
        return false;

        const AMD3DC_Virtio* virtio = &g_state->virtio;

    if (!virtio->initialized ||
        !virtio->enabled ||
        virtio->queue_data == NULL)
    {
        return false;
    }

    if (!amd3dc_gui_resize_buffer())
        return false;

    const size_t pixel_count =
        (size_t)virtio->width *
        (size_t)virtio->height;

    for (size_t i = 0;
        i < pixel_count && i * sizeof(uint32_t) < virtio->queue_data_size;
        ++i)
    {
        const uint8_t* source =
            virtio->queue_data +
            (i * sizeof(uint32_t));

        uint32_t color = 0;

        memcpy(
            &color,
            source,
            sizeof(color)
        );

        const uint32_t red =
            (color >> 16) & 0xFFu;

        const uint32_t green =
            (color >> 8) & 0xFFu;

        const uint32_t blue =
            color & 0xFFu;

        g_dib_pixels[i] =
            blue |
            (green << 8) |
            (red << 16);
    }

    return true;
}

static void amd3dc_gui_draw_display(
    HDC hdc,
    const RECT* client_rect)
{
    if (hdc == NULL ||
        client_rect == NULL ||
        g_state == NULL)
    {
        return;
    }

    if (!amd3dc_gui_update_framebuffer())
    {
        FillRect(
            hdc,
            client_rect,
            (HBRUSH)(
                COLOR_WINDOW + 1
                )
        );

        return;
    }

    const int client_width =
        client_rect->right -
        client_rect->left;

    const int client_height =
        client_rect->bottom -
        client_rect->top;

    const int source_width =
        (int)g_state->virtio.width;

    const int source_height =
        (int)g_state->virtio.height;

    if (source_width <= 0 ||
        source_height <= 0 ||
        client_width <= 0 ||
        client_height <= 0)
    {
        return;
    }

    double scale_x =
        (double)client_width /
        (double)source_width;

    double scale_y =
        (double)client_height /
        (double)source_height;

    double scale =
        scale_x < scale_y
        ? scale_x
        : scale_y;

    int display_width =
        (int)((double)source_width * scale);

    int display_height =
        (int)((double)source_height * scale);

    int display_x =
        (client_width - display_width) / 2;

    int display_y =
        (client_height - display_height) / 2;

    HBRUSH background =
        CreateSolidBrush(
            AMD3DC_GUI_BG_COLOR
        );

    if (background != NULL)
    {
        FillRect(
            hdc,
            client_rect,
            background
        );

        DeleteObject(background);
    }

    SetStretchBltMode(
        hdc,
        HALFTONE
    );

    SetBrushOrgEx(
        hdc,
        0,
        0,
        NULL
    );

    StretchDIBits(
        hdc,
        display_x,
        display_y,
        display_width,
        display_height,
        0,
        0,
        source_width,
        source_height,
        g_dib_pixels,
        &g_bitmap_info,
        DIB_RGB_COLORS,
        SRCCOPY
    );
}

static LRESULT CALLBACK
amd3dc_gui_wnd_proc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    (void)lParam;

    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;

        HDC hdc =
            BeginPaint(
                hwnd,
                &ps
            );

        RECT client_rect;

        GetClientRect(
            hwnd,
            &client_rect
        );

        amd3dc_gui_draw_display(
            hdc,
            &client_rect
        );

        EndPaint(
            hwnd,
            &ps
        );

        return 0;
    }

    case WM_TIMER:
        if (wParam == AMD3DC_GUI_TIMER_ID)
        {
            /*
             * The Virtio device renders the guest display.
             * The host GUI only presents the framebuffer.
             */
            amd3dc_virtio_render_system_screen(
                &g_state->virtio
            );

            InvalidateRect(
                hwnd,
                NULL,
                FALSE
            );
        }

        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_KEYDOWN:
        /*
         * F10 = power off the virtual machine.
         *
         * Guest keyboard emulation will be connected
         * here later.
         */
        if (wParam == VK_F10)
        {
            DestroyWindow(hwnd);
            return 0;
        }

        return 0;

    case WM_DESTROY:
        KillTimer(
            hwnd,
            AMD3DC_GUI_TIMER_ID
        );

        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam
    );
}

bool amd3dc_gui_init(
    AMD3DC_State* state)
{
    if (state == NULL)
        return false;

    if (!state->virtio.initialized ||
        !state->virtio.enabled ||
        state->virtio.width == 0 ||
        state->virtio.height == 0)
    {
        return false;
    }

    g_state = state;

    /*
     * Render the first guest frame before
     * the window is displayed.
     */
    amd3dc_virtio_render_system_screen(
        &state->virtio
    );

    HINSTANCE instance =
        GetModuleHandleW(NULL);

    WNDCLASSW window_class;

    memset(
        &window_class,
        0,
        sizeof(window_class)
    );

    window_class.style =
        CS_HREDRAW |
        CS_VREDRAW;

    window_class.lpfnWndProc =
        amd3dc_gui_wnd_proc;

    window_class.hInstance =
        instance;

    window_class.hCursor =
        LoadCursorW(NULL, IDC_ARROW);

    window_class.lpszClassName =
        AMD3DC_GUI_CLASS_NAME;

    if (!RegisterClassW(&window_class))
    {
        g_state = NULL;
        return false;
    }

    g_window = CreateWindowW(
        AMD3DC_GUI_CLASS_NAME,
        AMD3DC_GUI_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        800,
        600,
        NULL,
        NULL,
        instance,
        NULL
    );

    if (g_window == NULL)
    {
        UnregisterClassW(
            AMD3DC_GUI_CLASS_NAME,
            instance
        );

        g_state = NULL;
        return false;
    }

    ShowWindow(g_window, SW_SHOW);
    UpdateWindow(g_window);

    SetTimer(
        g_window,
        AMD3DC_GUI_TIMER_ID,
        AMD3DC_GUI_REFRESH,
        NULL
    );

    return true;
}

void amd3dc_gui_destroy(void)
{
    if (g_window != NULL)
    {
        DestroyWindow(g_window);
        g_window = NULL;
    }

    if (g_dib_pixels != NULL)
    {
        free(g_dib_pixels);
        g_dib_pixels = NULL;
    }

    g_dib_pixel_count = 0;
    g_state = NULL;
}

bool amd3dc_gui_render(void)
{
    if (g_window == NULL)
        return false;

    MSG msg;

    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);

        if (msg.message == WM_QUIT)
            return false;
    }

    return true;
}

int amd3dc_gui_run(
    AMD3DC_State* state)
{
    if (!state || g_state != state || g_window == NULL)
        return -1;

    while (amd3dc_gui_render())
        Sleep(16);

    return 0;
}

void amd3dc_gui_shutdown(void)
{
    amd3dc_gui_destroy();
}