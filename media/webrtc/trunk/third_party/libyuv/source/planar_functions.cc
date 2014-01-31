/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/planar_functions.h"

#include <string.h>  // for memset()

#include "libyuv/cpu_id.h"
#ifdef HAVE_JPEG
#include "libyuv/mjpeg_decoder.h"
#endif
#include "libyuv/row.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Copy a plane of data
LIBYUV_API
void CopyPlane(const uint8* src_y, int src_stride_y,
               uint8* dst_y, int dst_stride_y,
               int width, int height) {
  void (*CopyRow)(const uint8* src, uint8* dst, int width) = CopyRow_C;
#if defined(HAS_COPYROW_NEON)
  if (TestCpuFlag(kCpuHasNEON) && IS_ALIGNED(width, 64)) {
    CopyRow = CopyRow_NEON;
  }
#endif
#if defined(HAS_COPYROW_X86)
  if (TestCpuFlag(kCpuHasX86) && IS_ALIGNED(width, 4)) {
    CopyRow = CopyRow_X86;
  }
#endif
#if defined(HAS_COPYROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) && IS_ALIGNED(width, 32) &&
      IS_ALIGNED(src_y, 16) && IS_ALIGNED(src_stride_y, 16) &&
      IS_ALIGNED(dst_y, 16) && IS_ALIGNED(dst_stride_y, 16)) {
    CopyRow = CopyRow_SSE2;
  }
#endif

  // Copy plane
  for (int y = 0; y < height; ++y) {
    CopyRow(src_y, dst_y, width);
    src_y += src_stride_y;
    dst_y += dst_stride_y;
  }
}

// Convert I420 to I400.
LIBYUV_API
int I420ToI400(const uint8* src_y, int src_stride_y,
               uint8*, int,  // src_u
               uint8*, int,  // src_v
               uint8* dst_y, int dst_stride_y,
               int width, int height) {
  if (!src_y || !dst_y || width <= 0 || height == 0) {
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

// Mirror a plane of data
void MirrorPlane(const uint8* src_y, int src_stride_y,
                 uint8* dst_y, int dst_stride_y,
                 int width, int height) {
  void (*MirrorRow)(const uint8* src, uint8* dst, int width) = MirrorRow_C;
#if defined(HAS_MIRRORROW_NEON)
  if (TestCpuFlag(kCpuHasNEON) && IS_ALIGNED(width, 16)) {
    MirrorRow = MirrorRow_NEON;
  }
#endif
#if defined(HAS_MIRRORROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) && IS_ALIGNED(width, 16)) {
    MirrorRow = MirrorRow_SSE2;
#if defined(HAS_MIRRORROW_SSSE3)
    if (TestCpuFlag(kCpuHasSSSE3) &&
        IS_ALIGNED(src_y, 16) && IS_ALIGNED(src_stride_y, 16)) {
      MirrorRow = MirrorRow_SSSE3;
    }
#endif
  }
#endif

  // Mirror plane
  for (int y = 0; y < height; ++y) {
    MirrorRow(src_y, dst_y, width);
    src_y += src_stride_y;
    dst_y += dst_stride_y;
  }
}

// Mirror I420 with optional flipping
LIBYUV_API
int I420Mirror(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height) {
  if (!src_y || !src_u || !src_v || !dst_y || !dst_u || !dst_v ||
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
  if (dst_y) {
    MirrorPlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height);
  }
  MirrorPlane(src_u, src_stride_u, dst_u, dst_stride_u, halfwidth, halfheight);
  MirrorPlane(src_v, src_stride_v, dst_v, dst_stride_v, halfwidth, halfheight);
  return 0;
}

// ARGB mirror.
LIBYUV_API
int ARGBMirror(const uint8* src_argb, int src_stride_argb,
               uint8* dst_argb, int dst_stride_argb,
               int width, int height) {
  if (!src_argb || !dst_argb || width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }

  void (*ARGBMirrorRow)(const uint8* src, uint8* dst, int width) =
      ARGBMirrorRow_C;
#if defined(HAS_ARGBMIRRORROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) && IS_ALIGNED(width, 4) &&
      IS_ALIGNED(src_argb, 16) && IS_ALIGNED(src_stride_argb, 16) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    ARGBMirrorRow = ARGBMirrorRow_SSSE3;
  }
#endif

  // Mirror plane
  for (int y = 0; y < height; ++y) {
    ARGBMirrorRow(src_argb, dst_argb, width);
    src_argb += src_stride_argb;
    dst_argb += dst_stride_argb;
  }
  return 0;
}

// Get a blender that optimized for the CPU, alignment and pixel count.
// As there are 6 blenders to choose from, the caller should try to use
// the same blend function for all pixels if possible.
LIBYUV_API
ARGBBlendRow GetARGBBlend() {
  void (*ARGBBlendRow)(const uint8* src_argb, const uint8* src_argb1,
                       uint8* dst_argb, int width) = ARGBBlendRow_C;
#if defined(HAS_ARGBBLENDROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    ARGBBlendRow = ARGBBlendRow_SSSE3;
    return ARGBBlendRow;
  }
#endif
#if defined(HAS_ARGBBLENDROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    ARGBBlendRow = ARGBBlendRow_SSE2;
  }
