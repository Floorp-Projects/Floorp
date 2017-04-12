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

#ifndef INCLUDE_LIBYUV_COMPARE_H_  // NOLINT
#define INCLUDE_LIBYUV_COMPARE_H_

#include "libyuv/basic_types.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Compute a hash for specified memory. Seed of 5381 recommended.
LIBYUV_API
uint32 HashDjb2(const uint8* src, uint64 count, uint32 seed);

// Scan an opaque argb image and return fourcc based on alpha offset.
// Returns FOURCC_ARGB, FOURCC_BGRA, or 0 if unknown.
LIBYUV_API
uint32 ARGBDetect(const uint8* argb, int stride_argb, int width, int height);

// Sum Square Error - used to compute Mean Square Error or PSNR.
LIBYUV_API
uint64 ComputeSumSquareError(const uint8* src_a,
                             const uint8* src_b, int count);

LIBYUV_API
uint64 ComputeSumSquareErrorPlane(const uint8* src_a, int stride_a,
                                  const uint8* src_b, int stride_b,
                                  int width, int height);

static const int kMaxPsnr = 128;

LIBYUV_API
double SumSquareErrorToPsnr(uint64 sse, uint64 count);

LIBYUV_API
double CalcFramePsnr(const uint8* src_a, int stride_a,
                     const uint8* src_b, int stride_b,
                     int width, int height);

LIBYUV_API
double I420Psnr(const uint8* src_y_a, int stride_y_a,
                const uint8* src_u_a, int stride_u_a,
                const uint8* src_v_a, int stride_v_a,
                const uint8* src_y_b, int stride_y_b,
                const uint8* src_u_b, int stride_u_b,
                const uint8* src_v_b, int stride_v_b,
                int width, int height);

LIBYUV_API
double CalcFrameSsim(const uint8* src_a, int stride_a,
                     const uint8* src_b, int stride_b,
                     int width, int height);

LIBYUV_API
double I420Ssim(const uint8* src_y_a, int stride_y_a,
                const uint8* src_u_a, int stride_u_a,
                const uint8* src_v_a, int stride_v_a,
                const uint8* src_y_b, int stride_y_b,
                const uint8* src_u_b, int stride_u_b,
                const uint8* src_v_b, int stride_v_b,
                int width, int height);

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // INCLUDE_LIBYUV_COMPARE_H_  NOLINT
