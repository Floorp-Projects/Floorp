struct BufferCopy {
    uint4 SrcDst;
};

struct ImageCopy {
    uint4 Src;
    uint4 Dst;
};

struct BufferImageCopy {
    // x=offset, yz=size
    uint4 BufferVars;
    uint4 ImageOffset;
    uint4 ImageExtent;
    uint4 ImageSize;
};

cbuffer CopyConstants : register(b0) {
    BufferCopy BufferCopies;
    ImageCopy ImageCopies;
    BufferImageCopy BufferImageCopies;
};


uint3 GetDestBounds()
{
    return min(
        BufferImageCopies.ImageOffset + BufferImageCopies.ImageExtent,
        BufferImageCopies.ImageSize
    );
}

uint3 GetImageCopyDst(uint3 dispatch_thread_id)
{
    return uint3(ImageCopies.Dst.xy + dispatch_thread_id.xy, ImageCopies.Dst.z);
}

uint3 GetImageCopySrc(uint3 dispatch_thread_id)
{
    return uint3(ImageCopies.Src.xy + dispatch_thread_id.xy, ImageCopies.Src.z);
}

uint3 GetImageDst(uint3 dispatch_thread_id)
{
    return uint3(BufferImageCopies.ImageOffset.xy + dispatch_thread_id.xy, BufferImageCopies.ImageOffset.z);
}

uint3 GetImageSrc(uint3 dispatch_thread_id)
{
    return uint3(BufferImageCopies.ImageOffset.xy + dispatch_thread_id.xy, BufferImageCopies.ImageOffset.z);
}

uint GetBufferDst128(uint3 dispatch_thread_id)
{
    return BufferImageCopies.BufferVars.x + dispatch_thread_id.x * 16 + dispatch_thread_id.y * 16 * max(BufferImageCopies.BufferVars.y, BufferImageCopies.ImageExtent.x);
}
uint GetBufferSrc128(uint3 dispatch_thread_id)
{
    return BufferImageCopies.BufferVars.x + dispatch_thread_id.x * 16 + dispatch_thread_id.y * 16 * max(BufferImageCopies.BufferVars.y, BufferImageCopies.ImageExtent.x);
}

uint GetBufferDst64(uint3 dispatch_thread_id)
{
    return BufferImageCopies.BufferVars.x + dispatch_thread_id.x * 8 + dispatch_thread_id.y * 8 * max(BufferImageCopies.BufferVars.y, BufferImageCopies.ImageExtent.x);
}
uint GetBufferSrc64(uint3 dispatch_thread_id)
{
    return BufferImageCopies.BufferVars.x + dispatch_thread_id.x * 8 + dispatch_thread_id.y * 8 * max(BufferImageCopies.BufferVars.y, BufferImageCopies.ImageExtent.x);
}

uint GetBufferDst32(uint3 dispatch_thread_id)
{
    return BufferImageCopies.BufferVars.x + dispatch_thread_id.x * 4 + dispatch_thread_id.y * 4 * max(BufferImageCopies.BufferVars.y, BufferImageCopies.ImageExtent.x);
}
uint GetBufferSrc32(uint3 dispatch_thread_id)
{
    return BufferImageCopies.BufferVars.x + dispatch_thread_id.x * 4 + dispatch_thread_id.y * 4 * max(BufferImageCopies.BufferVars.y, BufferImageCopies.ImageExtent.x);
}

uint GetBufferDst16(uint3 dispatch_thread_id)
{
    return BufferImageCopies.BufferVars.x + dispatch_thread_id.x * 4 + dispatch_thread_id.y * 2 * max(BufferImageCopies.BufferVars.y, BufferImageCopies.ImageExtent.x);
}
uint GetBufferSrc16(uint3 dispatch_thread_id)
{
    return BufferImageCopies.BufferVars.x + dispatch_thread_id.x * 4 + dispatch_thread_id.y * 2 * max(BufferImageCopies.BufferVars.y, BufferImageCopies.ImageExtent.x);
}

uint GetBufferDst8(uint3 dispatch_thread_id)
{
    return BufferImageCopies.BufferVars.x + dispatch_thread_id.x * 4 + dispatch_thread_id.y * max(BufferImageCopies.BufferVars.y, BufferImageCopies.ImageExtent.x);
}
uint GetBufferSrc8(uint3 dispatch_thread_id)
{
    return BufferImageCopies.BufferVars.x + dispatch_thread_id.x * 4 + dispatch_thread_id.y * max(BufferImageCopies.BufferVars.y, BufferImageCopies.ImageExtent.x);
}


