/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/convert_argb.h"

#include <string.h>  // for memset()

#include "libyuv/cpu_id.h"
#include "libyuv/format_conversion.h"
#ifdef HAVE_JPEG
#include "libyuv/mjpeg_decoder.h"
#endif
#include "libyuv/rotate_argb.h"
#include "libyuv/video_common.h"
#include "libyuv/row.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Copy ARGB with optional flipping
LIBYUV_API
int ARGBCopy(const uint8* src_argb, int src_stride_argb,
             uint8* dst_argb, int dst_stride_argb,
             int width, int height) {
  if (!src_argb || !dst_argb ||
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

// Convert I444 to ARGB.
LIBYUV_API
int I444ToARGB(const uint8* src_y, int src_stride_y,
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
  void (*I444ToARGBRow)(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width) = I444ToARGBRow_C;
#if defined(HAS_I444TOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) && width >= 8) {
    I444ToARGBRow = I444ToARGBRow_Any_SSSE3;
    if (IS_ALIGNED(width, 8)) {
      I444ToARGBRow = I444ToARGBRow_Unaligned_SSSE3;
      if (IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
        I444ToARGBRow = I444ToARGBRow_SSSE3;
      }
    }
  }
#endif

  for (int y = 0; y < height; ++y) {
    I444ToARGBRow(src_y, src_u, src_v, dst_argb, width);
    dst_argb += dst_stride_argb;
    src_y += src_stride_y;
    src_u += src_stride_u;
    src_v += src_stride_v;
  }
  return 0;
}

// Convert I422 to ARGB.
LIBYUV_API
int I422ToARGB(const uint8* src_y, int src_stride_y,
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
    src_u += src_stride_u;
    src_v += src_stride_v;
  }
  return 0;
}

// Convert I411 to ARGB.
LIBYUV_API
int I411ToARGB(const uint8* src_y, int src_stride_y,
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
  void (*I411ToARGBRow)(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width) = I411ToARGBRow_C;
#if defined(HAS_I411TOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) && width >= 8) {
    I411ToARGBRow = I411ToARGBRow_Any_SSSE3;
    if (IS_ALIGNED(width, 8)) {
      I411ToARGBRow = I411ToARGBRow_Unaligned_SSSE3;
      if (IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
        I411ToARGBRow = I411ToARGBRow_SSSE3;
      }
    }
  }
#endif

  for (int y = 0; y < height; ++y) {
    I411ToARGBRow(src_y, src_u, src_v, dst_argb, width);
    dst_argb += dst_stride_argb;
    src_y += src_stride_y;
    src_u += src_stride_u;
    src_v += src_stride_v;
  }
  return 0;
}


// Convert I400 to ARGB.
LIBYUV_API
int I400ToARGB_Reference(const uint8* src_y, int src_stride_y,
                         uint8* dst_argb, int dst_stride_argb,
                         int width, int height) {
  if (!src_y || !dst_argb ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_argb = dst_argb + (height - 1) * dst_stride_argb;
    dst_stride_argb = -dst_stride_argb;
  }
  void (*YToARGBRow)(const uint8* y_buf,
                     uint8* rgb_buf,
                     int width) = YToARGBRow_C;
#if defined(HAS_YTOARGBROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(width, 8) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    YToARGBRow = YToARGBRow_SSE2;
  }
#endif

  for (int y = 0; y < height; ++y) {
    YToARGBRow(src_y, dst_argb, width);
    dst_argb += dst_stride_argb;
    src_y += src_stride_y;
  }
  return 0;
}

// Convert I400 to ARGB.
LIBYUV_API
int I400ToARGB(const uint8* src_y, int src_stride_y,
               uint8* dst_argb, int dst_stride_argb,
               int width, int height) {
  if (!src_y || !dst_argb ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_y = src_y + (height - 1) * src_stride_y;
    src_stride_y = -src_stride_y;
  }
  void (*I400ToARGBRow)(const uint8* src_y, uint8* dst_argb, int pix) =
      I400ToARGBRow_C;
#if defined(HAS_I400TOARGBROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(width, 8) &&
      IS_ALIGNED(src_y, 8) && IS_ALIGNED(src_stride_y, 8) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    I400ToARGBRow = I400ToARGBRow_SSE2;
  }
#endif

  for (int y = 0; y < height; ++y) {
    I400ToARGBRow(src_y, dst_argb, width);
    src_y += src_stride_y;
    dst_argb += dst_stride_argb;
  }
  return 0;
}

// Convert BGRA to ARGB.
LIBYUV_API
int BGRAToARGB(const uint8* src_bgra, int src_stride_bgra,
               uint8* dst_argb, int dst_stride_argb,
               int width, int height) {
  if (!src_bgra || !dst_argb ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_bgra = src_bgra + (height - 1) * src_stride_bgra;
    src_stride_bgra = -src_stride_bgra;
  }
  void (*BGRAToARGBRow)(const uint8* src_bgra, uint8* dst_argb, int pix) =
      BGRAToARGBRow_C;
#if defined(HAS_BGRATOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(width, 4) &&
      IS_ALIGNED(src_bgra, 16) && IS_ALIGNED(src_stride_bgra, 16) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    BGRAToARGBRow = BGRAToARGBRow_SSSE3;
  }
