/*
 *  Copyright (c) 2011 The LibYuv project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/planar_functions.h"

#include <string.h>

#include "libyuv/cpu_id.h"
#include "row.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

#if defined(__ARM_NEON__) && !defined(YUV_DISABLE_ASM)
#define HAS_SPLITUV_NEON
// Reads 16 pairs of UV and write even values to dst_u and odd to dst_v
// Alignment requirement: 16 bytes for pointers, and multiple of 16 pixels.
static void SplitUV_NEON(const uint8* src_uv,
                         uint8* dst_u, uint8* dst_v, int pix) {
  asm volatile (
    "1:                                        \n"
    "vld2.u8    {q0,q1}, [%0]!                 \n"  // load 16 pairs of UV
    "vst1.u8    {q0}, [%1]!                    \n"  // store U
    "vst1.u8    {q1}, [%2]!                    \n"  // Store V
    "subs       %3, %3, #16                    \n"  // 16 processed per loop
    "bhi        1b                             \n"
    : "+r"(src_uv),
      "+r"(dst_u),
      "+r"(dst_v),
      "+r"(pix)             // Output registers
    :                       // Input registers
    : "memory", "cc", "q0", "q1" // Clobber List
  );
}

#elif defined(_M_IX86) && !defined(YUV_DISABLE_ASM)
#define HAS_SPLITUV_SSE2
__declspec(naked)
static void SplitUV_SSE2(const uint8* src_uv,
                         uint8* dst_u, uint8* dst_v, int pix) {
  __asm {
    push       edi
    mov        eax, [esp + 4 + 4]    // src_uv
    mov        edx, [esp + 4 + 8]    // dst_u
    mov        edi, [esp + 4 + 12]   // dst_v
    mov        ecx, [esp + 4 + 16]   // pix
    pcmpeqb    xmm5, xmm5            // generate mask 0x00ff00ff
    psrlw      xmm5, 8
    sub        edi, edx

  convertloop:
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    lea        eax,  [eax + 32]
    movdqa     xmm2, xmm0
    movdqa     xmm3, xmm1
    pand       xmm0, xmm5   // even bytes
    pand       xmm1, xmm5
    packuswb   xmm0, xmm1
    psrlw      xmm2, 8      // odd bytes
    psrlw      xmm3, 8
    packuswb   xmm2, xmm3
    movdqa     [edx], xmm0
    movdqa     [edx + edi], xmm2
    lea        edx, [edx + 16]
    sub        ecx, 16
    ja         convertloop
    pop        edi
    ret
  }
}

#elif (defined(__x86_64__) || defined(__i386__)) && !defined(YUV_DISABLE_ASM)
#define HAS_SPLITUV_SSE2
static void SplitUV_SSE2(const uint8* src_uv,
                         uint8* dst_u, uint8* dst_v, int pix) {
 asm volatile (
  "pcmpeqb    %%xmm5,%%xmm5                    \n"
  "psrlw      $0x8,%%xmm5                      \n"
  "sub        %1,%2                            \n"

"1:                                            \n"
  "movdqa     (%0),%%xmm0                      \n"
  "movdqa     0x10(%0),%%xmm1                  \n"
  "lea        0x20(%0),%0                      \n"
  "movdqa     %%xmm0,%%xmm2                    \n"
  "movdqa     %%xmm1,%%xmm3                    \n"
  "pand       %%xmm5,%%xmm0                    \n"
  "pand       %%xmm5,%%xmm1                    \n"
  "packuswb   %%xmm1,%%xmm0                    \n"
  "psrlw      $0x8,%%xmm2                      \n"
  "psrlw      $0x8,%%xmm3                      \n"
  "packuswb   %%xmm3,%%xmm2                    \n"
  "movdqa     %%xmm0,(%1)                      \n"
  "movdqa     %%xmm2,(%1,%2)                   \n"
  "lea        0x10(%1),%1                      \n"
  "sub        $0x10,%3                         \n"
  "ja         1b                               \n"
  : "+r"(src_uv),     // %0
    "+r"(dst_u),      // %1
    "+r"(dst_v),      // %2
    "+r"(pix)         // %3
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm5"
#endif
);
}
#endif

static void SplitUV_C(const uint8* src_uv,
                      uint8* dst_u, uint8* dst_v, int pix) {
  // Copy a row of UV.
  for (int x = 0; x < pix; ++x) {
    dst_u[0] = src_uv[0];
    dst_v[0] = src_uv[1];
    src_uv += 2;
    dst_u += 1;
    dst_v += 1;
  }
}

// CopyRows copys 'count' bytes using a 16 byte load/store, 64 bytes at time
#if defined(_M_IX86) && !defined(YUV_DISABLE_ASM)
#define HAS_COPYROW_SSE2
__declspec(naked)
void CopyRow_SSE2(const uint8* src, uint8* dst, int count) {
  __asm {
    mov        eax, [esp + 4]   // src
    mov        edx, [esp + 8]   // dst
    mov        ecx, [esp + 12]  // count
    sub        edx, eax
  convertloop:
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    movdqa     [eax + edx], xmm0
    movdqa     [eax + edx + 16], xmm1
    lea        eax, [eax + 32]
    sub        ecx, 32
    ja         convertloop
    ret
  }
}

#define HAS_COPYROW_X86
__declspec(naked)
void CopyRow_X86(const uint8* src, uint8* dst, int count) {
  __asm {
    mov        eax, esi
    mov        edx, edi
    mov        esi, [esp + 4]   // src
    mov        edi, [esp + 8]   // dst
    mov        ecx, [esp + 12]  // count
    shr        ecx, 2
    rep movsd
    mov        edi, edx
    mov        esi, eax
    ret
  }
}
#elif (defined(__x86_64__) || defined(__i386__)) && !defined(YUV_DISABLE_ASM)
#define HAS_COPYROW_SSE2
void CopyRow_SSE2(const uint8* src, uint8* dst, int count) {
  asm volatile (
  "sub        %0,%1                            \n"
  "1:                                          \n"
    "movdqa    (%0),%%xmm0                     \n"
    "movdqa    0x10(%0),%%xmm1                 \n"
    "movdqa    %%xmm0,(%0,%1)                  \n"
    "movdqa    %%xmm1,0x10(%0,%1)              \n"
    "lea       0x20(%0),%0                     \n"
    "sub       $0x20,%2                        \n"
    "ja        1b                              \n"
  : "+r"(src),   // %0
    "+r"(dst),   // %1
    "+r"(count)  // %2
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1"
#endif
  );
}

#define HAS_COPYROW_X86
void CopyRow_X86(const uint8* src, uint8* dst, int width) {
  size_t width_tmp = static_cast<size_t>(width);
  asm volatile (
    "shr       $0x2,%2                         \n"
    "rep movsl                                 \n"
  : "+S"(src),  // %0
    "+D"(dst),  // %1
    "+c"(width_tmp) // %2
  :
  : "memory", "cc"
  );
}
#endif

void CopyRow_C(const uint8* src, uint8* dst, int count) {
  memcpy(dst, src, count);
}

// Copy a plane of data
void CopyPlane(const uint8* src_y, int src_stride_y,
               uint8* dst_y, int dst_stride_y,
               int width, int height) {
  void (*CopyRow)(const uint8* src, uint8* dst, int width);
#if defined(HAS_COPYROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(width, 32) &&
      IS_ALIGNED(src_y, 16) && IS_ALIGNED(src_stride_y, 16) &&
      IS_ALIGNED(dst_y, 16) && IS_ALIGNED(dst_stride_y, 16)) {
    CopyRow = CopyRow_SSE2;
  } else
#endif
#if defined(HAS_COPYROW_X86)
  if (IS_ALIGNED(width, 4) &&
      IS_ALIGNED(src_y, 4) && IS_ALIGNED(src_stride_y, 4) &&
      IS_ALIGNED(dst_y, 4) && IS_ALIGNED(dst_stride_y, 4)) {
    CopyRow = CopyRow_X86;
  } else
#endif
  {
    CopyRow = CopyRow_C;
  }

  // Copy plane
  for (int y = 0; y < height; ++y) {
    CopyRow(src_y, dst_y, width);
    src_y += src_stride_y;
    dst_y += dst_stride_y;
  }
}

// Copy I420 with optional flipping
int I420Copy(const uint8* src_y, int src_stride_y,
             const uint8* src_u, int src_stride_u,
             const uint8* src_v, int src_stride_v,
             uint8* dst_y, int dst_stride_y,
             uint8* dst_u, int dst_stride_u,
             uint8* dst_v, int dst_stride_v,
             int width, int height) {
  if (!src_y || !src_u || !src_v ||
      !dst_y || !dst_u || !dst_v ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    int halfheight = (height + 1) >> 1;
    src_y = src_y + (height - 1) * src_stride_y;
    src_u = src_u + (halfheight - 1) * src_stride_u;
    src_v = src_v + (halfheight - 1) * src_stride_v;
    src_stride_y = -src_stride_y;
    src_stride_u = -src_stride_u;
    src_stride_v = -src_stride_v;
  }

  int halfwidth = (width + 1) >> 1;
  int halfheight = (height + 1) >> 1;
  CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height);
  CopyPlane(src_u, src_stride_u, dst_u, dst_stride_u, halfwidth, halfheight);
  CopyPlane(src_v, src_stride_v, dst_v, dst_stride_v, halfwidth, halfheight);
  return 0;
}

// Copy ARGB with optional flipping
int ARGBCopy(const uint8* src_argb, int src_stride_argb,
             uint8* dst_argb, int dst_stride_argb,
             int width, int height) {
  if (!src_argb ||
      !dst_argb ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }

  CopyPlane(src_argb, src_stride_argb, dst_argb, dst_stride_argb,
            width * 4, height);
  return 0;
}

int I420Mirror(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height) {
  if (!src_y || !src_u || !src_v ||
      !dst_y || !dst_u || !dst_v ||
      width <= 0 || height == 0) {
    return -1;
  }
  int halfwidth = (width + 1) >> 1;
  int halfheight = (height + 1) >> 1;

  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    halfheight = (height + 1) >> 1;
    src_y = src_y + (height - 1) * src_stride_y;
    src_u = src_u + (halfheight - 1) * src_stride_u;
    src_v = src_v + (halfheight - 1) * src_stride_v;
    src_stride_y = -src_stride_y;
    src_stride_u = -src_stride_u;
    src_stride_v = -src_stride_v;
  }
  void (*ReverseRow)(const uint8* src, uint8* dst, int width);
#if defined(HAS_REVERSE_ROW_NEON)
  if (TestCpuFlag(kCpuHasNEON) &&
      IS_ALIGNED(width, 32)) {
    ReverseRow = ReverseRow_NEON;
  } else
#endif
#if defined(HAS_REVERSE_ROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(width, 32) &&
      IS_ALIGNED(src_y, 16) && IS_ALIGNED(src_stride_y, 16) &&
      IS_ALIGNED(src_u, 16) && IS_ALIGNED(src_stride_u, 16) &&
      IS_ALIGNED(src_v, 16) && IS_ALIGNED(src_stride_v, 16) &&
      IS_ALIGNED(dst_y, 16) && IS_ALIGNED(dst_stride_y, 16) &&
      IS_ALIGNED(dst_u, 16) && IS_ALIGNED(dst_stride_u, 16) &&
      IS_ALIGNED(dst_v, 16) && IS_ALIGNED(dst_stride_v, 16)) {
    ReverseRow = ReverseRow_SSSE3;
  } else
#endif
#if defined(HAS_REVERSE_ROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(width, 32) &&
      IS_ALIGNED(src_y, 16) && IS_ALIGNED(src_stride_y, 16) &&
      IS_ALIGNED(src_u, 16) && IS_ALIGNED(src_stride_u, 16) &&
      IS_ALIGNED(src_v, 16) && IS_ALIGNED(src_stride_v, 16) &&
      IS_ALIGNED(dst_y, 16) && IS_ALIGNED(dst_stride_y, 16) &&
      IS_ALIGNED(dst_u, 16) && IS_ALIGNED(dst_stride_u, 16) &&
      IS_ALIGNED(dst_v, 16) && IS_ALIGNED(dst_stride_v, 16)) {
    ReverseRow = ReverseRow_SSE2;
  } else
#endif
  {
    ReverseRow = ReverseRow_C;
  }

  // Y Plane
  int y;
  for (y = 0; y < height; ++y) {
    ReverseRow(src_y, dst_y, width);
    src_y += src_stride_y;
    dst_y += dst_stride_y;
  }
  // U Plane
  for (y = 0; y < halfheight; ++y) {
    ReverseRow(src_u, dst_u, halfwidth);
    src_u += src_stride_u;
    dst_u += dst_stride_u;
  }
  // V Plane
  for (y = 0; y < halfheight; ++y) {
    ReverseRow(src_v, dst_v, halfwidth);
    src_v += src_stride_v;
    dst_v += dst_stride_v;
  }
  return 0;
}

#if defined(_M_IX86) && !defined(YUV_DISABLE_ASM)
#define HAS_HALFROW_SSE2
__declspec(naked)
static void HalfRow_SSE2(const uint8* src_uv, int src_uv_stride,
                         uint8* dst_uv, int pix) {
  __asm {
    push       edi
    mov        eax, [esp + 4 + 4]    // src_uv
    mov        edx, [esp + 4 + 8]    // src_uv_stride
    mov        edi, [esp + 4 + 12]   // dst_v
    mov        ecx, [esp + 4 + 16]   // pix
    sub        edi, eax

  convertloop:
    movdqa     xmm0, [eax]
    pavgb      xmm0, [eax + edx]
    movdqa     [eax + edi], xmm0
    lea        eax,  [eax + 16]
    sub        ecx, 16
    ja         convertloop
    pop        edi
    ret
  }
}

#elif (defined(__x86_64__) || defined(__i386__)) && !defined(YUV_DISABLE_ASM)
#define HAS_HALFROW_SSE2
static void HalfRow_SSE2(const uint8* src_uv, int src_uv_stride,
                         uint8* dst_uv, int pix) {
 asm volatile (
  "sub        %0,%1                            \n"
"1:                                            \n"
  "movdqa     (%0),%%xmm0                      \n"
  "pavgb      (%0,%3),%%xmm0                   \n"
  "movdqa     %%xmm0,(%0,%1)                   \n"
  "lea        0x10(%0),%0                      \n"
  "sub        $0x10,%2                         \n"
  "ja         1b                               \n"
  : "+r"(src_uv),  // %0
    "+r"(dst_uv),  // %1
    "+r"(pix)      // %2
  : "r"(static_cast<intptr_t>(src_uv_stride))  // %3
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0"
#endif
);
}
#endif

void HalfRow_C(const uint8* src_uv, int src_uv_stride,
               uint8* dst_uv, int pix) {
  for (int x = 0; x < pix; ++x) {
    dst_uv[x] = (src_uv[x] + src_uv[src_uv_stride + x] + 1) >> 1;
  }
}

int I422ToI420(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height) {
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_y = src_y + (height - 1) * src_stride_y;
    src_u = src_u + (height - 1) * src_stride_u;
    src_v = src_v + (height - 1) * src_stride_v;
    src_stride_y = -src_stride_y;
    src_stride_u = -src_stride_u;
    src_stride_v = -src_stride_v;
  }
  int halfwidth = (width + 1) >> 1;
  void (*HalfRow)(const uint8* src_uv, int src_uv_stride,
                  uint8* dst_uv, int pix);
#if defined(HAS_HALFROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(halfwidth, 16) &&
      IS_ALIGNED(src_u, 16) && IS_ALIGNED(src_stride_u, 16) &&
      IS_ALIGNED(src_v, 16) && IS_ALIGNED(src_stride_v, 16) &&
      IS_ALIGNED(dst_u, 16) && IS_ALIGNED(dst_stride_u, 16) &&
      IS_ALIGNED(dst_v, 16) && IS_ALIGNED(dst_stride_v, 16)) {
    HalfRow = HalfRow_SSE2;
  } else
#endif
  {
    HalfRow = HalfRow_C;
  }

  // Copy Y plane
  CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height);

  // SubSample U plane.
  int y;
  for (y = 0; y < height - 1; y += 2) {
    HalfRow(src_u, src_stride_u, dst_u, halfwidth);
    src_u += src_stride_u * 2;
    dst_u += dst_stride_u;
  }
  if (height & 1) {
    HalfRow(src_u, 0, dst_u, halfwidth);
  }

  // SubSample V plane.
  for (y = 0; y < height - 1; y += 2) {
    HalfRow(src_v, src_stride_v, dst_v, halfwidth);
    src_v += src_stride_v * 2;
    dst_v += dst_stride_v;
  }
  if (height & 1) {
    HalfRow(src_v, 0, dst_v, halfwidth);
  }
  return 0;
}

int I420ToI422(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height) {
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_y = dst_y + (height - 1) * dst_stride_y;
    dst_u = dst_u + (height - 1) * dst_stride_u;
    dst_v = dst_v + (height - 1) * dst_stride_v;
    dst_stride_y = -dst_stride_y;
    dst_stride_u = -dst_stride_u;
    dst_stride_v = -dst_stride_v;
  }
  // Copy Y plane
  CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height);

  int halfwidth = (width + 1) >> 1;
  // UpSample U plane.
  int y;
  for (y = 0; y < height - 1; y += 2) {
    memcpy(dst_u, src_u, halfwidth);
    memcpy(dst_u + dst_stride_u, src_u, halfwidth);
    src_u += src_stride_u;
    dst_u += dst_stride_u * 2;
  }
  if (height & 1) {
    memcpy(dst_u, src_u, halfwidth);
  }

  // UpSample V plane.
  for (y = 0; y < height - 1; y += 2) {
    memcpy(dst_v, src_v, halfwidth);
    memcpy(dst_v + dst_stride_v, src_v, halfwidth);
    src_v += src_stride_v;
    dst_v += dst_stride_v * 2;
  }
  if (height & 1) {
    memcpy(dst_v, src_v, halfwidth);
  }
  return 0;
}

// Blends 32x2 pixels to 16x1
// source in scale.cc
#if defined(__ARM_NEON__) && !defined(YUV_DISABLE_ASM)
#define HAS_SCALEROWDOWN2_NEON
void ScaleRowDown2Int_NEON(const uint8* src_ptr, int src_stride,
                           uint8* dst, int dst_width);
#elif (defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)) && \
    !defined(YUV_DISABLE_ASM)
void ScaleRowDown2Int_SSE2(const uint8* src_ptr, int src_stride,
                           uint8* dst_ptr, int dst_width);
#endif
void ScaleRowDown2Int_C(const uint8* src_ptr, int src_stride,
                        uint8* dst_ptr, int dst_width);

int I444ToI420(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height) {
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_y = src_y + (height - 1) * src_stride_y;
    src_u = src_u + (height - 1) * src_stride_u;
    src_v = src_v + (height - 1) * src_stride_v;
    src_stride_y = -src_stride_y;
    src_stride_u = -src_stride_u;
    src_stride_v = -src_stride_v;
  }
  int halfwidth = (width + 1) >> 1;
  void (*ScaleRowDown2)(const uint8* src_ptr, int src_stride,
                        uint8* dst_ptr, int dst_width);
#if defined(HAS_SCALEROWDOWN2_NEON)
  if (TestCpuFlag(kCpuHasNEON) &&
      IS_ALIGNED(halfwidth, 16)) {
    ScaleRowDown2 = ScaleRowDown2Int_NEON;
  } else
#endif
#if defined(HAS_SCALEROWDOWN2_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(halfwidth, 16) &&
      IS_ALIGNED(src_u, 16) && IS_ALIGNED(src_stride_u, 16) &&
      IS_ALIGNED(src_v, 16) && IS_ALIGNED(src_stride_v, 16) &&
      IS_ALIGNED(dst_u, 16) && IS_ALIGNED(dst_stride_u, 16) &&
      IS_ALIGNED(dst_v, 16) && IS_ALIGNED(dst_stride_v, 16)) {
    ScaleRowDown2 = ScaleRowDown2Int_SSE2;
#endif
  {
    ScaleRowDown2 = ScaleRowDown2Int_C;
  }

  // Copy Y plane
  CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height);

  // SubSample U plane.
  int y;
  for (y = 0; y < height - 1; y += 2) {
    ScaleRowDown2(src_u, src_stride_u, dst_u, halfwidth);
    src_u += src_stride_u * 2;
    dst_u += dst_stride_u;
  }
  if (height & 1) {
    ScaleRowDown2(src_u, 0, dst_u, halfwidth);
  }

  // SubSample V plane.
  for (y = 0; y < height - 1; y += 2) {
    ScaleRowDown2(src_v, src_stride_v, dst_v, halfwidth);
    src_v += src_stride_v * 2;
    dst_v += dst_stride_v;
  }
  if (height & 1) {
    ScaleRowDown2(src_v, 0, dst_v, halfwidth);
  }
  return 0;
}

// use Bilinear for upsampling chroma
void ScalePlaneBilinear(int src_width, int src_height,
                        int dst_width, int dst_height,
                        int src_stride, int dst_stride,
                        const uint8* src_ptr, uint8* dst_ptr);

int I420ToI444(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height) {
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_y = dst_y + (height - 1) * dst_stride_y;
    dst_u = dst_u + (height - 1) * dst_stride_u;
    dst_v = dst_v + (height - 1) * dst_stride_v;
    dst_stride_y = -dst_stride_y;
    dst_stride_u = -dst_stride_u;
    dst_stride_v = -dst_stride_v;
  }

  // Copy Y plane
  CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height);

  int halfwidth = (width + 1) >> 1;
  int halfheight = (height + 1) >> 1;

  // Upsample U plane.
  ScalePlaneBilinear(halfwidth, halfheight,
                     width, height,
                     src_stride_u,
                     dst_stride_u,
                     src_u, dst_u);

  // Upsample V plane.
  ScalePlaneBilinear(halfwidth, halfheight,
                     width, height,
                     src_stride_v,
                     dst_stride_v,
                     src_v, dst_v);
  return 0;
}


static void CopyPlane2(const uint8* src, int src_stride_0, int src_stride_1,
                           uint8* dst, int dst_stride_frame,
                           int width, int height) {
  // Copy plane
  for (int y = 0; y < height; y += 2) {
    memcpy(dst, src, width);
    src += src_stride_0;
    dst += dst_stride_frame;
    memcpy(dst, src, width);
    src += src_stride_1;
    dst += dst_stride_frame;
  }
}

// Support converting from FOURCC_M420
// Useful for bandwidth constrained transports like USB 1.0 and 2.0 and for
// easy conversion to I420.
// M420 format description:
// M420 is row biplanar 420: 2 rows of Y and 1 row of VU.
// Chroma is half width / half height. (420)
// src_stride_m420 is row planar.  Normally this will be the width in pixels.
//   The UV plane is half width, but 2 values, so src_stride_m420 applies to
//   this as well as the two Y planes.
static int X420ToI420(const uint8* src_y,
                      int src_stride_y0, int src_stride_y1,
                      const uint8* src_uv, int src_stride_uv,
                      uint8* dst_y, int dst_stride_y,
                      uint8* dst_u, int dst_stride_u,
                      uint8* dst_v, int dst_stride_v,
                      int width, int height) {
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    int halfheight = (height + 1) >> 1;
    dst_y = dst_y + (height - 1) * dst_stride_y;
    dst_u = dst_u + (halfheight - 1) * dst_stride_u;
    dst_v = dst_v + (halfheight - 1) * dst_stride_v;
    dst_stride_y = -dst_stride_y;
    dst_stride_u = -dst_stride_u;
    dst_stride_v = -dst_stride_v;
  }

  int halfwidth = (width + 1) >> 1;
  void (*SplitUV)(const uint8* src_uv, uint8* dst_u, uint8* dst_v, int pix);
#if defined(HAS_SPLITUV_NEON)
  if (TestCpuFlag(kCpuHasNEON) &&
      IS_ALIGNED(halfwidth, 16) &&
      IS_ALIGNED(src_uv, 16) && IS_ALIGNED(src_stride_uv, 16) &&
      IS_ALIGNED(dst_u, 16) && IS_ALIGNED(dst_stride_u, 16) &&
      IS_ALIGNED(dst_v, 16) && IS_ALIGNED(dst_stride_v, 16)) {
    SplitUV = SplitUV_NEON;
  } else
#elif defined(HAS_SPLITUV_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(halfwidth, 16) &&
      IS_ALIGNED(src_uv, 16) && IS_ALIGNED(src_stride_uv, 16) &&
      IS_ALIGNED(dst_u, 16) && IS_ALIGNED(dst_stride_u, 16) &&
      IS_ALIGNED(dst_v, 16) && IS_ALIGNED(dst_stride_v, 16)) {
    SplitUV = SplitUV_SSE2;
  } else
#endif
  {
    SplitUV = SplitUV_C;
  }

  CopyPlane2(src_y, src_stride_y0, src_stride_y1, dst_y, dst_stride_y,
                 width, height);

  int halfheight = (height + 1) >> 1;
  for (int y = 0; y < halfheight; ++y) {
    // Copy a row of UV.
    SplitUV(src_uv, dst_u, dst_v, halfwidth);
    dst_u += dst_stride_u;
    dst_v += dst_stride_v;
    src_uv += src_stride_uv;
  }
  return 0;
}

// Convert M420 to I420.
int M420ToI420(const uint8* src_m420, int src_stride_m420,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height) {
  return X420ToI420(src_m420, src_stride_m420, src_stride_m420 * 2,
                    src_m420 + src_stride_m420 * 2, src_stride_m420 * 3,
                    dst_y, dst_stride_y,
                    dst_u, dst_stride_u,
                    dst_v, dst_stride_v,
                    width, height);
}

// Convert NV12 to I420.
int NV12ToI420(const uint8* src_y, int src_stride_y,
               const uint8* src_uv, int src_stride_uv,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height) {
  return X420ToI420(src_y, src_stride_y, src_stride_y,
                    src_uv, src_stride_uv,
                    dst_y, dst_stride_y,
                    dst_u, dst_stride_u,
                    dst_v, dst_stride_v,
                    width, height);
}

#if defined(_M_IX86) && !defined(YUV_DISABLE_ASM)
#define HAS_SPLITYUY2_SSE2
__declspec(naked)
static void SplitYUY2_SSE2(const uint8* src_yuy2,
                           uint8* dst_y, uint8* dst_u, uint8* dst_v, int pix) {
  __asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]    // src_yuy2
    mov        edx, [esp + 8 + 8]    // dst_y
    mov        esi, [esp + 8 + 12]   // dst_u
    mov        edi, [esp + 8 + 16]   // dst_v
    mov        ecx, [esp + 8 + 20]   // pix
    pcmpeqb    xmm5, xmm5            // generate mask 0x00ff00ff
    psrlw      xmm5, 8

  convertloop:
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    lea        eax,  [eax + 32]
    movdqa     xmm2, xmm0
    movdqa     xmm3, xmm1
    pand       xmm2, xmm5   // even bytes are Y
    pand       xmm3, xmm5
    packuswb   xmm2, xmm3
    movdqa     [edx], xmm2
    lea        edx, [edx + 16]
    psrlw      xmm0, 8      // YUYV -> UVUV
    psrlw      xmm1, 8
    packuswb   xmm0, xmm1
    movdqa     xmm1, xmm0
    pand       xmm0, xmm5  // U
    packuswb   xmm0, xmm0
    movq       qword ptr [esi], xmm0
    lea        esi, [esi + 8]
    psrlw      xmm1, 8     // V
    packuswb   xmm1, xmm1
    movq       qword ptr [edi], xmm1
    lea        edi, [edi + 8]
    sub        ecx, 16
    ja         convertloop

    pop        edi
    pop        esi
    ret
  }
}

#elif (defined(__x86_64__) || defined(__i386__)) && !defined(YUV_DISABLE_ASM)
#define HAS_SPLITYUY2_SSE2
static void SplitYUY2_SSE2(const uint8* src_yuy2, uint8* dst_y,
                           uint8* dst_u, uint8* dst_v, int pix) {
  asm volatile (
  "pcmpeqb    %%xmm5,%%xmm5                    \n"
  "psrlw      $0x8,%%xmm5                      \n"
"1:                                            \n"
  "movdqa     (%0),%%xmm0                      \n"
  "movdqa     0x10(%0),%%xmm1                  \n"
  "lea        0x20(%0),%0                      \n"
  "movdqa     %%xmm0,%%xmm2                    \n"
  "movdqa     %%xmm1,%%xmm3                    \n"
  "pand       %%xmm5,%%xmm2                    \n"
  "pand       %%xmm5,%%xmm3                    \n"
  "packuswb   %%xmm3,%%xmm2                    \n"
  "movdqa     %%xmm2,(%1)                      \n"
  "lea        0x10(%1),%1                      \n"
  "psrlw      $0x8,%%xmm0                      \n"
  "psrlw      $0x8,%%xmm1                      \n"
  "packuswb   %%xmm1,%%xmm0                    \n"
  "movdqa     %%xmm0,%%xmm1                    \n"
  "pand       %%xmm5,%%xmm0                    \n"
  "packuswb   %%xmm0,%%xmm0                    \n"
  "movq       %%xmm0,(%2)                      \n"
  "lea        0x8(%2),%2                       \n"
  "psrlw      $0x8,%%xmm1                      \n"
  "packuswb   %%xmm1,%%xmm1                    \n"
  "movq       %%xmm1,(%3)                      \n"
  "lea        0x8(%3),%3                       \n"
  "sub        $0x10,%4                         \n"
  "ja         1b                               \n"
  : "+r"(src_yuy2),    // %0
    "+r"(dst_y),       // %1
    "+r"(dst_u),       // %2
    "+r"(dst_v),       // %3
    "+r"(pix)          // %4
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm5"
#endif
);
}
#endif

static void SplitYUY2_C(const uint8* src_yuy2,
                        uint8* dst_y, uint8* dst_u, uint8* dst_v, int pix) {
  // Copy a row of YUY2.
  for (int x = 0; x < pix; x += 2) {
    dst_y[0] = src_yuy2[0];
    dst_y[1] = src_yuy2[2];
    dst_u[0] = src_yuy2[1];
    dst_v[0] = src_yuy2[3];
    src_yuy2 += 4;
    dst_y += 2;
    dst_u += 1;
    dst_v += 1;
  }
}

// Convert Q420 to I420.
// Format is rows of YY/YUYV
int Q420ToI420(const uint8* src_y, int src_stride_y,
               const uint8* src_yuy2, int src_stride_yuy2,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height) {
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    int halfheight = (height + 1) >> 1;
    dst_y = dst_y + (height - 1) * dst_stride_y;
    dst_u = dst_u + (halfheight - 1) * dst_stride_u;
    dst_v = dst_v + (halfheight - 1) * dst_stride_v;
    dst_stride_y = -dst_stride_y;
    dst_stride_u = -dst_stride_u;
    dst_stride_v = -dst_stride_v;
  }
  void (*SplitYUY2)(const uint8* src_yuy2,
                    uint8* dst_y, uint8* dst_u, uint8* dst_v, int pix);
#if defined(HAS_SPLITYUY2_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(width, 16) &&
      IS_ALIGNED(src_yuy2, 16) && IS_ALIGNED(src_stride_yuy2, 16) &&
      IS_ALIGNED(dst_y, 16) && IS_ALIGNED(dst_stride_y, 16) &&
      IS_ALIGNED(dst_u, 8) && IS_ALIGNED(dst_stride_u, 8) &&
      IS_ALIGNED(dst_v, 8) && IS_ALIGNED(dst_stride_v, 8)) {
    SplitYUY2 = SplitYUY2_SSE2;
  } else
#endif
  {
    SplitYUY2 = SplitYUY2_C;
  }
  for (int y = 0; y < height; y += 2) {
    memcpy(dst_y, src_y, width);
    dst_y += dst_stride_y;
    src_y += src_stride_y;

    // Copy a row of YUY2.
    SplitYUY2(src_yuy2, dst_y, dst_u, dst_v, width);
    dst_y += dst_stride_y;
    dst_u += dst_stride_u;
    dst_v += dst_stride_v;
    src_yuy2 += src_stride_yuy2;
  }
  return 0;
}

#if defined(_M_IX86) && !defined(YUV_DISABLE_ASM)
#define HAS_YUY2TOI420ROW_SSE2
__declspec(naked)
void YUY2ToI420RowY_SSE2(const uint8* src_yuy2,
                         uint8* dst_y, int pix) {
  __asm {
    mov        eax, [esp + 4]    // src_yuy2
    mov        edx, [esp + 8]    // dst_y
    mov        ecx, [esp + 12]   // pix
    pcmpeqb    xmm5, xmm5        // generate mask 0x00ff00ff
    psrlw      xmm5, 8

  convertloop:
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    lea        eax,  [eax + 32]
    pand       xmm0, xmm5   // even bytes are Y
    pand       xmm1, xmm5
    packuswb   xmm0, xmm1
    movdqa     [edx], xmm0
    lea        edx, [edx + 16]
    sub        ecx, 16
    ja         convertloop
    ret
  }
}

__declspec(naked)
void YUY2ToI420RowUV_SSE2(const uint8* src_yuy2, int stride_yuy2,
                          uint8* dst_u, uint8* dst_y, int pix) {
  __asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]    // src_yuy2
    mov        esi, [esp + 8 + 8]    // stride_yuy2
    mov        edx, [esp + 8 + 12]   // dst_u
    mov        edi, [esp + 8 + 16]   // dst_v
    mov        ecx, [esp + 8 + 20]   // pix
    pcmpeqb    xmm5, xmm5            // generate mask 0x00ff00ff
    psrlw      xmm5, 8
    sub        edi, edx

  convertloop:
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    movdqa     xmm2, [eax + esi]
    movdqa     xmm3, [eax + esi + 16]
    lea        eax,  [eax + 32]
    pavgb      xmm0, xmm2
    pavgb      xmm1, xmm3
    psrlw      xmm0, 8      // YUYV -> UVUV
    psrlw      xmm1, 8
    packuswb   xmm0, xmm1
    movdqa     xmm1, xmm0
    pand       xmm0, xmm5  // U
    packuswb   xmm0, xmm0
    psrlw      xmm1, 8     // V
    packuswb   xmm1, xmm1
    movq       qword ptr [edx], xmm0
    movq       qword ptr [edx + edi], xmm1
    lea        edx, [edx + 8]
    sub        ecx, 16
    ja         convertloop

    pop        edi
    pop        esi
    ret
  }
}

__declspec(naked)
void YUY2ToI420RowY_Unaligned_SSE2(const uint8* src_yuy2,
                                   uint8* dst_y, int pix) {
  __asm {
    mov        eax, [esp + 4]    // src_yuy2
    mov        edx, [esp + 8]    // dst_y
    mov        ecx, [esp + 12]   // pix
    pcmpeqb    xmm5, xmm5        // generate mask 0x00ff00ff
    psrlw      xmm5, 8

  convertloop:
    movdqu     xmm0, [eax]
    movdqu     xmm1, [eax + 16]
    lea        eax,  [eax + 32]
    pand       xmm0, xmm5   // even bytes are Y
    pand       xmm1, xmm5
    packuswb   xmm0, xmm1
    movdqu     [edx], xmm0
    lea        edx, [edx + 16]
    sub        ecx, 16
    ja         convertloop
    ret
  }
}

__declspec(naked)
void YUY2ToI420RowUV_Unaligned_SSE2(const uint8* src_yuy2, int stride_yuy2,
                                    uint8* dst_u, uint8* dst_y, int pix) {
  __asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]    // src_yuy2
    mov        esi, [esp + 8 + 8]    // stride_yuy2
    mov        edx, [esp + 8 + 12]   // dst_u
    mov        edi, [esp + 8 + 16]   // dst_v
    mov        ecx, [esp + 8 + 20]   // pix
    pcmpeqb    xmm5, xmm5            // generate mask 0x00ff00ff
    psrlw      xmm5, 8
    sub        edi, edx

  convertloop:
    movdqu     xmm0, [eax]
    movdqu     xmm1, [eax + 16]
    movdqu     xmm2, [eax + esi]
    movdqu     xmm3, [eax + esi + 16]
    lea        eax,  [eax + 32]
    pavgb      xmm0, xmm2
    pavgb      xmm1, xmm3
    psrlw      xmm0, 8      // YUYV -> UVUV
    psrlw      xmm1, 8
    packuswb   xmm0, xmm1
    movdqa     xmm1, xmm0
    pand       xmm0, xmm5  // U
    packuswb   xmm0, xmm0
    psrlw      xmm1, 8     // V
    packuswb   xmm1, xmm1
    movq       qword ptr [edx], xmm0
    movq       qword ptr [edx + edi], xmm1
    lea        edx, [edx + 8]
    sub        ecx, 16
    ja         convertloop

    pop        edi
    pop        esi
    ret
  }
}

#define HAS_UYVYTOI420ROW_SSE2
__declspec(naked)
void UYVYToI420RowY_SSE2(const uint8* src_uyvy,
                         uint8* dst_y, int pix) {
  __asm {
    mov        eax, [esp + 4]    // src_uyvy
    mov        edx, [esp + 8]    // dst_y
    mov        ecx, [esp + 12]   // pix

  convertloop:
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    lea        eax,  [eax + 32]
    psrlw      xmm0, 8    // odd bytes are Y
    psrlw      xmm1, 8
    packuswb   xmm0, xmm1
    movdqa     [edx], xmm0
    lea        edx, [edx + 16]
    sub        ecx, 16
    ja         convertloop
    ret
  }
}

__declspec(naked)
void UYVYToI420RowUV_SSE2(const uint8* src_uyvy, int stride_uyvy,
                          uint8* dst_u, uint8* dst_y, int pix) {
  __asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]    // src_yuy2
    mov        esi, [esp + 8 + 8]    // stride_yuy2
    mov        edx, [esp + 8 + 12]   // dst_u
    mov        edi, [esp + 8 + 16]   // dst_v
    mov        ecx, [esp + 8 + 20]   // pix
    pcmpeqb    xmm5, xmm5            // generate mask 0x00ff00ff
    psrlw      xmm5, 8
    sub        edi, edx

  convertloop:
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    movdqa     xmm2, [eax + esi]
    movdqa     xmm3, [eax + esi + 16]
    lea        eax,  [eax + 32]
    pavgb      xmm0, xmm2
    pavgb      xmm1, xmm3
    pand       xmm0, xmm5   // UYVY -> UVUV
    pand       xmm1, xmm5
    packuswb   xmm0, xmm1
    movdqa     xmm1, xmm0
    pand       xmm0, xmm5  // U
    packuswb   xmm0, xmm0
    psrlw      xmm1, 8     // V
    packuswb   xmm1, xmm1
    movq       qword ptr [edx], xmm0
    movq       qword ptr [edx + edi], xmm1
    lea        edx, [edx + 8]
    sub        ecx, 16
    ja         convertloop

    pop        edi
    pop        esi
    ret
  }
}

#elif (defined(__x86_64__) || defined(__i386__)) && !defined(YUV_DISABLE_ASM)

#define HAS_YUY2TOI420ROW_SSE2
static void YUY2ToI420RowY_SSE2(const uint8* src_yuy2,
                                uint8* dst_y, int pix) {
  asm volatile (
  "pcmpeqb    %%xmm5,%%xmm5                    \n"
  "psrlw      $0x8,%%xmm5                      \n"
"1:                                            \n"
  "movdqa     (%0),%%xmm0                      \n"
  "movdqa     0x10(%0),%%xmm1                  \n"
  "lea        0x20(%0),%0                      \n"
  "pand       %%xmm5,%%xmm0                    \n"
  "pand       %%xmm5,%%xmm1                    \n"
  "packuswb   %%xmm1,%%xmm0                    \n"
  "movdqa     %%xmm0,(%1)                      \n"
  "lea        0x10(%1),%1                      \n"
  "sub        $0x10,%2                         \n"
  "ja         1b                               \n"
  : "+r"(src_yuy2),  // %0
    "+r"(dst_y),     // %1
    "+r"(pix)        // %2
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm5"
#endif
);
}

static void YUY2ToI420RowUV_SSE2(const uint8* src_yuy2, int stride_yuy2,
                                 uint8* dst_u, uint8* dst_y, int pix) {
  asm volatile (
  "pcmpeqb    %%xmm5,%%xmm5                    \n"
  "psrlw      $0x8,%%xmm5                      \n"
  "sub        %1,%2                            \n"
"1:                                            \n"
  "movdqa     (%0),%%xmm0                      \n"
  "movdqa     0x10(%0),%%xmm1                  \n"
  "movdqa     (%0,%4,1),%%xmm2                 \n"
  "movdqa     0x10(%0,%4,1),%%xmm3             \n"
  "lea        0x20(%0),%0                      \n"
  "pavgb      %%xmm2,%%xmm0                    \n"
  "pavgb      %%xmm3,%%xmm1                    \n"
  "psrlw      $0x8,%%xmm0                      \n"
  "psrlw      $0x8,%%xmm1                      \n"
  "packuswb   %%xmm1,%%xmm0                    \n"
  "movdqa     %%xmm0,%%xmm1                    \n"
  "pand       %%xmm5,%%xmm0                    \n"
  "packuswb   %%xmm0,%%xmm0                    \n"
  "psrlw      $0x8,%%xmm1                      \n"
  "packuswb   %%xmm1,%%xmm1                    \n"
  "movq       %%xmm0,(%1)                      \n"
  "movq       %%xmm1,(%1,%2)                   \n"
  "lea        0x8(%1),%1                       \n"
  "sub        $0x10,%3                         \n"
  "ja         1b                               \n"
  : "+r"(src_yuy2),    // %0
    "+r"(dst_u),       // %1
    "+r"(dst_y),       // %2
    "+r"(pix)          // %3
  : "r"(static_cast<intptr_t>(stride_yuy2))  // %4
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm5"
#endif
);
}
static void YUY2ToI420RowY_Unaligned_SSE2(const uint8* src_yuy2,
                                          uint8* dst_y, int pix) {
  asm volatile (
  "pcmpeqb    %%xmm5,%%xmm5                    \n"
  "psrlw      $0x8,%%xmm5                      \n"
"1:                                            \n"
  "movdqu     (%0),%%xmm0                      \n"
  "movdqu     0x10(%0),%%xmm1                  \n"
  "lea        0x20(%0),%0                      \n"
  "pand       %%xmm5,%%xmm0                    \n"
  "pand       %%xmm5,%%xmm1                    \n"
  "packuswb   %%xmm1,%%xmm0                    \n"
  "movdqu     %%xmm0,(%1)                      \n"
  "lea        0x10(%1),%1                      \n"
  "sub        $0x10,%2                         \n"
  "ja         1b                               \n"
  : "+r"(src_yuy2),  // %0
    "+r"(dst_y),     // %1
    "+r"(pix)        // %2
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm5"
#endif
);
}

static void YUY2ToI420RowUV_Unaligned_SSE2(const uint8* src_yuy2,
                                           int stride_yuy2,
                                           uint8* dst_u, uint8* dst_y,
                                           int pix) {
  asm volatile (
  "pcmpeqb    %%xmm5,%%xmm5                    \n"
  "psrlw      $0x8,%%xmm5                      \n"
  "sub        %1,%2                            \n"
"1:                                            \n"
  "movdqu     (%0),%%xmm0                      \n"
  "movdqu     0x10(%0),%%xmm1                  \n"
  "movdqu     (%0,%4,1),%%xmm2                 \n"
  "movdqu     0x10(%0,%4,1),%%xmm3             \n"
  "lea        0x20(%0),%0                      \n"
  "pavgb      %%xmm2,%%xmm0                    \n"
  "pavgb      %%xmm3,%%xmm1                    \n"
  "psrlw      $0x8,%%xmm0                      \n"
  "psrlw      $0x8,%%xmm1                      \n"
  "packuswb   %%xmm1,%%xmm0                    \n"
  "movdqa     %%xmm0,%%xmm1                    \n"
  "pand       %%xmm5,%%xmm0                    \n"
  "packuswb   %%xmm0,%%xmm0                    \n"
  "psrlw      $0x8,%%xmm1                      \n"
  "packuswb   %%xmm1,%%xmm1                    \n"
  "movq       %%xmm0,(%1)                      \n"
  "movq       %%xmm1,(%1,%2)                   \n"
  "lea        0x8(%1),%1                       \n"
  "sub        $0x10,%3                         \n"
  "ja         1b                               \n"
  : "+r"(src_yuy2),    // %0
    "+r"(dst_u),       // %1
    "+r"(dst_y),       // %2
    "+r"(pix)          // %3
  : "r"(static_cast<intptr_t>(stride_yuy2))  // %4
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm5"
#endif
);
}
#define HAS_UYVYTOI420ROW_SSE2
static void UYVYToI420RowY_SSE2(const uint8* src_uyvy,
                                uint8* dst_y, int pix) {
  asm volatile (
"1:                                            \n"
  "movdqa     (%0),%%xmm0                      \n"
  "movdqa     0x10(%0),%%xmm1                  \n"
  "lea        0x20(%0),%0                      \n"
  "psrlw      $0x8,%%xmm0                      \n"
  "psrlw      $0x8,%%xmm1                      \n"
  "packuswb   %%xmm1,%%xmm0                    \n"
  "movdqa     %%xmm0,(%1)                      \n"
  "lea        0x10(%1),%1                      \n"
  "sub        $0x10,%2                         \n"
  "ja         1b                               \n"
  : "+r"(src_uyvy),  // %0
    "+r"(dst_y),     // %1
    "+r"(pix)        // %2
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1"
#endif
);
}

static void UYVYToI420RowUV_SSE2(const uint8* src_uyvy, int stride_uyvy,
                                 uint8* dst_u, uint8* dst_y, int pix) {
  asm volatile (
  "pcmpeqb    %%xmm5,%%xmm5                    \n"
  "psrlw      $0x8,%%xmm5                      \n"
  "sub        %1,%2                            \n"
"1:                                            \n"
  "movdqa     (%0),%%xmm0                      \n"
  "movdqa     0x10(%0),%%xmm1                  \n"
  "movdqa     (%0,%4,1),%%xmm2                 \n"
  "movdqa     0x10(%0,%4,1),%%xmm3             \n"
  "lea        0x20(%0),%0                      \n"
  "pavgb      %%xmm2,%%xmm0                    \n"
  "pavgb      %%xmm3,%%xmm1                    \n"
  "pand       %%xmm5,%%xmm0                    \n"
  "pand       %%xmm5,%%xmm1                    \n"
  "packuswb   %%xmm1,%%xmm0                    \n"
  "movdqa     %%xmm0,%%xmm1                    \n"
  "pand       %%xmm5,%%xmm0                    \n"
  "packuswb   %%xmm0,%%xmm0                    \n"
  "psrlw      $0x8,%%xmm1                      \n"
  "packuswb   %%xmm1,%%xmm1                    \n"
  "movq       %%xmm0,(%1)                      \n"
  "movq       %%xmm1,(%1,%2)                   \n"
  "lea        0x8(%1),%1                       \n"
  "sub        $0x10,%3                         \n"
  "ja         1b                               \n"
  : "+r"(src_uyvy),    // %0
    "+r"(dst_u),       // %1
    "+r"(dst_y),       // %2
    "+r"(pix)          // %3
  : "r"(static_cast<intptr_t>(stride_uyvy))  // %4
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm5"
#endif
);
}
#endif

// Filter 2 rows of YUY2 UV's (422) into U and V (420)
void YUY2ToI420RowUV_C(const uint8* src_yuy2, int src_stride_yuy2,
                       uint8* dst_u, uint8* dst_v, int pix) {
  // Output a row of UV values, filtering 2 rows of YUY2
  for (int x = 0; x < pix; x += 2) {
    dst_u[0] = (src_yuy2[1] + src_yuy2[src_stride_yuy2 + 1] + 1) >> 1;
    dst_v[0] = (src_yuy2[3] + src_yuy2[src_stride_yuy2 + 3] + 1) >> 1;
    src_yuy2 += 4;
    dst_u += 1;
    dst_v += 1;
  }
}

void YUY2ToI420RowY_C(const uint8* src_yuy2,
                      uint8* dst_y, int pix) {
  // Copy a row of yuy2 Y values
  for (int x = 0; x < pix; ++x) {
    dst_y[0] = src_yuy2[0];
    src_yuy2 += 2;
    dst_y += 1;
  }
}

void UYVYToI420RowUV_C(const uint8* src_uyvy, int src_stride_uyvy,
                       uint8* dst_u, uint8* dst_v, int pix) {
  // Copy a row of uyvy UV values
  for (int x = 0; x < pix; x += 2) {
    dst_u[0] = (src_uyvy[0] + src_uyvy[src_stride_uyvy + 0] + 1) >> 1;
    dst_v[0] = (src_uyvy[2] + src_uyvy[src_stride_uyvy + 2] + 1) >> 1;
    src_uyvy += 4;
    dst_u += 1;
    dst_v += 1;
  }
}

void UYVYToI420RowY_C(const uint8* src_uyvy,
                      uint8* dst_y, int pix) {
  // Copy a row of uyvy Y values
  for (int x = 0; x < pix; ++x) {
    dst_y[0] = src_uyvy[1];
    src_uyvy += 2;
    dst_y += 1;
  }
}

// Convert YUY2 to I420.
int YUY2ToI420(const uint8* src_yuy2, int src_stride_yuy2,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height) {
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_yuy2 = src_yuy2 + (height - 1) * src_stride_yuy2;
    src_stride_yuy2 = -src_stride_yuy2;
  }
  void (*YUY2ToI420RowUV)(const uint8* src_yuy2, int src_stride_yuy2,
                          uint8* dst_u, uint8* dst_v, int pix);
  void (*YUY2ToI420RowY)(const uint8* src_yuy2,
                         uint8* dst_y, int pix);
#if defined(HAS_YUY2TOI420ROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) && IS_ALIGNED(width, 16)) {
    if (IS_ALIGNED(src_yuy2, 16) && IS_ALIGNED(src_stride_yuy2, 16)) {
      YUY2ToI420RowUV = YUY2ToI420RowUV_SSE2;
    } else {
      YUY2ToI420RowUV = YUY2ToI420RowUV_Unaligned_SSE2;
    }
    if (IS_ALIGNED(src_yuy2, 16) && IS_ALIGNED(src_stride_yuy2, 16) &&
        IS_ALIGNED(dst_y, 16) && IS_ALIGNED(dst_stride_y, 16)) {
      YUY2ToI420RowY = YUY2ToI420RowY_SSE2;
    } else {
      YUY2ToI420RowY = YUY2ToI420RowY_Unaligned_SSE2;
    }
  } else
#endif
  {
    YUY2ToI420RowY = YUY2ToI420RowY_C;
    YUY2ToI420RowUV = YUY2ToI420RowUV_C;
  }
  for (int y = 0; y < height - 1; y += 2) {
    YUY2ToI420RowUV(src_yuy2, src_stride_yuy2, dst_u, dst_v, width);
    dst_u += dst_stride_u;
    dst_v += dst_stride_v;
    YUY2ToI420RowY(src_yuy2, dst_y, width);
    YUY2ToI420RowY(src_yuy2 + src_stride_yuy2, dst_y + dst_stride_y, width);
    dst_y += dst_stride_y * 2;
    src_yuy2 += src_stride_yuy2 * 2;
  }
  if (height & 1) {
    YUY2ToI420RowUV(src_yuy2, 0, dst_u, dst_v, width);
    YUY2ToI420RowY(src_yuy2, dst_y, width);
  }
  return 0;
}

// Convert UYVY to I420.
int UYVYToI420(const uint8* src_uyvy, int src_stride_uyvy,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height) {
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_uyvy = src_uyvy + (height - 1) * src_stride_uyvy;
    src_stride_uyvy = -src_stride_uyvy;
  }
  void (*UYVYToI420RowUV)(const uint8* src_uyvy, int src_stride_uyvy,
                          uint8* dst_u, uint8* dst_v, int pix);
  void (*UYVYToI420RowY)(const uint8* src_uyvy,
                         uint8* dst_y, int pix);
#if defined(HAS_UYVYTOI420ROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(width, 16) &&
      IS_ALIGNED(src_uyvy, 16) && IS_ALIGNED(src_stride_uyvy, 16) &&
      IS_ALIGNED(dst_y, 16) && IS_ALIGNED(dst_stride_y, 16) &&
      IS_ALIGNED(dst_u, 8) && IS_ALIGNED(dst_stride_u, 8) &&
      IS_ALIGNED(dst_v, 8) && IS_ALIGNED(dst_stride_v, 8)) {
    UYVYToI420RowY = UYVYToI420RowY_SSE2;
    UYVYToI420RowUV = UYVYToI420RowUV_SSE2;
  } else
#endif
  {
    UYVYToI420RowY = UYVYToI420RowY_C;
    UYVYToI420RowUV = UYVYToI420RowUV_C;
  }
  for (int y = 0; y < height - 1; y += 2) {
    UYVYToI420RowUV(src_uyvy, src_stride_uyvy, dst_u, dst_v, width);
    dst_u += dst_stride_u;
    dst_v += dst_stride_v;
    UYVYToI420RowY(src_uyvy, dst_y, width);
    UYVYToI420RowY(src_uyvy + src_stride_uyvy, dst_y + dst_stride_y, width);
    dst_y += dst_stride_y * 2;
    src_uyvy += src_stride_uyvy * 2;
  }
  if (height & 1) {
    UYVYToI420RowUV(src_uyvy, 0, dst_u, dst_v, width);
    UYVYToI420RowY(src_uyvy, dst_y, width);
  }
  return 0;
}

// Convert I420 to ARGB.
int I420ToARGB(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_argb, int dst_stride_argb,
               int width, int height) {
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_argb = dst_argb + (height - 1) * dst_stride_argb;
    dst_stride_argb = -dst_stride_argb;
  }
  void (*FastConvertYUVToARGBRow)(const uint8* y_buf,
                                  const uint8* u_buf,
                                  const uint8* v_buf,
                                  uint8* rgb_buf,
                                  int width);
#if defined(HAS_FASTCONVERTYUVTOARGBROW_NEON)
  if (TestCpuFlag(kCpuHasNEON) && IS_ALIGNED(width, 16)) {
    FastConvertYUVToARGBRow = FastConvertYUVToARGBRow_NEON;
  } else
#elif defined(HAS_FASTCONVERTYUVTOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(width, 8) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    FastConvertYUVToARGBRow = FastConvertYUVToARGBRow_SSSE3;
  } else
#endif
  {
    FastConvertYUVToARGBRow = FastConvertYUVToARGBRow_C;
  }
  for (int y = 0; y < height; ++y) {
    FastConvertYUVToARGBRow(src_y, src_u, src_v, dst_argb, width);
    dst_argb += dst_stride_argb;
    src_y += src_stride_y;
    if (y & 1) {
      src_u += src_stride_u;
      src_v += src_stride_v;
    }
  }
  return 0;
}

// Convert I420 to BGRA.
int I420ToBGRA(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_argb, int dst_stride_argb,
               int width, int height) {
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_argb = dst_argb + (height - 1) * dst_stride_argb;
    dst_stride_argb = -dst_stride_argb;
  }
  void (*FastConvertYUVToBGRARow)(const uint8* y_buf,
                                   const uint8* u_buf,
                                   const uint8* v_buf,
                                   uint8* rgb_buf,
                                   int width);
#if defined(HAS_FASTCONVERTYUVTOBGRAROW_NEON)
  if (TestCpuFlag(kCpuHasNEON) && IS_ALIGNED(width, 16)) {
    FastConvertYUVToBGRARow = FastConvertYUVToBGRARow_NEON;
  } else
#elif defined(HAS_FASTCONVERTYUVTOBGRAROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(width, 8) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    FastConvertYUVToBGRARow = FastConvertYUVToBGRARow_SSSE3;
  } else
#endif
  {
    FastConvertYUVToBGRARow = FastConvertYUVToBGRARow_C;
  }
  for (int y = 0; y < height; ++y) {
    FastConvertYUVToBGRARow(src_y, src_u, src_v, dst_argb, width);
    dst_argb += dst_stride_argb;
    src_y += src_stride_y;
    if (y & 1) {
      src_u += src_stride_u;
      src_v += src_stride_v;
    }
  }
  return 0;
}

// Convert I420 to ABGR.
int I420ToABGR(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_argb, int dst_stride_argb,
               int width, int height) {
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_argb = dst_argb + (height - 1) * dst_stride_argb;
    dst_stride_argb = -dst_stride_argb;
  }
  void (*FastConvertYUVToABGRRow)(const uint8* y_buf,
                                   const uint8* u_buf,
                                   const uint8* v_buf,
                                   uint8* rgb_buf,
                                   int width);
#if defined(HAS_FASTCONVERTYUVTOABGRROW_NEON)
  if (TestCpuFlag(kCpuHasNEON) && IS_ALIGNED(width, 16)) {
    FastConvertYUVToABGRRow = FastConvertYUVToABGRRow_NEON;
  } else
#elif defined(HAS_FASTCONVERTYUVTOABGRROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(width, 8) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    FastConvertYUVToABGRRow = FastConvertYUVToABGRRow_SSSE3;
  } else
#endif
  {
    FastConvertYUVToABGRRow = FastConvertYUVToABGRRow_C;
  }
  for (int y = 0; y < height; ++y) {
    FastConvertYUVToABGRRow(src_y, src_u, src_v, dst_argb, width);
    dst_argb += dst_stride_argb;
    src_y += src_stride_y;
    if (y & 1) {
      src_u += src_stride_u;
      src_v += src_stride_v;
    }
  }
  return 0;
}

// Convert I422 to ARGB.
int I422ToARGB(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_argb, int dst_stride_argb,
               int width, int height) {
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_argb = dst_argb + (height - 1) * dst_stride_argb;
    dst_stride_argb = -dst_stride_argb;
  }
  void (*FastConvertYUVToARGBRow)(const uint8* y_buf,
                                   const uint8* u_buf,
                                   const uint8* v_buf,
                                   uint8* rgb_buf,
                                   int width);
#if defined(HAS_FASTCONVERTYUVTOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(width, 8) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    FastConvertYUVToARGBRow = FastConvertYUVToARGBRow_SSSE3;
  } else
#endif
  {
    FastConvertYUVToARGBRow = FastConvertYUVToARGBRow_C;
  }
  for (int y = 0; y < height; ++y) {
    FastConvertYUVToARGBRow(src_y, src_u, src_v, dst_argb, width);
    dst_argb += dst_stride_argb;
    src_y += src_stride_y;
    src_u += src_stride_u;
    src_v += src_stride_v;
  }
  return 0;
}

// Convert I444 to ARGB.
int I444ToARGB(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_argb, int dst_stride_argb,
               int width, int height) {
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_argb = dst_argb + (height - 1) * dst_stride_argb;
    dst_stride_argb = -dst_stride_argb;
  }
  void (*FastConvertYUV444ToARGBRow)(const uint8* y_buf,
                                      const uint8* u_buf,
                                      const uint8* v_buf,
                                      uint8* rgb_buf,
                                      int width);
#if defined(HAS_FASTCONVERTYUV444TOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(width, 8) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    FastConvertYUV444ToARGBRow = FastConvertYUV444ToARGBRow_SSSE3;
  } else
#endif
  {
    FastConvertYUV444ToARGBRow = FastConvertYUV444ToARGBRow_C;
  }
  for (int y = 0; y < height; ++y) {
    FastConvertYUV444ToARGBRow(src_y, src_u, src_v, dst_argb, width);
    dst_argb += dst_stride_argb;
    src_y += src_stride_y;
    src_u += src_stride_u;
    src_v += src_stride_v;
  }
  return 0;
}

// Convert I400 to ARGB.
int I400ToARGB_Reference(const uint8* src_y, int src_stride_y,
                         uint8* dst_argb, int dst_stride_argb,
                         int width, int height) {
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_argb = dst_argb + (height - 1) * dst_stride_argb;
    dst_stride_argb = -dst_stride_argb;
  }
  void (*FastConvertYToARGBRow)(const uint8* y_buf,
                                 uint8* rgb_buf,
                                 int width);
#if defined(HAS_FASTCONVERTYTOARGBROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(width, 8) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    FastConvertYToARGBRow = FastConvertYToARGBRow_SSE2;
  } else
#endif
  {
    FastConvertYToARGBRow = FastConvertYToARGBRow_C;
  }
  for (int y = 0; y < height; ++y) {
    FastConvertYToARGBRow(src_y, dst_argb, width);
    dst_argb += dst_stride_argb;
    src_y += src_stride_y;
  }
  return 0;
}

// Convert I400 to ARGB.
int I400ToARGB(const uint8* src_y, int src_stride_y,
               uint8* dst_argb, int dst_stride_argb,
               int width, int height) {
  if (height < 0) {
    height = -height;
    src_y = src_y + (height - 1) * src_stride_y;
    src_stride_y = -src_stride_y;
  }
  void (*I400ToARGBRow)(const uint8* src_y, uint8* dst_argb, int pix);
#if defined(HAS_I400TOARGBROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(width, 8) &&
      IS_ALIGNED(src_y, 8) && IS_ALIGNED(src_stride_y, 8) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    I400ToARGBRow = I400ToARGBRow_SSE2;
  } else
#endif
  {
    I400ToARGBRow = I400ToARGBRow_C;
  }

  for (int y = 0; y < height; ++y) {
    I400ToARGBRow(src_y, dst_argb, width);
    src_y += src_stride_y;
    dst_argb += dst_stride_argb;
  }
  return 0;
}

int ABGRToARGB(const uint8* src_abgr, int src_stride_abgr,
               uint8* dst_argb, int dst_stride_argb,
               int width, int height) {
  if (height < 0) {
    height = -height;
    src_abgr = src_abgr + (height - 1) * src_stride_abgr;
    src_stride_abgr = -src_stride_abgr;
  }
void (*ABGRToARGBRow)(const uint8* src_abgr, uint8* dst_argb, int pix);
#if defined(HAS_ABGRTOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(width, 4) &&
      IS_ALIGNED(src_abgr, 16) && IS_ALIGNED(src_stride_abgr, 16) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    ABGRToARGBRow = ABGRToARGBRow_SSSE3;
  } else
#endif
  {
    ABGRToARGBRow = ABGRToARGBRow_C;
  }

  for (int y = 0; y < height; ++y) {
    ABGRToARGBRow(src_abgr, dst_argb, width);
    src_abgr += src_stride_abgr;
    dst_argb += dst_stride_argb;
  }
  return 0;
}

// Convert BGRA to ARGB.
int BGRAToARGB(const uint8* src_bgra, int src_stride_bgra,
               uint8* dst_argb, int dst_stride_argb,
               int width, int height) {
  if (height < 0) {
    height = -height;
    src_bgra = src_bgra + (height - 1) * src_stride_bgra;
    src_stride_bgra = -src_stride_bgra;
  }
  void (*BGRAToARGBRow)(const uint8* src_bgra, uint8* dst_argb, int pix);
#if defined(HAS_BGRATOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(width, 4) &&
      IS_ALIGNED(src_bgra, 16) && IS_ALIGNED(src_stride_bgra, 16) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    BGRAToARGBRow = BGRAToARGBRow_SSSE3;
  } else
#endif
  {
    BGRAToARGBRow = BGRAToARGBRow_C;
  }

  for (int y = 0; y < height; ++y) {
    BGRAToARGBRow(src_bgra, dst_argb, width);
    src_bgra += src_stride_bgra;
    dst_argb += dst_stride_argb;
  }
  return 0;
}

// Convert ARGB to I400.
int ARGBToI400(const uint8* src_argb, int src_stride_argb,
               uint8* dst_y, int dst_stride_y,
               int width, int height) {
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
void (*ARGBToYRow)(const uint8* src_argb, uint8* dst_y, int pix);
#if defined(HAS_ARGBTOYROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(width, 4) &&
      IS_ALIGNED(src_argb, 16) && IS_ALIGNED(src_stride_argb, 16) &&
      IS_ALIGNED(dst_y, 16) && IS_ALIGNED(dst_stride_y, 16)) {
    ARGBToYRow = ARGBToYRow_SSSE3;
  } else
#endif
  {
    ARGBToYRow = ARGBToYRow_C;
  }

  for (int y = 0; y < height; ++y) {
    ARGBToYRow(src_argb, dst_y, width);
    src_argb += src_stride_argb;
    dst_y += dst_stride_y;
  }
  return 0;
}


// Convert RAW to ARGB.
int RAWToARGB(const uint8* src_raw, int src_stride_raw,
              uint8* dst_argb, int dst_stride_argb,
              int width, int height) {
  if (height < 0) {
    height = -height;
    src_raw = src_raw + (height - 1) * src_stride_raw;
    src_stride_raw = -src_stride_raw;
  }
  void (*RAWToARGBRow)(const uint8* src_raw, uint8* dst_argb, int pix);
#if defined(HAS_RAWTOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(width, 16) &&
      IS_ALIGNED(src_raw, 16) && IS_ALIGNED(src_stride_raw, 16) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    RAWToARGBRow = RAWToARGBRow_SSSE3;
  } else
#endif
  {
    RAWToARGBRow = RAWToARGBRow_C;
  }

  for (int y = 0; y < height; ++y) {
    RAWToARGBRow(src_raw, dst_argb, width);
    src_raw += src_stride_raw;
    dst_argb += dst_stride_argb;
  }
  return 0;
}

// Convert BG24 to ARGB.
int BG24ToARGB(const uint8* src_bg24, int src_stride_bg24,
               uint8* dst_argb, int dst_stride_argb,
               int width, int height) {
  if (height < 0) {
    height = -height;
    src_bg24 = src_bg24 + (height - 1) * src_stride_bg24;
    src_stride_bg24 = -src_stride_bg24;
  }
  void (*BG24ToARGBRow)(const uint8* src_bg24, uint8* dst_argb, int pix);
#if defined(HAS_BG24TOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(width, 16) &&
      IS_ALIGNED(src_bg24, 16) && IS_ALIGNED(src_stride_bg24, 16) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    BG24ToARGBRow = BG24ToARGBRow_SSSE3;
  } else
#endif
  {
    BG24ToARGBRow = BG24ToARGBRow_C;
  }

  for (int y = 0; y < height; ++y) {
    BG24ToARGBRow(src_bg24, dst_argb, width);
    src_bg24 += src_stride_bg24;
    dst_argb += dst_stride_argb;
  }
  return 0;
}


// SetRow8 writes 'count' bytes using a 32 bit value repeated
// SetRow32 writes 'count' words using a 32 bit value repeated

#if defined(__ARM_NEON__) && !defined(YUV_DISABLE_ASM)
#define HAS_SETROW_NEON
static void SetRow8_NEON(uint8* dst, uint32 v32, int count) {
  asm volatile (
    "vdup.u32  q0, %2                          \n"  // duplicate 4 ints
    "1:                                        \n"
    "vst1.u32  {q0}, [%0]!                     \n"  // store
    "subs      %1, %1, #16                     \n"  // 16 bytes per loop
    "bhi       1b                              \n"
  : "+r"(dst),  // %0
    "+r"(count) // %1
  : "r"(v32)    // %2
  : "q0", "memory", "cc"
  );
}

// TODO(fbarchard): Make fully assembler
static void SetRows32_NEON(uint8* dst, uint32 v32, int width,
                           int dst_stride, int height) {
  for (int y = 0; y < height; ++y) {
    SetRow8_NEON(dst, v32, width << 2);
    dst += dst_stride;
  }
}

#elif defined(_M_IX86) && !defined(YUV_DISABLE_ASM)
#define HAS_SETROW_X86
__declspec(naked)
static void SetRow8_X86(uint8* dst, uint32 v32, int count) {
  __asm {
    mov        edx, edi
    mov        edi, [esp + 4]   // dst
    mov        eax, [esp + 8]   // v32
    mov        ecx, [esp + 12]  // count
    shr        ecx, 2
    rep stosd
    mov        edi, edx
    ret
  }
}

__declspec(naked)
static void SetRows32_X86(uint8* dst, uint32 v32, int width,
                         int dst_stride, int height) {
  __asm {
    push       edi
    push       ebp
    mov        edi, [esp + 8 + 4]   // dst
    mov        eax, [esp + 8 + 8]   // v32
    mov        ebp, [esp + 8 + 12]  // width
    mov        edx, [esp + 8 + 16]  // dst_stride
    mov        ebx, [esp + 8 + 20]  // height
    lea        ecx, [ebp * 4]
    sub        edx, ecx             // stride - width * 4

  convertloop:
    mov        ecx, ebp
    rep stosd
    add        edi, edx
    sub        ebx, 1
    ja         convertloop

    pop        ebp
    pop        edi
    ret
  }
}

#elif (defined(__x86_64__) || defined(__i386__)) && !defined(YUV_DISABLE_ASM)
#define HAS_SETROW_X86
static void SetRow8_X86(uint8* dst, uint32 v32, int width) {
  size_t width_tmp = static_cast<size_t>(width);
  asm volatile (
    "shr       $0x2,%1                         \n"
    "rep stosl                                 \n"
  : "+D"(dst),  // %0
    "+c"(width_tmp) // %1
  : "a"(v32)    // %2
  : "memory", "cc"
  );
}

static void SetRows32_X86(uint8* dst, uint32 v32, int width,
                         int dst_stride, int height) {
  for (int y = 0; y < height; ++y) {
    size_t width_tmp = static_cast<size_t>(width);
    uint32* d = reinterpret_cast<uint32*>(dst);
    asm volatile (
      "rep stosl                               \n"
    : "+D"(d),  // %0
      "+c"(width_tmp) // %1
    : "a"(v32)    // %2
    : "memory", "cc"
    );
    dst += dst_stride;
  }
}
#endif

#if !defined(HAS_SETROW_X86)
static void SetRow8_C(uint8* dst, uint32 v8, int count) {
#ifdef _MSC_VER
  for (int x = 0; x < count; ++x) {
    dst[x] = v8;
  }
#else
  memset(dst, v8, count);
#endif
}

static void SetRows32_C(uint8* dst, uint32 v32, int width,
                        int dst_stride, int height) {
  for (int y = 0; y < height; ++y) {
    uint32* d = reinterpret_cast<uint32*>(dst);
    for (int x = 0; x < width; ++x) {
      d[x] = v32;
    }
    dst += dst_stride;
  }
}
#endif

static void SetPlane(uint8* dst_y, int dst_stride_y,
                     int width, int height,
                     uint32 value) {
  void (*SetRow)(uint8* dst, uint32 value, int pix);
#if defined(HAS_SETROW_NEON)
  if (TestCpuFlag(kCpuHasNEON) &&
      IS_ALIGNED(width, 16) &&
      IS_ALIGNED(dst_y, 16) && IS_ALIGNED(dst_stride_y, 16)) {
    SetRow = SetRow8_NEON;
  } else
#elif defined(HAS_SETROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(width, 16) &&
      IS_ALIGNED(dst_y, 16) && IS_ALIGNED(dst_stride_y, 16)) {
    SetRow = SetRow8_SSE2;
  } else
#endif
  {
#if defined(HAS_SETROW_X86)
    SetRow = SetRow8_X86;
#else
    SetRow = SetRow8_C;
#endif
  }

  uint32 v32 = value | (value << 8) | (value << 16) | (value << 24);
  // Set plane
  for (int y = 0; y < height; ++y) {
    SetRow(dst_y, v32, width);
    dst_y += dst_stride_y;
  }
}

// Draw a rectangle into I420
int I420Rect(uint8* dst_y, int dst_stride_y,
             uint8* dst_u, int dst_stride_u,
             uint8* dst_v, int dst_stride_v,
             int x, int y,
             int width, int height,
             int value_y, int value_u, int value_v) {
  if (!dst_y || !dst_u || !dst_v ||
      width <= 0 || height <= 0 ||
      x < 0 || y < 0 ||
      value_y < 0 || value_y > 255 ||
      value_u < 0 || value_u > 255 ||
      value_v < 0 || value_v > 255) {
    return -1;
  }
  int halfwidth = (width + 1) >> 1;
  int halfheight = (height + 1) >> 1;
  uint8* start_y = dst_y + y * dst_stride_y + x;
  uint8* start_u = dst_u + (y / 2) * dst_stride_u + (x / 2);
  uint8* start_v = dst_v + (y / 2) * dst_stride_v + (x / 2);

  SetPlane(start_y, dst_stride_y, width, height, value_y);
  SetPlane(start_u, dst_stride_u, halfwidth, halfheight, value_u);
  SetPlane(start_v, dst_stride_v, halfwidth, halfheight, value_v);
  return 0;
}

// Draw a rectangle into ARGB
int ARGBRect(uint8* dst_argb, int dst_stride_argb,
             int dst_x, int dst_y,
             int width, int height,
             uint32 value) {
  if (!dst_argb ||
      width <= 0 || height <= 0 ||
      dst_x < 0 || dst_y < 0) {
    return -1;
  }
  uint8* dst = dst_argb + dst_y * dst_stride_argb + dst_x * 4;
  void (*SetRows)(uint8* dst, uint32 value, int width,
                  int dst_stride, int height);
#if defined(HAS_SETROW_NEON)
  if (TestCpuFlag(kCpuHasNEON) &&
      IS_ALIGNED(width, 16) &&
      IS_ALIGNED(dst, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    SetRows = SetRows32_NEON;
  } else
#endif
  {
#if defined(HAS_SETROW_X86)
    SetRows = SetRows32_X86;
#else
    SetRows = SetRows32_C;
#endif
  }
  SetRows(dst, value, width, dst_stride_argb, height);
  return 0;
}

// I400 is greyscale typically used in MJPG
int I400ToI420(const uint8* src_y, int src_stride_y,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height) {
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_y = src_y + (height - 1) * src_stride_y;
    src_stride_y = -src_stride_y;
  }
  int halfwidth = (width + 1) >> 1;
  int halfheight = (height + 1) >> 1;
  CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height);
  SetPlane(dst_u, dst_stride_u, halfwidth, halfheight, 128);
  SetPlane(dst_v, dst_stride_v, halfwidth, halfheight, 128);
  return 0;
}

// Copy to I400.  Source can be I420,422,444,400,NV12,NV21
int I400Copy(const uint8* src_y, int src_stride_y,
             uint8* dst_y, int dst_stride_y,
             int width, int height) {
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_y = src_y + (height - 1) * src_stride_y;
    src_stride_y = -src_stride_y;
  }
  CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height);
  return 0;
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
