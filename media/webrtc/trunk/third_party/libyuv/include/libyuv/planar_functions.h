/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef INCLUDE_LIBYUV_PLANAR_FUNCTIONS_H_  // NOLINT
#define INCLUDE_LIBYUV_PLANAR_FUNCTIONS_H_

#include "libyuv/basic_types.h"

// TODO(fbarchard): Remove the following headers includes.
#include "libyuv/convert.h"
#include "libyuv/convert_argb.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

LIBYUV_API
void SetPlane(uint8* dst_y, int dst_stride_y,
              int width, int height,
              uint32 value);

// Copy a plane of data (I420 to I400).
LIBYUV_API
void CopyPlane(const uint8* src_y, int src_stride_y,
               uint8* dst_y, int dst_stride_y,
               int width, int height);

// Convert I420 to I400.  (calls CopyPlane ignoring u/v).
LIBYUV_API
int I420ToI400(const uint8* src_y, int src_stride_y,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height);

// I420 mirror.
LIBYUV_API
int I420Mirror(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height);

// ARGB mirror.
LIBYUV_API
int ARGBMirror(const uint8* src_argb, int src_stride_argb,
               uint8* dst_argb, int dst_stride_argb,
               int width, int height);

// Convert NV12 to RGB565.
LIBYUV_API
int NV12ToRGB565(const uint8* src_y, int src_stride_y,
                 const uint8* src_uv, int src_stride_uv,
                 uint8* dst_rgb565, int dst_stride_rgb565,
                 int width, int height);

// Convert NV21 to RGB565.
LIBYUV_API
int NV21ToRGB565(const uint8* src_y, int src_stride_y,
                 const uint8* src_uv, int src_stride_uv,
                 uint8* dst_rgb565, int dst_stride_rgb565,
                 int width, int height);

// Aliases.
#define ARGBToBGRA BGRAToARGB
#define ARGBToABGR ABGRToARGB

// Convert ARGB To RGBA.
LIBYUV_API
int ARGBToRGBA(const uint8* src_frame, int src_stride_frame,
               uint8* dst_argb, int dst_stride_argb,
               int width, int height);

// Convert ARGB To RGB24.
LIBYUV_API
int ARGBToRGB24(const uint8* src_argb, int src_stride_argb,
                uint8* dst_rgb24, int dst_stride_rgb24,
                int width, int height);

// Convert ARGB To RAW.
LIBYUV_API
int ARGBToRAW(const uint8* src_argb, int src_stride_argb,
              uint8* dst_rgb, int dst_stride_rgb,
              int width, int height);

// Convert ARGB To RGB565.
LIBYUV_API
int ARGBToRGB565(const uint8* src_argb, int src_stride_argb,
                 uint8* dst_rgb565, int dst_stride_rgb565,
                 int width, int height);

// Convert ARGB To ARGB1555.
LIBYUV_API
int ARGBToARGB1555(const uint8* src_argb, int src_stride_argb,
                   uint8* dst_argb1555, int dst_stride_argb1555,
                   int width, int height);

// Convert ARGB To ARGB4444.
LIBYUV_API
int ARGBToARGB4444(const uint8* src_argb, int src_stride_argb,
                   uint8* dst_argb4444, int dst_stride_argb4444,
                   int width, int height);

// Convert ARGB to I400.
LIBYUV_API
int ARGBToI400(const uint8* src_argb, int src_stride_argb,
               uint8* dst_y, int dst_stride_y,
               int width, int height);

// ARGB little endian (bgra in memory) to I422.
LIBYUV_API
int ARGBToI422(const uint8* src_frame, int src_stride_frame,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height);

// I422ToARGB is in convert_argb.h
// Convert I422 to BGRA.
LIBYUV_API
int I422ToBGRA(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_bgra, int dst_stride_bgra,
               int width, int height);

// Convert I422 to ABGR.
LIBYUV_API
int I422ToABGR(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_abgr, int dst_stride_abgr,
               int width, int height);

// Convert I422 to RGBA.
LIBYUV_API
int I422ToRGBA(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_rgba, int dst_stride_rgba,
               int width, int height);

// Draw a rectangle into I420.
LIBYUV_API
int I420Rect(uint8* dst_y, int dst_stride_y,
             uint8* dst_u, int dst_stride_u,
             uint8* dst_v, int dst_stride_v,
             int x, int y, int width, int height,
             int value_y, int value_u, int value_v);

// Draw a rectangle into ARGB.
LIBYUV_API
int ARGBRect(uint8* dst_argb, int dst_stride_argb,
             int x, int y, int width, int height, uint32 value);

// Convert ARGB to gray scale ARGB.
LIBYUV_API
int ARGBGrayTo(const uint8* src_argb, int src_stride_argb,
               uint8* dst_argb, int dst_stride_argb,
               int width, int height);

// Make a rectangle of ARGB gray scale.
LIBYUV_API
int ARGBGray(uint8* dst_argb, int dst_stride_argb,
             int x, int y, int width, int height);

// Make a rectangle of ARGB Sepia tone.
LIBYUV_API
int ARGBSepia(uint8* dst_argb, int dst_stride_argb,
              int x, int y, int width, int height);