#endif

  for (int y = 0; y < height; ++y) {
    BGRAToARGBRow(src_bgra, dst_argb, width);
    src_bgra += src_stride_bgra;
    dst_argb += dst_stride_argb;
  }
  return 0;
}

// Convert ABGR to ARGB.
LIBYUV_API
int ABGRToARGB(const uint8* src_abgr, int src_stride_abgr,
               uint8* dst_argb, int dst_stride_argb,
               int width, int height) {
  if (!src_abgr || !dst_argb ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_abgr = src_abgr + (height - 1) * src_stride_abgr;
    src_stride_abgr = -src_stride_abgr;
  }
  void (*ABGRToARGBRow)(const uint8* src_abgr, uint8* dst_argb, int pix) =
      ABGRToARGBRow_C;
#if defined(HAS_ABGRTOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(width, 4) &&
      IS_ALIGNED(src_abgr, 16) && IS_ALIGNED(src_stride_abgr, 16) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    ABGRToARGBRow = ABGRToARGBRow_SSSE3;
  }
#endif

  for (int y = 0; y < height; ++y) {
    ABGRToARGBRow(src_abgr, dst_argb, width);
    src_abgr += src_stride_abgr;
    dst_argb += dst_stride_argb;
  }
  return 0;
}

// Convert RGBA to ARGB.
LIBYUV_API
int RGBAToARGB(const uint8* src_rgba, int src_stride_rgba,
               uint8* dst_argb, int dst_stride_argb,
               int width, int height) {
  if (!src_rgba || !dst_argb ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_rgba = src_rgba + (height - 1) * src_stride_rgba;
    src_stride_rgba = -src_stride_rgba;
  }
  void (*RGBAToARGBRow)(const uint8* src_rgba, uint8* dst_argb, int pix) =
      RGBAToARGBRow_C;
#if defined(HAS_RGBATOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(width, 4) &&
      IS_ALIGNED(src_rgba, 16) && IS_ALIGNED(src_stride_rgba, 16) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    RGBAToARGBRow = RGBAToARGBRow_SSSE3;
  }
#endif

  for (int y = 0; y < height; ++y) {
    RGBAToARGBRow(src_rgba, dst_argb, width);
    src_rgba += src_stride_rgba;
    dst_argb += dst_stride_argb;
  }
  return 0;
}

// Convert RAW to ARGB.
LIBYUV_API
int RAWToARGB(const uint8* src_raw, int src_stride_raw,
              uint8* dst_argb, int dst_stride_argb,
              int width, int height) {
  if (!src_raw || !dst_argb ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_raw = src_raw + (height - 1) * src_stride_raw;
    src_stride_raw = -src_stride_raw;
  }
  void (*RAWToARGBRow)(const uint8* src_raw, uint8* dst_argb, int pix) =
      RAWToARGBRow_C;
#if defined(HAS_RAWTOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(width, 16) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    RAWToARGBRow = RAWToARGBRow_SSSE3;
  }
#endif

  for (int y = 0; y < height; ++y) {
    RAWToARGBRow(src_raw, dst_argb, width);
    src_raw += src_stride_raw;
    dst_argb += dst_stride_argb;
  }
  return 0;
}

// Convert RGB24 to ARGB.
LIBYUV_API
int RGB24ToARGB(const uint8* src_rgb24, int src_stride_rgb24,
                uint8* dst_argb, int dst_stride_argb,
                int width, int height) {
  if (!src_rgb24 || !dst_argb ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_rgb24 = src_rgb24 + (height - 1) * src_stride_rgb24;
    src_stride_rgb24 = -src_stride_rgb24;
  }
  void (*RGB24ToARGBRow)(const uint8* src_rgb24, uint8* dst_argb, int pix) =
      RGB24ToARGBRow_C;
#if defined(HAS_RGB24TOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(width, 16) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    RGB24ToARGBRow = RGB24ToARGBRow_SSSE3;
  }
#endif

  for (int y = 0; y < height; ++y) {
    RGB24ToARGBRow(src_rgb24, dst_argb, width);
    src_rgb24 += src_stride_rgb24;
    dst_argb += dst_stride_argb;
  }
  return 0;
}

// Convert RGB565 to ARGB.
LIBYUV_API
int RGB565ToARGB(const uint8* src_rgb565, int src_stride_rgb565,
                 uint8* dst_argb, int dst_stride_argb,
                 int width, int height) {
  if (!src_rgb565 || !dst_argb ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_rgb565 = src_rgb565 + (height - 1) * src_stride_rgb565;
    src_stride_rgb565 = -src_stride_rgb565;
  }
  void (*RGB565ToARGBRow)(const uint8* src_rgb565, uint8* dst_argb, int pix) =
      RGB565ToARGBRow_C;
#if defined(HAS_RGB565TOARGBROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(width, 8) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    RGB565ToARGBRow = RGB565ToARGBRow_SSE2;
  }
#endif

  for (int y = 0; y < height; ++y) {
    RGB565ToARGBRow(src_rgb565, dst_argb, width);
    src_rgb565 += src_stride_rgb565;
    dst_argb += dst_stride_argb;
  }
  return 0;
}

