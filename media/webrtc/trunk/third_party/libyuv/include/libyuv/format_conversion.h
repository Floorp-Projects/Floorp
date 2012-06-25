/*
 *  Copyright (c) 2011 The LibYuv project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef INCLUDE_LIBYUV_FORMATCONVERSION_H_
#define INCLUDE_LIBYUV_FORMATCONVERSION_H_

#include "libyuv/basic_types.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Converts any Bayer RGB format to I420.
int BayerRGBToI420(const uint8* src_bayer, int src_stride_bayer,
                   uint32 src_fourcc_bayer,
                   uint8* dst_y, int dst_stride_y,
                   uint8* dst_u, int dst_stride_u,
                   uint8* dst_v, int dst_stride_v,
                   int width, int height);

// Converts any Bayer RGB format to ARGB.
int BayerRGBToARGB(const uint8* src_bayer, int src_stride_bayer,
                   uint32 src_fourcc_bayer,
                   uint8* dst_rgb, int dst_stride_rgb,
                   int width, int height);

// Converts ARGB to any Bayer RGB format.
int ARGBToBayerRGB(const uint8* src_rgb, int src_stride_rgb,
                   uint8* dst_bayer, int dst_stride_bayer,
                   uint32 dst_fourcc_bayer,
                   int width, int height);

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // INCLUDE_LIBYUV_FORMATCONVERSION_H_
