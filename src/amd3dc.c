#include "amd3dc.h"
#include "isa.h"
#include "igpu.h"
#include "memory.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

bool amd3dc_init(
    AMD3DC_State* state,
    size_t core_count,
    size_t segment_count,
    size_t memory_size)
{
    if (state == NULL ||
        core_count == 0 ||
        segment_count == 0 ||
        memory_size == 0)
    {
        return false;
    }

    memset(state, 0, sizeof(*state));

    /* Cores */
    state->cores = calloc(
        core_count,
        sizeof(AMD3DC_Core)
    );

    if (state->cores == NULL)
        return false;

    state->core_count = core_count;

    for (size_t i = 0; i < core_count; ++i)
    {
        state->cores[i].type =
            AMD3DC_CORE_BROADWELL;

        state->cores[i].enabled = true;
        state->cores[i].faulty = false;
        state->cores[i].halted = false;
        state->cores[i].layer = 0;
    }

    /* Segments */
    state->segments = calloc(
        segment_count,
        sizeof(AMD3DC_Segment)
    );

    if (state->segments == NULL)
    {
        free(state->cores);
        memset(state, 0, sizeof(*state));
        return false;
    }

    state->segment_count = segment_count;

    for (size_t i = 0; i < segment_count; ++i)
    {
        state->segments[i].id =
            (uint32_t)i;

        state->segments[i].enabled = true;
        state->segments[i].faulty = false;
    }

    /* Memory */
    if (!amd3dc_memory_init(
        &state->memory,
        memory_size))
    {
        free(state->segments);
        free(state->cores);

        memset(state, 0, sizeof(*state));
        return false;
    }

    /* Virtio GPU Device */
    if (!amd3dc_virtio_init(
        &state->virtio,
        640,
        480))
    {
        amd3dc_memory_free(
            &state->memory
        );

        free(state->segments);
        free(state->cores);

        memset(state, 0, sizeof(*state));
        return false;
    }

    /* Cache placeholders */
    state->l1.enabled = true;
    state->l2.enabled = true;
    state->l3.enabled = true;
    state->l4.enabled = true;

    /* Controller / SMU */
    state->controller.initialized = true;
    state->controller.post_passed = false;

    state->controller.total_segments =
        (uint32_t)segment_count;

    state->controller.disabled_segments = 0;

    state->halted = false;

    return true;
}

void amd3dc_reset(
    AMD3DC_State* state)
{
    if (state == NULL)
        return;

    for (size_t i = 0;
        i < state->core_count;
        ++i)
    {
        AMD3DC_Core* core =
            &state->cores[i];

        memset(
            core->registers,
            0,
            sizeof(core->registers)
        );

        memset(
            core->vector_registers,
            0,
            sizeof(core->vector_registers)
        );

        core->pc = 0;
        core->sp = 0;
        core->flags = 0;
        core->layer = 0;
        core->halted = false;
    }

    for (size_t i = 0;
        i < state->segment_count;
        ++i)
    {
        state->segments[i].enabled =
            !state->segments[i].faulty;
    }

    state->controller.post_passed = false;
    state->controller.disabled_segments = 0;
    state->halted = false;
}

void amd3dc_destroy(
    AMD3DC_State* state)
{
    if (state == NULL)
        return;

    amd3dc_virtio_destroy(
        &state->virtio
    );

    amd3dc_memory_free(
        &state->memory
    );

    free(state->segments);
    free(state->cores);

    memset(state, 0, sizeof(*state));
}

bool amd3dc_post(
    AMD3DC_State* state)
{
    if (state == NULL ||
        !state->controller.initialized)
    {
        return false;
    }

    uint32_t disabled = 0;

    for (size_t i = 0;
        i < state->segment_count;
        ++i)
    {
        AMD3DC_Segment* segment =
            &state->segments[i];

        if (segment->faulty)
        {
            segment->enabled = false;
            ++disabled;
        }
        else
        {
            segment->enabled = true;
        }
    }

    state->controller.disabled_segments =
        disabled;

    state->controller.post_passed = true;

    return true;
}

uint64_t amd3dc_fetch64(
    AMD3DC_State* state)
{
    if (state == NULL ||
        state->cores == NULL ||
        state->core_count == 0)
    {
        return 0;
    }

    AMD3DC_Core* core =
        &state->cores[0];

    if (!core->enabled ||
        core->halted)
    {
        return 0;
    }

    uint64_t instruction =
        amd3dc_memory_read64(
            &state->memory,
            (size_t)core->pc
        );

    core->pc += 8;

    return instruction;
}
