#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Args
{
    float4x4 mvp;
};

struct spvDescriptorSetBuffer0
{
    constant Args* uniform_buffer [[id(2)]];
};

struct main0_out
{
    float2 uv [[user(locn0)]];
    float4 color [[user(locn1)]];
    float4 gl_Position [[position]];
};

struct main0_in
{
    float2 pos [[attribute(0)]];
    float2 in_uv [[attribute(1)]];
    float4 in_color [[attribute(2)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant spvDescriptorSetBuffer0& spvDescriptorSet0 [[buffer(0)]])
{
    main0_out out = {};
    out.uv = in.in_uv;
    out.color = in.in_color;
    out.gl_Position = (*spvDescriptorSet0.uniform_buffer).mvp * float4(in.pos.x, in.pos.y, 0.0, 1.0);
    return out;
}

