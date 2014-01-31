/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/row.h"

#include <string.h>  // For memcpy

#include "libyuv/basic_types.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

void BGRAToARGBRow_C(const uint8* src_bgra, uint8* dst_argb, int width) {
  for (int x = 0; x < width; ++x) {
    // To support in-place conversion.
    uint8 a = src_bgra[0];
    uint8 r = src_bgra[1];
    uint8 g = src_bgra[2];
    uint8 b = src_bgra[3];
    dst_argb[0] = b;
    dst_argb[1] = g;
    dst_argb[2] = r;
    dst_argb[3] = a;
    dst_argb += 4;
    src_bgra += 4;
  }
}

void ABGRToARGBRow_C(const uint8* src_abgr, uint8* dst_argb, int width) {
  for (int x = 0; x < width; ++x) {
    // To support in-place conversion.
    uint8 r = src_abgr[0];
    uint8 g = src_abgr[1];
    uint8 b = src_abgr[2];
    uint8 a = src_abgr[3];
    dst_argb[0] = b;
    dst_argb[1] = g;
    dst_argb[2] = r;
    dst_argb[3] = a;
    dst_argb += 4;
    src_abgr += 4;
  }
}

void RGBAToARGBRow_C(const uint8* src_abgr, uint8* dst_argb, int width) {
  for (int x = 0; x < width; ++x) {
    // To support in-place conversion.
    uint8 a = src_abgr[0];
    uint8 b = src_abgr[1];
    uint8 g = src_abgr[2];
    uint8 r = src_abgr[3];
    dst_argb[0] = b;
    dst_argb[1] = g;
    dst_argb[2] = r;
    dst_argb[3] = a;
    dst_argb += 4;
    src_abgr += 4;
  }
}

void RGB24ToARGBRow_C(const uint8* src_rgb24, uint8* dst_argb, int width) {
  for (int x = 0; x < width; ++x) {
    uint8 b = src_rgb24[0];
    uint8 g = src_rgb24[1];
    uint8 r = src_rgb24[2];
    dst_argb[0] = b;
    dst_argb[1] = g;
    dst_argb[2] = r;
    dst_argb[3] = 255u;
    dst_argb += 4;
    src_rgb24 += 3;
  }
}

void RAWToARGBRow_C(const uint8* src_raw, uint8* dst_argb, int width) {
  for (int x = 0; x < width; ++x) {
    uint8 r = src_raw[0];
    uint8 g = src_raw[1];
    uint8 b = src_raw[2];
    dst_argb[0] = b;
    dst_argb[1] = g;
    dst_argb[2] = r;
    dst_argb[3] = 255u;
    dst_argb += 4;
    src_raw += 3;
  }
}

void RGB565ToARGBRow_C(const uint8* src_rgb, uint8* dst_argb, int width) {
  for (int x = 0; x < width; ++x) {
    uint8 b = src_rgb[0] & 0x1f;
    uint8 g = (src_rgb[0] >> 5) | ((src_rgb[1] & 0x07) << 3);
    uint8 r = src_rgb[1] >> 3;
    dst_argb[0] = (b << 3) | (b >> 2);
    dst_argb[1] = (g << 2) | (g >> 4);
    dst_argb[2] = (r << 3) | (r >> 2);
    dst_argb[3] = 255u;
    dst_argb += 4;
    src_rgb += 2;
  }
}

void ARGB1555ToARGBRow_C(const uint8* src_rgb, uint8* dst_argb, int width) {
  for (int x = 0; x < width; ++x) {
    uint8 b = src_rgb[0] & 0x1f;
    uint8 g = (src_rgb[0] >> 5) | ((src_rgb[1] & 0x03) << 3);
    uint8 r = (src_rgb[1] & 0x7c) >> 2;
    uint8 a = src_rgb[1] >> 7;
    dst_argb[0] = (b << 3) | (b >> 2);
    dst_argb[1] = (g << 3) | (g >> 2);
    dst_argb[2] = (r << 3) | (r >> 2);
    dst_argb[3] = -a;
    dst_argb += 4;
    src_rgb += 2;
  }
}

void ARGB4444ToARGBRow_C(const uint8* src_rgb, uint8* dst_argb, int width) {
  for (int x = 0; x < width; ++x) {
    uint8 b = src_rgb[0] & 0x0f;
    uint8 g = src_rgb[0] >> 4;
    uint8 r = src_rgb[1] & 0x0f;
    uint8 a = src_rgb[1] >> 4;
    dst_argb[0] = (b << 4) | b;
    dst_argb[1] = (g << 4) | g;
    dst_argb[2] = (r << 4) | r;
    dst_argb[3] = (a << 4) | a;
    dst_argb += 4;
    src_rgb += 2;
  }
}

void ARGBToRGBARow_C(const uint8* src_argb, uint8* dst_rgb, int width) {
  for (int x = 0; x < width; ++x) {
    uint8 b = src_argb[0];
    uint8 g = src_argb[1];
    uint8 r = src_argb[2];
    uint8 a = src_argb[3];
    dst_rgb[0] = a;
    dst_rgb[1] = b;
    dst_rgb[2] = g;
    dst_rgb[3] = r;
    dst_rgb += 4;
    src_argb += 4;
  }
}

void ARGBToRGB24Row_C(const uint8* src_argb, uint8* dst_rgb, int width) {
  for (int x = 0; x < width; ++x) {
    uint8 b = src_argb[0];
    uint8 g = src_argb[1];
    uint8 r = src_argb[2];
    dst_rgb[0] = b;
    dst_rgb[1] = g;
    dst_rgb[2] = r;
    dst_rgb += 3;
    src_argb += 4;
  }
}

void ARGBToRAWRow_C(const uint8* src_argb, uint8* dst_rgb, int width) {
  for (int x = 0; x < width; ++x) {
    uint8 b = src_argb[0];
    uint8 g = src_argb[1];
    uint8 r = src_argb[2];
    dst_rgb[0] = r;
    dst_rgb[1] = g;
    dst_rgb[2] = b;
    dst_rgb += 3;
    src_argb += 4;
  }
}

// TODO(fbarchard): support big endian CPU
void ARGBToRGB565Row_C(const uint8* src_argb, uint8* dst_rgb, int width) {
  for (int x = 0; x < width - 1; x += 2) {
    uint8 b0 = src_argb[0] >> 3;
    uint8 g0 = src_argb[1] >> 2;
    uint8 r0 = src_argb[2] >> 3;
    uint8 b1 = src_argb[4] >> 3;
    uint8 g1 = src_argb[5] >> 2;
    uint8 r1 = src_argb[6] >> 3;
    *reinterpret_cast<uint32*>(dst_rgb) = b0 | (g0 << 5) | (r0 << 11) |
        (b1 << 16) | (g1 << 21) | (r1 << 27);
    dst_rgb += 4;
    src_argb += 8;
  }
  if (width & 1) {
    uint8 b0 = src_argb[0] >> 3;
    uint8 g0 = src_argb[1] >> 2;
    uint8 r0 = src_argb[2] >> 3;
    *reinterpret_cast<uint16*>(dst_rgb) = b0 | (g0 << 5) | (r0 << 11);
  }
}

