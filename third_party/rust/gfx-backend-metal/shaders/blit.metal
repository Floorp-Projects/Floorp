#include "macros.h"
#include <metal_stdlib>
using namespace metal;

typedef struct {
    float4 src_coords [[attribute(0)]];
    float4 dst_coords [[attribute(1)]];
} BlitAttributes;

typedef struct {
    float4 position [[position]];
    float4 uv;
    uint layer GFX_RENDER_TARGET_ARRAY_INDEX;
} BlitVertexData;

typedef struct {
  float depth [[depth(any)]];
} BlitDepthFragment;


vertex BlitVertexData vs_blit(BlitAttributes in [[stage_in]]) {
    float4 pos = { 0.0, 0.0, in.dst_coords.z, 1.0f };
    pos.xy = in.dst_coords.xy * 2.0 - 1.0;
    return BlitVertexData { pos, in.src_coords, uint(in.dst_coords.w) };
}

fragment float4 ps_blit_1d_float(
    BlitVertexData in [[stage_in]],
    texture1d<float> tex1D [[ texture(0) ]],
    sampler sampler2D [[ sampler(0) ]]
) {
  return tex1D.sample(sampler2D, in.uv.x);
}


fragment float4 ps_blit_1d_array_float(
    BlitVertexData in [[stage_in]],
    texture1d_array<float> tex1DArray [[ texture(0) ]],
    sampler sampler2D [[ sampler(0) ]]
) {
  return tex1DArray.sample(sampler2D, in.uv.x, uint(in.uv.z));
}


fragment float4 ps_blit_2d_float(
    BlitVertexData in [[stage_in]],
    texture2d<float> tex2D [[ texture(0) ]],
    sampler sampler2D [[ sampler(0) ]]
) {
  return tex2D.sample(sampler2D, in.uv.xy, level(in.uv.w));
}

fragment uint4 ps_blit_2d_uint(
    BlitVertexData in [[stage_in]],
    texture2d<uint> tex2D [[ texture(0) ]],
    sampler sampler2D [[ sampler(0) ]]
) {
  return tex2D.sample(sampler2D, in.uv.xy, level(in.uv.w));
}

fragment int4 ps_blit_2d_int(
    BlitVertexData in [[stage_in]],
    texture2d<int> tex2D [[ texture(0) ]],
    sampler sampler2D [[ sampler(0) ]]
) {
  return tex2D.sample(sampler2D, in.uv.xy, level(in.uv.w));
}

fragment BlitDepthFragment ps_blit_2d_depth(
    BlitVertexData in [[stage_in]],
    depth2d<float> tex2D [[ texture(0) ]],
    sampler sampler2D [[ sampler(0) ]]
) {
  float depth = tex2D.sample(sampler2D, in.uv.xy, level(in.uv.w));
  return BlitDepthFragment { depth };
}


fragment float4 ps_blit_2d_array_float(
    BlitVertexData in [[stage_in]],
    texture2d_array<float> tex2DArray [[ texture(0) ]],
    sampler sampler2D [[ sampler(0) ]]
) {
  return tex2DArray.sample(sampler2D, in.uv.xy, uint(in.uv.z), level(in.uv.w));
}

fragment uint4 ps_blit_2d_array_uint(
    BlitVertexData in [[stage_in]],
    texture2d_array<uint> tex2DArray [[ texture(0) ]],
    sampler sampler2D [[ sampler(0) ]]
) {
  return tex2DArray.sample(sampler2D, in.uv.xy, uint(in.uv.z), level(in.uv.w));
}

fragment int4 ps_blit_2d_array_int(
    BlitVertexData in [[stage_in]],
    texture2d_array<int> tex2DArray [[ texture(0) ]],
    sampler sampler2D [[ sampler(0) ]]
) {
  return tex2DArray.sample(sampler2D, in.uv.xy, uint(in.uv.z), level(in.uv.w));
}


fragment float4 ps_blit_3d_float(
    BlitVertexData in [[stage_in]],
    texture3d<float> tex3D [[ texture(0) ]],
    sampler sampler2D [[ sampler(0) ]]
) {
  return tex3D.sample(sampler2D, in.uv.xyz, level(in.uv.w));
}
