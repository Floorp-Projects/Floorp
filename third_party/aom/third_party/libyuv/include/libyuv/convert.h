/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef INCLUDE_LIBYUV_CONVERT_H_  // NOLINT
#define INCLUDE_LIBYUV_CONVERT_H_

#include "libyuv/basic_types.h"
// TODO(fbarchard): Remove the following headers includes.
#include "libyuv/convert_from.h"
#include "libyuv/planar_functions.h"
#include "libyuv/rotate.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Convert I444 to I420.
LIBYUV_API
int I444ToI420(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height);

// Convert I422 to I420.
LIBYUV_API
int I422ToI420(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height);

// Convert I411 to I420.
LIBYUV_API
int I411ToI420(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height);

// Copy I420 to I420.
#define I420ToI420 I420Copy
LIBYUV_API
int I420Copy(const uint8* src_y, int src_stride_y,
             const uint8* src_u, int src_stride_u,
             const uint8* src_v, int src_stride_v,
             uint8* dst_y, int dst_stride_y,
             uint8* dst_u, int dst_stride_u,
             uint8* dst_v, int dst_stride_v,
             int width, int height);

// Convert I400 (grey) to I420.
LIBYUV_API
int I400ToI420(const uint8* src_y, int src_stride_y,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height);

#define J400ToJ420 I400ToI420

// Convert NV12 to I420.
LIBYUV_API
int NV12ToI420(const uint8* src_y, int src_stride_y,
               const uint8* src_uv, int src_stride_uv,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height);

// Convert NV21 to I420.
LIBYUV_API
int NV21ToI420(const uint8* src_y, int src_stride_y,
               const uint8* src_vu, int src_stride_vu,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height);

// Convert YUY2 to I420.
LIBYUV_API
int YUY2ToI420(const uint8* src_yuy2, int src_stride_yuy2,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height);

// Convert UYVY to I420.
LIBYUV_API
int UYVYToI420(const uint8* src_uyvy, int src_stride_uyvy,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height);

// Convert M420 to I420.
LIBYUV_API
int M420ToI420(const uint8* src_m420, int src_stride_m420,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height);

// ARGB little endian (bgra in memory) to I420.
LIBYUV_API
int ARGBToI420(const uint8* src_frame, int src_stride_frame,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height);

// BGRA little endian (argb in memory) to I420.
LIBYUV_API
int BGRAToI420(const uint8* src_frame, int src_stride_frame,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height);

// ABGR little endian (rgba in memory) to I420.
LIBYUV_API
int ABGRToI420(const uint8* src_frame, int src_stride_frame,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height);

// RGBA little endian (abgr in memory) to I420.
LIBYUV_API
int RGBAToI420(const uint8* src_frame, int src_stride_frame,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height);

// RGB little endian (bgr in memory) to I420.
LIBYUV_API
int RGB24ToI420(const uint8* src_frame, int src_stride_frame,
                uint8* dst_y, int dst_stride_y,
                uint8* dst_u, int dst_stride_u,
                uint8* dst_v, int dst_stride_v,
                int width, int height);

// RGB big endian (rgb in memory) to I420.
LIBYUV_API
int RAWToI420(const uint8* src_frame, int src_stride_frame,
              uint8* dst_y, int dst_stride_y,
              uint8* dst_u, int dst_stride_u,
              uint8* dst_v, int dst_stride_v,
              int width, int height);

// RGB16 (RGBP fourcc) little endian to I420.
LIBYUV_API
int RGB565ToI420(const uint8* src_frame, int src_stride_frame,
                 uint8* dst_y, int dst_stride_y,
                 uint8* dst_u, int dst_stride_u,
                 uint8* dst_v, int dst_stride_v,
                 int width, int height);

// RGB15 (RGBO fourcc) little endian to I420.
LIBYUV_API
int ARGB1555ToI420(const uint8* src_frame, int src_stride_frame,
                   uint8* dst_y, int dst_stride_y,
                   uint8* dst_u, int dst_stride_u,
                   uint8* dst_v, int dst_stride_v,
                   int width, int height);

// RGB12 (R444 fourcc) little endian to I420.
LIBYUV_API
int ARGB4444ToI420(const uint8* src_frame, int src_stride_frame,
                   uint8* dst_y, int dst_stride_y,
                   uint8* dst_u, int dst_stride_u,
                   uint8* dst_v, int dst_stride_v,
                   int width, int height);

#ifdef HAVE_JPEG
// src_width/height provided by capture.
// dst_width/height for clipping determine final size.
LIBYUV_API
int MJPGToI420(const uint8* sample, size_t sample_size,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int src_width, int src_height,
               int dst_width, int dst_height);

// Query size of MJPG in pixels.
LIBYUV_API
int MJPGSize(const uint8* sample, size_t sample_size,
             int* width, int* height);
#endif

// Convert camera sample to I420 with cropping, rotation and vertical flip.
// "src_size" is needed to parse MJPG.
// "dst_stride_y" number of bytes in a row of the dst_y plane.
//   Normally this would be the same as dst_width, with recommended alignment
//   to 16 bytes for better efficiency.
//   If rotation of 90 or 270 is used, stride is affected. The caller should
//   allocate the I420 buffer according to rotation.
// "dst_stride_u" number of bytes in a row of the dst_u plane.
//   Normally this would be the same as (dst_width + 1) / 2, with
//   recommended alignment to 16 bytes for better efficiency.
//   If rotation of 90 or 270 is used, stride is affected.
// "crop_x" and "crop_y" are starting position for cropping.
//   To center, crop_x = (src_width - dst_width) / 2
//              crop_y = (src_height - dst_height) / 2
// "src_width" / "src_height" is size of src_frame in pixels.
//   "src_height" can be negative indicating a vertically flipped image source.
// "crop_width" / "crop_height" is the size to crop the src to.
//    Must be less than or equal to src_width/src_height
//    Cropping parameters are pre-rotation.
// "rotation" can be 0, 90, 180 or 270.
// "format" is a fourcc. ie 'I420', 'YUY2'
// Returns 0 for successful; -1 for invalid parameter. Non-zero for failure.
LIBYUV_API
int ConvertToI420(const uint8* src_frame, size_t src_size,
                  uint8* dst_y, int dst_stride_y,
                  uint8* dst_u, int dst_stride_u,
                  uint8* dst_v, int dst_stride_v,
                  int crop_x, int crop_y,
                  int src_width, int src_height,
                  int crop_width, int crop_height,
                  enum RotationMode rotation,
                  uint32 format);

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // INCLUDE_LIBYUV_CONVERT_H_  NOLINT