// Convert ARGB1555 to ARGB.
LIBYUV_API
int ARGB1555ToARGB(const uint8* src_argb1555, int src_stride_argb1555,
                   uint8* dst_argb, int dst_stride_argb,
                   int width, int height) {
  if (!src_argb1555 || !dst_argb ||
       width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_argb1555 = src_argb1555 + (height - 1) * src_stride_argb1555;
    src_stride_argb1555 = -src_stride_argb1555;
  }
  void (*ARGB1555ToARGBRow)(const uint8* src_argb1555, uint8* dst_argb,
                            int pix) = ARGB1555ToARGBRow_C;
#if defined(HAS_ARGB1555TOARGBROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(width, 8) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    ARGB1555ToARGBRow = ARGB1555ToARGBRow_SSE2;
  }
#endif

  for (int y = 0; y < height; ++y) {
    ARGB1555ToARGBRow(src_argb1555, dst_argb, width);
    src_argb1555 += src_stride_argb1555;
    dst_argb += dst_stride_argb;
  }
  return 0;
}

// Convert ARGB4444 to ARGB.
LIBYUV_API
int ARGB4444ToARGB(const uint8* src_argb4444, int src_stride_argb4444,
                   uint8* dst_argb, int dst_stride_argb,
                   int width, int height) {
  if (!src_argb4444 || !dst_argb ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_argb4444 = src_argb4444 + (height - 1) * src_stride_argb4444;
    src_stride_argb4444 = -src_stride_argb4444;
  }
  void (*ARGB4444ToARGBRow)(const uint8* src_argb4444, uint8* dst_argb,
                            int pix) = ARGB4444ToARGBRow_C;
#if defined(HAS_ARGB4444TOARGBROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(width, 8) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
    ARGB4444ToARGBRow = ARGB4444ToARGBRow_SSE2;
  }
#endif

  for (int y = 0; y < height; ++y) {
    ARGB4444ToARGBRow(src_argb4444, dst_argb, width);
    src_argb4444 += src_stride_argb4444;
    dst_argb += dst_stride_argb;
  }
  return 0;
}

// Convert NV12 to ARGB.
LIBYUV_API
int NV12ToARGB(const uint8* src_y, int src_stride_y,
               const uint8* src_uv, int src_stride_uv,
               uint8* dst_argb, int dst_stride_argb,
               int width, int height) {
  if (!src_y || !src_uv || !dst_argb ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_argb = dst_argb + (height - 1) * dst_stride_argb;
    dst_stride_argb = -dst_stride_argb;
  }
  void (*NV12ToARGBRow)(const uint8* y_buf,
                        const uint8* uv_buf,
                        uint8* rgb_buf,
                        int width) = NV12ToARGBRow_C;
#if defined(HAS_NV12TOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) && width >= 8) {
    NV12ToARGBRow = NV12ToARGBRow_Any_SSSE3;
    if (IS_ALIGNED(width, 8)) {
      NV12ToARGBRow = NV12ToARGBRow_Unaligned_SSSE3;
      if (IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
        NV12ToARGBRow = NV12ToARGBRow_SSSE3;
      }
    }
  }
#endif

  for (int y = 0; y < height; ++y) {
    NV12ToARGBRow(src_y, src_uv, dst_argb, width);
    dst_argb += dst_stride_argb;
    src_y += src_stride_y;
    if (y & 1) {
      src_uv += src_stride_uv;
    }
  }
  return 0;
}

// Convert NV21 to ARGB.
LIBYUV_API
int NV21ToARGB(const uint8* src_y, int src_stride_y,
               const uint8* src_vu, int src_stride_vu,
               uint8* dst_argb, int dst_stride_argb,
               int width, int height) {
  if (!src_y || !src_vu || !dst_argb ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_argb = dst_argb + (height - 1) * dst_stride_argb;
    dst_stride_argb = -dst_stride_argb;
  }
  void (*NV21ToARGBRow)(const uint8* y_buf,
                        const uint8* vu_buf,
                        uint8* rgb_buf,
                        int width) = NV21ToARGBRow_C;
#if defined(HAS_NV21TOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) && width >= 8) {
    NV21ToARGBRow = NV21ToARGBRow_Any_SSSE3;
    if (IS_ALIGNED(width, 8)) {
      NV21ToARGBRow = NV21ToARGBRow_Unaligned_SSSE3;
      if (IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
        NV21ToARGBRow = NV21ToARGBRow_SSSE3;
      }
    }
  }
#endif

  for (int y = 0; y < height; ++y) {
    NV21ToARGBRow(src_y, src_vu, dst_argb, width);
    dst_argb += dst_stride_argb;
    src_y += src_stride_y;
    if (y & 1) {
      src_vu += src_stride_vu;
    }
  }
  return 0;
}