uint4 Uint32ToUint8x4(uint data)
{
    return (data >> uint4(0, 8, 16, 24)) & 0xFF;
}

uint2 Uint32ToUint16x2(uint data)
{
    return (data >> uint2(0, 16)) & 0xFFFF;
}

uint Uint8x4ToUint32(uint4 data)
{
    return dot(min(data, 0xFF), 1 << uint4(0, 8, 16, 24));
}

uint Uint16x2ToUint32(uint2 data)
{
    return dot(min(data, 0xFFFF), 1 << uint2(0, 16));
}

uint2 Uint16ToUint8x2(uint data)
{
    return (data >> uint2(0, 8)) & 0xFF;
}

uint Uint8x2ToUint16(uint2 data)
{
    return dot(min(data, 0xFF), 1 << uint2(0, 8));
}

uint4 Float4ToUint8x4(float4 data)
{
    return uint4(data * 255 + .5f);
}

// Buffers are always R32-aligned
ByteAddressBuffer   BufferCopySrc : register(t0);
RWByteAddressBuffer BufferCopyDst : register(u0);

RWTexture1DArray<uint>  Image1CopyDstR    : register(u0);
RWTexture1DArray<uint2>  Image1CopyDstRg   : register(u0);
RWTexture1DArray<uint4>  Image1CopyDstRgba : register(u0);

Texture2DArray<uint4>   Image2CopySrc     : register(t0);
RWTexture2DArray<uint>  Image2CopyDstR    : register(u0);
RWTexture2DArray<uint2> Image2CopyDstRg   : register(u0);
RWTexture2DArray<uint4> Image2CopyDstRgba : register(u0);

Texture2DArray<float4>  ImageCopy2SrcBgra : register(t0);

// Image<->Image copies
[numthreads(1, 1, 1)]
void cs_copy_image2d_r8g8_image2d_r16(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint3 dst_idx = GetImageCopyDst(dispatch_thread_id);
    uint3 src_idx = GetImageCopySrc(dispatch_thread_id);

    Image2CopyDstR[dst_idx] = Uint8x2ToUint16(Image2CopySrc[src_idx]);
}

[numthreads(1, 1, 1)]
void cs_copy_image2d_r16_image2d_r8g8(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint3 dst_idx = GetImageCopyDst(dispatch_thread_id);
    uint3 src_idx = GetImageCopySrc(dispatch_thread_id);

    Image2CopyDstRg[dst_idx] = Uint16ToUint8x2(Image2CopySrc[src_idx]);
}

[numthreads(1, 1, 1)]
void cs_copy_image2d_r8g8b8a8_image2d_r32(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint3 dst_idx = GetImageCopyDst(dispatch_thread_id);
    uint3 src_idx = GetImageCopySrc(dispatch_thread_id);

    Image2CopyDstR[dst_idx] = Uint8x4ToUint32(Image2CopySrc[src_idx]);
}

[numthreads(1, 1, 1)]
void cs_copy_image2d_r8g8b8a8_image2d_r16g16(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint3 dst_idx = GetImageCopyDst(dispatch_thread_id);
    uint3 src_idx = GetImageCopySrc(dispatch_thread_id);

    Image2CopyDstRg[dst_idx] = Uint32ToUint16x2(Uint8x4ToUint32(Image2CopySrc[src_idx]));
}

[numthreads(1, 1, 1)]
void cs_copy_image2d_r16g16_image2d_r32(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint3 dst_idx = GetImageCopyDst(dispatch_thread_id);
    uint3 src_idx = GetImageCopySrc(dispatch_thread_id);

    Image2CopyDstR[dst_idx] = Uint16x2ToUint32(Image2CopySrc[src_idx]);
}

[numthreads(1, 1, 1)]
void cs_copy_image2d_r16g16_image2d_r8g8b8a8(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint3 dst_idx = GetImageCopyDst(dispatch_thread_id);
    uint3 src_idx = GetImageCopySrc(dispatch_thread_id);

    Image2CopyDstRgba[dst_idx] = Uint32ToUint8x4(Uint16x2ToUint32(Image2CopySrc[src_idx]));
}

[numthreads(1, 1, 1)]
void cs_copy_image2d_r32_image2d_r16g16(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint3 dst_idx = GetImageCopyDst(dispatch_thread_id);
    uint3 src_idx = GetImageCopySrc(dispatch_thread_id);

    Image2CopyDstRg[dst_idx] = Uint32ToUint16x2(Image2CopySrc[src_idx]);
}

