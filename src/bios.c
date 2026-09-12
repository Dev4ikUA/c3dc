#include "bios.h"
#include "bios.h"

#include <stdio.h>

bool amd3dc_bios_init(
    AMD3DC_State* state)
{
    if (state == NULL)
        return false;

    printf("\n");
    printf("============================================================\n");
    printf("                     AMD 3DC BIOS\n");
    printf("                  Cellarium 3DC v0.1\n");
    printf("============================================================\n");
    printf("\n");

    return true;
}

bool amd3dc_bios_post(
    AMD3DC_State* state)
{
    if (state == NULL)
        return false;

    if (!state->controller.initialized)
        return false;

    printf("POST\n");
    printf("------------------------------------------------------------\n");

    printf("CPU              : AMD 3DC\n");
    printf("Architecture     : 64-bit\n");
    printf("Byte order       : Big Endian\n");

    printf(
        "Cores            : %zu\n",
        state->core_count
    );

    printf(
        "Segments         : %zu\n",
        state->segment_count
    );

    printf(
        "HBM3             : %llu GiB\n",
        (unsigned long long)(
            state->memory.capacity /
            (1024ULL * 1024ULL * 1024ULL)
            )
    );

    printf(
        "L1               : %s\n",
        state->l1.enabled ? "OK" : "OFF"
    );

    printf(
        "L2               : %s\n",
        state->l2.enabled ? "OK" : "OFF"
    );

    printf(
        "L3               : %s\n",
        state->l3.enabled ? "OK" : "OFF"
    );

    printf(
        "L4               : %s\n",
        state->l4.enabled ? "OK" : "OFF"
    );

    printf(
        "Virtio GPU       : %s\n",
        state->virtio.enabled ? "OK" : "OFF"
    );

    if (!amd3dc_post(state))
    {
        printf("SMU POST         : FAIL\n");
        printf("\n");
        return false;
    }

    printf("SMU POST         : PASS\n");
    printf(
        "Disabled segments: %u\n",
        state->controller.disabled_segments
    );

    printf("\n");
    printf("POST: PASS\n");

    return true;
}

void amd3dc_bios_print(
    const AMD3DC_State* state)
{
    if (state == NULL)
        return;

    printf("\n");
    printf("AMD 3DC BIOS READY\n");
    printf("\n");
}

void amd3dc_bios_shutdown(
    AMD3DC_State* state)
{
    (void)state;

    printf("BIOS shutdown.\n");
}