#endif
  return ARGBBlendRow;
}

// Alpha Blend 2 ARGB images and store to destination.
LIBYUV_API
int ARGBBlend(const uint8* src_argb0, int src_stride_argb0,
              const uint8* src_argb1, int src_stride_argb1,
              uint8* dst_argb, int dst_stride_argb,
              int width, int height) {
  if (!src_argb0 || !src_argb1 || !dst_argb || width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_argb = dst_argb + (height - 1) * dst_stride_argb;
    dst_stride_argb = -dst_stride_argb;
  }
  void (*ARGBBlendRow)(const uint8* src_argb, const uint8* src_argb1,
                       uint8* dst_argb, int width) = GetARGBBlend();

  for (int y = 0; y < height; ++y) {
    ARGBBlendRow(src_argb0, src_argb1, dst_argb, width);
    src_argb0 += src_stride_argb0;
    src_argb1 += src_stride_argb1;
    dst_argb += dst_stride_argb;
  }
  return 0;
}

// Convert ARGB to I400.
LIBYUV_API
int ARGBToI400(const uint8* src_argb, int src_stride_argb,
               uint8* dst_y, int dst_stride_y,
               int width, int height) {
  if (!src_argb || !dst_y || width <= 0 || height == 0) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
  void (*ARGBToYRow)(const uint8* src_argb, uint8* dst_y, int pix) =
      ARGBToYRow_C;
#if defined(HAS_ARGBTOYROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(width, 4) &&
      IS_ALIGNED(src_argb, 16) && IS_ALIGNED(src_stride_argb, 16) &&
      IS_ALIGNED(dst_y, 16) && IS_ALIGNED(dst_stride_y, 16)) {
    ARGBToYRow = ARGBToYRow_SSSE3;
  }
#endif

  for (int y = 0; y < height; ++y) {
    ARGBToYRow(src_argb, dst_y, width);
    src_argb += src_stride_argb;
    dst_y += dst_stride_y;
  }
  return 0;
}

// ARGB little endian (bgra in memory) to I422
// same as I420 except UV plane is full height
LIBYUV_API
int ARGBToI422(const uint8* src_argb, int src_stride_argb,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height) {
  if (!src_argb || !dst_y || !dst_u || !dst_v || width <= 0 || height == 0) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
  void (*ARGBToYRow)(const uint8* src_argb, uint8* dst_y, int pix) =
      ARGBToYRow_C;
  void (*ARGBToUVRow)(const uint8* src_argb0, int src_stride_argb,
                      uint8* dst_u, uint8* dst_v, int width) = ARGBToUVRow_C;
#if defined(HAS_ARGBTOYROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    if (width > 16) {
      ARGBToUVRow = ARGBToUVRow_Any_SSSE3;
      ARGBToYRow = ARGBToYRow_Any_SSSE3;
    }
    if (IS_ALIGNED(width, 16)) {
      ARGBToUVRow = ARGBToUVRow_Unaligned_SSSE3;
      ARGBToYRow = ARGBToYRow_Unaligned_SSSE3;
      if (IS_ALIGNED(src_argb, 16) && IS_ALIGNED(src_stride_argb, 16)) {
        ARGBToUVRow = ARGBToUVRow_SSSE3;
        if (IS_ALIGNED(dst_y, 16) && IS_ALIGNED(dst_stride_y, 16)) {
          ARGBToYRow = ARGBToYRow_SSSE3;
        }
      }
    }
  }
#endif

  for (int y = 0; y < height; ++y) {
    ARGBToUVRow(src_argb, 0, dst_u, dst_v, width);
    ARGBToYRow(src_argb, dst_y, width);
    src_argb += src_stride_argb;
    dst_y += dst_stride_y;
    dst_u += dst_stride_u;
    dst_v += dst_stride_v;
  }
  return 0;
}

// Convert I422 to BGRA.
LIBYUV_API
int I422ToBGRA(const uint8* src_y, int src_stride_y,
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
    src_u += src_stride_u;
    src_v += src_stride_v;
  }
  return 0;
}

// Convert I422 to ABGR.
LIBYUV_API
int I422ToABGR(const uint8* src_y, int src_stride_y,
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
    src_u += src_stride_u;
    src_v += src_stride_v;
  }
  return 0;
}

// Convert I422 to RGBA.
LIBYUV_API
int I422ToRGBA(const uint8* src_y, int src_stride_y,
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
    src_u += src_stride_u;
    src_v += src_stride_v;
  }
  return 0;
}

// Convert ARGB to RGBA.
LIBYUV_API
int ARGBToRGBA(const uint8* src_argb, int src_stride_argb,
               uint8* dst_rgba, int dst_stride_rgba,
               int width, int height) {
  if (!src_argb || !dst_rgba ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
  void (*ARGBToRGBARow)(const uint8* src_argb, uint8* dst_rgba, int pix) =
      ARGBToRGBARow_C;
#if defined(HAS_ARGBTORGBAROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(width, 4) &&
      IS_ALIGNED(src_argb, 16) && IS_ALIGNED(src_stride_argb, 16) &&
      IS_ALIGNED(dst_rgba, 16) && IS_ALIGNED(dst_stride_rgba, 16)) {
    ARGBToRGBARow = ARGBToRGBARow_SSSE3;
  }
#endif
#if defined(HAS_ARGBTORGBAROW_NEON)
  if (TestCpuFlag(kCpuHasNEON) && IS_ALIGNED(width, 8)) {
    ARGBToRGBARow = ARGBToRGBARow_NEON;
  }
#endif

  for (int y = 0; y < height; ++y) {
    ARGBToRGBARow(src_argb, dst_rgba, width);
    src_argb += src_stride_argb;
    dst_rgba += dst_stride_rgba;
  }
  return 0;
}

