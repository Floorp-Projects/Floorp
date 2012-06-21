/*
 *  Copyright (c) 2011 The LibYuv project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SOURCE_ROTATE_PRIV_H_
#define SOURCE_ROTATE_PRIV_H_

#include "libyuv/basic_types.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Rotate planes by 90, 180, 270
void RotatePlane90(const uint8* src, int src_stride,
                   uint8* dst, int dst_stride,
                   int width, int height);

void RotatePlane180(const uint8* src, int src_stride,
                    uint8* dst, int dst_stride,
                    int width, int height);

void RotatePlane270(const uint8* src, int src_stride,
                    uint8* dst, int dst_stride,
                    int width, int height);

void RotateUV90(const uint8* src, int src_stride,
                uint8* dst_a, int dst_stride_a,
                uint8* dst_b, int dst_stride_b,
                int width, int height);

// Rotations for when U and V are interleaved.
// These functions take one input pointer and
// split the data into two buffers while
// rotating them.
void RotateUV180(const uint8* src, int src_stride,
                 uint8* dst_a, int dst_stride_a,
                 uint8* dst_b, int dst_stride_b,
                 int width, int height);

void RotateUV270(const uint8* src, int src_stride,
                 uint8* dst_a, int dst_stride_a,
                 uint8* dst_b, int dst_stride_b,
                 int width, int height);

// The 90 and 270 functions are based on transposes.
// Doing a transpose with reversing the read/write
// order will result in a rotation by +- 90 degrees.
void TransposePlane(const uint8* src, int src_stride,
                    uint8* dst, int dst_stride,
                    int width, int height);

void TransposeUV(const uint8* src, int src_stride,
                 uint8* dst_a, int dst_stride_a,
                 uint8* dst_b, int dst_stride_b,
                 int width, int height);

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // SOURCE_ROTATE_PRIV_H_
