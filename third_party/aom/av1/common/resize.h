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

#ifndef AV1_ENCODER_RESIZE_H_
#define AV1_ENCODER_RESIZE_H_

#include <stdio.h>
#include "aom/aom_integer.h"
#include "av1/common/onyxc_int.h"

#ifdef __cplusplus
extern "C" {
#endif

void av1_resize_plane(const uint8_t *const input, int height, int width,
                      int in_stride, uint8_t *output, int height2, int width2,
                      int out_stride);
void av1_resize_frame420(const uint8_t *const y, int y_stride,
                         const uint8_t *const u, const uint8_t *const v,
                         int uv_stride, int height, int width, uint8_t *oy,
                         int oy_stride, uint8_t *ou, uint8_t *ov,
                         int ouv_stride, int oheight, int owidth);
void av1_resize_frame422(const uint8_t *const y, int y_stride,
                         const uint8_t *const u, const uint8_t *const v,
                         int uv_stride, int height, int width, uint8_t *oy,
                         int oy_stride, uint8_t *ou, uint8_t *ov,
                         int ouv_stride, int oheight, int owidth);
void av1_resize_frame444(const uint8_t *const y, int y_stride,
                         const uint8_t *const u, const uint8_t *const v,
                         int uv_stride, int height, int width, uint8_t *oy,
                         int oy_stride, uint8_t *ou, uint8_t *ov,
                         int ouv_stride, int oheight, int owidth);

#if CONFIG_HIGHBITDEPTH
void av1_highbd_resize_plane(const uint8_t *const input, int height, int width,
                             int in_stride, uint8_t *output, int height2,
                             int width2, int out_stride, int bd);
void av1_highbd_resize_frame420(const uint8_t *const y, int y_stride,
                                const uint8_t *const u, const uint8_t *const v,
                                int uv_stride, int height, int width,
                                uint8_t *oy, int oy_stride, uint8_t *ou,
                                uint8_t *ov, int ouv_stride, int oheight,
                                int owidth, int bd);
void av1_highbd_resize_frame422(const uint8_t *const y, int y_stride,
                                const uint8_t *const u, const uint8_t *const v,
                                int uv_stride, int height, int width,
                                uint8_t *oy, int oy_stride, uint8_t *ou,
                                uint8_t *ov, int ouv_stride, int oheight,
                                int owidth, int bd);
void av1_highbd_resize_frame444(const uint8_t *const y, int y_stride,
                                const uint8_t *const u, const uint8_t *const v,
                                int uv_stride, int height, int width,
                                uint8_t *oy, int oy_stride, uint8_t *ou,
                                uint8_t *ov, int ouv_stride, int oheight,
                                int owidth, int bd);
#endif  // CONFIG_HIGHBITDEPTH

#if CONFIG_HIGHBITDEPTH
void av1_resize_and_extend_frame(const YV12_BUFFER_CONFIG *src,
                                 YV12_BUFFER_CONFIG *dst, int bd);
#else
void av1_resize_and_extend_frame(const YV12_BUFFER_CONFIG *src,
                                 YV12_BUFFER_CONFIG *dst);
#endif  // CONFIG_HIGHBITDEPTH

#if CONFIG_FRAME_SUPERRES
#if CONFIG_HIGHBITDEPTH
void av1_upscale_normative_and_extend_frame(const YV12_BUFFER_CONFIG *src,
                                            YV12_BUFFER_CONFIG *dst, int bd);
#else
void av1_upscale_normative_and_extend_frame(const YV12_BUFFER_CONFIG *src,
                                            YV12_BUFFER_CONFIG *dst);
#endif  // CONFIG_HIGHBITDEPTH
#endif  // CONFIG_FRAME_SUPERRES

YV12_BUFFER_CONFIG *av1_scale_if_required(AV1_COMMON *cm,
                                          YV12_BUFFER_CONFIG *unscaled,
                                          YV12_BUFFER_CONFIG *scaled);

// Calculates the scaled dimensions from the given original dimensions and the
// resize scale denominator.
void av1_calculate_scaled_size(int *width, int *height, int resize_denom);

#if CONFIG_FRAME_SUPERRES
// Similar to above, but calculates scaled dimensions after superres from the
// given original dimensions and superres scale denominator.
void av1_calculate_scaled_superres_size(int *width, int *height,
                                        int superres_denom);

// Inverse of av1_calculate_scaled_superres_size() above: calculates the
// original dimensions from the given scaled dimensions and the scale
// denominator.
void av1_calculate_unscaled_superres_size(int *width, int *height, int denom);

void av1_superres_upscale(AV1_COMMON *cm, BufferPool *const pool);

// Returns 1 if a superres upscaled frame is unscaled and 0 otherwise.
static INLINE int av1_superres_unscaled(const AV1_COMMON *cm) {
  return (cm->superres_scale_denominator == SCALE_NUMERATOR);
}
#endif  // CONFIG_FRAME_SUPERRES

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_RESIZE_H_