void ARGBToARGB1555Row_C(const uint8* src_argb, uint8* dst_rgb, int width) {
  for (int x = 0; x < width - 1; x += 2) {
    uint8 b0 = src_argb[0] >> 3;
    uint8 g0 = src_argb[1] >> 3;
    uint8 r0 = src_argb[2] >> 3;
    uint8 a0 = src_argb[3] >> 7;
    uint8 b1 = src_argb[4] >> 3;
    uint8 g1 = src_argb[5] >> 3;
    uint8 r1 = src_argb[6] >> 3;
    uint8 a1 = src_argb[7] >> 7;
    *reinterpret_cast<uint32*>(dst_rgb) =
        b0 | (g0 << 5) | (r0 << 10) | (a0 << 15) |
        (b1 << 16) | (g1 << 21) | (r1 << 26) | (a1 << 31);
    dst_rgb += 4;
    src_argb += 8;
  }
  if (width & 1) {
    uint8 b0 = src_argb[0] >> 3;
    uint8 g0 = src_argb[1] >> 3;
    uint8 r0 = src_argb[2] >> 3;
    uint8 a0 = src_argb[3] >> 7;
    *reinterpret_cast<uint16*>(dst_rgb) =
        b0 | (g0 << 5) | (r0 << 10) | (a0 << 15);
  }
}

void ARGBToARGB4444Row_C(const uint8* src_argb, uint8* dst_rgb, int width) {
  for (int x = 0; x < width - 1; x += 2) {
    uint8 b0 = src_argb[0] >> 4;
    uint8 g0 = src_argb[1] >> 4;
    uint8 r0 = src_argb[2] >> 4;
    uint8 a0 = src_argb[3] >> 4;
    uint8 b1 = src_argb[4] >> 4;
    uint8 g1 = src_argb[5] >> 4;
    uint8 r1 = src_argb[6] >> 4;
    uint8 a1 = src_argb[7] >> 4;
    *reinterpret_cast<uint32*>(dst_rgb) =
        b0 | (g0 << 4) | (r0 << 8) | (a0 << 12) |
        (b1 << 16) | (g1 << 20) | (r1 << 24) | (a1 << 28);
    dst_rgb += 4;
    src_argb += 8;
  }
  if (width & 1) {
    uint8 b0 = src_argb[0] >> 4;
    uint8 g0 = src_argb[1] >> 4;
    uint8 r0 = src_argb[2] >> 4;
    uint8 a0 = src_argb[3] >> 4;
    *reinterpret_cast<uint16*>(dst_rgb) =
        b0 | (g0 << 4) | (r0 << 8) | (a0 << 12);
  }
}

static __inline int RGBToY(uint8 r, uint8 g, uint8 b) {
  return (( 66 * r + 129 * g +  25 * b + 128) >> 8) + 16;
}

static __inline int RGBToU(uint8 r, uint8 g, uint8 b) {
  return ((-38 * r -  74 * g + 112 * b + 128) >> 8) + 128;
}
static __inline int RGBToV(uint8 r, uint8 g, uint8 b) {
  return ((112 * r -  94 * g -  18 * b + 128) >> 8) + 128;
}

#define MAKEROWY(NAME, R, G, B) \
void NAME ## ToYRow_C(const uint8* src_argb0, uint8* dst_y, int width) {       \
  for (int x = 0; x < width; ++x) {                                            \
    dst_y[0] = RGBToY(src_argb0[R], src_argb0[G], src_argb0[B]);               \
    src_argb0 += 4;                                                            \
    dst_y += 1;                                                                \
  }                                                                            \
}                                                                              \
void NAME ## ToUVRow_C(const uint8* src_rgb0, int src_stride_rgb,              \
                       uint8* dst_u, uint8* dst_v, int width) {                \
  const uint8* src_rgb1 = src_rgb0 + src_stride_rgb;                           \
  for (int x = 0; x < width - 1; x += 2) {                                     \
    uint8 ab = (src_rgb0[B] + src_rgb0[B + 4] +                                \
               src_rgb1[B] + src_rgb1[B + 4]) >> 2;                            \
    uint8 ag = (src_rgb0[G] + src_rgb0[G + 4] +                                \
               src_rgb1[G] + src_rgb1[G + 4]) >> 2;                            \
    uint8 ar = (src_rgb0[R] + src_rgb0[R + 4] +                                \
               src_rgb1[R] + src_rgb1[R + 4]) >> 2;                            \
    dst_u[0] = RGBToU(ar, ag, ab);                                             \
    dst_v[0] = RGBToV(ar, ag, ab);                                             \
    src_rgb0 += 8;                                                             \
    src_rgb1 += 8;                                                             \
    dst_u += 1;                                                                \
    dst_v += 1;                                                                \
  }                                                                            \
  if (width & 1) {                                                             \
    uint8 ab = (src_rgb0[B] + src_rgb1[B]) >> 1;                               \
    uint8 ag = (src_rgb0[G] + src_rgb1[G]) >> 1;                               \
    uint8 ar = (src_rgb0[R] + src_rgb1[R]) >> 1;                               \
    dst_u[0] = RGBToU(ar, ag, ab);                                             \
    dst_v[0] = RGBToV(ar, ag, ab);                                             \
  }                                                                            \
}

MAKEROWY(ARGB, 2, 1, 0)
MAKEROWY(BGRA, 1, 2, 3)
MAKEROWY(ABGR, 0, 1, 2)
MAKEROWY(RGBA, 3, 2, 1)

// http://en.wikipedia.org/wiki/Grayscale.
// 0.11 * B + 0.59 * G + 0.30 * R
// Coefficients rounded to multiple of 2 for consistency with SSSE3 version.
static __inline int RGBToGray(uint8 r, uint8 g, uint8 b) {
  return (( 76 * r + 152 * g +  28 * b) >> 8);
}

void ARGBGrayRow_C(const uint8* src_argb, uint8* dst_argb, int width) {
  for (int x = 0; x < width; ++x) {
    uint8 y = RGBToGray(src_argb[2], src_argb[1], src_argb[0]);
    dst_argb[2] = dst_argb[1] = dst_argb[0] = y;
    dst_argb[3] = src_argb[3];
    dst_argb += 4;
    src_argb += 4;
  }
}

// Convert a row of image to Sepia tone.
void ARGBSepiaRow_C(uint8* dst_argb, int width) {
  for (int x = 0; x < width; ++x) {
    int b = dst_argb[0];
    int g = dst_argb[1];
    int r = dst_argb[2];
    int sb = (b * 17 + g * 68 + r * 35) >> 7;
    int sg = (b * 22 + g * 88 + r * 45) >> 7;
    int sr = (b * 24 + g * 98 + r * 50) >> 7;
    // b does not over flow.  a is preserved from original.
    if (sg > 255) {
      sg = 255;
    }
    if (sr > 255) {
      sr = 255;
    }
    dst_argb[0] = sb;
    dst_argb[1] = sg;
    dst_argb[2] = sr;
    dst_argb += 4;
  }
}