[numthreads(1, 1, 1)]
void cs_copy_image2d_r32_image2d_r8g8b8a8(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint3 dst_idx = GetImageCopyDst(dispatch_thread_id);
    uint3 src_idx = GetImageCopySrc(dispatch_thread_id);

    Image2CopyDstRgba[dst_idx] = Uint32ToUint8x4(Image2CopySrc[src_idx]);
}

//#define COPY_1D_NUM_THREAD 64 //TODO
#define COPY_2D_NUM_THREAD_X 8
#define COPY_2D_NUM_THREAD_Y 8

// Buffer<->Image copies

// R32G32B32A32
[numthreads(COPY_2D_NUM_THREAD_X, COPY_2D_NUM_THREAD_Y, 1)]
void cs_copy_buffer_image2d_r32g32b32a32(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint3 dst_idx = GetImageDst(dispatch_thread_id);
    uint3 bounds = GetDestBounds();
    if (dst_idx.x >= bounds.x || dst_idx.y >= bounds.y) {
        return;
    }

    uint src_idx = GetBufferSrc128(dispatch_thread_id);

    Image2CopyDstRgba[dst_idx] = uint4(
        BufferCopySrc.Load(src_idx),
        BufferCopySrc.Load(src_idx + 1 * 4),
        BufferCopySrc.Load(src_idx + 2 * 4),
        BufferCopySrc.Load(src_idx + 3 * 4)
    );
}

[numthreads(COPY_2D_NUM_THREAD_X, COPY_2D_NUM_THREAD_Y, 1)]
void cs_copy_image2d_r32g32b32a32_buffer(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint3 src_idx = GetImageSrc(dispatch_thread_id);
    uint3 bounds = GetDestBounds();
    if (src_idx.x >= bounds.x || src_idx.y >= bounds.y) {
        return;
    }

    uint4 data = Image2CopySrc[src_idx];
    uint dst_idx = GetBufferDst128(dispatch_thread_id);

    BufferCopyDst.Store(dst_idx,         data.x);
    BufferCopyDst.Store(dst_idx + 1 * 4, data.y);
    BufferCopyDst.Store(dst_idx + 2 * 4, data.z);
    BufferCopyDst.Store(dst_idx + 3 * 4, data.w);
}

// R32G32
[numthreads(COPY_2D_NUM_THREAD_X, COPY_2D_NUM_THREAD_Y, 1)]
void cs_copy_buffer_image2d_r32g32(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint3 dst_idx = GetImageDst(dispatch_thread_id);
    uint3 bounds = GetDestBounds();
    if (dst_idx.x >= bounds.x || dst_idx.y >= bounds.y) {
        return;
    }

    uint src_idx = GetBufferSrc64(dispatch_thread_id);

    Image2CopyDstRg[dst_idx] = uint2(
        BufferCopySrc.Load(src_idx),
        BufferCopySrc.Load(src_idx + 1 * 4)
    );
}

[numthreads(COPY_2D_NUM_THREAD_X, COPY_2D_NUM_THREAD_Y, 1)]
void cs_copy_image2d_r32g32_buffer(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint3 src_idx = GetImageSrc(dispatch_thread_id);
    uint3 bounds = GetDestBounds();
    if (src_idx.x >= bounds.x || src_idx.y >= bounds.y) {
        return;
    }

    uint2 data = Image2CopySrc[src_idx].rg;
    uint dst_idx = GetBufferDst64(dispatch_thread_id);

    BufferCopyDst.Store(dst_idx        , data.x);
    BufferCopyDst.Store(dst_idx + 1 * 4, data.y);
}

// R16G16B16A16
[numthreads(COPY_2D_NUM_THREAD_X, COPY_2D_NUM_THREAD_Y, 1)]
void cs_copy_buffer_image2d_r16g16b16a16(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint3 dst_idx = GetImageDst(dispatch_thread_id);
    uint3 bounds = GetDestBounds();
    if (dst_idx.x >= bounds.x || dst_idx.y >= bounds.y) {
        return;
    }

    uint src_idx = GetBufferSrc64(dispatch_thread_id);

    Image2CopyDstRgba[dst_idx] = uint4(
        Uint32ToUint16x2(BufferCopySrc.Load(src_idx)),
        Uint32ToUint16x2(BufferCopySrc.Load(src_idx + 1 * 4))
    );
}

