#include <metal_stdlib>
using namespace metal;

typedef struct {
    uint value;
    uint length;
} FillBufferValue;

kernel void cs_fill_buffer(
    device uint *buffer [[ buffer(0) ]],
    constant FillBufferValue &fill [[ buffer(1) ]],
    uint index [[ thread_position_in_grid ]]
) {
    if (index < fill.length) {
        buffer[index] = fill.value;
    }
}

typedef struct {
    uint size;
    uint offsets;
} CopyBufferRange;

kernel void cs_copy_buffer(
    device uchar *dest [[ buffer(0) ]],
    device uchar *source [[ buffer(1) ]],
    constant CopyBufferRange &range [[ buffer(2) ]],
    uint index [[ thread_position_in_grid ]]
) {
    if (index < range.size) {
        dest[(range.offsets>>16) + index] = source[(range.offsets & 0xFFFF) + index];
    }
}