// Apply color matrix to a row of image.  Matrix is signed.
void ARGBColorMatrixRow_C(uint8* dst_argb, const int8* matrix_argb, int width) {
  for (int x = 0; x < width; ++x) {
    int b = dst_argb[0];
    int g = dst_argb[1];
    int r = dst_argb[2];
    int a = dst_argb[3];
    int sb = (b * matrix_argb[0] + g * matrix_argb[1] +
              r * matrix_argb[2] + a * matrix_argb[3]) >> 7;
    int sg = (b * matrix_argb[4] + g * matrix_argb[5] +
              r * matrix_argb[6] + a * matrix_argb[7]) >> 7;
    int sr = (b * matrix_argb[8] + g * matrix_argb[9] +
              r * matrix_argb[10] + a * matrix_argb[11]) >> 7;
    if (sb < 0) {
      sb = 0;
    }
    if (sb > 255) {
      sb = 255;
    }
    if (sg < 0) {
      sg = 0;
    }
    if (sg > 255) {
      sg = 255;
    }
    if (sr < 0) {
      sr = 0;
    }
    if (sr > 255) {
      sr = 255;
    }
    dst_argb[0] = sb;
    dst_argb[1] = sg;
    dst_argb[2] = sr;
    dst_argb += 4;
  }
}

// Apply color table to a row of image.
void ARGBColorTableRow_C(uint8* dst_argb, const uint8* table_argb, int width) {
  for (int x = 0; x < width; ++x) {
    int b = dst_argb[0];
    int g = dst_argb[1];
    int r = dst_argb[2];
    int a = dst_argb[3];
    dst_argb[0] = table_argb[b * 4 + 0];
    dst_argb[1] = table_argb[g * 4 + 1];
    dst_argb[2] = table_argb[r * 4 + 2];
    dst_argb[3] = table_argb[a * 4 + 3];
    dst_argb += 4;
  }
}

void ARGBQuantizeRow_C(uint8* dst_argb, int scale, int interval_size,
                       int interval_offset, int width) {
  for (int x = 0; x < width; ++x) {
    int b = dst_argb[0];
    int g = dst_argb[1];
    int r = dst_argb[2];
    dst_argb[0] = (b * scale >> 16) * interval_size + interval_offset;
    dst_argb[1] = (g * scale >> 16) * interval_size + interval_offset;
    dst_argb[2] = (r * scale >> 16) * interval_size + interval_offset;
    dst_argb += 4;
  }
}

void I400ToARGBRow_C(const uint8* src_y, uint8* dst_argb, int width) {
  // Copy a Y to RGB.
  for (int x = 0; x < width; ++x) {
    uint8 y = src_y[0];
    dst_argb[2] = dst_argb[1] = dst_argb[0] = y;
    dst_argb[3] = 255u;
    dst_argb += 4;
    ++src_y;
  }
}

// C reference code that mimics the YUV assembly.

#define YG 74 /* static_cast<int8>(1.164 * 64 + 0.5) */

#define UB 127 /* min(63,static_cast<int8>(2.018 * 64)) */
#define UG -25 /* static_cast<int8>(-0.391 * 64 - 0.5) */
#define UR 0

#define VB 0
#define VG -52 /* static_cast<int8>(-0.813 * 64 - 0.5) */
#define VR 102 /* static_cast<int8>(1.596 * 64 + 0.5) */

// Bias
#define BB UB * 128 + VB * 128
#define BG UG * 128 + VG * 128
#define BR UR * 128 + VR * 128

static __inline uint32 Clip(int32 val) {
  if (val < 0) {
    return static_cast<uint32>(0);
  } else if (val > 255) {
    return static_cast<uint32>(255);
  }
  return static_cast<uint32>(val);
}

static __inline void YuvPixel(uint8 y, uint8 u, uint8 v, uint8* rgb_buf,
                              int ashift, int rshift, int gshift, int bshift) {
  int32 y1 = (static_cast<int32>(y) - 16) * YG;
  uint32 b = Clip(static_cast<int32>((u * UB + v * VB) - (BB) + y1) >> 6);
  uint32 g = Clip(static_cast<int32>((u * UG + v * VG) - (BG) + y1) >> 6);
  uint32 r = Clip(static_cast<int32>((u * UR + v * VR) - (BR) + y1) >> 6);
  *reinterpret_cast<uint32*>(rgb_buf) = (b << bshift) |
                                        (g << gshift) |
                                        (r << rshift) |
                                        (255u << ashift);
}

void I444ToARGBRow_C(const uint8* y_buf,
                     const uint8* u_buf,
                     const uint8* v_buf,
                     uint8* rgb_buf,
                     int width) {
  for (int x = 0; x < width; ++x) {
    YuvPixel(y_buf[0], u_buf[0], v_buf[0], rgb_buf, 24, 16, 8, 0);
    y_buf += 1;
    u_buf += 1;
    v_buf += 1;
    rgb_buf += 4;  // Advance 1 pixel.
  }
}

// Also used for 420
void I422ToARGBRow_C(const uint8* y_buf,
                     const uint8* u_buf,
                     const uint8* v_buf,
                     uint8* rgb_buf,
                     int width) {
  for (int x = 0; x < width - 1; x += 2) {
    YuvPixel(y_buf[0], u_buf[0], v_buf[0], rgb_buf + 0, 24, 16, 8, 0);
    YuvPixel(y_buf[1], u_buf[0], v_buf[0], rgb_buf + 4, 24, 16, 8, 0);
    y_buf += 2;
    u_buf += 1;
    v_buf += 1;
    rgb_buf += 8;  // Advance 2 pixels.
  }
  if (width & 1) {
    YuvPixel(y_buf[0], u_buf[0], v_buf[0], rgb_buf + 0, 24, 16, 8, 0);
  }
}

void I411ToARGBRow_C(const uint8* y_buf,
                     const uint8* u_buf,
                     const uint8* v_buf,
                     uint8* rgb_buf,
                     int width) {
  for (int x = 0; x < width - 3; x += 4) {
    YuvPixel(y_buf[0], u_buf[0], v_buf[0], rgb_buf + 0, 24, 16, 8, 0);
    YuvPixel(y_buf[1], u_buf[0], v_buf[0], rgb_buf + 4, 24, 16, 8, 0);
    YuvPixel(y_buf[2], u_buf[0], v_buf[0], rgb_buf + 8, 24, 16, 8, 0);
    YuvPixel(y_buf[3], u_buf[0], v_buf[0], rgb_buf + 12, 24, 16, 8, 0);
    y_buf += 4;
    u_buf += 1;
    v_buf += 1;
    rgb_buf += 16;  // Advance 4 pixels.
  }
  if (width & 2) {
    YuvPixel(y_buf[0], u_buf[0], v_buf[0], rgb_buf + 0, 24, 16, 8, 0);
    YuvPixel(y_buf[1], u_buf[0], v_buf[0], rgb_buf + 4, 24, 16, 8, 0);
    y_buf += 2;
    rgb_buf += 8;  // Advance 2 pixels.
  }
  if (width & 1) {
    YuvPixel(y_buf[0], u_buf[0], v_buf[0], rgb_buf + 0, 24, 16, 8, 0);
  }
}

