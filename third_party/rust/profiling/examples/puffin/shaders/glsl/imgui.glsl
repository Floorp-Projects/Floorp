
// @[immutable_samplers([
//         (
//             mag_filter: Linear,
//             min_filter: Linear,
//             mip_map_mode: Linear,
//             address_mode_u: Repeat,
//             address_mode_v: Repeat,
//             address_mode_w: Repeat,
//         )
// ])]
layout (set = 0, binding = 0) uniform sampler smp;

layout (set = 0, binding = 1) uniform texture2D tex;

// @[internal_buffer]
layout (set = 0, binding = 2) uniform Args {
    mat4 mvp;
} uniform_buffer;