// Convert M420 to ARGB.
LIBYUV_API
int M420ToARGB(const uint8* src_m420, int src_stride_m420,
               uint8* dst_argb, int dst_stride_argb,
               int width, int height) {
  if (!src_m420 || !dst_argb ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    dst_argb = dst_argb + (height - 1) * dst_stride_argb;
    dst_stride_argb = -dst_stride_argb;
  }
  void (*NV12ToARGBRow)(const uint8* y_buf,
                        const uint8* uv_buf,
                        uint8* rgb_buf,
                        int width) = NV12ToARGBRow_C;
#if defined(HAS_NV12TOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) && width >= 8) {
    NV12ToARGBRow = NV12ToARGBRow_Any_SSSE3;
    if (IS_ALIGNED(width, 8)) {
      NV12ToARGBRow = NV12ToARGBRow_Unaligned_SSSE3;
      if (IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
        NV12ToARGBRow = NV12ToARGBRow_SSSE3;
      }
    }
  }
#endif

  for (int y = 0; y < height - 1; y += 2) {
    NV12ToARGBRow(src_m420, src_m420 + src_stride_m420 * 2, dst_argb, width);
    NV12ToARGBRow(src_m420 + src_stride_m420, src_m420 + src_stride_m420 * 2,
                  dst_argb + dst_stride_argb, width);
    dst_argb += dst_stride_argb * 2;
    src_m420 += src_stride_m420 * 3;
  }
  if (height & 1) {
    NV12ToARGBRow(src_m420, src_m420 + src_stride_m420 * 2, dst_argb, width);
  }
  return 0;
}

// Convert YUY2 to ARGB.
LIBYUV_API
int YUY2ToARGB(const uint8* src_yuy2, int src_stride_yuy2,
               uint8* dst_argb, int dst_stride_argb,
               int width, int height) {
  if (!src_yuy2 || !dst_argb ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_yuy2 = src_yuy2 + (height - 1) * src_stride_yuy2;
    src_stride_yuy2 = -src_stride_yuy2;
  }
  void (*YUY2ToUV422Row)(const uint8* src_yuy2, uint8* dst_u, uint8* dst_v,
      int pix) = YUY2ToUV422Row_C;
  void (*YUY2ToYRow)(const uint8* src_yuy2,
                     uint8* dst_y, int pix) = YUY2ToYRow_C;
#if defined(HAS_YUY2TOYROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    if (width > 16) {
      YUY2ToUV422Row = YUY2ToUV422Row_Any_SSE2;
      YUY2ToYRow = YUY2ToYRow_Any_SSE2;
    }
    if (IS_ALIGNED(width, 16)) {
      YUY2ToUV422Row = YUY2ToUV422Row_Unaligned_SSE2;
      YUY2ToYRow = YUY2ToYRow_Unaligned_SSE2;
      if (IS_ALIGNED(src_yuy2, 16) && IS_ALIGNED(src_stride_yuy2, 16)) {
        YUY2ToUV422Row = YUY2ToUV422Row_SSE2;
        YUY2ToYRow = YUY2ToYRow_SSE2;
      }
    }
  }
#elif defined(HAS_YUY2TOYROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    if (width > 8) {
      YUY2ToYRow = YUY2ToYRow_Any_NEON;
      if (width > 16) {
        YUY2ToUV422Row = YUY2ToUV422Row_Any_NEON;
      }
    }
    if (IS_ALIGNED(width, 8)) {
      YUY2ToYRow = YUY2ToYRow_NEON;
      if (IS_ALIGNED(width, 16)) {
        YUY2ToUV422Row = YUY2ToUV422Row_NEON;
      }
    }
  }
#endif

  void (*I422ToARGBRow)(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* argb_buf,
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
    if (IS_ALIGNED(width, 8) &&
        IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
      I422ToARGBRow = I422ToARGBRow_SSSE3;
    }
  }
#endif

  SIMD_ALIGNED(uint8 rowy[kMaxStride]);
  SIMD_ALIGNED(uint8 rowu[kMaxStride]);
  SIMD_ALIGNED(uint8 rowv[kMaxStride]);

  for (int y = 0; y < height; ++y) {
    YUY2ToUV422Row(src_yuy2, rowu, rowv, width);
    YUY2ToYRow(src_yuy2, rowy, width);
    I422ToARGBRow(rowy, rowu, rowv, dst_argb, width);
    src_yuy2 += src_stride_yuy2;
    dst_argb += dst_stride_argb;
  }
  return 0;
}

// Convert UYVY to ARGB.
LIBYUV_API
int UYVYToARGB(const uint8* src_uyvy, int src_stride_uyvy,
               uint8* dst_argb, int dst_stride_argb,
               int width, int height) {
  if (!src_uyvy || !dst_argb ||
      width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_uyvy = src_uyvy + (height - 1) * src_stride_uyvy;
    src_stride_uyvy = -src_stride_uyvy;
  }
  void (*UYVYToUV422Row)(const uint8* src_uyvy, uint8* dst_u, uint8* dst_v,
      int pix) = UYVYToUV422Row_C;
  void (*UYVYToYRow)(const uint8* src_uyvy,
                     uint8* dst_y, int pix) = UYVYToYRow_C;
#if defined(HAS_UYVYTOYROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    if (width > 16) {
      UYVYToUV422Row = UYVYToUV422Row_Any_SSE2;
      UYVYToYRow = UYVYToYRow_Any_SSE2;
    }
    if (IS_ALIGNED(width, 16)) {
      UYVYToUV422Row = UYVYToUV422Row_Unaligned_SSE2;
      UYVYToYRow = UYVYToYRow_Unaligned_SSE2;
      if (IS_ALIGNED(src_uyvy, 16) && IS_ALIGNED(src_stride_uyvy, 16)) {
        UYVYToUV422Row = UYVYToUV422Row_SSE2;
        UYVYToYRow = UYVYToYRow_SSE2;
      }
    }
  }