// Convert ARGB To RGB24.
LIBYUV_API
int ARGBToRGB24(const uint8* src_argb, int src_stride_argb,
                uint8* dst_rgb24, int dst_stride_rgb24,
                int width, int height) {
  if (!src_argb || !dst_rgb24 || width <= 0 || height == 0) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
  void (*ARGBToRGB24Row)(const uint8* src_argb, uint8* dst_rgb, int pix) =
      ARGBToRGB24Row_C;
#if defined(HAS_ARGBTORGB24ROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(src_argb, 16) && IS_ALIGNED(src_stride_argb, 16)) {
    if (width * 3 <= kMaxStride) {
      ARGBToRGB24Row = ARGBToRGB24Row_Any_SSSE3;
    }
    if (IS_ALIGNED(width, 16) &&
        IS_ALIGNED(dst_rgb24, 16) && IS_ALIGNED(dst_stride_rgb24, 16)) {
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
    ARGBToRGB24Row(src_argb, dst_rgb24, width);
    src_argb += src_stride_argb;
    dst_rgb24 += dst_stride_rgb24;
  }
  return 0;
}

// Convert ARGB To RAW.
LIBYUV_API
int ARGBToRAW(const uint8* src_argb, int src_stride_argb,
              uint8* dst_raw, int dst_stride_raw,
              int width, int height) {
  if (!src_argb || !dst_raw || width <= 0 || height == 0) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
  void (*ARGBToRAWRow)(const uint8* src_argb, uint8* dst_rgb, int pix) =
      ARGBToRAWRow_C;
#if defined(HAS_ARGBTORAWROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(src_argb, 16) && IS_ALIGNED(src_stride_argb, 16)) {
    if (width * 3 <= kMaxStride) {
      ARGBToRAWRow = ARGBToRAWRow_Any_SSSE3;
    }
    if (IS_ALIGNED(width, 16) &&
        IS_ALIGNED(dst_raw, 16) && IS_ALIGNED(dst_stride_raw, 16)) {
      ARGBToRAWRow = ARGBToRAWRow_SSSE3;
    }
  }
#endif
#if defined(HAS_ARGBTORAWROW_NEON)
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
    ARGBToRAWRow(src_argb, dst_raw, width);
    src_argb += src_stride_argb;
    dst_raw += dst_stride_raw;
  }
  return 0;
}

// Convert ARGB To RGB565.
LIBYUV_API
int ARGBToRGB565(const uint8* src_argb, int src_stride_argb,
                 uint8* dst_rgb565, int dst_stride_rgb565,
                 int width, int height) {
  if (!src_argb || !dst_rgb565 || width <= 0 || height == 0) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
  void (*ARGBToRGB565Row)(const uint8* src_argb, uint8* dst_rgb, int pix) =
      ARGBToRGB565Row_C;
#if defined(HAS_ARGBTORGB565ROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(src_argb, 16) && IS_ALIGNED(src_stride_argb, 16)) {
    if (width * 2 <= kMaxStride) {
      ARGBToRGB565Row = ARGBToRGB565Row_Any_SSE2;
    }
    if (IS_ALIGNED(width, 4)) {
      ARGBToRGB565Row = ARGBToRGB565Row_SSE2;
    }
  }
#endif

  for (int y = 0; y < height; ++y) {
    ARGBToRGB565Row(src_argb, dst_rgb565, width);
    src_argb += src_stride_argb;
    dst_rgb565 += dst_stride_rgb565;
  }
  return 0;
}

// Convert ARGB To ARGB1555.
LIBYUV_API
int ARGBToARGB1555(const uint8* src_argb, int src_stride_argb,
                   uint8* dst_argb1555, int dst_stride_argb1555,
                   int width, int height) {
  if (!src_argb || !dst_argb1555 || width <= 0 || height == 0) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
  void (*ARGBToARGB1555Row)(const uint8* src_argb, uint8* dst_rgb, int pix) =
      ARGBToARGB1555Row_C;
#if defined(HAS_ARGBTOARGB1555ROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(src_argb, 16) && IS_ALIGNED(src_stride_argb, 16)) {
    if (width * 2 <= kMaxStride) {
      ARGBToARGB1555Row = ARGBToARGB1555Row_Any_SSE2;
    }
    if (IS_ALIGNED(width, 4)) {
      ARGBToARGB1555Row = ARGBToARGB1555Row_SSE2;
    }
  }
#endif

  for (int y = 0; y < height; ++y) {
    ARGBToARGB1555Row(src_argb, dst_argb1555, width);
    src_argb += src_stride_argb;
    dst_argb1555 += dst_stride_argb1555;
  }
  return 0;
}

