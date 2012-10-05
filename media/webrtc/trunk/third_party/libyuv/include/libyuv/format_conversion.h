/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef INCLUDE_LIBYUV_FORMATCONVERSION_H_  // NOLINT
#define INCLUDE_LIBYUV_FORMATCONVERSION_H_

#include "libyuv/basic_types.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Convert Bayer RGB formats to I420.
int BayerBGGRToI420(const uint8* src_bayer, int src_stride_bayer,
                    uint8* dst_y, int dst_stride_y,
                    uint8* dst_u, int dst_stride_u,
                    uint8* dst_v, int dst_stride_v,
                    int width, int height);

int BayerGBRGToI420(const uint8* src_bayer, int src_stride_bayer,
                    uint8* dst_y, int dst_stride_y,
                    uint8* dst_u, int dst_stride_u,
                    uint8* dst_v, int dst_stride_v,
                    int width, int height);

int BayerGRBGToI420(const uint8* src_bayer, int src_stride_bayer,
                    uint8* dst_y, int dst_stride_y,
                    uint8* dst_u, int dst_stride_u,
                    uint8* dst_v, int dst_stride_v,
                    int width, int height);

int BayerRGGBToI420(const uint8* src_bayer, int src_stride_bayer,
                    uint8* dst_y, int dst_stride_y,
                    uint8* dst_u, int dst_stride_u,
                    uint8* dst_v, int dst_stride_v,
                    int width, int height);

// Temporary API mapper
#define BayerRGBToI420(b, bs, f, y, ys, u, us, v, vs, w, h) \
    BayerToI420(b, bs, y, ys, u, us, v, vs, w, h, f)

int BayerToI420(const uint8* src_bayer, int src_stride_bayer,
                uint8* dst_y, int dst_stride_y,
                uint8* dst_u, int dst_stride_u,
                uint8* dst_v, int dst_stride_v,
                int width, int height,
                uint32 src_fourcc_bayer);

// Convert I420 to Bayer RGB formats.
int I420ToBayerBGGR(const uint8* src_y, int src_stride_y,
                    const uint8* src_u, int src_stride_u,
                    const uint8* src_v, int src_stride_v,
                    uint8* dst_frame, int dst_stride_frame,
                    int width, int height);

int I420ToBayerGBRG(const uint8* src_y, int src_stride_y,
                    const uint8* src_u, int src_stride_u,
                    const uint8* src_v, int src_stride_v,
                    uint8* dst_frame, int dst_stride_frame,
                    int width, int height);

int I420ToBayerGRBG(const uint8* src_y, int src_stride_y,
                    const uint8* src_u, int src_stride_u,
                    const uint8* src_v, int src_stride_v,
                    uint8* dst_frame, int dst_stride_frame,
                    int width, int height);

int I420ToBayerRGGB(const uint8* src_y, int src_stride_y,
                    const uint8* src_u, int src_stride_u,
                    const uint8* src_v, int src_stride_v,
                    uint8* dst_frame, int dst_stride_frame,
                    int width, int height);

// Temporary API mapper
#define I420ToBayerRGB(y, ys, u, us, v, vs, b, bs, f, w, h) \
    I420ToBayer(y, ys, u, us, v, vs, b, bs, w, h, f)

int I420ToBayer(const uint8* src_y, int src_stride_y,
                const uint8* src_u, int src_stride_u,
                const uint8* src_v, int src_stride_v,
                uint8* dst_frame, int dst_stride_frame,
                int width, int height,
                uint32 dst_fourcc_bayer);

// Convert Bayer RGB formats to ARGB.
int BayerBGGRToARGB(const uint8* src_bayer, int src_stride_bayer,
                    uint8* dst_argb, int dst_stride_argb,
                    int width, int height);

int BayerGBRGToARGB(const uint8* src_bayer, int src_stride_bayer,
                    uint8* dst_argb, int dst_stride_argb,
                    int width, int height);

int BayerGRBGToARGB(const uint8* src_bayer, int src_stride_bayer,
                    uint8* dst_argb, int dst_stride_argb,
                    int width, int height);

int BayerRGGBToARGB(const uint8* src_bayer, int src_stride_bayer,
                    uint8* dst_argb, int dst_stride_argb,
                    int width, int height);

// Temporary API mapper
#define BayerRGBToARGB(b, bs, f, a, as, w, h) BayerToARGB(b, bs, a, as, w, h, f)

int BayerToARGB(const uint8* src_bayer, int src_stride_bayer,
                uint8* dst_argb, int dst_stride_argb,
                int width, int height,
                uint32 src_fourcc_bayer);

// Converts ARGB to Bayer RGB formats.
int ARGBToBayerBGGR(const uint8* src_argb, int src_stride_argb,
                    uint8* dst_bayer, int dst_stride_bayer,
                    int width, int height);

int ARGBToBayerGBRG(const uint8* src_argb, int src_stride_argb,
                    uint8* dst_bayer, int dst_stride_bayer,
                    int width, int height);

int ARGBToBayerGRBG(const uint8* src_argb, int src_stride_argb,
                    uint8* dst_bayer, int dst_stride_bayer,
                    int width, int height);

int ARGBToBayerRGGB(const uint8* src_argb, int src_stride_argb,
                    uint8* dst_bayer, int dst_stride_bayer,
                    int width, int height);

// Temporary API mapper
#define ARGBToBayerRGB(a, as, b, bs, f, w, h) ARGBToBayer(b, bs, a, as, w, h, f)

int ARGBToBayer(const uint8* src_argb, int src_stride_argb,
                uint8* dst_bayer, int dst_stride_bayer,
                int width, int height,
                uint32 dst_fourcc_bayer);

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // INCLUDE_LIBYUV_FORMATCONVERSION_H_  NOLINT