#endif
  void (*I422ToARGBRow)(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* argb_buf,
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
    if (IS_ALIGNED(width, 8) &&
        IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride_argb, 16)) {
      I422ToARGBRow = I422ToARGBRow_SSSE3;
    }
  }
#endif

  SIMD_ALIGNED(uint8 rowy[kMaxStride]);
  SIMD_ALIGNED(uint8 rowu[kMaxStride]);
  SIMD_ALIGNED(uint8 rowv[kMaxStride]);

  for (int y = 0; y < height; ++y) {
    UYVYToUV422Row(src_uyvy, rowu, rowv, width);
    UYVYToYRow(src_uyvy, rowy, width);
    I422ToARGBRow(rowy, rowu, rowv, dst_argb, width);
    src_uyvy += src_stride_uyvy;
    dst_argb += dst_stride_argb;
  }
  return 0;
}

#ifdef HAVE_JPEG
struct ARGBBuffers {
  uint8* argb;
  int argb_stride;
  int w;
  int h;
};

static void JpegI420ToARGB(void* opaque,
                         const uint8* const* data,
                         const int* strides,
                         int rows) {
  ARGBBuffers* dest = static_cast<ARGBBuffers*>(opaque);
  I420ToARGB(data[0], strides[0],
             data[1], strides[1],
             data[2], strides[2],
             dest->argb, dest->argb_stride,
             dest->w, rows);
  dest->argb += rows * dest->argb_stride;
  dest->h -= rows;
}

static void JpegI422ToARGB(void* opaque,
                           const uint8* const* data,
                           const int* strides,
                           int rows) {
  ARGBBuffers* dest = static_cast<ARGBBuffers*>(opaque);
  I422ToARGB(data[0], strides[0],
             data[1], strides[1],
             data[2], strides[2],
             dest->argb, dest->argb_stride,
             dest->w, rows);
  dest->argb += rows * dest->argb_stride;
  dest->h -= rows;
}

static void JpegI444ToARGB(void* opaque,
                           const uint8* const* data,
                           const int* strides,
                           int rows) {
  ARGBBuffers* dest = static_cast<ARGBBuffers*>(opaque);
  I444ToARGB(data[0], strides[0],
             data[1], strides[1],
             data[2], strides[2],
             dest->argb, dest->argb_stride,
             dest->w, rows);
  dest->argb += rows * dest->argb_stride;
  dest->h -= rows;
}

static void JpegI411ToARGB(void* opaque,
                           const uint8* const* data,
                           const int* strides,
                           int rows) {
  ARGBBuffers* dest = static_cast<ARGBBuffers*>(opaque);
  I411ToARGB(data[0], strides[0],
             data[1], strides[1],
             data[2], strides[2],
             dest->argb, dest->argb_stride,
             dest->w, rows);
  dest->argb += rows * dest->argb_stride;
  dest->h -= rows;
}

static void JpegI400ToARGB(void* opaque,
                           const uint8* const* data,
                           const int* strides,
                           int rows) {
  ARGBBuffers* dest = static_cast<ARGBBuffers*>(opaque);
  I400ToARGB(data[0], strides[0],
             dest->argb, dest->argb_stride,
             dest->w, rows);
  dest->argb += rows * dest->argb_stride;
  dest->h -= rows;
}