// Convert ARGB To ARGB4444.
LIBYUV_API
int ARGBToARGB4444(const uint8* src_argb, int src_stride_argb,
                   uint8* dst_argb4444, int dst_stride_argb4444,
                   int width, int height) {
  if (!src_argb || !dst_argb4444 || width <= 0 || height == 0) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
  void (*ARGBToARGB4444Row)(const uint8* src_argb, uint8* dst_rgb, int pix) =
      ARGBToARGB4444Row_C;
#if defined(HAS_ARGBTOARGB4444ROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(src_argb, 16) && IS_ALIGNED(src_stride_argb, 16)) {
    if (width * 2 <= kMaxStride) {
      ARGBToARGB4444Row = ARGBToARGB4444Row_Any_SSE2;
    }
    if (IS_ALIGNED(width, 4)) {
      ARGBToARGB4444Row = ARGBToARGB4444Row_SSE2;
    }
  }
#endif

  for (int y = 0; y < height; ++y) {
    ARGBToARGB4444Row(src_argb, dst_argb4444, width);
    src_argb += src_stride_argb;
    dst_argb4444 += dst_stride_argb4444;
  }
  return 0;
}

// Convert NV12 to RGB565.
// TODO(fbarchard): (Re) Optimize for Neon.
LIBYUV_API
int NV12ToRGB565(const uint8* src_y, int src_stride_y,
                 const uint8* src_uv, int src_stride_uv,
                 uint8* dst_rgb565, int dst_stride_rgb565,
                 int width, int height) {
  if (!src_y || !src_uv || !dst_rgb565 || width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_rgb565 = dst_rgb565 + (height - 1) * dst_stride_rgb565;
    dst_stride_rgb565 = -dst_stride_rgb565;
  }
  void (*NV12ToARGBRow)(const uint8* y_buf,
                        const uint8* uv_buf,
                        uint8* rgb_buf,
                        int width) = NV12ToARGBRow_C;
#if defined(HAS_NV12TOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) && width * 4 <= kMaxStride) {
    NV12ToARGBRow = NV12ToARGBRow_SSSE3;
  }
#endif

  SIMD_ALIGNED(uint8 row[kMaxStride]);
  void (*ARGBToRGB565Row)(const uint8* src_argb, uint8* dst_rgb, int pix) =
      ARGBToRGB565Row_C;
#if defined(HAS_ARGBTORGB565ROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) && IS_ALIGNED(width, 4)) {
    ARGBToRGB565Row = ARGBToRGB565Row_SSE2;
  }
#endif

  for (int y = 0; y < height; ++y) {
    NV12ToARGBRow(src_y, src_uv, row, width);
    ARGBToRGB565Row(row, dst_rgb565, width);
    dst_rgb565 += dst_stride_rgb565;
    src_y += src_stride_y;
    if (y & 1) {
      src_uv += src_stride_uv;
    }
  }
  return 0;
}

// Convert NV21 to RGB565.
LIBYUV_API
int NV21ToRGB565(const uint8* src_y, int src_stride_y,
                 const uint8* src_vu, int src_stride_vu,
                 uint8* dst_rgb565, int dst_stride_rgb565,
                 int width, int height) {
  if (!src_y || !src_vu || !dst_rgb565 || width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_rgb565 = dst_rgb565 + (height - 1) * dst_stride_rgb565;
    dst_stride_rgb565 = -dst_stride_rgb565;
  }
  void (*NV21ToARGBRow)(const uint8* y_buf,
                        const uint8* uv_buf,
                        uint8* rgb_buf,
                        int width) = NV21ToARGBRow_C;
#if defined(HAS_NV21TOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) && width * 4 <= kMaxStride) {
    NV21ToARGBRow = NV21ToARGBRow_SSSE3;
  }
#endif

  SIMD_ALIGNED(uint8 row[kMaxStride]);
  void (*ARGBToRGB565Row)(const uint8* src_argb, uint8* dst_rgb, int pix) =
      ARGBToRGB565Row_C;
#if defined(HAS_ARGBTORGB565ROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) && IS_ALIGNED(width, 4)) {
    ARGBToRGB565Row = ARGBToRGB565Row_SSE2;
  }
#endif

  for (int y = 0; y < height; ++y) {
    NV21ToARGBRow(src_y, src_vu, row, width);
    ARGBToRGB565Row(row, dst_rgb565, width);
    dst_rgb565 += dst_stride_rgb565;
    src_y += src_stride_y;
    if (y & 1) {
      src_vu += src_stride_vu;
    }
  }
  return 0;
}

// SetRow8 writes 'count' bytes using a 32 bit value repeated
// SetRow32 writes 'count' words using a 32 bit value repeated

#if !defined(YUV_DISABLE_ASM) && defined(__ARM_NEON__)
#define HAS_SETROW_NEON
static void SetRow8_NEON(uint8* dst, uint32 v32, int count) {
  asm volatile (  // NOLINT
    "vdup.u32  q0, %2                          \n"  // duplicate 4 ints
    "1:                                        \n"
    "subs      %1, %1, #16                     \n"  // 16 bytes per loop
    "vst1.u32  {q0}, [%0]!                     \n"  // store
    "bgt       1b                              \n"
    : "+r"(dst),   // %0
      "+r"(count)  // %1
    : "r"(v32)     // %2
    : "q0", "memory", "cc");
}