[numthreads(COPY_2D_NUM_THREAD_X, COPY_2D_NUM_THREAD_Y, 1)]
void cs_copy_image2d_r16g16b16a16_buffer(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint3 src_idx = GetImageSrc(dispatch_thread_id);
    uint3 bounds = GetDestBounds();
    if (src_idx.x >= bounds.x || src_idx.y >= bounds.y) {
        return;
    }

    uint4 data = Image2CopySrc[src_idx];
    uint dst_idx = GetBufferDst64(dispatch_thread_id);

    BufferCopyDst.Store(dst_idx,         Uint16x2ToUint32(data.xy));
    BufferCopyDst.Store(dst_idx + 1 * 4, Uint16x2ToUint32(data.zw));
}

// R32
[numthreads(COPY_2D_NUM_THREAD_X, 1, 1)]
void cs_copy_buffer_image1d_r32(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint3 dst_idx = GetImageDst(dispatch_thread_id);
    uint3 bounds = GetDestBounds();
    if (dst_idx.x >= bounds.x) {
        return;
    }

    uint src_idx = GetBufferSrc32(dispatch_thread_id);

    Image1CopyDstR[dst_idx.xz] = BufferCopySrc.Load(src_idx);
}
[numthreads(COPY_2D_NUM_THREAD_X, COPY_2D_NUM_THREAD_Y, 1)]
void cs_copy_buffer_image2d_r32(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint3 dst_idx = GetImageDst(dispatch_thread_id);
    uint3 bounds = GetDestBounds();
    if (dst_idx.x >= bounds.x || dst_idx.y >= bounds.y) {
        return;
    }

    uint src_idx = GetBufferSrc32(dispatch_thread_id);

    Image2CopyDstR[dst_idx] = BufferCopySrc.Load(src_idx);
}

[numthreads(COPY_2D_NUM_THREAD_X, COPY_2D_NUM_THREAD_Y, 1)]
void cs_copy_image2d_r32_buffer(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint3 src_idx = GetImageSrc(dispatch_thread_id);
    uint3 bounds = GetDestBounds();
    if (src_idx.x >= bounds.x || src_idx.y >= bounds.y) {
        return;
    }

    uint dst_idx = GetBufferDst32(dispatch_thread_id);

    BufferCopyDst.Store(dst_idx, Image2CopySrc[src_idx].r);
}

// R16G16
[numthreads(COPY_2D_NUM_THREAD_X, COPY_2D_NUM_THREAD_Y, 1)]
void cs_copy_buffer_image2d_r16g16(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint3 dst_idx = GetImageDst(dispatch_thread_id);
    uint3 bounds = GetDestBounds();
    if (dst_idx.x >= bounds.x || dst_idx.y >= bounds.y) {
        return;
    }

    uint src_idx = GetBufferSrc32(dispatch_thread_id);

    Image2CopyDstRg[dst_idx] = Uint32ToUint16x2(BufferCopySrc.Load(src_idx));
}

[numthreads(COPY_2D_NUM_THREAD_X, COPY_2D_NUM_THREAD_Y, 1)]
void cs_copy_image2d_r16g16_buffer(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint3 src_idx = GetImageSrc(dispatch_thread_id);
    uint3 bounds = GetDestBounds();
    if (src_idx.x >= bounds.x || src_idx.y >= bounds.y) {
        return;
    }

    uint dst_idx = GetBufferDst32(dispatch_thread_id);

    BufferCopyDst.Store(dst_idx, Uint16x2ToUint32(Image2CopySrc[src_idx].xy));
}

// R8G8B8A8
[numthreads(COPY_2D_NUM_THREAD_X, 1, 1)]
void cs_copy_buffer_image1d_r8g8b8a8(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint3 dst_idx = GetImageDst(dispatch_thread_id);
    uint3 bounds = GetDestBounds();
    if (dst_idx.x >= bounds.x) {
        return;
    }

    uint src_idx = GetBufferSrc32(dispatch_thread_id);

    Image1CopyDstRgba[dst_idx.xz] = Uint32ToUint8x4(BufferCopySrc.Load(src_idx));
}
[numthreads(COPY_2D_NUM_THREAD_X, COPY_2D_NUM_THREAD_Y, 1)]
void cs_copy_buffer_image2d_r8g8b8a8(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint3 dst_idx = GetImageDst(dispatch_thread_id);
    uint3 bounds = GetDestBounds();
    if (dst_idx.x >= bounds.x || dst_idx.y >= bounds.y) {
        return;
    }

    uint src_idx = GetBufferSrc32(dispatch_thread_id);

    Image2CopyDstRgba[dst_idx] = Uint32ToUint8x4(BufferCopySrc.Load(src_idx));
}

