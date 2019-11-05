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
vertex ColorInOut default_fragment(device vertex_t* vertex_array [[ buffer(0) ]],
                                   unsigned int vid [[ vertex_id ]])
{
    ColorInOut out;
    
    out.position = float4(float2(vertex_array[vid].position), 0.0, 1.0);
    out.color = float4(float3(vertex_array[vid].color), 1.0);
    
    return out;
}

// fragment shader function
fragment float4 default_vertex(ColorInOut in [[stage_in]])
{
    return in.color;
};
