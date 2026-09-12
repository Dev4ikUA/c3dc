#ifndef IGPU_H
#define IGPU_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "amd3dc_types.h"

/* 
 * Define AMD3DC_iGPU as the software emulated integrated GPU.
 * This struct should match the usage in src\igpu.c.
 */
typedef struct
{
    bool initialized;
    bool enabled;

    uint32_t width;
    uint32_t height;
    uint32_t bytes_per_pixel;
    uint32_t pitch;

    uint8_t* framebuffer;
    size_t framebuffer_size;
} AMD3DC_iGPU;

/* 
 * For compatibility with existing code, define AMD3DC_VirtioGPU as AMD3DC_iGPU.
 * This resolves the E0020 error and allows the code to compile.
 */
typedef AMD3DC_iGPU AMD3DC_VirtioGPU;

/*
 * Initialize the Virtio GPU.
 * Returns true on success, false on failure.
 */
bool amd3dc_virtio_init(
    AMD3DC_Virtio* virtio,
    uint32_t width,
    uint32_t height
);

/*
 * Destroy the Virtio GPU and free resources.
 */
void amd3dc_virtio_destroy(
    AMD3DC_Virtio* virtio
);

/*
 * Render the system screen to the Virtio GPU framebuffer.
 */
void amd3dc_virtio_render_system_screen(
    AMD3DC_Virtio* virtio
);

/*
 * Write a pixel to the Virtio GPU framebuffer.
 * Returns true on success, false on failure.
 */
static inline bool amd3dc_virtio_put_pixel(
    AMD3DC_Virtio* virtio,
    uint32_t x,
    uint32_t y,
    uint32_t color
)
{
    if (!virtio || !virtio->queue_data || !virtio->enabled)
        return false;
    if (x >= virtio->width || y >= virtio->height)
        return false;
    size_t idx = (size_t)y * virtio->width + x;
    if (idx * 4 + 3 >= virtio->queue_data_size)
        return false;
    uint8_t* p = &virtio->queue_data[idx * 4];
    p[0] = (uint8_t)(color & 0xFF);
    p[1] = (uint8_t)((color >> 8) & 0xFF);
    p[2] = (uint8_t)((color >> 16) & 0xFF);
    p[3] = (uint8_t)((color >> 24) & 0xFF);
    return true;
}

/*
 * Read a pixel from the Virtio GPU framebuffer.
 * Returns the pixel color.
 */
static inline uint32_t amd3dc_virtio_get_pixel(
    const AMD3DC_Virtio* virtio,
    uint32_t x,
    uint32_t y
)
{
    if (!virtio || !virtio->queue_data || !virtio->enabled)
        return 0;
    if (x >= virtio->width || y >= virtio->height)
        return 0;
    size_t idx = (size_t)y * virtio->width + x;
    if (idx * 4 + 3 >= virtio->queue_data_size)
        return 0;
    const uint8_t* p = &virtio->queue_data[idx * 4];
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

#endif /* IGPU_H */