void NV12ToARGBRow_C(const uint8* y_buf,
                     const uint8* uv_buf,
                     uint8* rgb_buf,
                     int width) {
  for (int x = 0; x < width - 1; x += 2) {
    YuvPixel(y_buf[0], uv_buf[0], uv_buf[1], rgb_buf + 0, 24, 16, 8, 0);
    YuvPixel(y_buf[1], uv_buf[0], uv_buf[1], rgb_buf + 4, 24, 16, 8, 0);
    y_buf += 2;
    uv_buf += 2;
    rgb_buf += 8;  // Advance 2 pixels.
  }
  if (width & 1) {
    YuvPixel(y_buf[0], uv_buf[0], uv_buf[1], rgb_buf + 0, 24, 16, 8, 0);
  }
}

void NV21ToARGBRow_C(const uint8* y_buf,
                     const uint8* vu_buf,
                     uint8* rgb_buf,
                     int width) {
  for (int x = 0; x < width - 1; x += 2) {
    YuvPixel(y_buf[0], vu_buf[1], vu_buf[0], rgb_buf + 0, 24, 16, 8, 0);
    YuvPixel(y_buf[1], vu_buf[1], vu_buf[0], rgb_buf + 4, 24, 16, 8, 0);
    y_buf += 2;
    vu_buf += 2;
    rgb_buf += 8;  // Advance 2 pixels.
  }
  if (width & 1) {
    YuvPixel(y_buf[0], vu_buf[1], vu_buf[0], rgb_buf + 0, 24, 16, 8, 0);
  }
}

void I422ToBGRARow_C(const uint8* y_buf,
                     const uint8* u_buf,
                     const uint8* v_buf,
                     uint8* rgb_buf,
                     int width) {
  for (int x = 0; x < width - 1; x += 2) {
    YuvPixel(y_buf[0], u_buf[0], v_buf[0], rgb_buf + 0, 0, 8, 16, 24);
    YuvPixel(y_buf[1], u_buf[0], v_buf[0], rgb_buf + 4, 0, 8, 16, 24);
    y_buf += 2;
    u_buf += 1;
    v_buf += 1;
    rgb_buf += 8;  // Advance 2 pixels.
  }
  if (width & 1) {
    YuvPixel(y_buf[0], u_buf[0], v_buf[0], rgb_buf, 0, 8, 16, 24);
  }
}

void I422ToABGRRow_C(const uint8* y_buf,
                     const uint8* u_buf,
                     const uint8* v_buf,
                     uint8* rgb_buf,
                     int width) {
  for (int x = 0; x < width - 1; x += 2) {
    YuvPixel(y_buf[0], u_buf[0], v_buf[0], rgb_buf + 0, 24, 0, 8, 16);
    YuvPixel(y_buf[1], u_buf[0], v_buf[0], rgb_buf + 4, 24, 0, 8, 16);
    y_buf += 2;
    u_buf += 1;
    v_buf += 1;
    rgb_buf += 8;  // Advance 2 pixels.
  }
  if (width & 1) {
    YuvPixel(y_buf[0], u_buf[0], v_buf[0], rgb_buf + 0, 24, 0, 8, 16);
  }
}

void I422ToRGBARow_C(const uint8* y_buf,
                     const uint8* u_buf,
                     const uint8* v_buf,
                     uint8* rgb_buf,
                     int width) {
  for (int x = 0; x < width - 1; x += 2) {
    YuvPixel(y_buf[0], u_buf[0], v_buf[0], rgb_buf + 0, 0, 24, 16, 8);
    YuvPixel(y_buf[1], u_buf[0], v_buf[0], rgb_buf + 4, 0, 24, 16, 8);
    y_buf += 2;
    u_buf += 1;
    v_buf += 1;
    rgb_buf += 8;  // Advance 2 pixels.
  }
  if (width & 1) {
    YuvPixel(y_buf[0], u_buf[0], v_buf[0], rgb_buf + 0, 0, 24, 16, 8);
  }
}

void YToARGBRow_C(const uint8* y_buf, uint8* rgb_buf, int width) {
  for (int x = 0; x < width; ++x) {
    YuvPixel(y_buf[0], 128, 128, rgb_buf, 24, 16, 8, 0);
    y_buf += 1;
    rgb_buf += 4;  // Advance 1 pixel.
  }
}

void MirrorRow_C(const uint8* src, uint8* dst, int width) {
  src += width - 1;
  for (int x = 0; x < width - 1; x += 2) {
    dst[x] = src[0];
    dst[x + 1] = src[-1];
    src -= 2;
  }
  if (width & 1) {
    dst[width - 1] = src[0];
  }
}

void MirrorRowUV_C(const uint8* src_uv, uint8* dst_u, uint8* dst_v, int width) {
  src_uv += (width - 1) << 1;
  for (int x = 0; x < width - 1; x += 2) {
    dst_u[x] = src_uv[0];
    dst_u[x + 1] = src_uv[-2];
    dst_v[x] = src_uv[1];
    dst_v[x + 1] = src_uv[-2 + 1];
    src_uv -= 4;
  }
  if (width & 1) {
    dst_u[width - 1] = src_uv[0];
    dst_v[width - 1] = src_uv[1];
  }
}

void ARGBMirrorRow_C(const uint8* src, uint8* dst, int width) {
  const uint32* src32 = reinterpret_cast<const uint32*>(src);
  uint32* dst32 = reinterpret_cast<uint32*>(dst);
  src32 += width - 1;
  for (int x = 0; x < width - 1; x += 2) {
    dst32[x] = src32[0];
    dst32[x + 1] = src32[-1];
    src32 -= 2;
  }
  if (width & 1) {
    dst32[width - 1] = src32[0];
  }
}

void SplitUV_C(const uint8* src_uv, uint8* dst_u, uint8* dst_v, int width) {
  for (int x = 0; x < width - 1; x += 2) {
    dst_u[x] = src_uv[0];
    dst_u[x + 1] = src_uv[2];
    dst_v[x] = src_uv[1];
    dst_v[x + 1] = src_uv[3];
    src_uv += 4;
  }
  if (width & 1) {
    dst_u[width - 1] = src_uv[0];
    dst_v[width - 1] = src_uv[1];
  }
}

void CopyRow_C(const uint8* src, uint8* dst, int count) {
  memcpy(dst, src, count);
}