// TODO(fbarchard): Make fully assembler
static void SetRows32_NEON(uint8* dst, uint32 v32, int width,
                           int dst_stride, int height) {
  for (int y = 0; y < height; ++y) {
    SetRow8_NEON(dst, v32, width << 2);
    dst += dst_stride;
  }
}

#elif !defined(YUV_DISABLE_ASM) && defined(_M_IX86)
#define HAS_SETROW_X86
__declspec(naked) __declspec(align(16))
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

__declspec(naked) __declspec(align(16))
static void SetRows32_X86(uint8* dst, uint32 v32, int width,
                         int dst_stride, int height) {
  __asm {
    push       esi
    push       edi
    push       ebp
    mov        edi, [esp + 12 + 4]   // dst
    mov        eax, [esp + 12 + 8]   // v32
    mov        ebp, [esp + 12 + 12]  // width
    mov        edx, [esp + 12 + 16]  // dst_stride
    mov        esi, [esp + 12 + 20]  // height
    lea        ecx, [ebp * 4]
    sub        edx, ecx             // stride - width * 4

    align      16
  convertloop:
    mov        ecx, ebp
    rep stosd
    add        edi, edx
    sub        esi, 1
    jg         convertloop

    pop        ebp
    pop        edi
    pop        esi
    ret
  }
}

#elif !defined(YUV_DISABLE_ASM) && (defined(__x86_64__) || defined(__i386__))
#define HAS_SETROW_X86
static void SetRow8_X86(uint8* dst, uint32 v32, int width) {
  size_t width_tmp = static_cast<size_t>(width);
  asm volatile (  // NOLINT
    "shr       $0x2,%1                         \n"
    "rep stosl                                 \n"
    : "+D"(dst),       // %0
      "+c"(width_tmp)  // %1
    : "a"(v32)         // %2
    : "memory", "cc");
}

static void SetRows32_X86(uint8* dst, uint32 v32, int width,
                         int dst_stride, int height) {
  for (int y = 0; y < height; ++y) {
    size_t width_tmp = static_cast<size_t>(width);
    uint32* d = reinterpret_cast<uint32*>(dst);
    asm volatile (  // NOLINT
      "rep stosl                               \n"
      : "+D"(d),         // %0
        "+c"(width_tmp)  // %1
      : "a"(v32)         // %2
      : "memory", "cc");
    dst += dst_stride;
  }
}
#endif

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

LIBYUV_API
void SetPlane(uint8* dst_y, int dst_stride_y,
              int width, int height,
              uint32 value) {
  void (*SetRow)(uint8* dst, uint32 value, int pix) = SetRow8_C;
#if defined(HAS_SETROW_NEON)
  if (TestCpuFlag(kCpuHasNEON) &&
      IS_ALIGNED(width, 16) &&
      IS_ALIGNED(dst_y, 16) && IS_ALIGNED(dst_stride_y, 16)) {
    SetRow = SetRow8_NEON;
  }
#endif
#if defined(HAS_SETROW_X86)
  if (TestCpuFlag(kCpuHasX86) && IS_ALIGNED(width, 4)) {
    SetRow = SetRow8_X86;
  }
#endif
#if defined(HAS_SETROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(width, 16) &&
      IS_ALIGNED(dst_y, 16) && IS_ALIGNED(dst_stride_y, 16)) {
    SetRow = SetRow8_SSE2;
  }
#endif

  uint32 v32 = value | (value << 8) | (value << 16) | (value << 24);
  // Set plane
  for (int y = 0; y < height; ++y) {
    SetRow(dst_y, v32, width);
    dst_y += dst_stride_y;
  }
}

// Draw a rectangle into I420
LIBYUV_API
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
LIBYUV_API
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
#if defined(HAS_SETROW_NEON)
  if (TestCpuFlag(kCpuHasNEON) && IS_ALIGNED(width, 16) &&
      IS_ALIGNED(dst, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    SetRows32_NEON(dst, value, width, dst_stride_argb, height);
    return 0;
  }
#endif
#if defined(HAS_SETROW_X86)
  if (TestCpuFlag(kCpuHasX86)) {
    SetRows32_X86(dst, value, width, dst_stride_argb, height);
    return 0;
  }
#endif
  SetRows32_C(dst, value, width, dst_stride_argb, height);
  return 0;
}

// Convert unattentuated ARGB to preattenuated ARGB.
// An unattenutated ARGB alpha blend uses the formula
// p = a * f + (1 - a) * b
// where
//   p is output pixel
//   f is foreground pixel
//   b is background pixel
//   a is alpha value from foreground pixel
// An preattenutated ARGB alpha blend uses the formula
// p = f + (1 - a) * b
// where
//   f is foreground pixel premultiplied by alpha

LIBYUV_API
int ARGBAttenuate(const uint8* src_argb, int src_stride_argb,
                  uint8* dst_argb, int dst_stride_argb,
                  int width, int height) {
  if (!src_argb || !dst_argb || width <= 0 || height == 0) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
  void (*ARGBAttenuateRow)(const uint8* src_argb, uint8* dst_argb,
                           int width) = ARGBAttenuateRow_C;
#if defined(HAS_ARGBATTENUATE_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) && IS_ALIGNED(width, 4) &&
      IS_ALIGNED(src_argb, 16) && IS_ALIGNED(src_stride_argb, 16) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    ARGBAttenuateRow = ARGBAttenuateRow_SSE2;
  }
