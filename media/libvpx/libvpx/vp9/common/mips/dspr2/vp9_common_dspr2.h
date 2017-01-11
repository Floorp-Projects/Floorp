/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_COMMON_MIPS_DSPR2_VP9_COMMON_DSPR2_H_
#define VP9_COMMON_MIPS_DSPR2_VP9_COMMON_DSPR2_H_

#include <assert.h>

#include "./vpx_config.h"
#include "vpx/vpx_integer.h"
#include "vp9/common/vp9_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#if HAVE_DSPR2
#define CROP_WIDTH 512
extern uint8_t *vp9_ff_cropTbl;

#define DCT_CONST_ROUND_SHIFT_TWICE_COSPI_16_64(input)                    ({   \
                                                                               \
  int32_t tmp, out;                                                            \
  int     dct_cost_rounding = DCT_CONST_ROUNDING;                              \
  int     in = input;                                                          \
                                                                               \
  __asm__ __volatile__ (                                                       \
      /* out = dct_const_round_shift(input_dc * cospi_16_64); */               \
      "mtlo     %[dct_cost_rounding],   $ac1                              \n\t"\
      "mthi     $zero,                  $ac1                              \n\t"\
      "madd     $ac1,                   %[in],            %[cospi_16_64]  \n\t"\
      "extp     %[tmp],                 $ac1,             31              \n\t"\
                                                                               \
      /* out = dct_const_round_shift(out * cospi_16_64); */                    \
      "mtlo     %[dct_cost_rounding],   $ac2                              \n\t"\
      "mthi     $zero,                  $ac2                              \n\t"\
      "madd     $ac2,                   %[tmp],           %[cospi_16_64]  \n\t"\
      "extp     %[out],                 $ac2,             31              \n\t"\
                                                                               \
      : [tmp] "=&r" (tmp), [out] "=r" (out)                                    \
      : [in] "r" (in),                                                         \
        [dct_cost_rounding] "r" (dct_cost_rounding),                           \
        [cospi_16_64] "r" (cospi_16_64)                                        \
   );                                                                          \
  out;                                                                    })

static INLINE void vp9_prefetch_load(const unsigned char *src) {
  __asm__ __volatile__ (
      "pref   0,  0(%[src])   \n\t"
      :
      : [src] "r" (src)
  );
}

/* prefetch data for store */
static INLINE void vp9_prefetch_store(unsigned char *dst) {
  __asm__ __volatile__ (
      "pref   1,  0(%[dst])   \n\t"
      :
      : [dst] "r" (dst)
  );
}

static INLINE void vp9_prefetch_load_streamed(const unsigned char *src) {
  __asm__ __volatile__ (
      "pref   4,  0(%[src])   \n\t"
      :
      : [src] "r" (src)
  );
}

/* prefetch data for store */
static INLINE void vp9_prefetch_store_streamed(unsigned char *dst) {
  __asm__ __volatile__ (
      "pref   5,  0(%[dst])   \n\t"
      :
      : [dst] "r" (dst)
  );
}

void vp9_idct32_cols_add_blk_dspr2(int16_t *input, uint8_t *dest,
                                   int dest_stride);

void vp9_convolve2_horiz_dspr2(const uint8_t *src, ptrdiff_t src_stride,
                               uint8_t *dst, ptrdiff_t dst_stride,
                               const int16_t *filter_x, int x_step_q4,
                               const int16_t *filter_y, int y_step_q4,
                               int w, int h);

void vp9_convolve2_avg_horiz_dspr2(const uint8_t *src, ptrdiff_t src_stride,
                                   uint8_t *dst, ptrdiff_t dst_stride,
                                   const int16_t *filter_x, int x_step_q4,
                                   const int16_t *filter_y, int y_step_q4,
                                   int w, int h);

void vp9_convolve2_avg_vert_dspr2(const uint8_t *src, ptrdiff_t src_stride,
                                  uint8_t *dst, ptrdiff_t dst_stride,
                                  const int16_t *filter_x, int x_step_q4,
                                  const int16_t *filter_y, int y_step_q4,
                                  int w, int h);

void vp9_convolve2_dspr2(const uint8_t *src, ptrdiff_t src_stride,
                         uint8_t *dst, ptrdiff_t dst_stride,
                         const int16_t *filter,
                         int w, int h);

void vp9_convolve2_vert_dspr2(const uint8_t *src, ptrdiff_t src_stride,
                              uint8_t *dst, ptrdiff_t dst_stride,
                              const int16_t *filter_x, int x_step_q4,
                              const int16_t *filter_y, int y_step_q4,
                              int w, int h);

#endif  // #if HAVE_DSPR2
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_COMMON_MIPS_DSPR2_VP9_COMMON_DSPR2_H_