// Filter 2 rows of YUY2 UV's (422) into U and V (420).
void YUY2ToUVRow_C(const uint8* src_yuy2, int src_stride_yuy2,
                   uint8* dst_u, uint8* dst_v, int width) {
  // Output a row of UV values, filtering 2 rows of YUY2.
  for (int x = 0; x < width; x += 2) {
    dst_u[0] = (src_yuy2[1] + src_yuy2[src_stride_yuy2 + 1] + 1) >> 1;
    dst_v[0] = (src_yuy2[3] + src_yuy2[src_stride_yuy2 + 3] + 1) >> 1;
    src_yuy2 += 4;
    dst_u += 1;
    dst_v += 1;
  }
}

// Copy row of YUY2 UV's (422) into U and V (422).
void YUY2ToUV422Row_C(const uint8* src_yuy2,
                      uint8* dst_u, uint8* dst_v, int width) {
  // Output a row of UV values.
  for (int x = 0; x < width; x += 2) {
    dst_u[0] = src_yuy2[1];
    dst_v[0] = src_yuy2[3];
    src_yuy2 += 4;
    dst_u += 1;
    dst_v += 1;
  }
}

// Copy row of YUY2 Y's (422) into Y (420/422).
void YUY2ToYRow_C(const uint8* src_yuy2, uint8* dst_y, int width) {
  // Output a row of Y values.
  for (int x = 0; x < width - 1; x += 2) {
    dst_y[x] = src_yuy2[0];
    dst_y[x + 1] = src_yuy2[2];
    src_yuy2 += 4;
  }
  if (width & 1) {
    dst_y[width - 1] = src_yuy2[0];
  }
}

// Filter 2 rows of UYVY UV's (422) into U and V (420).
void UYVYToUVRow_C(const uint8* src_uyvy, int src_stride_uyvy,
                   uint8* dst_u, uint8* dst_v, int width) {
  // Output a row of UV values.
  for (int x = 0; x < width; x += 2) {
    dst_u[0] = (src_uyvy[0] + src_uyvy[src_stride_uyvy + 0] + 1) >> 1;
    dst_v[0] = (src_uyvy[2] + src_uyvy[src_stride_uyvy + 2] + 1) >> 1;
    src_uyvy += 4;
    dst_u += 1;
    dst_v += 1;
  }
}

// Copy row of UYVY UV's (422) into U and V (422).
void UYVYToUV422Row_C(const uint8* src_uyvy,
                      uint8* dst_u, uint8* dst_v, int width) {
  // Output a row of UV values.
  for (int x = 0; x < width; x += 2) {
    dst_u[0] = src_uyvy[0];
    dst_v[0] = src_uyvy[2];
    src_uyvy += 4;
    dst_u += 1;
    dst_v += 1;
  }
}

// Copy row of UYVY Y's (422) into Y (420/422).
void UYVYToYRow_C(const uint8* src_uyvy, uint8* dst_y, int width) {
  // Output a row of Y values.
  for (int x = 0; x < width - 1; x += 2) {
    dst_y[x] = src_uyvy[1];
    dst_y[x + 1] = src_uyvy[3];
    src_uyvy += 4;
  }
  if (width & 1) {
    dst_y[width - 1] = src_uyvy[1];
  }
}

#define BLEND(f, b, a) (((256 - a) * b) >> 8) + f

// Blend src_argb0 over src_argb1 and store to dst_argb.
// dst_argb may be src_argb0 or src_argb1.
// This code mimics the SSSE3 version for better testability.
void ARGBBlendRow_C(const uint8* src_argb0, const uint8* src_argb1,
                    uint8* dst_argb, int width) {
  for (int x = 0; x < width - 1; x += 2) {
    uint32 fb = src_argb0[0];
    uint32 fg = src_argb0[1];
    uint32 fr = src_argb0[2];
    uint32 a = src_argb0[3];
    uint32 bb = src_argb1[0];
    uint32 bg = src_argb1[1];
    uint32 br = src_argb1[2];
    dst_argb[0] = BLEND(fb, bb, a);
    dst_argb[1] = BLEND(fg, bg, a);
    dst_argb[2] = BLEND(fr, br, a);
    dst_argb[3] = 255u;

    fb = src_argb0[4 + 0];
    fg = src_argb0[4 + 1];
    fr = src_argb0[4 + 2];
    a = src_argb0[4 + 3];
    bb = src_argb1[4 + 0];
    bg = src_argb1[4 + 1];
    br = src_argb1[4 + 2];
    dst_argb[4 + 0] = BLEND(fb, bb, a);
    dst_argb[4 + 1] = BLEND(fg, bg, a);
    dst_argb[4 + 2] = BLEND(fr, br, a);
    dst_argb[4 + 3] = 255u;
    src_argb0 += 8;
    src_argb1 += 8;
    dst_argb += 8;
  }

  if (width & 1) {
    uint32 fb = src_argb0[0];
    uint32 fg = src_argb0[1];
    uint32 fr = src_argb0[2];
    uint32 a = src_argb0[3];
    uint32 bb = src_argb1[0];
    uint32 bg = src_argb1[1];
    uint32 br = src_argb1[2];
    dst_argb[0] = BLEND(fb, bb, a);
    dst_argb[1] = BLEND(fg, bg, a);
    dst_argb[2] = BLEND(fr, br, a);
    dst_argb[3] = 255u;
  }
}
#undef BLEND
#define ATTENUATE(f, a) (a | (a << 8)) * (f | (f << 8)) >> 24

// Multiply source RGB by alpha and store to destination.
// This code mimics the SSSE3 version for better testability.
void ARGBAttenuateRow_C(const uint8* src_argb, uint8* dst_argb, int width) {
  for (int i = 0; i < width - 1; i += 2) {
    uint32 b = src_argb[0];
    uint32 g = src_argb[1];
    uint32 r = src_argb[2];
    uint32 a = src_argb[3];
    dst_argb[0] = ATTENUATE(b, a);
    dst_argb[1] = ATTENUATE(g, a);
    dst_argb[2] = ATTENUATE(r, a);
    dst_argb[3] = a;
    b = src_argb[4];
    g = src_argb[5];
    r = src_argb[6];
    a = src_argb[7];
    dst_argb[4] = ATTENUATE(b, a);
    dst_argb[5] = ATTENUATE(g, a);
    dst_argb[6] = ATTENUATE(r, a);
    dst_argb[7] = a;
    src_argb += 8;
    dst_argb += 8;
  }

  if (width & 1) {
    const uint32 b = src_argb[0];
    const uint32 g = src_argb[1];
    const uint32 r = src_argb[2];
    const uint32 a = src_argb[3];
    dst_argb[0] = ATTENUATE(b, a);
    dst_argb[1] = ATTENUATE(g, a);
    dst_argb[2] = ATTENUATE(r, a);
    dst_argb[3] = a;
  }
}
#undef ATTENUATE