// MJPG (Motion JPeg) to ARGB
// TODO(fbarchard): review w and h requirement.  dw and dh may be enough.
LIBYUV_API
int MJPGToARGB(const uint8* sample,
               size_t sample_size,
               uint8* argb, int argb_stride,
               int w, int h,
               int dw, int dh) {
  if (sample_size == kUnknownDataSize) {
    // ERROR: MJPEG frame size unknown
    return -1;
  }

  // TODO(fbarchard): Port to C
  MJpegDecoder mjpeg_decoder;
  bool ret = mjpeg_decoder.LoadFrame(sample, sample_size);
  if (ret && (mjpeg_decoder.GetWidth() != w ||
              mjpeg_decoder.GetHeight() != h)) {
    // ERROR: MJPEG frame has unexpected dimensions
    mjpeg_decoder.UnloadFrame();
    return 1;  // runtime failure
  }
  if (ret) {
    ARGBBuffers bufs = { argb, argb_stride, dw, dh };
    // YUV420
    if (mjpeg_decoder.GetColorSpace() ==
            MJpegDecoder::kColorSpaceYCbCr &&
        mjpeg_decoder.GetNumComponents() == 3 &&
        mjpeg_decoder.GetVertSampFactor(0) == 2 &&
        mjpeg_decoder.GetHorizSampFactor(0) == 2 &&
        mjpeg_decoder.GetVertSampFactor(1) == 1 &&
        mjpeg_decoder.GetHorizSampFactor(1) == 1 &&
        mjpeg_decoder.GetVertSampFactor(2) == 1 &&
        mjpeg_decoder.GetHorizSampFactor(2) == 1) {
      ret = mjpeg_decoder.DecodeToCallback(&JpegI420ToARGB, &bufs, dw, dh);
    // YUV422
    } else if (mjpeg_decoder.GetColorSpace() ==
                   MJpegDecoder::kColorSpaceYCbCr &&
               mjpeg_decoder.GetNumComponents() == 3 &&
               mjpeg_decoder.GetVertSampFactor(0) == 1 &&
               mjpeg_decoder.GetHorizSampFactor(0) == 2 &&
               mjpeg_decoder.GetVertSampFactor(1) == 1 &&
               mjpeg_decoder.GetHorizSampFactor(1) == 1 &&
               mjpeg_decoder.GetVertSampFactor(2) == 1 &&
               mjpeg_decoder.GetHorizSampFactor(2) == 1) {
      ret = mjpeg_decoder.DecodeToCallback(&JpegI422ToARGB, &bufs, dw, dh);
    // YUV444
    } else if (mjpeg_decoder.GetColorSpace() ==
                   MJpegDecoder::kColorSpaceYCbCr &&
               mjpeg_decoder.GetNumComponents() == 3 &&
               mjpeg_decoder.GetVertSampFactor(0) == 1 &&
               mjpeg_decoder.GetHorizSampFactor(0) == 1 &&
               mjpeg_decoder.GetVertSampFactor(1) == 1 &&
               mjpeg_decoder.GetHorizSampFactor(1) == 1 &&
               mjpeg_decoder.GetVertSampFactor(2) == 1 &&
               mjpeg_decoder.GetHorizSampFactor(2) == 1) {
      ret = mjpeg_decoder.DecodeToCallback(&JpegI444ToARGB, &bufs, dw, dh);
    // YUV411
    } else if (mjpeg_decoder.GetColorSpace() ==
                   MJpegDecoder::kColorSpaceYCbCr &&
               mjpeg_decoder.GetNumComponents() == 3 &&
               mjpeg_decoder.GetVertSampFactor(0) == 1 &&
               mjpeg_decoder.GetHorizSampFactor(0) == 4 &&
               mjpeg_decoder.GetVertSampFactor(1) == 1 &&
               mjpeg_decoder.GetHorizSampFactor(1) == 1 &&
               mjpeg_decoder.GetVertSampFactor(2) == 1 &&
               mjpeg_decoder.GetHorizSampFactor(2) == 1) {
      ret = mjpeg_decoder.DecodeToCallback(&JpegI411ToARGB, &bufs, dw, dh);
    // YUV400
    } else if (mjpeg_decoder.GetColorSpace() ==
                   MJpegDecoder::kColorSpaceGrayscale &&
               mjpeg_decoder.GetNumComponents() == 1 &&
               mjpeg_decoder.GetVertSampFactor(0) == 1 &&
               mjpeg_decoder.GetHorizSampFactor(0) == 1) {
      ret = mjpeg_decoder.DecodeToCallback(&JpegI400ToARGB, &bufs, dw, dh);
    } else {
      // TODO(fbarchard): Implement conversion for any other colorspace/sample
      // factors that occur in practice.  411 is supported by libjpeg
      // ERROR: Unable to convert MJPEG frame because format is not supported
      mjpeg_decoder.UnloadFrame();
      return 1;
    }
  }
  return 0;
}
#endif

