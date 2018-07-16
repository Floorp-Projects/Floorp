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
#ifndef AOM_DSP_AOM_CONVOLVE_H_
#define AOM_DSP_AOM_CONVOLVE_H_

#include "config/aom_config.h"

#include "aom/aom_integer.h"

#ifdef __cplusplus
extern "C" {
#endif

// Note: Fixed size intermediate buffers, place limits on parameters
// of some functions. 2d filtering proceeds in 2 steps:
//   (1) Interpolate horizontally into an intermediate buffer, temp.
//   (2) Interpolate temp vertically to derive the sub-pixel result.
// Deriving the maximum number of rows in the temp buffer (135):
// --Smallest scaling factor is x1/2 ==> y_step_q4 = 32 (Normative).
// --Largest block size is 64x64 pixels.
// --64 rows in the downscaled frame span a distance of (64 - 1) * 32 in the
//   original frame (in 1/16th pixel units).
// --Must round-up because block may be located at sub-pixel position.
// --Require an additional SUBPEL_TAPS rows for the 8-tap filter tails.
// --((64 - 1) * 32 + 15) >> 4 + 8 = 135.
// TODO(wtc): Update the above comment to explain the value 263 used in aom.
#define MAX_EXT_SIZE 263

#define EXTRAPREC_BITS 2
#define EXTRAPREC_CLAMP_LIMIT(bd) (1 << ((bd) + 1 + EXTRAPREC_BITS))

typedef void (*convolve_fn_t)(const uint8_t *src, ptrdiff_t src_stride,
                              uint8_t *dst, ptrdiff_t dst_stride,
                              const int16_t *filter_x, int x_step_q4,
                              const int16_t *filter_y, int y_step_q4, int w,
                              int h);

typedef void (*highbd_convolve_fn_t)(const uint8_t *src, ptrdiff_t src_stride,
                                     uint8_t *dst, ptrdiff_t dst_stride,
                                     const int16_t *filter_x, int x_step_q4,
                                     const int16_t *filter_y, int y_step_q4,
                                     int w, int h, int bd);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_DSP_AOM_CONVOLVE_H_