// Divide source RGB by alpha and store to destination.
// b = (b * 255 + (a / 2)) / a;
// g = (g * 255 + (a / 2)) / a;
// r = (r * 255 + (a / 2)) / a;
// Reciprocal method is off by 1 on some values. ie 125
// 8.16 fixed point inverse table
#define T(a) 0x10000 / a
uint32 fixed_invtbl8[256] = {
  0x0100, T(0x01), T(0x02), T(0x03), T(0x04), T(0x05), T(0x06), T(0x07),
  T(0x08), T(0x09), T(0x0a), T(0x0b), T(0x0c), T(0x0d), T(0x0e), T(0x0f),
  T(0x10), T(0x11), T(0x12), T(0x13), T(0x14), T(0x15), T(0x16), T(0x17),
  T(0x18), T(0x19), T(0x1a), T(0x1b), T(0x1c), T(0x1d), T(0x1e), T(0x1f),
  T(0x20), T(0x21), T(0x22), T(0x23), T(0x24), T(0x25), T(0x26), T(0x27),
  T(0x28), T(0x29), T(0x2a), T(0x2b), T(0x2c), T(0x2d), T(0x2e), T(0x2f),
  T(0x30), T(0x31), T(0x32), T(0x33), T(0x34), T(0x35), T(0x36), T(0x37),
  T(0x38), T(0x39), T(0x3a), T(0x3b), T(0x3c), T(0x3d), T(0x3e), T(0x3f),
  T(0x40), T(0x41), T(0x42), T(0x43), T(0x44), T(0x45), T(0x46), T(0x47),
  T(0x48), T(0x49), T(0x4a), T(0x4b), T(0x4c), T(0x4d), T(0x4e), T(0x4f),
  T(0x50), T(0x51), T(0x52), T(0x53), T(0x54), T(0x55), T(0x56), T(0x57),
  T(0x58), T(0x59), T(0x5a), T(0x5b), T(0x5c), T(0x5d), T(0x5e), T(0x5f),
  T(0x60), T(0x61), T(0x62), T(0x63), T(0x64), T(0x65), T(0x66), T(0x67),
  T(0x68), T(0x69), T(0x6a), T(0x6b), T(0x6c), T(0x6d), T(0x6e), T(0x6f),
  T(0x70), T(0x71), T(0x72), T(0x73), T(0x74), T(0x75), T(0x76), T(0x77),
  T(0x78), T(0x79), T(0x7a), T(0x7b), T(0x7c), T(0x7d), T(0x7e), T(0x7f),
  T(0x80), T(0x81), T(0x82), T(0x83), T(0x84), T(0x85), T(0x86), T(0x87),
  T(0x88), T(0x89), T(0x8a), T(0x8b), T(0x8c), T(0x8d), T(0x8e), T(0x8f),
  T(0x90), T(0x91), T(0x92), T(0x93), T(0x94), T(0x95), T(0x96), T(0x97),
  T(0x98), T(0x99), T(0x9a), T(0x9b), T(0x9c), T(0x9d), T(0x9e), T(0x9f),
  T(0xa0), T(0xa1), T(0xa2), T(0xa3), T(0xa4), T(0xa5), T(0xa6), T(0xa7),
  T(0xa8), T(0xa9), T(0xaa), T(0xab), T(0xac), T(0xad), T(0xae), T(0xaf),
  T(0xb0), T(0xb1), T(0xb2), T(0xb3), T(0xb4), T(0xb5), T(0xb6), T(0xb7),
  T(0xb8), T(0xb9), T(0xba), T(0xbb), T(0xbc), T(0xbd), T(0xbe), T(0xbf),
  T(0xc0), T(0xc1), T(0xc2), T(0xc3), T(0xc4), T(0xc5), T(0xc6), T(0xc7),
  T(0xc8), T(0xc9), T(0xca), T(0xcb), T(0xcc), T(0xcd), T(0xce), T(0xcf),
  T(0xd0), T(0xd1), T(0xd2), T(0xd3), T(0xd4), T(0xd5), T(0xd6), T(0xd7),
  T(0xd8), T(0xd9), T(0xda), T(0xdb), T(0xdc), T(0xdd), T(0xde), T(0xdf),
  T(0xe0), T(0xe1), T(0xe2), T(0xe3), T(0xe4), T(0xe5), T(0xe6), T(0xe7),
  T(0xe8), T(0xe9), T(0xea), T(0xeb), T(0xec), T(0xed), T(0xee), T(0xef),
  T(0xf0), T(0xf1), T(0xf2), T(0xf3), T(0xf4), T(0xf5), T(0xf6), T(0xf7),
  T(0xf8), T(0xf9), T(0xfa), T(0xfb), T(0xfc), T(0xfd), T(0xfe), 0x0100 };
#undef T

void ARGBUnattenuateRow_C(const uint8* src_argb, uint8* dst_argb, int width) {
  for (int i = 0; i < width; ++i) {
    uint32 b = src_argb[0];
    uint32 g = src_argb[1];
    uint32 r = src_argb[2];
    const uint32 a = src_argb[3];
    if (a) {
      const uint32 ia = fixed_invtbl8[a];  // 8.16 fixed point
      b = (b * ia) >> 8;
      g = (g * ia) >> 8;
      r = (r * ia) >> 8;
      // Clamping should not be necessary but is free in assembly.
      if (b > 255) {
        b = 255;
      }
      if (g > 255) {
        g = 255;
      }
      if (r > 255) {
        r = 255;
      }
    }
    dst_argb[0] = b;
    dst_argb[1] = g;
    dst_argb[2] = r;
    dst_argb[3] = a;
    src_argb += 4;
    dst_argb += 4;
  }
}

// Wrappers to handle odd width
#define YANY(NAMEANY, I420TORGB_SSE, I420TORGB_C, UV_SHIFT)                    \
    void NAMEANY(const uint8* y_buf,                                           \
                 const uint8* u_buf,                                           \
                 const uint8* v_buf,                                           \
                 uint8* rgb_buf,                                               \
                 int width) {                                                  \
      int n = width & ~7;                                                      \
      I420TORGB_SSE(y_buf, u_buf, v_buf, rgb_buf, n);                          \
      I420TORGB_C(y_buf + n,                                                   \
                  u_buf + (n >> UV_SHIFT),                                     \
                  v_buf + (n >> UV_SHIFT),                                     \
                  rgb_buf + n * 4, width & 7);                                 \
    }

// Wrappers to handle odd width
#define Y2NY(NAMEANY, NV12TORGB_SSE, NV12TORGB_C, UV_SHIFT)                    \
    void NAMEANY(const uint8* y_buf,                                           \
                 const uint8* uv_buf,                                          \
                 uint8* rgb_buf,                                               \
                 int width) {                                                  \
      int n = width & ~7;                                                      \
      NV12TORGB_SSE(y_buf, uv_buf, rgb_buf, n);                                \
      NV12TORGB_C(y_buf + n,                                                   \
                  uv_buf + (n >> UV_SHIFT),                                    \
                  rgb_buf + n * 4, width & 7);                                 \
    }