// Convert camera sample to I420 with cropping, rotation and vertical flip.
// src_width is used for source stride computation
// src_height is used to compute location of planes, and indicate inversion
// sample_size is measured in bytes and is the size of the frame.
//   With MJPEG it is the compressed size of the frame.
LIBYUV_API
int ConvertToARGB(const uint8* sample, size_t sample_size,
                  uint8* dst_argb, int argb_stride,
                  int crop_x, int crop_y,
                  int src_width, int src_height,
                  int dst_width, int dst_height,
                  RotationMode rotation,
                  uint32 format) {
  if (dst_argb == NULL || sample == NULL ||
      src_width <= 0 || dst_width <= 0 ||
      src_height == 0 || dst_height == 0) {
    return -1;
  }
  int aligned_src_width = (src_width + 1) & ~1;
  const uint8* src;
  const uint8* src_uv;
  int abs_src_height = (src_height < 0) ? -src_height : src_height;
  int inv_dst_height = (dst_height < 0) ? -dst_height : dst_height;
  if (src_height < 0) {
    inv_dst_height = -inv_dst_height;
  }
  int r = 0;

  // One pass rotation is available for some formats.  For the rest, convert
  // to I420 (with optional vertical flipping) into a temporary I420 buffer,
  // and then rotate the I420 to the final destination buffer.
  // For in-place conversion, if destination dst_argb is same as source sample,
  // also enable temporary buffer.
  bool need_buf = (rotation && format != FOURCC_ARGB) || dst_argb == sample;
  uint8* tmp_argb = dst_argb;
  int tmp_argb_stride = argb_stride;
  uint8* buf = NULL;
  int abs_dst_height = (dst_height < 0) ? -dst_height : dst_height;
  if (need_buf) {
    int argb_size = dst_width * abs_dst_height * 4;
    buf = new uint8[argb_size];
    if (!buf) {
      return 1;  // Out of memory runtime error.
    }
    dst_argb = buf;
    argb_stride = dst_width;
  }

  switch (format) {
    // Single plane formats
    case FOURCC_YUY2:
      src = sample + (aligned_src_width * crop_y + crop_x) * 2;
      r = YUY2ToARGB(src, aligned_src_width * 2,
                     dst_argb, argb_stride,
                     dst_width, inv_dst_height);
      break;
    case FOURCC_UYVY:
      src = sample + (aligned_src_width * crop_y + crop_x) * 2;
      r = UYVYToARGB(src, aligned_src_width * 2,
                     dst_argb, argb_stride,
                     dst_width, inv_dst_height);
      break;
//    case FOURCC_V210:
      // stride is multiple of 48 pixels (128 bytes).
      // pixels come in groups of 6 = 16 bytes
//      src = sample + (aligned_src_width + 47) / 48 * 128 * crop_y +
//            crop_x / 6 * 16;
//      r = V210ToARGB(src, (aligned_src_width + 47) / 48 * 128,
//                     dst_argb, argb_stride,
//                     dst_width, inv_dst_height);
//      break;
    case FOURCC_24BG:
      src = sample + (src_width * crop_y + crop_x) * 3;
      r = RGB24ToARGB(src, src_width * 3,
                      dst_argb, argb_stride,
                      dst_width, inv_dst_height);
      break;
    case FOURCC_RAW:
      src = sample + (src_width * crop_y + crop_x) * 3;
      r = RAWToARGB(src, src_width * 3,
                    dst_argb, argb_stride,
                    dst_width, inv_dst_height);
      break;
    case FOURCC_ARGB:
      src = sample + (src_width * crop_y + crop_x) * 4;
      r = ARGBToARGB(src, src_width * 4,
                     dst_argb, argb_stride,
                     dst_width, inv_dst_height);
      break;
    case FOURCC_BGRA:
      src = sample + (src_width * crop_y + crop_x) * 4;
      r = BGRAToARGB(src, src_width * 4,
                     dst_argb, argb_stride,
                     dst_width, inv_dst_height);
      break;
    case FOURCC_ABGR:
      src = sample + (src_width * crop_y + crop_x) * 4;
      r = ABGRToARGB(src, src_width * 4,
                     dst_argb, argb_stride,
                     dst_width, inv_dst_height);
      break;
    case FOURCC_RGBA:
      src = sample + (src_width * crop_y + crop_x) * 4;
      r = RGBAToARGB(src, src_width * 4,
                     dst_argb, argb_stride,
                     dst_width, inv_dst_height);
      break;
    case FOURCC_RGBP:
      src = sample + (src_width * crop_y + crop_x) * 2;
      r = RGB565ToARGB(src, src_width * 2,
                       dst_argb, argb_stride,
                       dst_width, inv_dst_height);
      break;
    case FOURCC_RGBO:
      src = sample + (src_width * crop_y + crop_x) * 2;
      r = ARGB1555ToARGB(src, src_width * 2,
                         dst_argb, argb_stride,
                         dst_width, inv_dst_height);
      break;
    case FOURCC_R444:
      src = sample + (src_width * crop_y + crop_x) * 2;
      r = ARGB4444ToARGB(src, src_width * 2,
                         dst_argb, argb_stride,
                         dst_width, inv_dst_height);
      break;
    // TODO(fbarchard): Support cropping Bayer by odd numbers
    // by adjusting fourcc.
    case FOURCC_BGGR:
      src = sample + (src_width * crop_y + crop_x);
      r = BayerBGGRToARGB(src, src_width,
                          dst_argb, argb_stride,
                          dst_width, inv_dst_height);
      break;

    case FOURCC_GBRG:
      src = sample + (src_width * crop_y + crop_x);
      r = BayerGBRGToARGB(src, src_width,
                          dst_argb, argb_stride,
                          dst_width, inv_dst_height);
      break;

    case FOURCC_GRBG:
      src = sample + (src_width * crop_y + crop_x);
      r = BayerGRBGToARGB(src, src_width,
                          dst_argb, argb_stride,
                          dst_width, inv_dst_height);
      break;

    case FOURCC_RGGB:
      src = sample + (src_width * crop_y + crop_x);
      r = BayerRGGBToARGB(src, src_width,
                          dst_argb, argb_stride,
                          dst_width, inv_dst_height);
      break;

    case FOURCC_I400:
      src = sample + src_width * crop_y + crop_x;
      r = I400ToARGB(src, src_width,
                     dst_argb, argb_stride,
                     dst_width, inv_dst_height);
      break;

    // Biplanar formats
    case FOURCC_NV12:
      src = sample + (src_width * crop_y + crop_x);
      src_uv = sample + aligned_src_width * (src_height + crop_y / 2) + crop_x;
      r = NV12ToARGB(src, src_width,
                     src_uv, aligned_src_width,
                     dst_argb, argb_stride,
                     dst_width, inv_dst_height);
      break;
    case FOURCC_NV21:
      src = sample + (src_width * crop_y + crop_x);
      src_uv = sample + aligned_src_width * (src_height + crop_y / 2) + crop_x;
      // Call NV12 but with u and v parameters swapped.
      r = NV21ToARGB(src, src_width,
                     src_uv, aligned_src_width,
                     dst_argb, argb_stride,
                     dst_width, inv_dst_height);
      break;
    case FOURCC_M420:
      src = sample + (src_width * crop_y) * 12 / 8 + crop_x;
      r = M420ToARGB(src, src_width,
                     dst_argb, argb_stride,
                     dst_width, inv_dst_height);
      break;
//    case FOURCC_Q420:
//      src = sample + (src_width + aligned_src_width * 2) * crop_y + crop_x;
//      src_uv = sample + (src_width + aligned_src_width * 2) * crop_y +
//               src_width + crop_x * 2;
//      r = Q420ToARGB(src, src_width * 3,
//                    src_uv, src_width * 3,
//                    dst_argb, argb_stride,
//                    dst_width, inv_dst_height);
//      break;
    // Triplanar formats
    case FOURCC_I420:
    case FOURCC_YV12: {
      const uint8* src_y = sample + (src_width * crop_y + crop_x);
      const uint8* src_u;
      const uint8* src_v;
      int halfwidth = (src_width + 1) / 2;
      int halfheight = (abs_src_height + 1) / 2;
      if (format == FOURCC_I420) {
        src_u = sample + src_width * abs_src_height +
            (halfwidth * crop_y + crop_x) / 2;
        src_v = sample + src_width * abs_src_height +
            halfwidth * (halfheight + crop_y / 2) + crop_x / 2;
      } else {
        src_v = sample + src_width * abs_src_height +
            (halfwidth * crop_y + crop_x) / 2;
        src_u = sample + src_width * abs_src_height +
            halfwidth * (halfheight + crop_y / 2) + crop_x / 2;
      }
      r = I420ToARGB(src_y, src_width,
                     src_u, halfwidth,
                     src_v, halfwidth,
                     dst_argb, argb_stride,
                     dst_width, inv_dst_height);
      break;
    }
    case FOURCC_I422:
    case FOURCC_YV16: {
      const uint8* src_y = sample + src_width * crop_y + crop_x;
      const uint8* src_u;
      const uint8* src_v;
      int halfwidth = (src_width + 1) / 2;
      if (format == FOURCC_I422) {
        src_u = sample + src_width * abs_src_height +
            halfwidth * crop_y + crop_x / 2;
        src_v = sample + src_width * abs_src_height +
            halfwidth * (abs_src_height + crop_y) + crop_x / 2;
      } else {
        src_v = sample + src_width * abs_src_height +
            halfwidth * crop_y + crop_x / 2;
        src_u = sample + src_width * abs_src_height +
            halfwidth * (abs_src_height + crop_y) + crop_x / 2;
      }
      r = I422ToARGB(src_y, src_width,
                     src_u, halfwidth,
                     src_v, halfwidth,
                     dst_argb, argb_stride,
                     dst_width, inv_dst_height);
      break;
    }
    case FOURCC_I444:
    case FOURCC_YV24: {
      const uint8* src_y = sample + src_width * crop_y + crop_x;
      const uint8* src_u;
      const uint8* src_v;
      if (format == FOURCC_I444) {
        src_u = sample + src_width * (abs_src_height + crop_y) + crop_x;
        src_v = sample + src_width * (abs_src_height * 2 + crop_y) + crop_x;
      } else {
        src_v = sample + src_width * (abs_src_height + crop_y) + crop_x;
        src_u = sample + src_width * (abs_src_height * 2 + crop_y) + crop_x;
      }
      r = I444ToARGB(src_y, src_width,
                     src_u, src_width,
                     src_v, src_width,
                     dst_argb, argb_stride,
                     dst_width, inv_dst_height);
      break;
    }
    case FOURCC_I411: {
      int quarterwidth = (src_width + 3) / 4;
      const uint8* src_y = sample + src_width * crop_y + crop_x;
      const uint8* src_u = sample + src_width * abs_src_height +
          quarterwidth * crop_y + crop_x / 4;
      const uint8* src_v = sample + src_width * abs_src_height +
          quarterwidth * (abs_src_height + crop_y) + crop_x / 4;
      r = I411ToARGB(src_y, src_width,
                     src_u, quarterwidth,
                     src_v, quarterwidth,
                     dst_argb, argb_stride,
                     dst_width, inv_dst_height);
      break;
    }
#ifdef HAVE_JPEG
    case FOURCC_MJPG:
      r = MJPGToARGB(sample, sample_size,
                     dst_argb, argb_stride,
                     src_width, abs_src_height, dst_width, inv_dst_height);
      break;
#endif
    default:
      r = -1;  // unknown fourcc - return failure code.
  }

  if (need_buf) {
    if (!r) {
      r = ARGBRotate(dst_argb, argb_stride,
                     tmp_argb, tmp_argb_stride,
                     dst_width, abs_dst_height, rotation);
    }
    delete buf;
  }

  return r;
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
