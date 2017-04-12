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

#ifndef INCLUDE_LIBYUV_ROTATE_H_  // NOLINT
#define INCLUDE_LIBYUV_ROTATE_H_

#include "libyuv/basic_types.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Supported rotation.
typedef enum RotationMode {
  kRotate0 = 0,  // No rotation.
  kRotate90 = 90,  // Rotate 90 degrees clockwise.
  kRotate180 = 180,  // Rotate 180 degrees.
  kRotate270 = 270,  // Rotate 270 degrees clockwise.

  // Deprecated.
  kRotateNone = 0,
  kRotateClockwise = 90,
  kRotateCounterClockwise = 270,
} RotationModeEnum;

// Rotate I420 frame.
LIBYUV_API
int I420Rotate(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int src_width, int src_height, enum RotationMode mode);

// Rotate NV12 input and store in I420.
LIBYUV_API
int NV12ToI420Rotate(const uint8* src_y, int src_stride_y,
                     const uint8* src_uv, int src_stride_uv,
                     uint8* dst_y, int dst_stride_y,
                     uint8* dst_u, int dst_stride_u,
                     uint8* dst_v, int dst_stride_v,
                     int src_width, int src_height, enum RotationMode mode);

// Rotate a plane by 0, 90, 180, or 270.
LIBYUV_API
int RotatePlane(const uint8* src, int src_stride,
                uint8* dst, int dst_stride,
                int src_width, int src_height, enum RotationMode mode);

// Rotate planes by 90, 180, 270. Deprecated.
LIBYUV_API
void RotatePlane90(const uint8* src, int src_stride,
                   uint8* dst, int dst_stride,
                   int width, int height);

LIBYUV_API
void RotatePlane180(const uint8* src, int src_stride,
                    uint8* dst, int dst_stride,
                    int width, int height);

LIBYUV_API
void RotatePlane270(const uint8* src, int src_stride,
                    uint8* dst, int dst_stride,
                    int width, int height);

LIBYUV_API
void RotateUV90(const uint8* src, int src_stride,
                uint8* dst_a, int dst_stride_a,
                uint8* dst_b, int dst_stride_b,
                int width, int height);

// Rotations for when U and V are interleaved.
// These functions take one input pointer and
// split the data into two buffers while
// rotating them. Deprecated.
LIBYUV_API
void RotateUV180(const uint8* src, int src_stride,
                 uint8* dst_a, int dst_stride_a,
                 uint8* dst_b, int dst_stride_b,
                 int width, int height);

LIBYUV_API
void RotateUV270(const uint8* src, int src_stride,
                 uint8* dst_a, int dst_stride_a,
                 uint8* dst_b, int dst_stride_b,
                 int width, int height);

// The 90 and 270 functions are based on transposes.
// Doing a transpose with reversing the read/write
// order will result in a rotation by +- 90 degrees.
// Deprecated.
LIBYUV_API
void TransposePlane(const uint8* src, int src_stride,
                    uint8* dst, int dst_stride,
                    int width, int height);

LIBYUV_API
void TransposeUV(const uint8* src, int src_stride,
                 uint8* dst_a, int dst_stride_a,
                 uint8* dst_b, int dst_stride_b,
                 int width, int height);

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // INCLUDE_LIBYUV_ROTATE_H_  NOLINT