#ifdef HAS_I422TOARGBROW_SSSE3
YANY(I444ToARGBRow_Any_SSSE3, I444ToARGBRow_Unaligned_SSSE3, I444ToARGBRow_C, 0)
YANY(I422ToARGBRow_Any_SSSE3, I422ToARGBRow_Unaligned_SSSE3, I422ToARGBRow_C, 1)
YANY(I411ToARGBRow_Any_SSSE3, I411ToARGBRow_Unaligned_SSSE3, I411ToARGBRow_C, 2)
Y2NY(NV12ToARGBRow_Any_SSSE3, NV12ToARGBRow_Unaligned_SSSE3, NV12ToARGBRow_C, 0)
Y2NY(NV21ToARGBRow_Any_SSSE3, NV21ToARGBRow_Unaligned_SSSE3, NV21ToARGBRow_C, 0)
YANY(I422ToBGRARow_Any_SSSE3, I422ToBGRARow_Unaligned_SSSE3, I422ToBGRARow_C, 1)
YANY(I422ToABGRRow_Any_SSSE3, I422ToABGRRow_Unaligned_SSSE3, I422ToABGRRow_C, 1)
#endif
#ifdef HAS_I422TORGBAROW_SSSE3
YANY(I422ToRGBARow_Any_SSSE3, I422ToRGBARow_Unaligned_SSSE3, I422ToRGBARow_C, 1)
#endif
#ifdef HAS_I422TOARGBROW_NEON
YANY(I422ToARGBRow_Any_NEON, I422ToARGBRow_NEON, I422ToARGBRow_C, 1)
YANY(I422ToBGRARow_Any_NEON, I422ToBGRARow_NEON, I422ToBGRARow_C, 1)
YANY(I422ToABGRRow_Any_NEON, I422ToABGRRow_NEON, I422ToABGRRow_C, 1)
YANY(I422ToRGBARow_Any_NEON, I422ToRGBARow_NEON, I422ToRGBARow_C, 1)
#endif
#undef YANY

#define RGBANY(NAMEANY, ARGBTORGB, BPP)                                        \
    void NAMEANY(const uint8* argb_buf,                                        \
                 uint8* rgb_buf,                                               \
                 int width) {                                                  \
      SIMD_ALIGNED(uint8 row[kMaxStride]);                                     \
      ARGBTORGB(argb_buf, row, width);                                         \
      memcpy(rgb_buf, row, width * BPP);                                       \
    }

#if defined(HAS_ARGBTORGB24ROW_SSSE3)
RGBANY(ARGBToRGB24Row_Any_SSSE3, ARGBToRGB24Row_SSSE3, 3)
RGBANY(ARGBToRAWRow_Any_SSSE3, ARGBToRAWRow_SSSE3, 3)
RGBANY(ARGBToRGB565Row_Any_SSE2, ARGBToRGB565Row_SSE2, 2)
RGBANY(ARGBToARGB1555Row_Any_SSE2, ARGBToARGB1555Row_SSE2, 2)
RGBANY(ARGBToARGB4444Row_Any_SSE2, ARGBToARGB4444Row_SSE2, 2)
#endif
#if defined(HAS_ARGBTORGB24ROW_NEON)
RGBANY(ARGBToRGB24Row_Any_NEON, ARGBToRGB24Row_NEON, 3)
RGBANY(ARGBToRAWRow_Any_NEON, ARGBToRAWRow_NEON, 3)
#endif
#undef RGBANY

#define YANY(NAMEANY, ARGBTOY_SSE, BPP)                                        \
    void NAMEANY(const uint8* src_argb, uint8* dst_y, int width) {             \
      ARGBTOY_SSE(src_argb, dst_y, width - 16);                                \
      ARGBTOY_SSE(src_argb + (width - 16) * BPP, dst_y + (width - 16), 16);    \
    }

#ifdef HAS_ARGBTOYROW_SSSE3
YANY(ARGBToYRow_Any_SSSE3, ARGBToYRow_Unaligned_SSSE3, 4)
YANY(BGRAToYRow_Any_SSSE3, BGRAToYRow_Unaligned_SSSE3, 4)
YANY(ABGRToYRow_Any_SSSE3, ABGRToYRow_Unaligned_SSSE3, 4)
#endif
#ifdef HAS_RGBATOYROW_SSSE3
YANY(RGBAToYRow_Any_SSSE3, RGBAToYRow_Unaligned_SSSE3, 4)
#endif
#ifdef HAS_YUY2TOYROW_SSE2
YANY(YUY2ToYRow_Any_SSE2, YUY2ToYRow_Unaligned_SSE2, 2)
YANY(UYVYToYRow_Any_SSE2, UYVYToYRow_Unaligned_SSE2, 2)
#endif
#ifdef HAS_YUY2TOYROW_NEON
YANY(YUY2ToYRow_Any_NEON, YUY2ToYRow_NEON, 2)
YANY(UYVYToYRow_Any_NEON, UYVYToYRow_NEON, 2)
#endif
#undef YANY

#define UVANY(NAMEANY, ANYTOUV_SSE, ANYTOUV_C, BPP)                            \
    void NAMEANY(const uint8* src_argb, int src_stride_argb,                   \
                 uint8* dst_u, uint8* dst_v, int width) {                      \
      int n = width & ~15;                                                     \
      ANYTOUV_SSE(src_argb, src_stride_argb, dst_u, dst_v, n);                 \
      ANYTOUV_C(src_argb  + n * BPP, src_stride_argb,                          \
                 dst_u + (n >> 1),                                             \
                 dst_v + (n >> 1),                                             \
                 width & 15);                                                  \
    }

#ifdef HAS_ARGBTOUVROW_SSSE3
UVANY(ARGBToUVRow_Any_SSSE3, ARGBToUVRow_Unaligned_SSSE3, ARGBToUVRow_C, 4)
UVANY(BGRAToUVRow_Any_SSSE3, BGRAToUVRow_Unaligned_SSSE3, BGRAToUVRow_C, 4)
UVANY(ABGRToUVRow_Any_SSSE3, ABGRToUVRow_Unaligned_SSSE3, ABGRToUVRow_C, 4)
#endif
#ifdef HAS_RGBATOYROW_SSSE3
UVANY(RGBAToUVRow_Any_SSSE3, RGBAToUVRow_Unaligned_SSSE3, RGBAToUVRow_C, 4)
#endif
#ifdef HAS_YUY2TOUVROW_SSE2
UVANY(YUY2ToUVRow_Any_SSE2, YUY2ToUVRow_Unaligned_SSE2, YUY2ToUVRow_C, 2)
UVANY(UYVYToUVRow_Any_SSE2, UYVYToUVRow_Unaligned_SSE2, UYVYToUVRow_C, 2)
#endif
#ifdef HAS_YUY2TOUVROW_NEON
UVANY(YUY2ToUVRow_Any_NEON, YUY2ToUVRow_NEON, YUY2ToUVRow_C, 2)
UVANY(UYVYToUVRow_Any_NEON, UYVYToUVRow_NEON, UYVYToUVRow_C, 2)
#endif
#undef UVANY