[numthreads(COPY_2D_NUM_THREAD_X, COPY_2D_NUM_THREAD_Y, 1)]
void cs_copy_image2d_r8g8b8a8_buffer(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint3 src_idx = GetImageSrc(dispatch_thread_id);
    uint3 bounds = GetDestBounds();
    if (src_idx.x >= bounds.x || src_idx.y >= bounds.y) {
        return;
    }

    uint dst_idx = GetBufferDst32(dispatch_thread_id);

    BufferCopyDst.Store(dst_idx, Uint8x4ToUint32(Image2CopySrc[src_idx]));
}

// B8G8R8A8
[numthreads(COPY_2D_NUM_THREAD_X, COPY_2D_NUM_THREAD_Y, 1)]
void cs_copy_image2d_b8g8r8a8_buffer(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint3 src_idx = GetImageSrc(dispatch_thread_id);
    uint3 bounds = GetDestBounds();
    if (src_idx.x >= bounds.x || src_idx.y >= bounds.y) {
        return;
    }

    uint dst_idx = GetBufferDst32(dispatch_thread_id);

    BufferCopyDst.Store(dst_idx, Uint8x4ToUint32(Float4ToUint8x4(ImageCopy2SrcBgra[src_idx].bgra)));
}

// R16
[numthreads(COPY_2D_NUM_THREAD_X, COPY_2D_NUM_THREAD_Y, 1)]
void cs_copy_buffer_image2d_r16(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint3 dst_idx = GetImageDst(uint3(2, 1, 0) * dispatch_thread_id);
    uint3 bounds = GetDestBounds();
    if (dst_idx.x >= bounds.x || dst_idx.y >= bounds.y) {
        return;
    }

    uint src_idx = GetBufferSrc16(dispatch_thread_id);
    uint2 data = Uint32ToUint16x2(BufferCopySrc.Load(src_idx));

    uint remaining_x = bounds.x - dst_idx.x;

    if (remaining_x >= 2) {
        Image2CopyDstR[dst_idx + uint3(1, 0, 0)] = data.y;
    } 
    if (remaining_x >= 1) {
        Image2CopyDstR[dst_idx + uint3(0, 0, 0)] = data.x;
    }
}

[numthreads(COPY_2D_NUM_THREAD_X, COPY_2D_NUM_THREAD_Y, 1)]
void cs_copy_image2d_r16_buffer(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint3 src_idx = GetImageSrc(uint3(2, 1, 0) * dispatch_thread_id);
    uint3 bounds = GetDestBounds();
    if (src_idx.x >= bounds.x || src_idx.y >= bounds.y) {
        return;
    }

    uint dst_idx = GetBufferDst16(dispatch_thread_id);

    uint upper = Image2CopySrc[src_idx].r;
    uint lower = Image2CopySrc[src_idx + uint3(1, 0, 0)].r;

    BufferCopyDst.Store(dst_idx, Uint16x2ToUint32(uint2(upper, lower)));
}

// R8G8
[numthreads(COPY_2D_NUM_THREAD_X, 1, 1)]
void cs_copy_buffer_image1d_r8g8(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint3 dst_idx = GetImageDst(uint3(2, 1, 0) * dispatch_thread_id);
    uint3 bounds = GetDestBounds();
    if (dst_idx.x >= bounds.x) {
        return;
    }

    uint src_idx = GetBufferSrc16(dispatch_thread_id);

    uint4 data = Uint32ToUint8x4(BufferCopySrc.Load(src_idx));

    uint remaining_x = bounds.x - dst_idx.x;

    if (remaining_x >= 2) {
        Image1CopyDstRg[dst_idx.xz + uint2(1, 0)] = data.zw;
    } 
    if (remaining_x >= 1) {
        Image1CopyDstRg[dst_idx.xz + uint2(0, 0)] = data.xy;
    }
}

[numthreads(COPY_2D_NUM_THREAD_X, COPY_2D_NUM_THREAD_Y, 1)]
void cs_copy_buffer_image2d_r8g8(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint3 dst_idx = GetImageDst(uint3(2, 1, 0) * dispatch_thread_id);
    uint3 bounds = GetDestBounds();
    if (dst_idx.x >= bounds.x || dst_idx.y >= bounds.y) {
        return;
    }

    uint src_idx = GetBufferSrc16(dispatch_thread_id);

    uint4 data = Uint32ToUint8x4(BufferCopySrc.Load(src_idx));

    uint remaining_x = bounds.x - dst_idx.x;

    if (remaining_x >= 2) {
        Image2CopyDstRg[dst_idx + uint3(1, 0, 0)] = data.zw;
    } 
    if (remaining_x >= 1) {
        Image2CopyDstRg[dst_idx + uint3(0, 0, 0)] = data.xy;
    }
}

