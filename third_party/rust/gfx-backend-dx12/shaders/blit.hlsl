
Texture2DArray BlitSource : register(t0);
SamplerState BlitSampler : register(s0);

cbuffer Region : register(b0) {
    float2 offset;
    float2 extent;
    float z;
    float level;
};

struct VsOutput {
    float4 pos: SV_POSITION;
    float4 uv: TEXCOORD0;
};

// Create a screen filling triangle
VsOutput vs_blit_2d(uint id: SV_VertexID) {
    float2 coord = float2((id << 1) & 2, id & 2);
    VsOutput output = {
        float4(float2(-1.0, 1.0) + coord * float2(2.0, -2.0), 0.0, 1.0),
        float4(offset + coord * extent, z, level)
    };
    return output;
}

float4 ps_blit_2d(VsOutput input) : SV_TARGET {
    return BlitSource.SampleLevel(BlitSampler, input.uv.xyz, input.uv.w);
}
