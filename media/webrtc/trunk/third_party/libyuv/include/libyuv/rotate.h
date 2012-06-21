/*
 *  Copyright (c) 2011 The LibYuv project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef INCLUDE_LIBYUV_ROTATE_H_
#define INCLUDE_LIBYUV_ROTATE_H_

#include "libyuv/basic_types.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Supported rotation
enum RotationMode {
  kRotate0 = 0, // No rotation
  kRotate90 = 90,  // Rotate 90 degrees clockwise
  kRotate180 = 180,  // Rotate 180 degrees
  kRotate270 = 270,  // Rotate 270 degrees clockwise

  // Deprecated
  kRotateNone = 0,
  kRotateClockwise = 90,
  kRotateCounterClockwise = 270,
};

// Rotate I420 frame
int I420Rotate(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height,
               RotationMode mode);

// Rotate NV12 input and store in I420
int NV12ToI420Rotate(const uint8* src_y, int src_stride_y,
                     const uint8* src_uv, int src_stride_uv,
                     uint8* dst_y, int dst_stride_y,
                     uint8* dst_u, int dst_stride_u,
                     uint8* dst_v, int dst_stride_v,
                     int width, int height,
                     RotationMode mode);

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // INCLUDE_LIBYUV_ROTATE_H_