#endif
#if defined(HAS_ARGBATTENUATEROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) && IS_ALIGNED(width, 4) &&
      IS_ALIGNED(src_argb, 16) && IS_ALIGNED(src_stride_argb, 16) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    ARGBAttenuateRow = ARGBAttenuateRow_SSSE3;
  }
#endif

  for (int y = 0; y < height; ++y) {
    ARGBAttenuateRow(src_argb, dst_argb, width);
    src_argb += src_stride_argb;
    dst_argb += dst_stride_argb;
  }
  return 0;
}

// Convert preattentuated ARGB to unattenuated ARGB.
LIBYUV_API
int ARGBUnattenuate(const uint8* src_argb, int src_stride_argb,
                    uint8* dst_argb, int dst_stride_argb,
                    int width, int height) {
  if (!src_argb || !dst_argb || width <= 0 || height == 0) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
  void (*ARGBUnattenuateRow)(const uint8* src_argb, uint8* dst_argb,
                             int width) = ARGBUnattenuateRow_C;
#if defined(HAS_ARGBUNATTENUATEROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) && IS_ALIGNED(width, 4) &&
      IS_ALIGNED(src_argb, 16) && IS_ALIGNED(src_stride_argb, 16) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    ARGBUnattenuateRow = ARGBUnattenuateRow_SSE2;
  }
#endif

  for (int y = 0; y < height; ++y) {
    ARGBUnattenuateRow(src_argb, dst_argb, width);
    src_argb += src_stride_argb;
    dst_argb += dst_stride_argb;
  }
  return 0;
}

// Convert ARGB to Grayed ARGB.
LIBYUV_API
int ARGBGrayTo(const uint8* src_argb, int src_stride_argb,
               uint8* dst_argb, int dst_stride_argb,
               int width, int height) {
  if (!src_argb || !dst_argb || width <= 0 || height == 0) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
  void (*ARGBGrayRow)(const uint8* src_argb, uint8* dst_argb,
                      int width) = ARGBGrayRow_C;
#if defined(HAS_ARGBGRAYROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) && IS_ALIGNED(width, 8) &&
      IS_ALIGNED(src_argb, 16) && IS_ALIGNED(src_stride_argb, 16) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    ARGBGrayRow = ARGBGrayRow_SSSE3;
  }
#endif

  for (int y = 0; y < height; ++y) {
    ARGBGrayRow(src_argb, dst_argb, width);
    src_argb += src_stride_argb;
    dst_argb += dst_stride_argb;
  }
  return 0;
}

// Make a rectangle of ARGB gray scale.
LIBYUV_API
int ARGBGray(uint8* dst_argb, int dst_stride_argb,
             int dst_x, int dst_y,
             int width, int height) {
  if (!dst_argb || width <= 0 || height <= 0 || dst_x < 0 || dst_y < 0) {
    return -1;
  }
  void (*ARGBGrayRow)(const uint8* src_argb, uint8* dst_argb,
                      int width) = ARGBGrayRow_C;
#if defined(HAS_ARGBGRAYROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) && IS_ALIGNED(width, 8) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    ARGBGrayRow = ARGBGrayRow_SSSE3;
  }
#endif
  uint8* dst = dst_argb + dst_y * dst_stride_argb + dst_x * 4;
  for (int y = 0; y < height; ++y) {
    ARGBGrayRow(dst, dst, width);
    dst += dst_stride_argb;
  }
  return 0;
}

// Make a rectangle of ARGB Sepia tone.
LIBYUV_API
int ARGBSepia(uint8* dst_argb, int dst_stride_argb,
              int dst_x, int dst_y, int width, int height) {
  if (!dst_argb || width <= 0 || height <= 0 || dst_x < 0 || dst_y < 0) {
    return -1;
  }
  void (*ARGBSepiaRow)(uint8* dst_argb, int width) = ARGBSepiaRow_C;
#if defined(HAS_ARGBSEPIAROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) && IS_ALIGNED(width, 8) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    ARGBSepiaRow = ARGBSepiaRow_SSSE3;
  }
#endif
  uint8* dst = dst_argb + dst_y * dst_stride_argb + dst_x * 4;
  for (int y = 0; y < height; ++y) {
    ARGBSepiaRow(dst, width);
    dst += dst_stride_argb;
  }
  return 0;
}

// Apply a 4x3 matrix rotation to each ARGB pixel.
LIBYUV_API
int ARGBColorMatrix(uint8* dst_argb, int dst_stride_argb,
                    const int8* matrix_argb,
                    int dst_x, int dst_y, int width, int height) {
  if (!dst_argb || !matrix_argb || width <= 0 || height <= 0 ||
      dst_x < 0 || dst_y < 0) {
    return -1;
  }
  void (*ARGBColorMatrixRow)(uint8* dst_argb, const int8* matrix_argb,
                             int width) = ARGBColorMatrixRow_C;
#if defined(HAS_ARGBCOLORMATRIXROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) && IS_ALIGNED(width, 8) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    ARGBColorMatrixRow = ARGBColorMatrixRow_SSSE3;
  }
