#include <metal_stdlib>

using namespace metal;

typedef struct {
	float2 position;
	float3 color;
} vertex_t;

struct ColorInOut {
    float4 position [[position]];
    float4 color;
};

// vertex shader function
vertex ColorInOut triangle_vertex(device vertex_t* vertex_array [[ buffer(0) ]],
                                   unsigned int vid [[ vertex_id ]])
{
    ColorInOut out;

    auto device const &v = vertex_array[vid];
    out.position = float4(v.position.x, v.position.y, 0.0, 1.0);
    out.color = float4(v.color.x, v.color.y, v.color.z, 1.0);

    return out;
}

// fragment shader function
fragment float4 triangle_fragment(ColorInOut in [[stage_in]])
{
    return in.color;
};
