#ifndef AMD3DC_TYPES_H
#define AMD3DC_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AMD3DC_REGISTER_COUNT 16u
#define AMD3DC_VECTOR_COUNT    8u
#define AMD3DC_TENSOR_REGISTER_COUNT 16u
#define AMD3DC_QUANTUM_REGISTER_COUNT 16u

#define AMD3DC_HBM3_CAPACITY \
    (24ULL * 1024ULL * 1024ULL * 1024ULL)

#ifndef AMD3DC_FIRMWARE_DIR
#define AMD3DC_FIRMWARE_DIR "./firmware/"
#endif

typedef uint64_t AMD3DC_Register;
typedef uint64_t AMD3DC_Address;
typedef uint64_t AMD3DC_Flags;
typedef uint64_t AMD3DC_Layer;

typedef enum
{
    AMD3DC_CORE_BROADWELL = 0,
    AMD3DC_CORE_ALPHA = 1
} AMD3DC_CoreType;

typedef struct
{
    float x;
    float y;
    float z;
    float w;
} AMD3DC_Vector4;

typedef struct
{
    uint8_t* data;
    size_t size;
    uint64_t capacity;
} AMD3DC_Memory;

typedef struct
{
    uint64_t size;
    bool enabled;
} AMD3DC_Cache;

typedef struct
{
    uint32_t id;
    bool enabled;
    bool faulty;
} AMD3DC_Segment;

typedef struct
{
    uint32_t device_id;
    uint32_t vendor_id;
    uint32_t device_features;
    uint32_t driver_features;
    
    uint16_t queue_size;
    uint16_t queue_count;
    
    uint32_t width;
    uint32_t height;
    
    uint8_t* queue_data;
    size_t queue_data_size;
    
    bool initialized;
    bool enabled;
    bool irq_enabled;
} AMD3DC_Virtio;

/*
 * Dedicated Quantum/Tensor chiplet.
 *
 * This is intentionally a separate architectural execution domain from
 * AMD3DC_Core.  The main 3D CPU die dispatches QCX/PQCX work to this die.
 */
typedef struct
{
    bool enabled;
    bool initialized;
    bool busy;

    uint32_t tensor_core_count;
    uint32_t quantum_core_count;
    uint32_t pqc_engine_count;

    AMD3DC_Vector4 tensor_registers[AMD3DC_TENSOR_REGISTER_COUNT];
    uint64_t quantum_registers[AMD3DC_QUANTUM_REGISTER_COUNT];

    uint64_t operations_issued;
    uint64_t tensor_operations;
    uint64_t quantum_operations;
    uint64_t pqc_operations;

} AMD3DC_QuantumTensorDie;

typedef struct
{
    AMD3DC_CoreType type;

    bool enabled;
    bool faulty;

    AMD3DC_Register registers[AMD3DC_REGISTER_COUNT];
    AMD3DC_Vector4 vector_registers[AMD3DC_VECTOR_COUNT];

    AMD3DC_Address pc;
    AMD3DC_Address sp;
    AMD3DC_Flags flags;

    /* Active logical 3D layer context for LAE/CSX. */
    AMD3DC_Layer layer;

    bool halted;
} AMD3DC_Core;

typedef struct
{
    bool initialized;
    bool post_passed;

    uint32_t total_segments;
    uint32_t disabled_segments;
} AMD3DC_Controller;

typedef struct
{
    AMD3DC_Controller controller;

    AMD3DC_Core* cores;
    size_t core_count;

    AMD3DC_Segment* segments;
    size_t segment_count;

    AMD3DC_Cache l1;
    AMD3DC_Cache l2;
    AMD3DC_Cache l3;
    AMD3DC_Cache l4;

    AMD3DC_Memory memory;

    AMD3DC_Virtio virtio;

    /* Separate chiplet containing quantum/tensor/PQC execution engines. */
    AMD3DC_QuantumTensorDie quantum_die;

    bool halted;
} AMD3DC_State;

#endif /* AMD3DC_TYPES_H */
