#ifndef AMD3DC_MEMORY_H
#define AMD3DC_MEMORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "amd3dc_types.h"

bool amd3dc_memory_init(AMD3DC_Memory* memory, size_t size);
void amd3dc_memory_free(AMD3DC_Memory* memory);
void amd3dc_memory_clear(AMD3DC_Memory* memory);
uint8_t amd3dc_memory_read8(const AMD3DC_Memory* memory, size_t address);
uint16_t amd3dc_memory_read16(const AMD3DC_Memory* memory, size_t address);
uint32_t amd3dc_memory_read32(const AMD3DC_Memory* memory, size_t address);
uint64_t amd3dc_memory_read64(const AMD3DC_Memory* memory, size_t address);
bool amd3dc_memory_write8(AMD3DC_Memory* memory, size_t address, uint8_t value);
bool amd3dc_memory_write16(AMD3DC_Memory* memory, size_t address, uint16_t value);
bool amd3dc_memory_write32(AMD3DC_Memory* memory, size_t address, uint32_t value);
bool amd3dc_memory_write64(AMD3DC_Memory* memory, size_t address, uint64_t value);

#endif /* AMD3DC_MEMORY_H */
