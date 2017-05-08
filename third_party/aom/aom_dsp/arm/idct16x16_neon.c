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

#include "aom_dsp/aom_dsp_common.h"

void aom_idct16x16_256_add_neon_pass1(const int16_t *input, int16_t *output,
                                      int output_stride);
void aom_idct16x16_256_add_neon_pass2(const int16_t *src, int16_t *output,
                                      int16_t *pass1Output, int16_t skip_adding,
                                      uint8_t *dest, int dest_stride);
void aom_idct16x16_10_add_neon_pass1(const int16_t *input, int16_t *output,
                                     int output_stride);
void aom_idct16x16_10_add_neon_pass2(const int16_t *src, int16_t *output,
                                     int16_t *pass1Output, int16_t skip_adding,
                                     uint8_t *dest, int dest_stride);

#if HAVE_NEON_ASM
/* For ARM NEON, d8-d15 are callee-saved registers, and need to be saved. */
extern void aom_push_neon(int64_t *store);
extern void aom_pop_neon(int64_t *store);
#endif  // HAVE_NEON_ASM

void aom_idct16x16_256_add_neon(const int16_t *input, uint8_t *dest,
                                int dest_stride) {
#if HAVE_NEON_ASM
  int64_t store_reg[8];
#endif
  int16_t pass1_output[16 * 16] = { 0 };
  int16_t row_idct_output[16 * 16] = { 0 };

#if HAVE_NEON_ASM
  // save d8-d15 register values.
  aom_push_neon(store_reg);
#endif

  /* Parallel idct on the upper 8 rows */
  // First pass processes even elements 0, 2, 4, 6, 8, 10, 12, 14 and save the
  // stage 6 result in pass1_output.
  aom_idct16x16_256_add_neon_pass1(input, pass1_output, 8);

  // Second pass processes odd elements 1, 3, 5, 7, 9, 11, 13, 15 and combines
  // with result in pass1(pass1_output) to calculate final result in stage 7
  // which will be saved into row_idct_output.
  aom_idct16x16_256_add_neon_pass2(input + 1, row_idct_output, pass1_output, 0,
                                   dest, dest_stride);

  /* Parallel idct on the lower 8 rows */
  // First pass processes even elements 0, 2, 4, 6, 8, 10, 12, 14 and save the
  // stage 6 result in pass1_output.
  aom_idct16x16_256_add_neon_pass1(input + 8 * 16, pass1_output, 8);

  // Second pass processes odd elements 1, 3, 5, 7, 9, 11, 13, 15 and combines
  // with result in pass1(pass1_output) to calculate final result in stage 7
  // which will be saved into row_idct_output.
  aom_idct16x16_256_add_neon_pass2(input + 8 * 16 + 1, row_idct_output + 8,
                                   pass1_output, 0, dest, dest_stride);

  /* Parallel idct on the left 8 columns */
  // First pass processes even elements 0, 2, 4, 6, 8, 10, 12, 14 and save the
  // stage 6 result in pass1_output.
  aom_idct16x16_256_add_neon_pass1(row_idct_output, pass1_output, 8);

  // Second pass processes odd elements 1, 3, 5, 7, 9, 11, 13, 15 and combines
  // with result in pass1(pass1_output) to calculate final result in stage 7.
  // Then add the result to the destination data.
  aom_idct16x16_256_add_neon_pass2(row_idct_output + 1, row_idct_output,
                                   pass1_output, 1, dest, dest_stride);

  /* Parallel idct on the right 8 columns */
  // First pass processes even elements 0, 2, 4, 6, 8, 10, 12, 14 and save the
  // stage 6 result in pass1_output.
  aom_idct16x16_256_add_neon_pass1(row_idct_output + 8 * 16, pass1_output, 8);

  // Second pass processes odd elements 1, 3, 5, 7, 9, 11, 13, 15 and combines
  // with result in pass1(pass1_output) to calculate final result in stage 7.
  // Then add the result to the destination data.
  aom_idct16x16_256_add_neon_pass2(row_idct_output + 8 * 16 + 1,
                                   row_idct_output + 8, pass1_output, 1,
                                   dest + 8, dest_stride);

#if HAVE_NEON_ASM
  // restore d8-d15 register values.
  aom_pop_neon(store_reg);
#endif

  return;
}

void aom_idct16x16_10_add_neon(const int16_t *input, uint8_t *dest,
                               int dest_stride) {
#if HAVE_NEON_ASM
  int64_t store_reg[8];
#endif
  int16_t pass1_output[16 * 16] = { 0 };
  int16_t row_idct_output[16 * 16] = { 0 };

#if HAVE_NEON_ASM
  // save d8-d15 register values.
  aom_push_neon(store_reg);
#endif

  /* Parallel idct on the upper 8 rows */
  // First pass processes even elements 0, 2, 4, 6, 8, 10, 12, 14 and save the
  // stage 6 result in pass1_output.
  aom_idct16x16_10_add_neon_pass1(input, pass1_output, 8);

  // Second pass processes odd elements 1, 3, 5, 7, 9, 11, 13, 15 and combines
  // with result in pass1(pass1_output) to calculate final result in stage 7
  // which will be saved into row_idct_output.
  aom_idct16x16_10_add_neon_pass2(input + 1, row_idct_output, pass1_output, 0,
                                  dest, dest_stride);

  /* Skip Parallel idct on the lower 8 rows as they are all 0s */

  /* Parallel idct on the left 8 columns */
  // First pass processes even elements 0, 2, 4, 6, 8, 10, 12, 14 and save the
  // stage 6 result in pass1_output.
  aom_idct16x16_256_add_neon_pass1(row_idct_output, pass1_output, 8);

  // Second pass processes odd elements 1, 3, 5, 7, 9, 11, 13, 15 and combines
  // with result in pass1(pass1_output) to calculate final result in stage 7.
  // Then add the result to the destination data.
  aom_idct16x16_256_add_neon_pass2(row_idct_output + 1, row_idct_output,
                                   pass1_output, 1, dest, dest_stride);

  /* Parallel idct on the right 8 columns */
  // First pass processes even elements 0, 2, 4, 6, 8, 10, 12, 14 and save the
  // stage 6 result in pass1_output.
  aom_idct16x16_256_add_neon_pass1(row_idct_output + 8 * 16, pass1_output, 8);

  // Second pass processes odd elements 1, 3, 5, 7, 9, 11, 13, 15 and combines
  // with result in pass1(pass1_output) to calculate final result in stage 7.
  // Then add the result to the destination data.
  aom_idct16x16_256_add_neon_pass2(row_idct_output + 8 * 16 + 1,
                                   row_idct_output + 8, pass1_output, 1,
                                   dest + 8, dest_stride);

#if HAVE_NEON_ASM
  // restore d8-d15 register values.
  aom_pop_neon(store_reg);
#endif

  return;
}