#endif
  uint8* dst = dst_argb + dst_y * dst_stride_argb + dst_x * 4;
  for (int y = 0; y < height; ++y) {
    ARGBColorMatrixRow(dst, matrix_argb, width);
    dst += dst_stride_argb;
  }
  return 0;
}

// Apply a color table each ARGB pixel.
// Table contains 256 ARGB values.
LIBYUV_API
int ARGBColorTable(uint8* dst_argb, int dst_stride_argb,
                   const uint8* table_argb,
                   int dst_x, int dst_y, int width, int height) {
  if (!dst_argb || !table_argb || width <= 0 || height <= 0 ||
      dst_x < 0 || dst_y < 0) {
    return -1;
  }
  void (*ARGBColorTableRow)(uint8* dst_argb, const uint8* table_argb,
                            int width) = ARGBColorTableRow_C;
#if defined(HAS_ARGBCOLORTABLEROW_X86)
  if (TestCpuFlag(kCpuHasX86)) {
    ARGBColorTableRow = ARGBColorTableRow_X86;
  }
#endif
  uint8* dst = dst_argb + dst_y * dst_stride_argb + dst_x * 4;
  for (int y = 0; y < height; ++y) {
    ARGBColorTableRow(dst, table_argb, width);
    dst += dst_stride_argb;
  }
  return 0;
}

// ARGBQuantize is used to posterize art.
// e.g. rgb / qvalue * qvalue + qvalue / 2
// But the low levels implement efficiently with 3 parameters, and could be
// used for other high level operations.
// The divide is replaces with a multiply by reciprocal fixed point multiply.
// Caveat - although SSE2 saturates, the C function does not and should be used
// with care if doing anything but quantization.
LIBYUV_API
int ARGBQuantize(uint8* dst_argb, int dst_stride_argb,
                 int scale, int interval_size, int interval_offset,
                 int dst_x, int dst_y, int width, int height) {
  if (!dst_argb || width <= 0 || height <= 0 || dst_x < 0 || dst_y < 0 ||
      interval_size < 1 || interval_size > 255) {
    return -1;
  }
  void (*ARGBQuantizeRow)(uint8* dst_argb, int scale, int interval_size,
                          int interval_offset, int width) = ARGBQuantizeRow_C;
#if defined(HAS_ARGBQUANTIZEROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) && IS_ALIGNED(width, 4) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    ARGBQuantizeRow = ARGBQuantizeRow_SSE2;
  }
#endif
  uint8* dst = dst_argb + dst_y * dst_stride_argb + dst_x * 4;
  for (int y = 0; y < height; ++y) {
    ARGBQuantizeRow(dst, scale, interval_size, interval_offset, width);
    dst += dst_stride_argb;
  }
  return 0;
}

// Computes table of cumulative sum for image where the value is the sum
// of all values above and to the left of the entry.  Used by ARGBBlur.
LIBYUV_API
int ARGBComputeCumulativeSum(const uint8* src_argb, int src_stride_argb,
                             int32* dst_cumsum, int dst_stride32_cumsum,
                             int width, int height) {
  if (!dst_cumsum || !src_argb || width <= 0 || height <= 0) {
    return -1;
  }
  void (*ComputeCumulativeSumRow)(const uint8* row, int32* cumsum,
      const int32* previous_cumsum, int width) = ComputeCumulativeSumRow_C;
#if defined(HAS_CUMULATIVESUMTOAVERAGE_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    ComputeCumulativeSumRow = ComputeCumulativeSumRow_SSE2;
  }
#endif
  memset(dst_cumsum, 0, width * sizeof(dst_cumsum[0]) * 4);  // 4 int per pixel.
  int32* previous_cumsum = dst_cumsum;
  for (int y = 0; y < height; ++y) {
    ComputeCumulativeSumRow(src_argb, dst_cumsum, previous_cumsum, width);
    previous_cumsum = dst_cumsum;
    dst_cumsum += dst_stride32_cumsum;
    src_argb += src_stride_argb;
  }
  return 0;
}

// Blur ARGB image.
// Caller should allocate CumulativeSum table of width * height * 16 bytes
// aligned to 16 byte boundary.  height can be radius * 2 + 2 to save memory
// as the buffer is treated as circular.
LIBYUV_API
int ARGBBlur(const uint8* src_argb, int src_stride_argb,
             uint8* dst_argb, int dst_stride_argb,
             int32* dst_cumsum, int dst_stride32_cumsum,
             int width, int height, int radius) {
  if (!src_argb || !dst_argb || width <= 0 || height == 0) {
    return -1;
  }
  void (*ComputeCumulativeSumRow)(const uint8* row, int32* cumsum,
      const int32* previous_cumsum, int width) = ComputeCumulativeSumRow_C;
  void (*CumulativeSumToAverage)(const int32* topleft, const int32* botleft,
      int width, int area, uint8* dst, int count) = CumulativeSumToAverage_C;
#if defined(HAS_CUMULATIVESUMTOAVERAGE_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    ComputeCumulativeSumRow = ComputeCumulativeSumRow_SSE2;
    CumulativeSumToAverage = CumulativeSumToAverage_SSE2;
  }
