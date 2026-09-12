#include "memory.h"

#include <stdlib.h>
#include <string.h>

static bool valid_range(
    const AMD3DC_Memory* memory,
    size_t address,
    size_t size)
{
    if (memory == NULL || memory->data == NULL)
        return false;

    if (address > memory->size)
        return false;

    return size <= memory->size - address;
}

bool amd3dc_memory_init(
    AMD3DC_Memory* memory,
    size_t size)
{
    if (memory == NULL || size == 0)
        return false;

    memory->data = calloc(size, sizeof(uint8_t));

    if (memory->data == NULL)
    {
        memory->size = 0;
        memory->capacity = 0;
        return false;
    }

    memory->size = size;
    memory->capacity = size;

    return true;
}

void amd3dc_memory_free(
    AMD3DC_Memory* memory)
{
    if (memory == NULL)
        return;

    free(memory->data);

    memory->data = NULL;
    memory->size = 0;
    memory->capacity = 0;
}

void amd3dc_memory_clear(
    AMD3DC_Memory* memory)
{
    if (memory == NULL || memory->data == NULL)
        return;

    memset(memory->data, 0, memory->size);
}

uint8_t amd3dc_memory_read8(
    const AMD3DC_Memory* memory,
    size_t address)
{
    if (!valid_range(memory, address, 1))
        return 0;

    return memory->data[address];
}

uint16_t amd3dc_memory_read16(
    const AMD3DC_Memory* memory,
    size_t address)
{
    if (!valid_range(memory, address, 2))
        return 0;

    return ((uint16_t)memory->data[address] << 8) |
        ((uint16_t)memory->data[address + 1]);
}

uint32_t amd3dc_memory_read32(
    const AMD3DC_Memory* memory,
    size_t address)
{
    if (!valid_range(memory, address, 4))
        return 0;

    return ((uint32_t)memory->data[address] << 24) |
        ((uint32_t)memory->data[address + 1] << 16) |
        ((uint32_t)memory->data[address + 2] << 8) |
        ((uint32_t)memory->data[address + 3]);
}

uint64_t amd3dc_memory_read64(
    const AMD3DC_Memory* memory,
    size_t address)
{
    if (!valid_range(memory, address, 8))
        return 0;

    return ((uint64_t)memory->data[address] << 56) |
        ((uint64_t)memory->data[address + 1] << 48) |
        ((uint64_t)memory->data[address + 2] << 40) |
        ((uint64_t)memory->data[address + 3] << 32) |
        ((uint64_t)memory->data[address + 4] << 24) |
        ((uint64_t)memory->data[address + 5] << 16) |
        ((uint64_t)memory->data[address + 6] << 8) |
        ((uint64_t)memory->data[address + 7]);
}

bool amd3dc_memory_write8(
    AMD3DC_Memory* memory,
    size_t address,
    uint8_t value)
{
    if (!valid_range(memory, address, 1))
        return false;

    memory->data[address] = value;
    return true;
}

bool amd3dc_memory_write16(
    AMD3DC_Memory* memory,
    size_t address,
    uint16_t value)
{
    if (!valid_range(memory, address, 2))
        return false;

    memory->data[address] = (uint8_t)(value >> 8);
    memory->data[address + 1] = (uint8_t)value;

    return true;
}

bool amd3dc_memory_write32(
    AMD3DC_Memory* memory,
    size_t address,
    uint32_t value)
{
    if (!valid_range(memory, address, 4))
        return false;

    memory->data[address] = (uint8_t)(value >> 24);
    memory->data[address + 1] = (uint8_t)(value >> 16);
    memory->data[address + 2] = (uint8_t)(value >> 8);
    memory->data[address + 3] = (uint8_t)value;

    return true;
}

bool amd3dc_memory_write64(
    AMD3DC_Memory* memory,
    size_t address,
    uint64_t value)
{
    if (!valid_range(memory, address, 8))
        return false;

    memory->data[address] = (uint8_t)(value >> 56);
    memory->data[address + 1] = (uint8_t)(value >> 48);
    memory->data[address + 2] = (uint8_t)(value >> 40);
    memory->data[address + 3] = (uint8_t)(value >> 32);
    memory->data[address + 4] = (uint8_t)(value >> 24);
    memory->data[address + 5] = (uint8_t)(value >> 16);
    memory->data[address + 6] = (uint8_t)(value >> 8);
    memory->data[address + 7] = (uint8_t)value;

    return true;
}