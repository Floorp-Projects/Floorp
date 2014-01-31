/*
 *  Copyright 2012 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/convert_from.h"

#include "libyuv/basic_types.h"
#include "libyuv/convert.h"  // For I420Copy
#include "libyuv/cpu_id.h"
#include "libyuv/format_conversion.h"
#include "libyuv/planar_functions.h"
#include "libyuv/rotate.h"
#include "libyuv/video_common.h"
#include "libyuv/row.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

LIBYUV_API
int I420ToI422(const uint8* src_y, int src_stride_y,
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
    dst_y = dst_y + (height - 1) * dst_stride_y;
    dst_u = dst_u + (height - 1) * dst_stride_u;
    dst_v = dst_v + (height - 1) * dst_stride_v;
    dst_stride_y = -dst_stride_y;
    dst_stride_u = -dst_stride_u;
    dst_stride_v = -dst_stride_v;
  }
  int halfwidth = (width + 1) >> 1;
  void (*CopyRow)(const uint8* src, uint8* dst, int width) = CopyRow_C;
#if defined(HAS_COPYROW_NEON)
  if (TestCpuFlag(kCpuHasNEON) && IS_ALIGNED(halfwidth, 64)) {
    CopyRow = CopyRow_NEON;
  }
#elif defined(HAS_COPYROW_X86)
  if (IS_ALIGNED(halfwidth, 4)) {
    CopyRow = CopyRow_X86;
#if defined(HAS_COPYROW_SSE2)
    if (TestCpuFlag(kCpuHasSSE2) && IS_ALIGNED(halfwidth, 32) &&
        IS_ALIGNED(src_u, 16) && IS_ALIGNED(src_stride_u, 16) &&
        IS_ALIGNED(src_v, 16) && IS_ALIGNED(src_stride_v, 16) &&
        IS_ALIGNED(dst_u, 16) && IS_ALIGNED(dst_stride_u, 16) &&
        IS_ALIGNED(dst_v, 16) && IS_ALIGNED(dst_stride_v, 16)) {
      CopyRow = CopyRow_SSE2;
    }
#endif
  }
#endif

  // Copy Y plane
  if (dst_y) {
    CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height);
  }

  // UpSample U plane.
  int y;
  for (y = 0; y < height - 1; y += 2) {
    CopyRow(src_u, dst_u, halfwidth);
    CopyRow(src_u, dst_u + dst_stride_u, halfwidth);
    src_u += src_stride_u;
    dst_u += dst_stride_u * 2;
  }
  if (height & 1) {
    CopyRow(src_u, dst_u, halfwidth);
  }

  // UpSample V plane.
  for (y = 0; y < height - 1; y += 2) {
    CopyRow(src_v, dst_v, halfwidth);
    CopyRow(src_v, dst_v + dst_stride_v, halfwidth);
    src_v += src_stride_v;
    dst_v += dst_stride_v * 2;
  }
  if (height & 1) {
    CopyRow(src_v, dst_v, halfwidth);
  }
  return 0;
}

// use Bilinear for upsampling chroma
void ScalePlaneBilinear(int src_width, int src_height,
                        int dst_width, int dst_height,
                        int src_stride, int dst_stride,
                        const uint8* src_ptr, uint8* dst_ptr);

LIBYUV_API
int I420ToI444(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height) {
  if (!src_y || !src_u|| !src_v ||
      !dst_y || !dst_u || !dst_v ||
      width <= 0 || height == 0) {
    return -1;
  }
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
  if (dst_y) {
    CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height);
  }

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

// 420 chroma is 1/2 width, 1/2 height
// 411 chroma is 1/4 width, 1x height
LIBYUV_API
int I420ToI411(const uint8* src_y, int src_stride_y,
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
    dst_y = dst_y + (height - 1) * dst_stride_y;
    dst_u = dst_u + (height - 1) * dst_stride_u;
    dst_v = dst_v + (height - 1) * dst_stride_v;
    dst_stride_y = -dst_stride_y;
    dst_stride_u = -dst_stride_u;
    dst_stride_v = -dst_stride_v;
  }

  // Copy Y plane
  if (dst_y) {
    CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height);
  }

  int halfwidth = (width + 1) >> 1;
  int halfheight = (height + 1) >> 1;
  int quarterwidth = (width + 3) >> 2;

  // Resample U plane.
  ScalePlaneBilinear(halfwidth, halfheight,  // from 1/2 width, 1/2 height
                     quarterwidth, height,  // to 1/4 width, 1x height
                     src_stride_u,
                     dst_stride_u,
                     src_u, dst_u);

  // Resample V plane.
  ScalePlaneBilinear(halfwidth, halfheight,  // from 1/2 width, 1/2 height
                     quarterwidth, height,  // to 1/4 width, 1x height
                     src_stride_v,
                     dst_stride_v,
                     src_v, dst_v);
  return 0;
}

// Copy to I400.  Source can be I420,422,444,400,NV12,NV21
LIBYUV_API
int I400Copy(const uint8* src_y, int src_stride_y,
             uint8* dst_y, int dst_stride_y,
             int width, int height) {
  if (!src_y || !dst_y ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_y = src_y + (height - 1) * src_stride_y;
    src_stride_y = -src_stride_y;
  }
  CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height);
  return 0;
}

// YUY2 - Macro-pixel = 2 image pixels
// Y0U0Y1V0....Y2U2Y3V2...Y4U4Y5V4....

// UYVY - Macro-pixel = 2 image pixels
// U0Y0V0Y1

#if !defined(YUV_DISABLE_ASM) && defined(_M_IX86)
#define HAS_I42XTOYUY2ROW_SSE2
__declspec(naked) __declspec(align(16))
static void I42xToYUY2Row_SSE2(const uint8* src_y,
                               const uint8* src_u,
                               const uint8* src_v,
                               uint8* dst_frame, int width) {
  __asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]    // src_y
    mov        esi, [esp + 8 + 8]    // src_u
    mov        edx, [esp + 8 + 12]   // src_v
    mov        edi, [esp + 8 + 16]   // dst_frame
    mov        ecx, [esp + 8 + 20]   // width
    sub        edx, esi

    align      16
  convertloop:
    movq       xmm2, qword ptr [esi] // U
    movq       xmm3, qword ptr [esi + edx] // V
    lea        esi, [esi + 8]
    punpcklbw  xmm2, xmm3 // UV
    movdqa     xmm0, [eax] // Y
    lea        eax, [eax + 16]
    movdqa     xmm1, xmm0
    punpcklbw  xmm0, xmm2 // YUYV
    punpckhbw  xmm1, xmm2
    movdqa     [edi], xmm0
    movdqa     [edi + 16], xmm1
    lea        edi, [edi + 32]
    sub        ecx, 16
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}

#define HAS_I42XTOUYVYROW_SSE2
__declspec(naked) __declspec(align(16))
static void I42xToUYVYRow_SSE2(const uint8* src_y,
                               const uint8* src_u,
                               const uint8* src_v,
                               uint8* dst_frame, int width) {
  __asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]    // src_y
    mov        esi, [esp + 8 + 8]    // src_u
    mov        edx, [esp + 8 + 12]   // src_v
    mov        edi, [esp + 8 + 16]   // dst_frame
    mov        ecx, [esp + 8 + 20]   // width
    sub        edx, esi

    align      16
  convertloop:
    movq       xmm2, qword ptr [esi] // U
    movq       xmm3, qword ptr [esi + edx] // V
    lea        esi, [esi + 8]
    punpcklbw  xmm2, xmm3 // UV
    movdqa     xmm0, [eax] // Y
    movdqa     xmm1, xmm2
    lea        eax, [eax + 16]
    punpcklbw  xmm1, xmm0 // UYVY
    punpckhbw  xmm2, xmm0
    movdqa     [edi], xmm1
    movdqa     [edi + 16], xmm2
    lea        edi, [edi + 32]
    sub        ecx, 16
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}
#elif !defined(YUV_DISABLE_ASM) && (defined(__x86_64__) || defined(__i386__))
#define HAS_I42XTOYUY2ROW_SSE2
static void I42xToYUY2Row_SSE2(const uint8* src_y,
                               const uint8* src_u,
                               const uint8* src_v,
                               uint8* dst_frame, int width) {
 asm volatile (
    "sub        %1,%2                            \n"
    ".p2align  4                                 \n"
  "1:                                            \n"
    "movq      (%1),%%xmm2                       \n"
    "movq      (%1,%2,1),%%xmm3                  \n"
    "lea       0x8(%1),%1                        \n"
    "punpcklbw %%xmm3,%%xmm2                     \n"
    "movdqa    (%0),%%xmm0                       \n"
    "lea       0x10(%0),%0                       \n"
    "movdqa    %%xmm0,%%xmm1                     \n"
    "punpcklbw %%xmm2,%%xmm0                     \n"
    "punpckhbw %%xmm2,%%xmm1                     \n"
    "movdqa    %%xmm0,(%3)                       \n"
    "movdqa    %%xmm1,0x10(%3)                   \n"
    "lea       0x20(%3),%3                       \n"
    "sub       $0x10,%4                          \n"
    "jg         1b                               \n"
    : "+r"(src_y),  // %0
      "+r"(src_u),  // %1
      "+r"(src_v),  // %2
      "+r"(dst_frame),  // %3
      "+rm"(width)  // %4
    :
    : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3"
#endif
  );
}

#define HAS_I42XTOUYVYROW_SSE2
static void I42xToUYVYRow_SSE2(const uint8* src_y,
                               const uint8* src_u,
                               const uint8* src_v,
                               uint8* dst_frame, int width) {
 asm volatile (
    "sub        %1,%2                            \n"
    ".p2align  4                                 \n"
  "1:                                            \n"
    "movq      (%1),%%xmm2                       \n"
    "movq      (%1,%2,1),%%xmm3                  \n"
    "lea       0x8(%1),%1                        \n"
    "punpcklbw %%xmm3,%%xmm2                     \n"
    "movdqa    (%0),%%xmm0                       \n"
    "movdqa    %%xmm2,%%xmm1                     \n"
    "lea       0x10(%0),%0                       \n"
    "punpcklbw %%xmm0,%%xmm1                     \n"
    "punpckhbw %%xmm0,%%xmm2                     \n"
    "movdqa    %%xmm1,(%3)                       \n"
    "movdqa    %%xmm2,0x10(%3)                   \n"
    "lea       0x20(%3),%3                       \n"
    "sub       $0x10,%4                          \n"
    "jg         1b                               \n"
    : "+r"(src_y),  // %0
      "+r"(src_u),  // %1
      "+r"(src_v),  // %2
      "+r"(dst_frame),  // %3
      "+rm"(width)  // %4
    :
    : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3"
#endif
  );
}
#endif

static void I42xToYUY2Row_C(const uint8* src_y,
                            const uint8* src_u,
                            const uint8* src_v,
                            uint8* dst_frame, int width) {
    for (int x = 0; x < width - 1; x += 2) {
      dst_frame[0] = src_y[0];
      dst_frame[1] = src_u[0];
      dst_frame[2] = src_y[1];
      dst_frame[3] = src_v[0];
      dst_frame += 4;
      src_y += 2;
      src_u += 1;
      src_v += 1;
    }
    if (width & 1) {
      dst_frame[0] = src_y[0];
      dst_frame[1] = src_u[0];
      dst_frame[2] = src_y[0];  // duplicate last y
      dst_frame[3] = src_v[0];
    }
}

static void I42xToUYVYRow_C(const uint8* src_y,
                            const uint8* src_u,
                            const uint8* src_v,
                            uint8* dst_frame, int width) {
    for (int x = 0; x < width - 1; x += 2) {
      dst_frame[0] = src_u[0];
      dst_frame[1] = src_y[0];
      dst_frame[2] = src_v[0];
      dst_frame[3] = src_y[1];
      dst_frame += 4;
      src_y += 2;
      src_u += 1;
      src_v += 1;
    }
    if (width & 1) {
      dst_frame[0] = src_u[0];
      dst_frame[1] = src_y[0];
      dst_frame[2] = src_v[0];
      dst_frame[3] = src_y[0];  // duplicate last y
    }
}

// Visual C x86 or GCC little endian.
#if defined(__x86_64__) || defined(_M_X64) || \
  defined(__i386__) || defined(_M_IX86) || \
  defined(__arm__) || defined(_M_ARM) || \
  (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define LIBYUV_LITTLE_ENDIAN
#endif

#ifdef LIBYUV_LITTLE_ENDIAN
#define WRITEWORD(p, v) *reinterpret_cast<uint32*>(p) = v
#else
static inline void WRITEWORD(uint8* p, uint32 v) {
  p[0] = (uint8)(v & 255);
  p[1] = (uint8)((v >> 8) & 255);
  p[2] = (uint8)((v >> 16) & 255);
  p[3] = (uint8)((v >> 24) & 255);
}
#endif

#define EIGHTTOTEN(x) (x << 2 | x >> 6)
static void UYVYToV210Row_C(const uint8* src_uyvy, uint8* dst_v210, int width) {
  for (int x = 0; x < width; x += 6) {
    WRITEWORD(dst_v210 + 0, (EIGHTTOTEN(src_uyvy[0])) |
                            (EIGHTTOTEN(src_uyvy[1]) << 10) |
                            (EIGHTTOTEN(src_uyvy[2]) << 20));
    WRITEWORD(dst_v210 + 4, (EIGHTTOTEN(src_uyvy[3])) |
                            (EIGHTTOTEN(src_uyvy[4]) << 10) |
                            (EIGHTTOTEN(src_uyvy[5]) << 20));
    WRITEWORD(dst_v210 + 8, (EIGHTTOTEN(src_uyvy[6])) |
                            (EIGHTTOTEN(src_uyvy[7]) << 10) |
                            (EIGHTTOTEN(src_uyvy[8]) << 20));
    WRITEWORD(dst_v210 + 12, (EIGHTTOTEN(src_uyvy[9])) |
                             (EIGHTTOTEN(src_uyvy[10]) << 10) |
                             (EIGHTTOTEN(src_uyvy[11]) << 20));
    src_uyvy += 12;
    dst_v210 += 16;
  }
}

// TODO(fbarchard): Deprecate, move or expand 422 support?
LIBYUV_API
int I422ToYUY2(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_frame, int dst_stride_frame,
               int width, int height) {
  if (!src_y || !src_u || !src_v || !dst_frame ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_frame = dst_frame + (height - 1) * dst_stride_frame;
    dst_stride_frame = -dst_stride_frame;
  }
  void (*I42xToYUY2Row)(const uint8* src_y, const uint8* src_u,
                        const uint8* src_v, uint8* dst_frame, int width) =
      I42xToYUY2Row_C;
#if defined(HAS_I42XTOYUY2ROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(width, 16) &&
      IS_ALIGNED(src_y, 16) && IS_ALIGNED(src_stride_y, 16) &&
      IS_ALIGNED(dst_frame, 16) && IS_ALIGNED(dst_stride_frame, 16)) {
    I42xToYUY2Row = I42xToYUY2Row_SSE2;
  }
#endif

  for (int y = 0; y < height; ++y) {
    I42xToYUY2Row(src_y, src_u, src_y, dst_frame, width);
    src_y += src_stride_y;
    src_u += src_stride_u;
    src_v += src_stride_v;
    dst_frame += dst_stride_frame;
  }
  return 0;
}

LIBYUV_API
int I420ToYUY2(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_frame, int dst_stride_frame,
               int width, int height) {
  if (!src_y || !src_u || !src_v || !dst_frame ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_frame = dst_frame + (height - 1) * dst_stride_frame;
    dst_stride_frame = -dst_stride_frame;
  }
  void (*I42xToYUY2Row)(const uint8* src_y, const uint8* src_u,
                        const uint8* src_v, uint8* dst_frame, int width) =
      I42xToYUY2Row_C;
#if defined(HAS_I42XTOYUY2ROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(width, 16) &&
      IS_ALIGNED(src_y, 16) && IS_ALIGNED(src_stride_y, 16) &&
      IS_ALIGNED(dst_frame, 16) && IS_ALIGNED(dst_stride_frame, 16)) {
    I42xToYUY2Row = I42xToYUY2Row_SSE2;
  }
#endif

  for (int y = 0; y < height - 1; y += 2) {
    I42xToYUY2Row(src_y, src_u, src_v, dst_frame, width);
    I42xToYUY2Row(src_y + src_stride_y, src_u, src_v,
                  dst_frame + dst_stride_frame, width);
    src_y += src_stride_y * 2;
    src_u += src_stride_u;
    src_v += src_stride_v;
    dst_frame += dst_stride_frame * 2;
  }
  if (height & 1) {
    I42xToYUY2Row(src_y, src_u, src_v, dst_frame, width);
  }
  return 0;
}

// TODO(fbarchard): Deprecate, move or expand 422 support?
LIBYUV_API
int I422ToUYVY(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_frame, int dst_stride_frame,
               int width, int height) {
  if (!src_y || !src_u || !src_v || !dst_frame ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_frame = dst_frame + (height - 1) * dst_stride_frame;
    dst_stride_frame = -dst_stride_frame;
  }
  void (*I42xToUYVYRow)(const uint8* src_y, const uint8* src_u,
                        const uint8* src_v, uint8* dst_frame, int width) =
      I42xToUYVYRow_C;
#if defined(HAS_I42XTOUYVYROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(width, 16) &&
      IS_ALIGNED(src_y, 16) && IS_ALIGNED(src_stride_y, 16) &&
      IS_ALIGNED(dst_frame, 16) && IS_ALIGNED(dst_stride_frame, 16)) {
    I42xToUYVYRow = I42xToUYVYRow_SSE2;
  }
#endif

  for (int y = 0; y < height; ++y) {
    I42xToUYVYRow(src_y, src_u, src_y, dst_frame, width);
    src_y += src_stride_y;
    src_u += src_stride_u;
    src_v += src_stride_v;
    dst_frame += dst_stride_frame;
  }
  return 0;
}

LIBYUV_API
int I420ToUYVY(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_frame, int dst_stride_frame,
               int width, int height) {
  if (!src_y || !src_u || !src_v || !dst_frame ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_frame = dst_frame + (height - 1) * dst_stride_frame;
    dst_stride_frame = -dst_stride_frame;
  }
  void (*I42xToUYVYRow)(const uint8* src_y, const uint8* src_u,
                        const uint8* src_v, uint8* dst_frame, int width) =
      I42xToUYVYRow_C;
#if defined(HAS_I42XTOUYVYROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(width, 16) &&
      IS_ALIGNED(src_y, 16) && IS_ALIGNED(src_stride_y, 16) &&
      IS_ALIGNED(dst_frame, 16) && IS_ALIGNED(dst_stride_frame, 16)) {
    I42xToUYVYRow = I42xToUYVYRow_SSE2;
  }
#endif

  for (int y = 0; y < height - 1; y += 2) {
    I42xToUYVYRow(src_y, src_u, src_v, dst_frame, width);
    I42xToUYVYRow(src_y + src_stride_y, src_u, src_v,
                  dst_frame + dst_stride_frame, width);
    src_y += src_stride_y * 2;
    src_u += src_stride_u;
    src_v += src_stride_v;
    dst_frame += dst_stride_frame * 2;
  }
  if (height & 1) {
    I42xToUYVYRow(src_y, src_u, src_v, dst_frame, width);
  }
  return 0;
}

LIBYUV_API
int I420ToV210(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_frame, int dst_stride_frame,
               int width, int height) {
  if (width * 16 / 6 > kMaxStride) {  // Row buffer of V210 is required.
    return -1;
  } else if (!src_y || !src_u || !src_v || !dst_frame ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_frame = dst_frame + (height - 1) * dst_stride_frame;
    dst_stride_frame = -dst_stride_frame;
  }

  SIMD_ALIGNED(uint8 row[kMaxStride]);
  void (*UYVYToV210Row)(const uint8* src_uyvy, uint8* dst_v210, int pix);
  UYVYToV210Row = UYVYToV210Row_C;

  void (*I42xToUYVYRow)(const uint8* src_y, const uint8* src_u,
                        const uint8* src_v, uint8* dst_frame, int width) =
      I42xToUYVYRow_C;
#if defined(HAS_I42XTOUYVYROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(width, 16) &&
      IS_ALIGNED(src_y, 16) && IS_ALIGNED(src_stride_y, 16)) {
    I42xToUYVYRow = I42xToUYVYRow_SSE2;
  }
#endif

  for (int y = 0; y < height - 1; y += 2) {
    I42xToUYVYRow(src_y, src_u, src_v, row, width);
    UYVYToV210Row(row, dst_frame, width);
    I42xToUYVYRow(src_y + src_stride_y, src_u, src_v, row, width);
    UYVYToV210Row(row, dst_frame + dst_stride_frame, width);

    src_y += src_stride_y * 2;
    src_u += src_stride_u;
    src_v += src_stride_v;
    dst_frame += dst_stride_frame * 2;
  }
  if (height & 1) {
    I42xToUYVYRow(src_y, src_u, src_v, row, width);
    UYVYToV210Row(row, dst_frame, width);
  }
  return 0;
}

// Convert I420 to ARGB.
LIBYUV_API
int I420ToARGB(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_argb, int dst_stride_argb,
               int width, int height) {
  if (!src_y || !src_u || !src_v || !dst_argb ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_argb = dst_argb + (height - 1) * dst_stride_argb;
    dst_stride_argb = -dst_stride_argb;
  }
  void (*I422ToARGBRow)(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width) = I422ToARGBRow_C;
#if defined(HAS_I422TOARGBROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    I422ToARGBRow = I422ToARGBRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      I422ToARGBRow = I422ToARGBRow_NEON;
    }
  }
#elif defined(HAS_I422TOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) && width >= 8) {
    I422ToARGBRow = I422ToARGBRow_Any_SSSE3;
    if (IS_ALIGNED(width, 8)) {
      I422ToARGBRow = I422ToARGBRow_Unaligned_SSSE3;
      if (IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
        I422ToARGBRow = I422ToARGBRow_SSSE3;
      }
    }
  }
#endif

  for (int y = 0; y < height; ++y) {
    I422ToARGBRow(src_y, src_u, src_v, dst_argb, width);
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
LIBYUV_API
int I420ToBGRA(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_bgra, int dst_stride_bgra,
               int width, int height) {
  if (!src_y || !src_u || !src_v ||
      !dst_bgra ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_bgra = dst_bgra + (height - 1) * dst_stride_bgra;
    dst_stride_bgra = -dst_stride_bgra;
  }
  void (*I422ToBGRARow)(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width) = I422ToBGRARow_C;
#if defined(HAS_I422TOBGRAROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    I422ToBGRARow = I422ToBGRARow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      I422ToBGRARow = I422ToBGRARow_NEON;
    }
  }
#elif defined(HAS_I422TOBGRAROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) && width >= 8) {
    I422ToBGRARow = I422ToBGRARow_Any_SSSE3;
    if (IS_ALIGNED(width, 8)) {
      I422ToBGRARow = I422ToBGRARow_Unaligned_SSSE3;
      if (IS_ALIGNED(dst_bgra, 16) && IS_ALIGNED(dst_stride_bgra, 16)) {
        I422ToBGRARow = I422ToBGRARow_SSSE3;
      }
    }
  }
#endif

  for (int y = 0; y < height; ++y) {
    I422ToBGRARow(src_y, src_u, src_v, dst_bgra, width);
    dst_bgra += dst_stride_bgra;
    src_y += src_stride_y;
    if (y & 1) {
      src_u += src_stride_u;
      src_v += src_stride_v;
    }
  }
  return 0;
}

// Convert I420 to ABGR.
LIBYUV_API
int I420ToABGR(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_abgr, int dst_stride_abgr,
               int width, int height) {
  if (!src_y || !src_u || !src_v ||
      !dst_abgr ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_abgr = dst_abgr + (height - 1) * dst_stride_abgr;
    dst_stride_abgr = -dst_stride_abgr;
  }
  void (*I422ToABGRRow)(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width) = I422ToABGRRow_C;
#if defined(HAS_I422TOABGRROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    I422ToABGRRow = I422ToABGRRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      I422ToABGRRow = I422ToABGRRow_NEON;
    }
  }
#elif defined(HAS_I422TOABGRROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) && width >= 8) {
    I422ToABGRRow = I422ToABGRRow_Any_SSSE3;
    if (IS_ALIGNED(width, 8)) {
      I422ToABGRRow = I422ToABGRRow_Unaligned_SSSE3;
      if (IS_ALIGNED(dst_abgr, 16) && IS_ALIGNED(dst_stride_abgr, 16)) {
        I422ToABGRRow = I422ToABGRRow_SSSE3;
      }
    }
  }
#endif

  for (int y = 0; y < height; ++y) {
    I422ToABGRRow(src_y, src_u, src_v, dst_abgr, width);
    dst_abgr += dst_stride_abgr;
    src_y += src_stride_y;
    if (y & 1) {
      src_u += src_stride_u;
      src_v += src_stride_v;
    }
  }
  return 0;
}

// Convert I420 to RGBA.
LIBYUV_API
int I420ToRGBA(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_rgba, int dst_stride_rgba,
               int width, int height) {
  if (!src_y || !src_u || !src_v ||
      !dst_rgba ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_rgba = dst_rgba + (height - 1) * dst_stride_rgba;
    dst_stride_rgba = -dst_stride_rgba;
  }
  void (*I422ToRGBARow)(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width) = I422ToRGBARow_C;
#if defined(HAS_I422TORGBAROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    I422ToRGBARow = I422ToRGBARow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      I422ToRGBARow = I422ToRGBARow_NEON;
    }
  }
#elif defined(HAS_I422TORGBAROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) && width >= 8) {
    I422ToRGBARow = I422ToRGBARow_Any_SSSE3;
    if (IS_ALIGNED(width, 8)) {
      I422ToRGBARow = I422ToRGBARow_Unaligned_SSSE3;
      if (IS_ALIGNED(dst_rgba, 16) && IS_ALIGNED(dst_stride_rgba, 16)) {
        I422ToRGBARow = I422ToRGBARow_SSSE3;
      }
    }
  }
#endif

  for (int y = 0; y < height; ++y) {
    I422ToRGBARow(src_y, src_u, src_v, dst_rgba, width);
    dst_rgba += dst_stride_rgba;
    src_y += src_stride_y;
    if (y & 1) {
      src_u += src_stride_u;
      src_v += src_stride_v;
    }
  }
  return 0;
}

// Convert I420 to RGB24.
// TODO(fbarchard): One step I420ToRGB24Row_NEON.
LIBYUV_API
int I420ToRGB24(const uint8* src_y, int src_stride_y,
                const uint8* src_u, int src_stride_u,
                const uint8* src_v, int src_stride_v,
                uint8* dst_argb, int dst_stride_argb,
                int width, int height) {
  if (!src_y || !src_u || !src_v ||
      !dst_argb ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_argb = dst_argb + (height - 1) * dst_stride_argb;
    dst_stride_argb = -dst_stride_argb;
  }
  void (*I422ToARGBRow)(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width) = I422ToARGBRow_C;
#if defined(HAS_I422TOARGBROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    I422ToARGBRow = I422ToARGBRow_NEON;
  }
#elif defined(HAS_I422TOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    I422ToARGBRow = I422ToARGBRow_SSSE3;
  }
#endif

  SIMD_ALIGNED(uint8 row[kMaxStride]);
  void (*ARGBToRGB24Row)(const uint8* src_argb, uint8* dst_rgb, int pix) =
      ARGBToRGB24Row_C;
#if defined(HAS_ARGBTORGB24ROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    if (width * 3 <= kMaxStride) {
      ARGBToRGB24Row = ARGBToRGB24Row_Any_SSSE3;
    }
    if (IS_ALIGNED(width, 16) &&
        IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
      ARGBToRGB24Row = ARGBToRGB24Row_SSSE3;
    }
  }
#endif
#if defined(HAS_ARGBTORGB24ROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    if (width * 3 <= kMaxStride) {
      ARGBToRGB24Row = ARGBToRGB24Row_Any_NEON;
    }
    if (IS_ALIGNED(width, 8)) {
      ARGBToRGB24Row = ARGBToRGB24Row_NEON;
    }
  }
#endif

  for (int y = 0; y < height; ++y) {
    I422ToARGBRow(src_y, src_u, src_v, row, width);
    ARGBToRGB24Row(row, dst_argb, width);
    dst_argb += dst_stride_argb;
    src_y += src_stride_y;
    if (y & 1) {
      src_u += src_stride_u;
      src_v += src_stride_v;
    }
  }
  return 0;
}

// Convert I420 to RAW.
// TODO(fbarchard): One step I420ToRAWRow_NEON.
LIBYUV_API
int I420ToRAW(const uint8* src_y, int src_stride_y,
              const uint8* src_u, int src_stride_u,
              const uint8* src_v, int src_stride_v,
              uint8* dst_argb, int dst_stride_argb,
              int width, int height) {
  if (!src_y || !src_u || !src_v ||
      !dst_argb ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_argb = dst_argb + (height - 1) * dst_stride_argb;
    dst_stride_argb = -dst_stride_argb;
  }
  void (*I422ToARGBRow)(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width) = I422ToARGBRow_C;
#if defined(HAS_I422TOARGBROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    I422ToARGBRow = I422ToARGBRow_NEON;
  }
#elif defined(HAS_I422TOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    I422ToARGBRow = I422ToARGBRow_SSSE3;
  }
#endif

  SIMD_ALIGNED(uint8 row[kMaxStride]);
  void (*ARGBToRAWRow)(const uint8* src_argb, uint8* dst_rgb, int pix) =
      ARGBToRAWRow_C;
#if defined(HAS_ARGBTORAWROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    if (width * 3 <= kMaxStride) {
      ARGBToRAWRow = ARGBToRAWRow_Any_SSSE3;
    }
    if (IS_ALIGNED(width, 16) &&
        IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
      ARGBToRAWRow = ARGBToRAWRow_SSSE3;
    }
  }
#elif defined(HAS_ARGBTORAWROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    if (width * 3 <= kMaxStride) {
      ARGBToRAWRow = ARGBToRAWRow_Any_NEON;
    }
    if (IS_ALIGNED(width, 8)) {
      ARGBToRAWRow = ARGBToRAWRow_NEON;
    }
  }
#endif

  for (int y = 0; y < height; ++y) {
    I422ToARGBRow(src_y, src_u, src_v, row, width);
    ARGBToRAWRow(row, dst_argb, width);
    dst_argb += dst_stride_argb;
    src_y += src_stride_y;
    if (y & 1) {
      src_u += src_stride_u;
      src_v += src_stride_v;
    }
  }
  return 0;
}

// Convert I420 to RGB565.
LIBYUV_API
int I420ToRGB565(const uint8* src_y, int src_stride_y,
                 const uint8* src_u, int src_stride_u,
                 const uint8* src_v, int src_stride_v,
                 uint8* dst_rgb, int dst_stride_rgb,
                 int width, int height) {
  if (!src_y || !src_u || !src_v ||
      !dst_rgb ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_rgb = dst_rgb + (height - 1) * dst_stride_rgb;
    dst_stride_rgb = -dst_stride_rgb;
  }
  void (*I422ToARGBRow)(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width) = I422ToARGBRow_C;
#if defined(HAS_I422TOARGBROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    I422ToARGBRow = I422ToARGBRow_NEON;
  }
#elif defined(HAS_I422TOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    I422ToARGBRow = I422ToARGBRow_SSSE3;
  }
#endif

  SIMD_ALIGNED(uint8 row[kMaxStride]);
  void (*ARGBToRGB565Row)(const uint8* src_rgb, uint8* dst_rgb, int pix) =
      ARGBToRGB565Row_C;
#if defined(HAS_ARGBTORGB565ROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    if (width * 2 <= kMaxStride) {
      ARGBToRGB565Row = ARGBToRGB565Row_Any_SSE2;
    }
    if (IS_ALIGNED(width, 4)) {
      ARGBToRGB565Row = ARGBToRGB565Row_SSE2;
    }
  }
#endif

  for (int y = 0; y < height; ++y) {
    I422ToARGBRow(src_y, src_u, src_v, row, width);
    ARGBToRGB565Row(row, dst_rgb, width);
    dst_rgb += dst_stride_rgb;
    src_y += src_stride_y;
    if (y & 1) {
      src_u += src_stride_u;
      src_v += src_stride_v;
    }
  }
  return 0;
}

// Convert I420 to ARGB1555.
LIBYUV_API
int I420ToARGB1555(const uint8* src_y, int src_stride_y,
                   const uint8* src_u, int src_stride_u,
                   const uint8* src_v, int src_stride_v,
                   uint8* dst_argb, int dst_stride_argb,
                   int width, int height) {
  if (!src_y || !src_u || !src_v ||
      !dst_argb ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_argb = dst_argb + (height - 1) * dst_stride_argb;
    dst_stride_argb = -dst_stride_argb;
  }
  void (*I422ToARGBRow)(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width) = I422ToARGBRow_C;
#if defined(HAS_I422TOARGBROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    I422ToARGBRow = I422ToARGBRow_NEON;
  }
#elif defined(HAS_I422TOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    I422ToARGBRow = I422ToARGBRow_SSSE3;
  }
#endif

  SIMD_ALIGNED(uint8 row[kMaxStride]);
  void (*ARGBToARGB1555Row)(const uint8* src_argb, uint8* dst_rgb, int pix) =
      ARGBToARGB1555Row_C;
#if defined(HAS_ARGBTOARGB1555ROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    if (width * 2 <= kMaxStride) {
      ARGBToARGB1555Row = ARGBToARGB1555Row_Any_SSE2;
    }
    if (IS_ALIGNED(width, 4)) {
      ARGBToARGB1555Row = ARGBToARGB1555Row_SSE2;
    }
  }
#endif

  for (int y = 0; y < height; ++y) {
    I422ToARGBRow(src_y, src_u, src_v, row, width);
    ARGBToARGB1555Row(row, dst_argb, width);
    dst_argb += dst_stride_argb;
    src_y += src_stride_y;
    if (y & 1) {
      src_u += src_stride_u;
      src_v += src_stride_v;
    }
  }
  return 0;
}

// Convert I420 to ARGB4444.
LIBYUV_API
int I420ToARGB4444(const uint8* src_y, int src_stride_y,
                   const uint8* src_u, int src_stride_u,
                   const uint8* src_v, int src_stride_v,
                   uint8* dst_argb, int dst_stride_argb,
                   int width, int height) {
  if (!src_y || !src_u || !src_v ||
      !dst_argb ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_argb = dst_argb + (height - 1) * dst_stride_argb;
    dst_stride_argb = -dst_stride_argb;
  }
  void (*I422ToARGBRow)(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width) = I422ToARGBRow_C;
#if defined(HAS_I422TOARGBROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    I422ToARGBRow = I422ToARGBRow_NEON;
  }
#elif defined(HAS_I422TOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    I422ToARGBRow = I422ToARGBRow_SSSE3;
  }
#endif

  SIMD_ALIGNED(uint8 row[kMaxStride]);
  void (*ARGBToARGB4444Row)(const uint8* src_argb, uint8* dst_rgb, int pix) =
     ARGBToARGB4444Row_C;
#if defined(HAS_ARGBTOARGB4444ROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    if (width * 2 <= kMaxStride) {
      ARGBToARGB4444Row = ARGBToARGB4444Row_Any_SSE2;
    }
    if (IS_ALIGNED(width, 4)) {
      ARGBToARGB4444Row = ARGBToARGB4444Row_SSE2;
    }
  }
#endif

  for (int y = 0; y < height; ++y) {
    I422ToARGBRow(src_y, src_u, src_v, row, width);
    ARGBToARGB4444Row(row, dst_argb, width);
    dst_argb += dst_stride_argb;
    src_y += src_stride_y;
    if (y & 1) {
      src_u += src_stride_u;
      src_v += src_stride_v;
    }
  }
  return 0;
}

// Convert I420 to specified format
LIBYUV_API
int ConvertFromI420(const uint8* y, int y_stride,
                    const uint8* u, int u_stride,
                    const uint8* v, int v_stride,
                    uint8* dst_sample, int dst_sample_stride,
                    int width, int height,
                    uint32 format) {
  if (!y || !u|| !v || !dst_sample ||
      width <= 0 || height == 0) {
    return -1;
  }
  int r = 0;
  switch (format) {
    // Single plane formats
    case FOURCC_YUY2:
      r = I420ToYUY2(y, y_stride,
                     u, u_stride,
                     v, v_stride,
                     dst_sample,
                     dst_sample_stride ? dst_sample_stride : width * 2,
                     width, height);
      break;
    case FOURCC_UYVY:
      r = I420ToUYVY(y, y_stride,
                     u, u_stride,
                     v, v_stride,
                     dst_sample,
                     dst_sample_stride ? dst_sample_stride : width * 2,
                     width, height);
      break;
    case FOURCC_V210:
      r = I420ToV210(y, y_stride,
                     u, u_stride,
                     v, v_stride,
                     dst_sample,
                     dst_sample_stride ? dst_sample_stride :
                         (width + 47) / 48 * 128,
                     width, height);
      break;
    case FOURCC_RGBP:
      r = I420ToRGB565(y, y_stride,
                       u, u_stride,
                       v, v_stride,
                       dst_sample,
                       dst_sample_stride ? dst_sample_stride : width * 2,
                       width, height);
      break;
    case FOURCC_RGBO:
      r = I420ToARGB1555(y, y_stride,
                         u, u_stride,
                         v, v_stride,
                         dst_sample,
                         dst_sample_stride ? dst_sample_stride : width * 2,
                         width, height);
      break;
    case FOURCC_R444:
      r = I420ToARGB4444(y, y_stride,
                         u, u_stride,
                         v, v_stride,
                         dst_sample,
                         dst_sample_stride ? dst_sample_stride : width * 2,
                         width, height);
      break;
    case FOURCC_24BG:
      r = I420ToRGB24(y, y_stride,
                      u, u_stride,
                      v, v_stride,
                      dst_sample,
                      dst_sample_stride ? dst_sample_stride : width * 3,
                      width, height);
      break;
    case FOURCC_RAW:
      r = I420ToRAW(y, y_stride,
                    u, u_stride,
                    v, v_stride,
                    dst_sample,
                    dst_sample_stride ? dst_sample_stride : width * 3,
                    width, height);
      break;
    case FOURCC_ARGB:
      r = I420ToARGB(y, y_stride,
                     u, u_stride,
                     v, v_stride,
                     dst_sample,
                     dst_sample_stride ? dst_sample_stride : width * 4,
                     width, height);
      break;
    case FOURCC_BGRA:
      r = I420ToBGRA(y, y_stride,
                     u, u_stride,
                     v, v_stride,
                     dst_sample,
                     dst_sample_stride ? dst_sample_stride : width * 4,
                     width, height);
      break;
    case FOURCC_ABGR:
      r = I420ToABGR(y, y_stride,
                     u, u_stride,
                     v, v_stride,
                     dst_sample,
                     dst_sample_stride ? dst_sample_stride : width * 4,
                     width, height);
      break;
    case FOURCC_RGBA:
      r = I420ToRGBA(y, y_stride,
                     u, u_stride,
                     v, v_stride,
                     dst_sample,
                     dst_sample_stride ? dst_sample_stride : width * 4,
                     width, height);
      break;
    case FOURCC_BGGR:
      r = I420ToBayerBGGR(y, y_stride,
                          u, u_stride,
                          v, v_stride,
                          dst_sample,
                          dst_sample_stride ? dst_sample_stride : width,
                          width, height);
      break;
    case FOURCC_GBRG:
      r = I420ToBayerGBRG(y, y_stride,
                          u, u_stride,
                          v, v_stride,
                          dst_sample,
                          dst_sample_stride ? dst_sample_stride : width,
                          width, height);
      break;
    case FOURCC_GRBG:
      r = I420ToBayerGRBG(y, y_stride,
                          u, u_stride,
                          v, v_stride,
                          dst_sample,
                          dst_sample_stride ? dst_sample_stride : width,
                          width, height);
      break;
    case FOURCC_RGGB:
      r = I420ToBayerRGGB(y, y_stride,
                          u, u_stride,
                          v, v_stride,
                          dst_sample,
                          dst_sample_stride ? dst_sample_stride : width,
                          width, height);
      break;
    case FOURCC_I400:
      r = I400Copy(y, y_stride,
                   dst_sample,
                   dst_sample_stride ? dst_sample_stride : width,
                   width, height);
      break;
    // Triplanar formats
    // TODO(fbarchard): halfstride instead of halfwidth
    case FOURCC_I420:
    case FOURCC_YV12: {
      int halfwidth = (width + 1) / 2;
      int halfheight = (height + 1) / 2;
      uint8* dst_u;
      uint8* dst_v;
      if (format == FOURCC_I420) {
        dst_u = dst_sample + width * height;
        dst_v = dst_u + halfwidth * halfheight;
      } else {
        dst_v = dst_sample + width * height;
        dst_u = dst_v + halfwidth * halfheight;
      }
      r = I420Copy(y, y_stride,
                   u, u_stride,
                   v, v_stride,
                   dst_sample, width,
                   dst_u, halfwidth,
                   dst_v, halfwidth,
                   width, height);
      break;
    }
    case FOURCC_I422:
    case FOURCC_YV16: {
      int halfwidth = (width + 1) / 2;
      uint8* dst_u;
      uint8* dst_v;
      if (format == FOURCC_I422) {
        dst_u = dst_sample + width * height;
        dst_v = dst_u + halfwidth * height;
      } else {
        dst_v = dst_sample + width * height;
        dst_u = dst_v + halfwidth * height;
      }
      r = I420ToI422(y, y_stride,
                     u, u_stride,
                     v, v_stride,
                     dst_sample, width,
                     dst_u, halfwidth,
                     dst_v, halfwidth,
                     width, height);
      break;
    }
    case FOURCC_I444:
    case FOURCC_YV24: {
      uint8* dst_u;
      uint8* dst_v;
      if (format == FOURCC_I444) {
        dst_u = dst_sample + width * height;
        dst_v = dst_u + width * height;
      } else {
        dst_v = dst_sample + width * height;
        dst_u = dst_v + width * height;
      }
      r = I420ToI444(y, y_stride,
                     u, u_stride,
                     v, v_stride,
                     dst_sample, width,
                     dst_u, width,
                     dst_v, width,
                     width, height);
      break;
    }
    case FOURCC_I411: {
      int quarterwidth = (width + 3) / 4;
      uint8* dst_u = dst_sample + width * height;
      uint8* dst_v = dst_u + quarterwidth * height;
      r = I420ToI411(y, y_stride,
                     u, u_stride,
                     v, v_stride,
                     dst_sample, width,
                     dst_u, quarterwidth,
                     dst_v, quarterwidth,
                     width, height);
      break;
    }

    // Formats not supported - MJPG, biplanar, some rgb formats.
    default:
      return -1;  // unknown fourcc - return failure code.
  }
  return r;
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