#define UV422ANY(NAMEANY, ANYTOUV_SSE, ANYTOUV_C, BPP)                         \
    void NAMEANY(const uint8* src_argb,                                        \
                 uint8* dst_u, uint8* dst_v, int width) {                      \
      int n = width & ~15;                                                     \
      ANYTOUV_SSE(src_argb, dst_u, dst_v, n);                                  \
      ANYTOUV_C(src_argb  + n * BPP,                                           \
                 dst_u + (n >> 1),                                             \
                 dst_v + (n >> 1),                                             \
                 width & 15);                                                  \
    }

#ifdef HAS_YUY2TOUV422ROW_SSE2
UV422ANY(YUY2ToUV422Row_Any_SSE2, YUY2ToUV422Row_Unaligned_SSE2,               \
         YUY2ToUV422Row_C, 2)
UV422ANY(UYVYToUV422Row_Any_SSE2, UYVYToUV422Row_Unaligned_SSE2,               \
         UYVYToUV422Row_C, 2)
#endif
#ifdef HAS_YUY2TOUV422ROW_NEON
UV422ANY(YUY2ToUV422Row_Any_NEON, YUY2ToUV422Row_NEON,                         \
         YUY2ToUV422Row_C, 2)
UV422ANY(UYVYToUV422Row_Any_NEON, UYVYToUV422Row_NEON,                         \
         UYVYToUV422Row_C, 2)
#endif
#undef UV422ANY

void ComputeCumulativeSumRow_C(const uint8* row, int32* cumsum,
                               const int32* previous_cumsum, int width) {
  int32 row_sum[4] = {0, 0, 0, 0};
  for (int x = 0; x < width; ++x) {
    row_sum[0] += row[x * 4 + 0];
    row_sum[1] += row[x * 4 + 1];
    row_sum[2] += row[x * 4 + 2];
    row_sum[3] += row[x * 4 + 3];
    cumsum[x * 4 + 0] = row_sum[0]  + previous_cumsum[x * 4 + 0];
    cumsum[x * 4 + 1] = row_sum[1]  + previous_cumsum[x * 4 + 1];
    cumsum[x * 4 + 2] = row_sum[2]  + previous_cumsum[x * 4 + 2];
    cumsum[x * 4 + 3] = row_sum[3]  + previous_cumsum[x * 4 + 3];
  }
}

void CumulativeSumToAverage_C(const int32* tl, const int32* bl,
                              int w, int area, uint8* dst, int count) {
  float ooa = 1.0f / area;
  for (int i = 0; i < count; ++i) {
    dst[0] = static_cast<uint8>((bl[w + 0] + tl[0] - bl[0] - tl[w + 0]) * ooa);
    dst[1] = static_cast<uint8>((bl[w + 1] + tl[1] - bl[1] - tl[w + 1]) * ooa);
    dst[2] = static_cast<uint8>((bl[w + 2] + tl[2] - bl[2] - tl[w + 2]) * ooa);
    dst[3] = static_cast<uint8>((bl[w + 3] + tl[3] - bl[3] - tl[w + 3]) * ooa);
    dst += 4;
    tl += 4;
    bl += 4;
  }
}

#define REPEAT8(v) (v) | ((v) << 8)
#define SHADE(f, v) v * f >> 24

void ARGBShadeRow_C(const uint8* src_argb, uint8* dst_argb, int width,
                    uint32 value) {
  const uint32 b_scale = REPEAT8(value & 0xff);
  const uint32 g_scale = REPEAT8((value >> 8) & 0xff);
  const uint32 r_scale = REPEAT8((value >> 16) & 0xff);
  const uint32 a_scale = REPEAT8(value >> 24);

  for (int i = 0; i < width; ++i) {
    const uint32 b = REPEAT8(src_argb[0]);
    const uint32 g = REPEAT8(src_argb[1]);
    const uint32 r = REPEAT8(src_argb[2]);
    const uint32 a = REPEAT8(src_argb[3]);
    dst_argb[0] = SHADE(b, b_scale);
    dst_argb[1] = SHADE(g, g_scale);
    dst_argb[2] = SHADE(r, r_scale);
    dst_argb[3] = SHADE(a, a_scale);
    src_argb += 4;
    dst_argb += 4;
  }
}
#undef REPEAT8
#undef SHADE

// Copy pixels from rotated source to destination row with a slope.
LIBYUV_API
void ARGBAffineRow_C(const uint8* src_argb, int src_argb_stride,
                     uint8* dst_argb, const float* uv_dudv, int width) {
  // Render a row of pixels from source into a buffer.
  float uv[2];
  uv[0] = uv_dudv[0];
  uv[1] = uv_dudv[1];
  for (int i = 0; i < width; ++i) {
    int x = static_cast<int>(uv[0]);
    int y = static_cast<int>(uv[1]);
    *reinterpret_cast<uint32*>(dst_argb) =
        *reinterpret_cast<const uint32*>(src_argb + y * src_argb_stride +
                                         x * 4);
    dst_argb += 4;
    uv[0] += uv_dudv[2];
    uv[1] += uv_dudv[3];
  }
}

// C version 2x2 -> 2x1.
void ARGBInterpolateRow_C(uint8* dst_ptr, const uint8* src_ptr,
                          ptrdiff_t src_stride,
                          int dst_width, int source_y_fraction) {
  int y1_fraction = source_y_fraction;
  int y0_fraction = 256 - y1_fraction;
  const uint8* src_ptr1 = src_ptr + src_stride;
  uint8* end = dst_ptr + (dst_width << 2);
  do {
    dst_ptr[0] = (src_ptr[0] * y0_fraction + src_ptr1[0] * y1_fraction) >> 8;
    dst_ptr[1] = (src_ptr[1] * y0_fraction + src_ptr1[1] * y1_fraction) >> 8;
    dst_ptr[2] = (src_ptr[2] * y0_fraction + src_ptr1[2] * y1_fraction) >> 8;
    dst_ptr[3] = (src_ptr[3] * y0_fraction + src_ptr1[3] * y1_fraction) >> 8;
    dst_ptr[4] = (src_ptr[4] * y0_fraction + src_ptr1[4] * y1_fraction) >> 8;
    dst_ptr[5] = (src_ptr[5] * y0_fraction + src_ptr1[5] * y1_fraction) >> 8;
    dst_ptr[6] = (src_ptr[6] * y0_fraction + src_ptr1[6] * y1_fraction) >> 8;
    dst_ptr[7] = (src_ptr[7] * y0_fraction + src_ptr1[7] * y1_fraction) >> 8;
    src_ptr += 8;
    src_ptr1 += 8;
    dst_ptr += 8;
  } while (dst_ptr < end);
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