// Apply a matrix rotation to each ARGB pixel.
// matrix_argb is 3 signed ARGB values. -128 to 127 representing -1 to 1.
// The first 4 coefficients apply to B, G, R, A and produce B of the output.
// The next 4 coefficients apply to B, G, R, A and produce G of the output.
// The last 4 coefficients apply to B, G, R, A and produce R of the output.
LIBYUV_API
int ARGBColorMatrix(uint8* dst_argb, int dst_stride_argb,
                    const int8* matrix_argb,
                    int x, int y, int width, int height);

// Apply a color table each ARGB pixel.
// Table contains 256 ARGB values.
LIBYUV_API
int ARGBColorTable(uint8* dst_argb, int dst_stride_argb,
                   const uint8* table_argb,
                   int x, int y, int width, int height);

// Quantize a rectangle of ARGB.  Alpha unaffected.
// scale is a 16 bit fractional fixed point scaler between 0 and 65535.
// interval_size should be a value between 1 and 255.
// interval_offset should be a value between 0 and 255.
LIBYUV_API
int ARGBQuantize(uint8* dst_argb, int dst_stride_argb,
                 int scale, int interval_size, int interval_offset,
                 int x, int y, int width, int height);

// Copy ARGB to ARGB.
LIBYUV_API
int ARGBCopy(const uint8* src_argb, int src_stride_argb,
             uint8* dst_argb, int dst_stride_argb,
             int width, int height);

typedef void (*ARGBBlendRow)(const uint8* src_argb0, const uint8* src_argb1,
                             uint8* dst_argb, int width);

// Get function to Alpha Blend ARGB pixels and store to destination.
LIBYUV_API
ARGBBlendRow GetARGBBlend();

// Alpha Blend ARGB images and store to destination.
// Alpha of destination is set to 255.
LIBYUV_API
int ARGBBlend(const uint8* src_argb0, int src_stride_argb0,
              const uint8* src_argb1, int src_stride_argb1,
              uint8* dst_argb, int dst_stride_argb,
              int width, int height);

// Convert I422 to YUY2.
LIBYUV_API
int I422ToYUY2(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_frame, int dst_stride_frame,
               int width, int height);

// Convert I422 to UYVY.
LIBYUV_API
int I422ToUYVY(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_frame, int dst_stride_frame,
               int width, int height);

// Convert unattentuated ARGB to preattenuated ARGB.
LIBYUV_API
int ARGBAttenuate(const uint8* src_argb, int src_stride_argb,
                  uint8* dst_argb, int dst_stride_argb,
                  int width, int height);

// Convert preattentuated ARGB to unattenuated ARGB.
LIBYUV_API
int ARGBUnattenuate(const uint8* src_argb, int src_stride_argb,
                    uint8* dst_argb, int dst_stride_argb,
                    int width, int height);

// Convert MJPG to ARGB.
LIBYUV_API
int MJPGToARGB(const uint8* sample, size_t sample_size,
               uint8* argb, int argb_stride,
               int w, int h, int dw, int dh);

// Computes table of cumulative sum for image where the value is the sum
// of all values above and to the left of the entry.  Used by ARGBBlur.
LIBYUV_API
int ARGBComputeCumulativeSum(const uint8* src_argb, int src_stride_argb,
                             int32* dst_cumsum, int dst_stride32_cumsum,
                             int width, int height);

// Blur ARGB image.
// Caller should allocate dst_cumsum table of width * height * 16 bytes aligned
// to 16 byte boundary.
LIBYUV_API
int ARGBBlur(const uint8* src_argb, int src_stride_argb,
             uint8* dst_argb, int dst_stride_argb,
             int32* dst_cumsum, int dst_stride32_cumsum,
             int width, int height, int radius);

// Multiply ARGB image by ARGB value.
LIBYUV_API
int ARGBShade(const uint8* src_argb, int src_stride_argb,
              uint8* dst_argb, int dst_stride_argb,
              int width, int height, uint32 value);

// Interpolate between two ARGB images using specified amount of interpolation
// (0 to 255) and store to destination.
// 'interpolation' is specified as 8 bit fraction where 0 means 100% src_argb0
// and 255 means 1% src_argb0 and 99% src_argb1.
// Internally uses ARGBScale bilinear filtering.
// Caveat: This function will write up to 16 bytes beyond the end of dst_argb.
LIBYUV_API
int ARGBInterpolate(const uint8* src_argb0, int src_stride_argb0,
                    const uint8* src_argb1, int src_stride_argb1,
                    uint8* dst_argb, int dst_stride_argb,
                    int width, int height, int interpolation);

#if defined(__CLR_VER) || defined(COVERAGE_ENABLED) || \
    defined(TARGET_IPHONE_SIMULATOR)
#define YUV_DISABLE_ASM
#endif
// Row functions for copying a pixels from a source with a slope to a row
// of destination.  Useful for scaling, rotation, mirror, texture mapping.
LIBYUV_API
void ARGBAffineRow_C(const uint8* src_argb, int src_argb_stride,
                     uint8* dst_argb, const float* uv_dudv, int width);
// The following are available on all x86 platforms:
#if !defined(YUV_DISABLE_ASM) && \
    (defined(_M_IX86) || defined(__x86_64__) || defined(__i386__))
LIBYUV_API
void ARGBAffineRow_SSE2(const uint8* src_argb, int src_argb_stride,
                        uint8* dst_argb, const float* uv_dudv, int width);
#define HAS_ARGBAFFINEROW_SSE2
#endif

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // INCLUDE_LIBYUV_PLANAR_FUNCTIONS_H_  NOLINT
