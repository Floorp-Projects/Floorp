#include "macros.h"
#include <metal_stdlib>
using namespace metal;

//TODO: simplified code path for Metal 2.0?
//> Starting in Metal 2.0, the [[color(n)]] and [[raster_order_group(index)]] indices can
//> also be a function constant. The function constant specified as indices for color and
//> raster order group attributes must be a scalar integer type.

typedef struct {
    float4 coords [[attribute(0)]];
} ClearAttributes;

typedef struct {
    float4 position [[position]];
    uint layer GFX_RENDER_TARGET_ARRAY_INDEX;
} ClearVertexData;

vertex ClearVertexData vs_clear(ClearAttributes in [[stage_in]]) {
    float4 pos = { 0.0, 0.0, in.coords.z, 1.0f };
    pos.xy = in.coords.xy * 2.0 - 1.0;
    return ClearVertexData { pos, uint(in.coords.w) };
}


fragment float4 ps_clear0_float(
    ClearVertexData in [[stage_in]],
    constant float4 &value [[ buffer(0) ]]
) {
  return value;
}

fragment int4 ps_clear0_int(
    ClearVertexData in [[stage_in]],
    constant int4 &value [[ buffer(0) ]]
) {
  return value;
}

fragment uint4 ps_clear0_uint(
    ClearVertexData in [[stage_in]],
    constant uint4 &value [[ buffer(0) ]]
) {
  return value;
}


typedef struct {
    float4 color [[color(1)]];
} Clear1FloatFragment;

fragment Clear1FloatFragment ps_clear1_float(
    ClearVertexData in [[stage_in]],
    constant float4 &value [[ buffer(0) ]]
) {
  return Clear1FloatFragment { value };
}

typedef struct {
    int4 color [[color(1)]];
} Clear1IntFragment;

fragment Clear1IntFragment ps_clear1_int(
    ClearVertexData in [[stage_in]],
    constant int4 &value [[ buffer(0) ]]
) {
  return Clear1IntFragment { value };
}

typedef struct {
    uint4 color [[color(1)]];
} Clear1UintFragment;

fragment Clear1UintFragment ps_clear1_uint(
    ClearVertexData in [[stage_in]],
    constant uint4 &value [[ buffer(0) ]]
) {
  return Clear1UintFragment { value };
}
