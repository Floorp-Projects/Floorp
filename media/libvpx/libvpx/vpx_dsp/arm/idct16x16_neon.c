/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "./vpx_dsp_rtcd.h"
#include "vpx_dsp/vpx_dsp_common.h"

void vpx_idct16x16_256_add_neon_pass1(const int16_t *input, int16_t *output);
void vpx_idct16x16_256_add_neon_pass2(const int16_t *src, int16_t *output,
                                      int16_t *pass1_output,
                                      int16_t skip_adding, uint8_t *dest,
                                      int stride);
#if CONFIG_VP9_HIGHBITDEPTH
void vpx_idct16x16_256_add_neon_pass1_tran_low(const tran_low_t *input,
                                               int16_t *output);
void vpx_idct16x16_256_add_neon_pass2_tran_low(const tran_low_t *src,
                                               int16_t *output,
                                               int16_t *pass1_output,
                                               int16_t skip_adding,
                                               uint8_t *dest, int stride);
#else
#define vpx_idct16x16_256_add_neon_pass1_tran_low \
  vpx_idct16x16_256_add_neon_pass1
#define vpx_idct16x16_256_add_neon_pass2_tran_low \
  vpx_idct16x16_256_add_neon_pass2
#endif

void vpx_idct16x16_10_add_neon_pass1(const tran_low_t *input, int16_t *output);
void vpx_idct16x16_10_add_neon_pass2(const tran_low_t *src, int16_t *output,
                                     int16_t *pass1_output);

#if HAVE_NEON_ASM
/* For ARM NEON, d8-d15 are callee-saved registers, and need to be saved. */
extern void vpx_push_neon(int64_t *store);
extern void vpx_pop_neon(int64_t *store);
#endif  // HAVE_NEON_ASM

void vpx_idct16x16_256_add_neon(const tran_low_t *input, uint8_t *dest,
                                int stride) {
#if HAVE_NEON_ASM
  int64_t store_reg[8];
#endif
  int16_t pass1_output[16 * 16] = { 0 };
  int16_t row_idct_output[16 * 16] = { 0 };

#if HAVE_NEON_ASM
  // save d8-d15 register values.
  vpx_push_neon(store_reg);
#endif

  /* Parallel idct on the upper 8 rows */
  // First pass processes even elements 0, 2, 4, 6, 8, 10, 12, 14 and save the
  // stage 6 result in pass1_output.
  vpx_idct16x16_256_add_neon_pass1_tran_low(input, pass1_output);

  // Second pass processes odd elements 1, 3, 5, 7, 9, 11, 13, 15 and combines
  // with result in pass1(pass1_output) to calculate final result in stage 7
  // which will be saved into row_idct_output.
  vpx_idct16x16_256_add_neon_pass2_tran_low(input + 1, row_idct_output,
                                            pass1_output, 0, dest, stride);

  /* Parallel idct on the lower 8 rows */
  // First pass processes even elements 0, 2, 4, 6, 8, 10, 12, 14 and save the
  // stage 6 result in pass1_output.
  vpx_idct16x16_256_add_neon_pass1_tran_low(input + 8 * 16, pass1_output);

  // Second pass processes odd elements 1, 3, 5, 7, 9, 11, 13, 15 and combines
  // with result in pass1(pass1_output) to calculate final result in stage 7
  // which will be saved into row_idct_output.
  vpx_idct16x16_256_add_neon_pass2_tran_low(
      input + 8 * 16 + 1, row_idct_output + 8, pass1_output, 0, dest, stride);

  /* Parallel idct on the left 8 columns */
  // First pass processes even elements 0, 2, 4, 6, 8, 10, 12, 14 and save the
  // stage 6 result in pass1_output.
  vpx_idct16x16_256_add_neon_pass1(row_idct_output, pass1_output);

  // Second pass processes odd elements 1, 3, 5, 7, 9, 11, 13, 15 and combines
  // with result in pass1(pass1_output) to calculate final result in stage 7.
  // Then add the result to the destination data.
  vpx_idct16x16_256_add_neon_pass2(row_idct_output + 1, row_idct_output,
                                   pass1_output, 1, dest, stride);

  /* Parallel idct on the right 8 columns */
  // First pass processes even elements 0, 2, 4, 6, 8, 10, 12, 14 and save the
  // stage 6 result in pass1_output.
  vpx_idct16x16_256_add_neon_pass1(row_idct_output + 8 * 16, pass1_output);

  // Second pass processes odd elements 1, 3, 5, 7, 9, 11, 13, 15 and combines
  // with result in pass1(pass1_output) to calculate final result in stage 7.
  // Then add the result to the destination data.
  vpx_idct16x16_256_add_neon_pass2(row_idct_output + 8 * 16 + 1,
                                   row_idct_output + 8, pass1_output, 1,
                                   dest + 8, stride);

#if HAVE_NEON_ASM
  // restore d8-d15 register values.
  vpx_pop_neon(store_reg);
#endif
}

void vpx_idct16x16_10_add_neon(const tran_low_t *input, uint8_t *dest,
                               int stride) {
#if HAVE_NEON_ASM
  int64_t store_reg[8];
#endif
  int16_t pass1_output[16 * 16] = { 0 };
  int16_t row_idct_output[16 * 16] = { 0 };

#if HAVE_NEON_ASM
  // save d8-d15 register values.
  vpx_push_neon(store_reg);
#endif

  /* Parallel idct on the upper 8 rows */
  // First pass processes even elements 0, 2, 4, 6, 8, 10, 12, 14 and save the
  // stage 6 result in pass1_output.
  vpx_idct16x16_10_add_neon_pass1(input, pass1_output);

  // Second pass processes odd elements 1, 3, 5, 7, 9, 11, 13, 15 and combines
  // with result in pass1(pass1_output) to calculate final result in stage 7
  // which will be saved into row_idct_output.
  vpx_idct16x16_10_add_neon_pass2(input + 1, row_idct_output, pass1_output);

  /* Skip Parallel idct on the lower 8 rows as they are all 0s */

  /* Parallel idct on the left 8 columns */
  // First pass processes even elements 0, 2, 4, 6, 8, 10, 12, 14 and save the
  // stage 6 result in pass1_output.
  vpx_idct16x16_256_add_neon_pass1(row_idct_output, pass1_output);

  // Second pass processes odd elements 1, 3, 5, 7, 9, 11, 13, 15 and combines
  // with result in pass1(pass1_output) to calculate final result in stage 7.
  // Then add the result to the destination data.
  vpx_idct16x16_256_add_neon_pass2(row_idct_output + 1, row_idct_output,
                                   pass1_output, 1, dest, stride);

  /* Parallel idct on the right 8 columns */
  // First pass processes even elements 0, 2, 4, 6, 8, 10, 12, 14 and save the
  // stage 6 result in pass1_output.
  vpx_idct16x16_256_add_neon_pass1(row_idct_output + 8 * 16, pass1_output);

  // Second pass processes odd elements 1, 3, 5, 7, 9, 11, 13, 15 and combines
  // with result in pass1(pass1_output) to calculate final result in stage 7.
  // Then add the result to the destination data.
  vpx_idct16x16_256_add_neon_pass2(row_idct_output + 8 * 16 + 1,
                                   row_idct_output + 8, pass1_output, 1,
                                   dest + 8, stride);

#if HAVE_NEON_ASM
  // restore d8-d15 register values.
  vpx_pop_neon(store_reg);
#endif
}