#endif
  // Compute enough CumulativeSum for first row to be blurred.  After this
  // one row of CumulativeSum is updated at a time.
  ARGBComputeCumulativeSum(src_argb, src_stride_argb,
                           dst_cumsum, dst_stride32_cumsum,
                           width, radius);

  src_argb = src_argb + radius * src_stride_argb;
  int32* cumsum_bot_row = &dst_cumsum[(radius - 1) * dst_stride32_cumsum];

  const int32* max_cumsum_bot_row =
      &dst_cumsum[(radius * 2 + 2) * dst_stride32_cumsum];
  const int32* cumsum_top_row = &dst_cumsum[0];

  for (int y = 0; y < height; ++y) {
    int top_y = ((y - radius - 1) >= 0) ? (y - radius - 1) : 0;
    int bot_y = ((y + radius) < height) ? (y + radius) : (height - 1);
    int area = radius * (bot_y - top_y);

    // Increment cumsum_top_row pointer with circular buffer wrap around.
    if (top_y) {
      cumsum_top_row += dst_stride32_cumsum;
      if (cumsum_top_row >= max_cumsum_bot_row) {
        cumsum_top_row = dst_cumsum;
      }
    }
    // Increment cumsum_bot_row pointer with circular buffer wrap around and
    // then fill in a row of CumulativeSum.
    if ((y + radius) < height) {
      const int32* prev_cumsum_bot_row = cumsum_bot_row;
      cumsum_bot_row += dst_stride32_cumsum;
      if (cumsum_bot_row >= max_cumsum_bot_row) {
        cumsum_bot_row = dst_cumsum;
      }
      ComputeCumulativeSumRow(src_argb, cumsum_bot_row, prev_cumsum_bot_row,
                              width);
      src_argb += src_stride_argb;
    }

    // Left clipped.
    int boxwidth = radius * 4;
    int x;
    for (x = 0; x < radius + 1; ++x) {
      CumulativeSumToAverage(cumsum_top_row, cumsum_bot_row,
                              boxwidth, area, &dst_argb[x * 4], 1);
      area += (bot_y - top_y);
      boxwidth += 4;
    }

    // Middle unclipped.
    int n = (width - 1) - radius - x + 1;
    CumulativeSumToAverage(cumsum_top_row, cumsum_bot_row,
                           boxwidth, area, &dst_argb[x * 4], n);

    // Right clipped.
    for (x += n; x <= width - 1; ++x) {
      area -= (bot_y - top_y);
      boxwidth -= 4;
      CumulativeSumToAverage(cumsum_top_row + (x - radius - 1) * 4,
                             cumsum_bot_row + (x - radius - 1) * 4,
                             boxwidth, area, &dst_argb[x * 4], 1);
    }
    dst_argb += dst_stride_argb;
  }
  return 0;
}

// Multiply ARGB image by a specified ARGB value.
LIBYUV_API
int ARGBShade(const uint8* src_argb, int src_stride_argb,
              uint8* dst_argb, int dst_stride_argb,
              int width, int height, uint32 value) {
  if (!src_argb || !dst_argb || width <= 0 || height == 0 || value == 0u) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
  void (*ARGBShadeRow)(const uint8* src_argb, uint8* dst_argb,
                       int width, uint32 value) = ARGBShadeRow_C;
#if defined(HAS_ARGBSHADE_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) && IS_ALIGNED(width, 4) &&
      IS_ALIGNED(src_argb, 16) && IS_ALIGNED(src_stride_argb, 16) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    ARGBShadeRow = ARGBShadeRow_SSE2;
  }
#endif

  for (int y = 0; y < height; ++y) {
    ARGBShadeRow(src_argb, dst_argb, width, value);
    src_argb += src_stride_argb;
    dst_argb += dst_stride_argb;
  }
  return 0;
}

// Interpolate 2 ARGB images by specified amount (0 to 255).
LIBYUV_API
int ARGBInterpolate(const uint8* src_argb0, int src_stride_argb0,
                    const uint8* src_argb1, int src_stride_argb1,
                    uint8* dst_argb, int dst_stride_argb,
                    int width, int height, int interpolation) {
  if (!src_argb0 || !src_argb1 || !dst_argb || width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_argb = dst_argb + (height - 1) * dst_stride_argb;
    dst_stride_argb = -dst_stride_argb;
  }
  void (*ARGBInterpolateRow)(uint8* dst_ptr, const uint8* src_ptr,
                              ptrdiff_t src_stride, int dst_width,
                              int source_y_fraction) = ARGBInterpolateRow_C;
#if defined(HAS_ARGBINTERPOLATEROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(src_argb0, 16) && IS_ALIGNED(src_stride_argb0, 16) &&
      IS_ALIGNED(src_argb1, 16) && IS_ALIGNED(src_stride_argb1, 16) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    ARGBInterpolateRow = ARGBInterpolateRow_SSSE3;
  }
#endif
  for (int y = 0; y < height; ++y) {
    ARGBInterpolateRow(dst_argb, src_argb0, src_argb1 - src_argb0,
                       width, interpolation);
    src_argb0 += src_stride_argb0;
    src_argb1 += src_stride_argb1;
    dst_argb += dst_stride_argb;
  }
  return 0;
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