[numthreads(COPY_2D_NUM_THREAD_X, COPY_2D_NUM_THREAD_Y, 1)]
void cs_copy_image2d_r8g8_buffer(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint3 src_idx = GetImageSrc(uint3(2, 1, 0) * dispatch_thread_id);
    uint3 bounds = GetDestBounds();
    if (src_idx.x >= bounds.x || src_idx.y >= bounds.y) {
        return;
    }

    uint dst_idx = GetBufferDst16(dispatch_thread_id);

    uint2 lower = Image2CopySrc[src_idx].xy;
    uint2 upper = Image2CopySrc[src_idx + uint3(1, 0, 0)].xy;

    BufferCopyDst.Store(dst_idx, Uint8x4ToUint32(uint4(lower.x, lower.y, upper.x, upper.y)));
}

// R8
[numthreads(COPY_2D_NUM_THREAD_X, 1, 1)]
void cs_copy_buffer_image1d_r8(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint3 dst_idx = GetImageDst(uint3(4, 1, 0) * dispatch_thread_id);
    uint3 bounds = GetDestBounds();
    if (dst_idx.x >= bounds.x) {
        return;
    }

    uint src_idx = GetBufferSrc8(dispatch_thread_id);
    uint4 data = Uint32ToUint8x4(BufferCopySrc.Load(src_idx));

    uint remaining_x = bounds.x - dst_idx.x;

    if (remaining_x >= 4) {
        Image1CopyDstR[dst_idx.xz + uint2(3, 0)] = data.w;
    } 
    if (remaining_x >= 3) {
        Image1CopyDstR[dst_idx.xz + uint2(2, 0)] = data.z;
    } 
    if (remaining_x >= 2) {
        Image1CopyDstR[dst_idx.xz + uint2(1, 0)] = data.y;
    } 
    if (remaining_x >= 1) {
        Image1CopyDstR[dst_idx.xz + uint2(0, 0)] = data.x;
    }
}
[numthreads(COPY_2D_NUM_THREAD_X, COPY_2D_NUM_THREAD_Y, 1)]
void cs_copy_buffer_image2d_r8(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint3 dst_idx = GetImageDst(uint3(4, 1, 0) * dispatch_thread_id);
    uint3 bounds = GetDestBounds();
    if (dst_idx.x >= bounds.x || dst_idx.y >= bounds.y) {
        return;
    }

    uint src_idx = GetBufferSrc8(dispatch_thread_id);
    uint4 data = Uint32ToUint8x4(BufferCopySrc.Load(src_idx));

    uint remaining_x = bounds.x - dst_idx.x;

    if (remaining_x >= 4) {
        Image2CopyDstR[dst_idx + uint3(3, 0, 0)] = data.w;
    } 
    if (remaining_x >= 3) {
        Image2CopyDstR[dst_idx + uint3(2, 0, 0)] = data.z;
    } 
    if (remaining_x >= 2) {
        Image2CopyDstR[dst_idx + uint3(1, 0, 0)] = data.y;
    } 
    if (remaining_x >= 1) {
        Image2CopyDstR[dst_idx + uint3(0, 0, 0)] = data.x;
    }
}
[numthreads(COPY_2D_NUM_THREAD_X, COPY_2D_NUM_THREAD_Y, 1)]
void cs_copy_image2d_r8_buffer(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint3 src_idx = GetImageSrc(uint3(4, 1, 0) * dispatch_thread_id);
    uint3 bounds = GetDestBounds();
    if (src_idx.x >= bounds.x || src_idx.y >= bounds.y) {
        return;
    }

    uint dst_idx = GetBufferDst8(dispatch_thread_id);

    BufferCopyDst.Store(dst_idx, Uint8x4ToUint32(uint4(
        Image2CopySrc[src_idx].r,
        Image2CopySrc[src_idx + uint3(1, 0, 0)].r,
        Image2CopySrc[src_idx + uint3(2, 0, 0)].r,
        Image2CopySrc[src_idx + uint3(3, 0, 0)].r
    )));
}
