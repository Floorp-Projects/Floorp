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

#ifndef INCLUDE_LIBYUV_SCALE_ARGB_H_  // NOLINT
#define INCLUDE_LIBYUV_SCALE_ARGB_H_

#include "libyuv/basic_types.h"
#include "libyuv/scale.h"  // For FilterMode

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

LIBYUV_API
int ARGBScale(const uint8* src_argb, int src_stride_argb,
              int src_width, int src_height,
              uint8* dst_argb, int dst_stride_argb,
              int dst_width, int dst_height,
              enum FilterMode filtering);

// Clipped scale takes destination rectangle coordinates for clip values.
LIBYUV_API
int ARGBScaleClip(const uint8* src_argb, int src_stride_argb,
                  int src_width, int src_height,
                  uint8* dst_argb, int dst_stride_argb,
                  int dst_width, int dst_height,
                  int clip_x, int clip_y, int clip_width, int clip_height,
                  enum FilterMode filtering);

// TODO(fbarchard): Implement this.
// Scale with YUV conversion to ARGB and clipping.
LIBYUV_API
int YUVToARGBScaleClip(const uint8* src_y, int src_stride_y,
                       const uint8* src_u, int src_stride_u,
                       const uint8* src_v, int src_stride_v,
                       uint32 src_fourcc,
                       int src_width, int src_height,
                       uint8* dst_argb, int dst_stride_argb,
                       uint32 dst_fourcc,
                       int dst_width, int dst_height,
                       int clip_x, int clip_y, int clip_width, int clip_height,
                       enum FilterMode filtering);

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // INCLUDE_LIBYUV_SCALE_ARGB_H_  NOLINT
