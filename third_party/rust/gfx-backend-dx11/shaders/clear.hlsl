cbuffer ClearColorF32   : register(b0) { float4 ClearF32; };
cbuffer ClearColorU32   : register(b0) { uint4 ClearU32; };
cbuffer ClearColorI32   : register(b0) { int4 ClearI32; };
cbuffer ClearColorDepth : register(b0) { float ClearDepth; };

// fullscreen triangle
float4 vs_partial_clear(uint id : SV_VertexID) : SV_Position
{
    return float4(
        float(id / 2) * 4.0 - 1.0,
        float(id % 2) * 4.0 - 1.0,
        0.0,
        1.0
    );
}

// TODO: send constants through VS as flat attributes
float4 ps_partial_clear_float() : SV_Target0 { return ClearF32; }
uint4  ps_partial_clear_uint() : SV_Target0 { return ClearU32; }
int4   ps_partial_clear_int() : SV_Target0 { return ClearI32; }
float  ps_partial_clear_depth() : SV_Depth { return ClearDepth; }
void   ps_partial_clear_stencil() { }
