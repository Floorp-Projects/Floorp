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

#include <stdlib.h>
#include "aom_dsp/inv_txfm.h"
#include "av1/common/av1_inv_txfm1d.h"
#if CONFIG_COEFFICIENT_RANGE_CHECKING

void range_check_func(int32_t stage, const int32_t *input, const int32_t *buf,
                      int32_t size, int8_t bit) {
  const int64_t maxValue = (1LL << (bit - 1)) - 1;
  const int64_t minValue = -(1LL << (bit - 1));

  int in_range = 1;

  for (int i = 0; i < size; ++i) {
    if (buf[i] < minValue || buf[i] > maxValue) {
      in_range = 0;
    }
  }

  if (!in_range) {
    fprintf(stderr, "Error: coeffs contain out-of-range values\n");
    fprintf(stderr, "stage: %d\n", stage);
    fprintf(stderr, "allowed range: [%" PRId64 ";%" PRId64 "]\n", minValue,
            maxValue);

    fprintf(stderr, "coeffs: ");

    fprintf(stderr, "[");
    for (int j = 0; j < size; j++) {
      if (j > 0) fprintf(stderr, ", ");
      fprintf(stderr, "%d", input[j]);
    }
    fprintf(stderr, "]\n");

    fprintf(stderr, "   buf: ");

    fprintf(stderr, "[");
    for (int j = 0; j < size; j++) {
      if (j > 0) fprintf(stderr, ", ");
      fprintf(stderr, "%d", buf[j]);
    }
    fprintf(stderr, "]\n\n");
  }

  assert(in_range);
}

#define range_check(stage, input, buf, size, bit) \
  range_check_func(stage, input, buf, size, bit)
#else
#define range_check(stage, input, buf, size, bit) \
  {                                               \
    (void)stage;                                  \
    (void)input;                                  \
    (void)buf;                                    \
    (void)size;                                   \
    (void)bit;                                    \
  }
#endif

// TODO(angiebird): Make 1-d txfm functions static
void av1_idct4_new(const int32_t *input, int32_t *output, const int8_t *cos_bit,
                   const int8_t *stage_range) {
  const int32_t size = 4;
  const int32_t *cospi;

  int32_t stage = 0;
  int32_t *bf0, *bf1;
  int32_t step[4];

  // stage 0;
  range_check(stage, input, input, size, stage_range[stage]);

  // stage 1;
  stage++;
  assert(output != input);
  bf1 = output;
  bf1[0] = input[0];
  bf1[1] = input[2];
  bf1[2] = input[1];
  bf1[3] = input[3];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 2
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = half_btf(cospi[32], bf0[0], cospi[32], bf0[1], cos_bit[stage]);
  bf1[1] = half_btf(cospi[32], bf0[0], -cospi[32], bf0[1], cos_bit[stage]);
  bf1[2] = half_btf(cospi[48], bf0[2], -cospi[16], bf0[3], cos_bit[stage]);
  bf1[3] = half_btf(cospi[16], bf0[2], cospi[48], bf0[3], cos_bit[stage]);
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 3
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0] + bf0[3];
  bf1[1] = bf0[1] + bf0[2];
  bf1[2] = bf0[1] - bf0[2];
  bf1[3] = bf0[0] - bf0[3];
  range_check(stage, input, bf1, size, stage_range[stage]);
}

void av1_idct8_new(const int32_t *input, int32_t *output, const int8_t *cos_bit,
                   const int8_t *stage_range) {
  const int32_t size = 8;
  const int32_t *cospi;

  int32_t stage = 0;
  int32_t *bf0, *bf1;
  int32_t step[8];

  // stage 0;
  range_check(stage, input, input, size, stage_range[stage]);

  // stage 1;
  stage++;
  assert(output != input);
  bf1 = output;
  bf1[0] = input[0];
  bf1[1] = input[4];
  bf1[2] = input[2];
  bf1[3] = input[6];
  bf1[4] = input[1];
  bf1[5] = input[5];
  bf1[6] = input[3];
  bf1[7] = input[7];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 2
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = half_btf(cospi[56], bf0[4], -cospi[8], bf0[7], cos_bit[stage]);
  bf1[5] = half_btf(cospi[24], bf0[5], -cospi[40], bf0[6], cos_bit[stage]);
  bf1[6] = half_btf(cospi[40], bf0[5], cospi[24], bf0[6], cos_bit[stage]);
  bf1[7] = half_btf(cospi[8], bf0[4], cospi[56], bf0[7], cos_bit[stage]);
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 3
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = step;
  bf1 = output;
  bf1[0] = half_btf(cospi[32], bf0[0], cospi[32], bf0[1], cos_bit[stage]);
  bf1[1] = half_btf(cospi[32], bf0[0], -cospi[32], bf0[1], cos_bit[stage]);
  bf1[2] = half_btf(cospi[48], bf0[2], -cospi[16], bf0[3], cos_bit[stage]);
  bf1[3] = half_btf(cospi[16], bf0[2], cospi[48], bf0[3], cos_bit[stage]);
  bf1[4] = bf0[4] + bf0[5];
  bf1[5] = bf0[4] - bf0[5];
  bf1[6] = -bf0[6] + bf0[7];
  bf1[7] = bf0[6] + bf0[7];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 4
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0] + bf0[3];
  bf1[1] = bf0[1] + bf0[2];
  bf1[2] = bf0[1] - bf0[2];
  bf1[3] = bf0[0] - bf0[3];
  bf1[4] = bf0[4];
  bf1[5] = half_btf(-cospi[32], bf0[5], cospi[32], bf0[6], cos_bit[stage]);
  bf1[6] = half_btf(cospi[32], bf0[5], cospi[32], bf0[6], cos_bit[stage]);
  bf1[7] = bf0[7];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 5
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0] + bf0[7];
  bf1[1] = bf0[1] + bf0[6];
  bf1[2] = bf0[2] + bf0[5];
  bf1[3] = bf0[3] + bf0[4];
  bf1[4] = bf0[3] - bf0[4];
  bf1[5] = bf0[2] - bf0[5];
  bf1[6] = bf0[1] - bf0[6];
  bf1[7] = bf0[0] - bf0[7];
  range_check(stage, input, bf1, size, stage_range[stage]);
}

void av1_idct16_new(const int32_t *input, int32_t *output,
                    const int8_t *cos_bit, const int8_t *stage_range) {
  const int32_t size = 16;
  const int32_t *cospi;

  int32_t stage = 0;
  int32_t *bf0, *bf1;
  int32_t step[16];

  // stage 0;
  range_check(stage, input, input, size, stage_range[stage]);

  // stage 1;
  stage++;
  assert(output != input);
  bf1 = output;
  bf1[0] = input[0];
  bf1[1] = input[8];
  bf1[2] = input[4];
  bf1[3] = input[12];
  bf1[4] = input[2];
  bf1[5] = input[10];
  bf1[6] = input[6];
  bf1[7] = input[14];
  bf1[8] = input[1];
  bf1[9] = input[9];
  bf1[10] = input[5];
  bf1[11] = input[13];
  bf1[12] = input[3];
  bf1[13] = input[11];
  bf1[14] = input[7];
  bf1[15] = input[15];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 2
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = bf0[4];
  bf1[5] = bf0[5];
  bf1[6] = bf0[6];
  bf1[7] = bf0[7];
  bf1[8] = half_btf(cospi[60], bf0[8], -cospi[4], bf0[15], cos_bit[stage]);
  bf1[9] = half_btf(cospi[28], bf0[9], -cospi[36], bf0[14], cos_bit[stage]);
  bf1[10] = half_btf(cospi[44], bf0[10], -cospi[20], bf0[13], cos_bit[stage]);
  bf1[11] = half_btf(cospi[12], bf0[11], -cospi[52], bf0[12], cos_bit[stage]);
  bf1[12] = half_btf(cospi[52], bf0[11], cospi[12], bf0[12], cos_bit[stage]);
  bf1[13] = half_btf(cospi[20], bf0[10], cospi[44], bf0[13], cos_bit[stage]);
  bf1[14] = half_btf(cospi[36], bf0[9], cospi[28], bf0[14], cos_bit[stage]);
  bf1[15] = half_btf(cospi[4], bf0[8], cospi[60], bf0[15], cos_bit[stage]);
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 3
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = half_btf(cospi[56], bf0[4], -cospi[8], bf0[7], cos_bit[stage]);
  bf1[5] = half_btf(cospi[24], bf0[5], -cospi[40], bf0[6], cos_bit[stage]);
  bf1[6] = half_btf(cospi[40], bf0[5], cospi[24], bf0[6], cos_bit[stage]);
  bf1[7] = half_btf(cospi[8], bf0[4], cospi[56], bf0[7], cos_bit[stage]);
  bf1[8] = bf0[8] + bf0[9];
  bf1[9] = bf0[8] - bf0[9];
  bf1[10] = -bf0[10] + bf0[11];
  bf1[11] = bf0[10] + bf0[11];
  bf1[12] = bf0[12] + bf0[13];
  bf1[13] = bf0[12] - bf0[13];
  bf1[14] = -bf0[14] + bf0[15];
  bf1[15] = bf0[14] + bf0[15];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 4
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = half_btf(cospi[32], bf0[0], cospi[32], bf0[1], cos_bit[stage]);
  bf1[1] = half_btf(cospi[32], bf0[0], -cospi[32], bf0[1], cos_bit[stage]);
  bf1[2] = half_btf(cospi[48], bf0[2], -cospi[16], bf0[3], cos_bit[stage]);
  bf1[3] = half_btf(cospi[16], bf0[2], cospi[48], bf0[3], cos_bit[stage]);
  bf1[4] = bf0[4] + bf0[5];
  bf1[5] = bf0[4] - bf0[5];
  bf1[6] = -bf0[6] + bf0[7];
  bf1[7] = bf0[6] + bf0[7];
  bf1[8] = bf0[8];
  bf1[9] = half_btf(-cospi[16], bf0[9], cospi[48], bf0[14], cos_bit[stage]);
  bf1[10] = half_btf(-cospi[48], bf0[10], -cospi[16], bf0[13], cos_bit[stage]);
  bf1[11] = bf0[11];
  bf1[12] = bf0[12];
  bf1[13] = half_btf(-cospi[16], bf0[10], cospi[48], bf0[13], cos_bit[stage]);
  bf1[14] = half_btf(cospi[48], bf0[9], cospi[16], bf0[14], cos_bit[stage]);
  bf1[15] = bf0[15];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 5
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0] + bf0[3];
  bf1[1] = bf0[1] + bf0[2];
  bf1[2] = bf0[1] - bf0[2];
  bf1[3] = bf0[0] - bf0[3];
  bf1[4] = bf0[4];
  bf1[5] = half_btf(-cospi[32], bf0[5], cospi[32], bf0[6], cos_bit[stage]);
  bf1[6] = half_btf(cospi[32], bf0[5], cospi[32], bf0[6], cos_bit[stage]);
  bf1[7] = bf0[7];
  bf1[8] = bf0[8] + bf0[11];
  bf1[9] = bf0[9] + bf0[10];
  bf1[10] = bf0[9] - bf0[10];
  bf1[11] = bf0[8] - bf0[11];
  bf1[12] = -bf0[12] + bf0[15];
  bf1[13] = -bf0[13] + bf0[14];
  bf1[14] = bf0[13] + bf0[14];
  bf1[15] = bf0[12] + bf0[15];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 6
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0] + bf0[7];
  bf1[1] = bf0[1] + bf0[6];
  bf1[2] = bf0[2] + bf0[5];
  bf1[3] = bf0[3] + bf0[4];
  bf1[4] = bf0[3] - bf0[4];
  bf1[5] = bf0[2] - bf0[5];
  bf1[6] = bf0[1] - bf0[6];
  bf1[7] = bf0[0] - bf0[7];
  bf1[8] = bf0[8];
  bf1[9] = bf0[9];
  bf1[10] = half_btf(-cospi[32], bf0[10], cospi[32], bf0[13], cos_bit[stage]);
  bf1[11] = half_btf(-cospi[32], bf0[11], cospi[32], bf0[12], cos_bit[stage]);
  bf1[12] = half_btf(cospi[32], bf0[11], cospi[32], bf0[12], cos_bit[stage]);
  bf1[13] = half_btf(cospi[32], bf0[10], cospi[32], bf0[13], cos_bit[stage]);
  bf1[14] = bf0[14];
  bf1[15] = bf0[15];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 7
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0] + bf0[15];
  bf1[1] = bf0[1] + bf0[14];
  bf1[2] = bf0[2] + bf0[13];
  bf1[3] = bf0[3] + bf0[12];
  bf1[4] = bf0[4] + bf0[11];
  bf1[5] = bf0[5] + bf0[10];
  bf1[6] = bf0[6] + bf0[9];
  bf1[7] = bf0[7] + bf0[8];
  bf1[8] = bf0[7] - bf0[8];
  bf1[9] = bf0[6] - bf0[9];
  bf1[10] = bf0[5] - bf0[10];
  bf1[11] = bf0[4] - bf0[11];
  bf1[12] = bf0[3] - bf0[12];
  bf1[13] = bf0[2] - bf0[13];
  bf1[14] = bf0[1] - bf0[14];
  bf1[15] = bf0[0] - bf0[15];
  range_check(stage, input, bf1, size, stage_range[stage]);
}

void av1_idct32_new(const int32_t *input, int32_t *output,
                    const int8_t *cos_bit, const int8_t *stage_range) {
  const int32_t size = 32;
  const int32_t *cospi;

  int32_t stage = 0;
  int32_t *bf0, *bf1;
  int32_t step[32];

  // stage 0;
  range_check(stage, input, input, size, stage_range[stage]);

  // stage 1;
  stage++;
  assert(output != input);
  bf1 = output;
  bf1[0] = input[0];
  bf1[1] = input[16];
  bf1[2] = input[8];
  bf1[3] = input[24];
  bf1[4] = input[4];
  bf1[5] = input[20];
  bf1[6] = input[12];
  bf1[7] = input[28];
  bf1[8] = input[2];
  bf1[9] = input[18];
  bf1[10] = input[10];
  bf1[11] = input[26];
  bf1[12] = input[6];
  bf1[13] = input[22];
  bf1[14] = input[14];
  bf1[15] = input[30];
  bf1[16] = input[1];
  bf1[17] = input[17];
  bf1[18] = input[9];
  bf1[19] = input[25];
  bf1[20] = input[5];
  bf1[21] = input[21];
  bf1[22] = input[13];
  bf1[23] = input[29];
  bf1[24] = input[3];
  bf1[25] = input[19];
  bf1[26] = input[11];
  bf1[27] = input[27];
  bf1[28] = input[7];
  bf1[29] = input[23];
  bf1[30] = input[15];
  bf1[31] = input[31];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 2
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = bf0[4];
  bf1[5] = bf0[5];
  bf1[6] = bf0[6];
  bf1[7] = bf0[7];
  bf1[8] = bf0[8];
  bf1[9] = bf0[9];
  bf1[10] = bf0[10];
  bf1[11] = bf0[11];
  bf1[12] = bf0[12];
  bf1[13] = bf0[13];
  bf1[14] = bf0[14];
  bf1[15] = bf0[15];
  bf1[16] = half_btf(cospi[62], bf0[16], -cospi[2], bf0[31], cos_bit[stage]);
  bf1[17] = half_btf(cospi[30], bf0[17], -cospi[34], bf0[30], cos_bit[stage]);
  bf1[18] = half_btf(cospi[46], bf0[18], -cospi[18], bf0[29], cos_bit[stage]);
  bf1[19] = half_btf(cospi[14], bf0[19], -cospi[50], bf0[28], cos_bit[stage]);
  bf1[20] = half_btf(cospi[54], bf0[20], -cospi[10], bf0[27], cos_bit[stage]);
  bf1[21] = half_btf(cospi[22], bf0[21], -cospi[42], bf0[26], cos_bit[stage]);
  bf1[22] = half_btf(cospi[38], bf0[22], -cospi[26], bf0[25], cos_bit[stage]);
  bf1[23] = half_btf(cospi[6], bf0[23], -cospi[58], bf0[24], cos_bit[stage]);
  bf1[24] = half_btf(cospi[58], bf0[23], cospi[6], bf0[24], cos_bit[stage]);
  bf1[25] = half_btf(cospi[26], bf0[22], cospi[38], bf0[25], cos_bit[stage]);
  bf1[26] = half_btf(cospi[42], bf0[21], cospi[22], bf0[26], cos_bit[stage]);
  bf1[27] = half_btf(cospi[10], bf0[20], cospi[54], bf0[27], cos_bit[stage]);
  bf1[28] = half_btf(cospi[50], bf0[19], cospi[14], bf0[28], cos_bit[stage]);
  bf1[29] = half_btf(cospi[18], bf0[18], cospi[46], bf0[29], cos_bit[stage]);
  bf1[30] = half_btf(cospi[34], bf0[17], cospi[30], bf0[30], cos_bit[stage]);
  bf1[31] = half_btf(cospi[2], bf0[16], cospi[62], bf0[31], cos_bit[stage]);
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 3
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = bf0[4];
  bf1[5] = bf0[5];
  bf1[6] = bf0[6];
  bf1[7] = bf0[7];
  bf1[8] = half_btf(cospi[60], bf0[8], -cospi[4], bf0[15], cos_bit[stage]);
  bf1[9] = half_btf(cospi[28], bf0[9], -cospi[36], bf0[14], cos_bit[stage]);
  bf1[10] = half_btf(cospi[44], bf0[10], -cospi[20], bf0[13], cos_bit[stage]);
  bf1[11] = half_btf(cospi[12], bf0[11], -cospi[52], bf0[12], cos_bit[stage]);
  bf1[12] = half_btf(cospi[52], bf0[11], cospi[12], bf0[12], cos_bit[stage]);
  bf1[13] = half_btf(cospi[20], bf0[10], cospi[44], bf0[13], cos_bit[stage]);
  bf1[14] = half_btf(cospi[36], bf0[9], cospi[28], bf0[14], cos_bit[stage]);
  bf1[15] = half_btf(cospi[4], bf0[8], cospi[60], bf0[15], cos_bit[stage]);
  bf1[16] = bf0[16] + bf0[17];
  bf1[17] = bf0[16] - bf0[17];
  bf1[18] = -bf0[18] + bf0[19];
  bf1[19] = bf0[18] + bf0[19];
  bf1[20] = bf0[20] + bf0[21];
  bf1[21] = bf0[20] - bf0[21];
  bf1[22] = -bf0[22] + bf0[23];
  bf1[23] = bf0[22] + bf0[23];
  bf1[24] = bf0[24] + bf0[25];
  bf1[25] = bf0[24] - bf0[25];
  bf1[26] = -bf0[26] + bf0[27];
  bf1[27] = bf0[26] + bf0[27];
  bf1[28] = bf0[28] + bf0[29];
  bf1[29] = bf0[28] - bf0[29];
  bf1[30] = -bf0[30] + bf0[31];
  bf1[31] = bf0[30] + bf0[31];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 4
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = half_btf(cospi[56], bf0[4], -cospi[8], bf0[7], cos_bit[stage]);
  bf1[5] = half_btf(cospi[24], bf0[5], -cospi[40], bf0[6], cos_bit[stage]);
  bf1[6] = half_btf(cospi[40], bf0[5], cospi[24], bf0[6], cos_bit[stage]);
  bf1[7] = half_btf(cospi[8], bf0[4], cospi[56], bf0[7], cos_bit[stage]);
  bf1[8] = bf0[8] + bf0[9];
  bf1[9] = bf0[8] - bf0[9];
  bf1[10] = -bf0[10] + bf0[11];
  bf1[11] = bf0[10] + bf0[11];
  bf1[12] = bf0[12] + bf0[13];
  bf1[13] = bf0[12] - bf0[13];
  bf1[14] = -bf0[14] + bf0[15];
  bf1[15] = bf0[14] + bf0[15];
  bf1[16] = bf0[16];
  bf1[17] = half_btf(-cospi[8], bf0[17], cospi[56], bf0[30], cos_bit[stage]);
  bf1[18] = half_btf(-cospi[56], bf0[18], -cospi[8], bf0[29], cos_bit[stage]);
  bf1[19] = bf0[19];
  bf1[20] = bf0[20];
  bf1[21] = half_btf(-cospi[40], bf0[21], cospi[24], bf0[26], cos_bit[stage]);
  bf1[22] = half_btf(-cospi[24], bf0[22], -cospi[40], bf0[25], cos_bit[stage]);
  bf1[23] = bf0[23];
  bf1[24] = bf0[24];
  bf1[25] = half_btf(-cospi[40], bf0[22], cospi[24], bf0[25], cos_bit[stage]);
  bf1[26] = half_btf(cospi[24], bf0[21], cospi[40], bf0[26], cos_bit[stage]);
  bf1[27] = bf0[27];
  bf1[28] = bf0[28];
  bf1[29] = half_btf(-cospi[8], bf0[18], cospi[56], bf0[29], cos_bit[stage]);
  bf1[30] = half_btf(cospi[56], bf0[17], cospi[8], bf0[30], cos_bit[stage]);
  bf1[31] = bf0[31];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 5
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = step;
  bf1 = output;
  bf1[0] = half_btf(cospi[32], bf0[0], cospi[32], bf0[1], cos_bit[stage]);
  bf1[1] = half_btf(cospi[32], bf0[0], -cospi[32], bf0[1], cos_bit[stage]);
  bf1[2] = half_btf(cospi[48], bf0[2], -cospi[16], bf0[3], cos_bit[stage]);
  bf1[3] = half_btf(cospi[16], bf0[2], cospi[48], bf0[3], cos_bit[stage]);
  bf1[4] = bf0[4] + bf0[5];
  bf1[5] = bf0[4] - bf0[5];
  bf1[6] = -bf0[6] + bf0[7];
  bf1[7] = bf0[6] + bf0[7];
  bf1[8] = bf0[8];
  bf1[9] = half_btf(-cospi[16], bf0[9], cospi[48], bf0[14], cos_bit[stage]);
  bf1[10] = half_btf(-cospi[48], bf0[10], -cospi[16], bf0[13], cos_bit[stage]);
  bf1[11] = bf0[11];
  bf1[12] = bf0[12];
  bf1[13] = half_btf(-cospi[16], bf0[10], cospi[48], bf0[13], cos_bit[stage]);
  bf1[14] = half_btf(cospi[48], bf0[9], cospi[16], bf0[14], cos_bit[stage]);
  bf1[15] = bf0[15];
  bf1[16] = bf0[16] + bf0[19];
  bf1[17] = bf0[17] + bf0[18];
  bf1[18] = bf0[17] - bf0[18];
  bf1[19] = bf0[16] - bf0[19];
  bf1[20] = -bf0[20] + bf0[23];
  bf1[21] = -bf0[21] + bf0[22];
  bf1[22] = bf0[21] + bf0[22];
  bf1[23] = bf0[20] + bf0[23];
  bf1[24] = bf0[24] + bf0[27];
  bf1[25] = bf0[25] + bf0[26];
  bf1[26] = bf0[25] - bf0[26];
  bf1[27] = bf0[24] - bf0[27];
  bf1[28] = -bf0[28] + bf0[31];
  bf1[29] = -bf0[29] + bf0[30];
  bf1[30] = bf0[29] + bf0[30];
  bf1[31] = bf0[28] + bf0[31];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 6
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0] + bf0[3];
  bf1[1] = bf0[1] + bf0[2];
  bf1[2] = bf0[1] - bf0[2];
  bf1[3] = bf0[0] - bf0[3];
  bf1[4] = bf0[4];
  bf1[5] = half_btf(-cospi[32], bf0[5], cospi[32], bf0[6], cos_bit[stage]);
  bf1[6] = half_btf(cospi[32], bf0[5], cospi[32], bf0[6], cos_bit[stage]);
  bf1[7] = bf0[7];
  bf1[8] = bf0[8] + bf0[11];
  bf1[9] = bf0[9] + bf0[10];
  bf1[10] = bf0[9] - bf0[10];
  bf1[11] = bf0[8] - bf0[11];
  bf1[12] = -bf0[12] + bf0[15];
  bf1[13] = -bf0[13] + bf0[14];
  bf1[14] = bf0[13] + bf0[14];
  bf1[15] = bf0[12] + bf0[15];
  bf1[16] = bf0[16];
  bf1[17] = bf0[17];
  bf1[18] = half_btf(-cospi[16], bf0[18], cospi[48], bf0[29], cos_bit[stage]);
  bf1[19] = half_btf(-cospi[16], bf0[19], cospi[48], bf0[28], cos_bit[stage]);
  bf1[20] = half_btf(-cospi[48], bf0[20], -cospi[16], bf0[27], cos_bit[stage]);
  bf1[21] = half_btf(-cospi[48], bf0[21], -cospi[16], bf0[26], cos_bit[stage]);
  bf1[22] = bf0[22];
  bf1[23] = bf0[23];
  bf1[24] = bf0[24];
  bf1[25] = bf0[25];
  bf1[26] = half_btf(-cospi[16], bf0[21], cospi[48], bf0[26], cos_bit[stage]);
  bf1[27] = half_btf(-cospi[16], bf0[20], cospi[48], bf0[27], cos_bit[stage]);
  bf1[28] = half_btf(cospi[48], bf0[19], cospi[16], bf0[28], cos_bit[stage]);
  bf1[29] = half_btf(cospi[48], bf0[18], cospi[16], bf0[29], cos_bit[stage]);
  bf1[30] = bf0[30];
  bf1[31] = bf0[31];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 7
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0] + bf0[7];
  bf1[1] = bf0[1] + bf0[6];
  bf1[2] = bf0[2] + bf0[5];
  bf1[3] = bf0[3] + bf0[4];
  bf1[4] = bf0[3] - bf0[4];
  bf1[5] = bf0[2] - bf0[5];
  bf1[6] = bf0[1] - bf0[6];
  bf1[7] = bf0[0] - bf0[7];
  bf1[8] = bf0[8];
  bf1[9] = bf0[9];
  bf1[10] = half_btf(-cospi[32], bf0[10], cospi[32], bf0[13], cos_bit[stage]);
  bf1[11] = half_btf(-cospi[32], bf0[11], cospi[32], bf0[12], cos_bit[stage]);
  bf1[12] = half_btf(cospi[32], bf0[11], cospi[32], bf0[12], cos_bit[stage]);
  bf1[13] = half_btf(cospi[32], bf0[10], cospi[32], bf0[13], cos_bit[stage]);
  bf1[14] = bf0[14];
  bf1[15] = bf0[15];
  bf1[16] = bf0[16] + bf0[23];
  bf1[17] = bf0[17] + bf0[22];
  bf1[18] = bf0[18] + bf0[21];
  bf1[19] = bf0[19] + bf0[20];
  bf1[20] = bf0[19] - bf0[20];
  bf1[21] = bf0[18] - bf0[21];
  bf1[22] = bf0[17] - bf0[22];
  bf1[23] = bf0[16] - bf0[23];
  bf1[24] = -bf0[24] + bf0[31];
  bf1[25] = -bf0[25] + bf0[30];
  bf1[26] = -bf0[26] + bf0[29];
  bf1[27] = -bf0[27] + bf0[28];
  bf1[28] = bf0[27] + bf0[28];
  bf1[29] = bf0[26] + bf0[29];
  bf1[30] = bf0[25] + bf0[30];
  bf1[31] = bf0[24] + bf0[31];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 8
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0] + bf0[15];
  bf1[1] = bf0[1] + bf0[14];
  bf1[2] = bf0[2] + bf0[13];
  bf1[3] = bf0[3] + bf0[12];
  bf1[4] = bf0[4] + bf0[11];
  bf1[5] = bf0[5] + bf0[10];
  bf1[6] = bf0[6] + bf0[9];
  bf1[7] = bf0[7] + bf0[8];
  bf1[8] = bf0[7] - bf0[8];
  bf1[9] = bf0[6] - bf0[9];
  bf1[10] = bf0[5] - bf0[10];
  bf1[11] = bf0[4] - bf0[11];
  bf1[12] = bf0[3] - bf0[12];
  bf1[13] = bf0[2] - bf0[13];
  bf1[14] = bf0[1] - bf0[14];
  bf1[15] = bf0[0] - bf0[15];
  bf1[16] = bf0[16];
  bf1[17] = bf0[17];
  bf1[18] = bf0[18];
  bf1[19] = bf0[19];
  bf1[20] = half_btf(-cospi[32], bf0[20], cospi[32], bf0[27], cos_bit[stage]);
  bf1[21] = half_btf(-cospi[32], bf0[21], cospi[32], bf0[26], cos_bit[stage]);
  bf1[22] = half_btf(-cospi[32], bf0[22], cospi[32], bf0[25], cos_bit[stage]);
  bf1[23] = half_btf(-cospi[32], bf0[23], cospi[32], bf0[24], cos_bit[stage]);
  bf1[24] = half_btf(cospi[32], bf0[23], cospi[32], bf0[24], cos_bit[stage]);
  bf1[25] = half_btf(cospi[32], bf0[22], cospi[32], bf0[25], cos_bit[stage]);
  bf1[26] = half_btf(cospi[32], bf0[21], cospi[32], bf0[26], cos_bit[stage]);
  bf1[27] = half_btf(cospi[32], bf0[20], cospi[32], bf0[27], cos_bit[stage]);
  bf1[28] = bf0[28];
  bf1[29] = bf0[29];
  bf1[30] = bf0[30];
  bf1[31] = bf0[31];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 9
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0] + bf0[31];
  bf1[1] = bf0[1] + bf0[30];
  bf1[2] = bf0[2] + bf0[29];
  bf1[3] = bf0[3] + bf0[28];
  bf1[4] = bf0[4] + bf0[27];
  bf1[5] = bf0[5] + bf0[26];
  bf1[6] = bf0[6] + bf0[25];
  bf1[7] = bf0[7] + bf0[24];
  bf1[8] = bf0[8] + bf0[23];
  bf1[9] = bf0[9] + bf0[22];
  bf1[10] = bf0[10] + bf0[21];
  bf1[11] = bf0[11] + bf0[20];
  bf1[12] = bf0[12] + bf0[19];
  bf1[13] = bf0[13] + bf0[18];
  bf1[14] = bf0[14] + bf0[17];
  bf1[15] = bf0[15] + bf0[16];
  bf1[16] = bf0[15] - bf0[16];
  bf1[17] = bf0[14] - bf0[17];
  bf1[18] = bf0[13] - bf0[18];
  bf1[19] = bf0[12] - bf0[19];
  bf1[20] = bf0[11] - bf0[20];
  bf1[21] = bf0[10] - bf0[21];
  bf1[22] = bf0[9] - bf0[22];
  bf1[23] = bf0[8] - bf0[23];
  bf1[24] = bf0[7] - bf0[24];
  bf1[25] = bf0[6] - bf0[25];
  bf1[26] = bf0[5] - bf0[26];
  bf1[27] = bf0[4] - bf0[27];
  bf1[28] = bf0[3] - bf0[28];
  bf1[29] = bf0[2] - bf0[29];
  bf1[30] = bf0[1] - bf0[30];
  bf1[31] = bf0[0] - bf0[31];
  range_check(stage, input, bf1, size, stage_range[stage]);
}

void av1_iadst4_new(const int32_t *input, int32_t *output,
                    const int8_t *cos_bit, const int8_t *stage_range) {
  const int32_t size = 4;
  const int32_t *cospi;

  int32_t stage = 0;
  int32_t *bf0, *bf1;
  int32_t step[4];

  // stage 0;
  range_check(stage, input, input, size, stage_range[stage]);

  // stage 1;
  stage++;
  assert(output != input);
  bf1 = output;
  bf1[0] = input[0];
  bf1[1] = -input[3];
  bf1[2] = -input[1];
  bf1[3] = input[2];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 2
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = half_btf(cospi[32], bf0[2], cospi[32], bf0[3], cos_bit[stage]);
  bf1[3] = half_btf(cospi[32], bf0[2], -cospi[32], bf0[3], cos_bit[stage]);
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 3
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0] + bf0[2];
  bf1[1] = bf0[1] + bf0[3];
  bf1[2] = bf0[0] - bf0[2];
  bf1[3] = bf0[1] - bf0[3];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 4
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = half_btf(cospi[8], bf0[0], cospi[56], bf0[1], cos_bit[stage]);
  bf1[1] = half_btf(cospi[56], bf0[0], -cospi[8], bf0[1], cos_bit[stage]);
  bf1[2] = half_btf(cospi[40], bf0[2], cospi[24], bf0[3], cos_bit[stage]);
  bf1[3] = half_btf(cospi[24], bf0[2], -cospi[40], bf0[3], cos_bit[stage]);
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 5
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[1];
  bf1[1] = bf0[2];
  bf1[2] = bf0[3];
  bf1[3] = bf0[0];
  range_check(stage, input, bf1, size, stage_range[stage]);
}

void av1_iadst8_new(const int32_t *input, int32_t *output,
                    const int8_t *cos_bit, const int8_t *stage_range) {
  const int32_t size = 8;
  const int32_t *cospi;

  int32_t stage = 0;
  int32_t *bf0, *bf1;
  int32_t step[8];

  // stage 0;
  range_check(stage, input, input, size, stage_range[stage]);

  // stage 1;
  stage++;
  assert(output != input);
  bf1 = output;
  bf1[0] = input[0];
  bf1[1] = -input[7];
  bf1[2] = -input[3];
  bf1[3] = input[4];
  bf1[4] = -input[1];
  bf1[5] = input[6];
  bf1[6] = input[2];
  bf1[7] = -input[5];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 2
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = half_btf(cospi[32], bf0[2], cospi[32], bf0[3], cos_bit[stage]);
  bf1[3] = half_btf(cospi[32], bf0[2], -cospi[32], bf0[3], cos_bit[stage]);
  bf1[4] = bf0[4];
  bf1[5] = bf0[5];
  bf1[6] = half_btf(cospi[32], bf0[6], cospi[32], bf0[7], cos_bit[stage]);
  bf1[7] = half_btf(cospi[32], bf0[6], -cospi[32], bf0[7], cos_bit[stage]);
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 3
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0] + bf0[2];
  bf1[1] = bf0[1] + bf0[3];
  bf1[2] = bf0[0] - bf0[2];
  bf1[3] = bf0[1] - bf0[3];
  bf1[4] = bf0[4] + bf0[6];
  bf1[5] = bf0[5] + bf0[7];
  bf1[6] = bf0[4] - bf0[6];
  bf1[7] = bf0[5] - bf0[7];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 4
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = half_btf(cospi[16], bf0[4], cospi[48], bf0[5], cos_bit[stage]);
  bf1[5] = half_btf(cospi[48], bf0[4], -cospi[16], bf0[5], cos_bit[stage]);
  bf1[6] = half_btf(-cospi[48], bf0[6], cospi[16], bf0[7], cos_bit[stage]);
  bf1[7] = half_btf(cospi[16], bf0[6], cospi[48], bf0[7], cos_bit[stage]);
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 5
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0] + bf0[4];
  bf1[1] = bf0[1] + bf0[5];
  bf1[2] = bf0[2] + bf0[6];
  bf1[3] = bf0[3] + bf0[7];
  bf1[4] = bf0[0] - bf0[4];
  bf1[5] = bf0[1] - bf0[5];
  bf1[6] = bf0[2] - bf0[6];
  bf1[7] = bf0[3] - bf0[7];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 6
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = half_btf(cospi[4], bf0[0], cospi[60], bf0[1], cos_bit[stage]);
  bf1[1] = half_btf(cospi[60], bf0[0], -cospi[4], bf0[1], cos_bit[stage]);
  bf1[2] = half_btf(cospi[20], bf0[2], cospi[44], bf0[3], cos_bit[stage]);
  bf1[3] = half_btf(cospi[44], bf0[2], -cospi[20], bf0[3], cos_bit[stage]);
  bf1[4] = half_btf(cospi[36], bf0[4], cospi[28], bf0[5], cos_bit[stage]);
  bf1[5] = half_btf(cospi[28], bf0[4], -cospi[36], bf0[5], cos_bit[stage]);
  bf1[6] = half_btf(cospi[52], bf0[6], cospi[12], bf0[7], cos_bit[stage]);
  bf1[7] = half_btf(cospi[12], bf0[6], -cospi[52], bf0[7], cos_bit[stage]);
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 7
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[1];
  bf1[1] = bf0[6];
  bf1[2] = bf0[3];
  bf1[3] = bf0[4];
  bf1[4] = bf0[5];
  bf1[5] = bf0[2];
  bf1[6] = bf0[7];
  bf1[7] = bf0[0];
  range_check(stage, input, bf1, size, stage_range[stage]);
}

void av1_iadst16_new(const int32_t *input, int32_t *output,
                     const int8_t *cos_bit, const int8_t *stage_range) {
  const int32_t size = 16;
  const int32_t *cospi;

  int32_t stage = 0;
  int32_t *bf0, *bf1;
  int32_t step[16];

  // stage 0;
  range_check(stage, input, input, size, stage_range[stage]);

  // stage 1;
  stage++;
  assert(output != input);
  bf1 = output;
  bf1[0] = input[0];
  bf1[1] = -input[15];
  bf1[2] = -input[7];
  bf1[3] = input[8];
  bf1[4] = -input[3];
  bf1[5] = input[12];
  bf1[6] = input[4];
  bf1[7] = -input[11];
  bf1[8] = -input[1];
  bf1[9] = input[14];
  bf1[10] = input[6];
  bf1[11] = -input[9];
  bf1[12] = input[2];
  bf1[13] = -input[13];
  bf1[14] = -input[5];
  bf1[15] = input[10];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 2
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = half_btf(cospi[32], bf0[2], cospi[32], bf0[3], cos_bit[stage]);
  bf1[3] = half_btf(cospi[32], bf0[2], -cospi[32], bf0[3], cos_bit[stage]);
  bf1[4] = bf0[4];
  bf1[5] = bf0[5];
  bf1[6] = half_btf(cospi[32], bf0[6], cospi[32], bf0[7], cos_bit[stage]);
  bf1[7] = half_btf(cospi[32], bf0[6], -cospi[32], bf0[7], cos_bit[stage]);
  bf1[8] = bf0[8];
  bf1[9] = bf0[9];
  bf1[10] = half_btf(cospi[32], bf0[10], cospi[32], bf0[11], cos_bit[stage]);
  bf1[11] = half_btf(cospi[32], bf0[10], -cospi[32], bf0[11], cos_bit[stage]);
  bf1[12] = bf0[12];
  bf1[13] = bf0[13];
  bf1[14] = half_btf(cospi[32], bf0[14], cospi[32], bf0[15], cos_bit[stage]);
  bf1[15] = half_btf(cospi[32], bf0[14], -cospi[32], bf0[15], cos_bit[stage]);
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 3
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0] + bf0[2];
  bf1[1] = bf0[1] + bf0[3];
  bf1[2] = bf0[0] - bf0[2];
  bf1[3] = bf0[1] - bf0[3];
  bf1[4] = bf0[4] + bf0[6];
  bf1[5] = bf0[5] + bf0[7];
  bf1[6] = bf0[4] - bf0[6];
  bf1[7] = bf0[5] - bf0[7];
  bf1[8] = bf0[8] + bf0[10];
  bf1[9] = bf0[9] + bf0[11];
  bf1[10] = bf0[8] - bf0[10];
  bf1[11] = bf0[9] - bf0[11];
  bf1[12] = bf0[12] + bf0[14];
  bf1[13] = bf0[13] + bf0[15];
  bf1[14] = bf0[12] - bf0[14];
  bf1[15] = bf0[13] - bf0[15];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 4
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = half_btf(cospi[16], bf0[4], cospi[48], bf0[5], cos_bit[stage]);
  bf1[5] = half_btf(cospi[48], bf0[4], -cospi[16], bf0[5], cos_bit[stage]);
  bf1[6] = half_btf(-cospi[48], bf0[6], cospi[16], bf0[7], cos_bit[stage]);
  bf1[7] = half_btf(cospi[16], bf0[6], cospi[48], bf0[7], cos_bit[stage]);
  bf1[8] = bf0[8];
  bf1[9] = bf0[9];
  bf1[10] = bf0[10];
  bf1[11] = bf0[11];
  bf1[12] = half_btf(cospi[16], bf0[12], cospi[48], bf0[13], cos_bit[stage]);
  bf1[13] = half_btf(cospi[48], bf0[12], -cospi[16], bf0[13], cos_bit[stage]);
  bf1[14] = half_btf(-cospi[48], bf0[14], cospi[16], bf0[15], cos_bit[stage]);
  bf1[15] = half_btf(cospi[16], bf0[14], cospi[48], bf0[15], cos_bit[stage]);
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 5
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0] + bf0[4];
  bf1[1] = bf0[1] + bf0[5];
  bf1[2] = bf0[2] + bf0[6];
  bf1[3] = bf0[3] + bf0[7];
  bf1[4] = bf0[0] - bf0[4];
  bf1[5] = bf0[1] - bf0[5];
  bf1[6] = bf0[2] - bf0[6];
  bf1[7] = bf0[3] - bf0[7];
  bf1[8] = bf0[8] + bf0[12];
  bf1[9] = bf0[9] + bf0[13];
  bf1[10] = bf0[10] + bf0[14];
  bf1[11] = bf0[11] + bf0[15];
  bf1[12] = bf0[8] - bf0[12];
  bf1[13] = bf0[9] - bf0[13];
  bf1[14] = bf0[10] - bf0[14];
  bf1[15] = bf0[11] - bf0[15];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 6
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = bf0[4];
  bf1[5] = bf0[5];
  bf1[6] = bf0[6];
  bf1[7] = bf0[7];
  bf1[8] = half_btf(cospi[8], bf0[8], cospi[56], bf0[9], cos_bit[stage]);
  bf1[9] = half_btf(cospi[56], bf0[8], -cospi[8], bf0[9], cos_bit[stage]);
  bf1[10] = half_btf(cospi[40], bf0[10], cospi[24], bf0[11], cos_bit[stage]);
  bf1[11] = half_btf(cospi[24], bf0[10], -cospi[40], bf0[11], cos_bit[stage]);
  bf1[12] = half_btf(-cospi[56], bf0[12], cospi[8], bf0[13], cos_bit[stage]);
  bf1[13] = half_btf(cospi[8], bf0[12], cospi[56], bf0[13], cos_bit[stage]);
  bf1[14] = half_btf(-cospi[24], bf0[14], cospi[40], bf0[15], cos_bit[stage]);
  bf1[15] = half_btf(cospi[40], bf0[14], cospi[24], bf0[15], cos_bit[stage]);
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 7
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0] + bf0[8];
  bf1[1] = bf0[1] + bf0[9];
  bf1[2] = bf0[2] + bf0[10];
  bf1[3] = bf0[3] + bf0[11];
  bf1[4] = bf0[4] + bf0[12];
  bf1[5] = bf0[5] + bf0[13];
  bf1[6] = bf0[6] + bf0[14];
  bf1[7] = bf0[7] + bf0[15];
  bf1[8] = bf0[0] - bf0[8];
  bf1[9] = bf0[1] - bf0[9];
  bf1[10] = bf0[2] - bf0[10];
  bf1[11] = bf0[3] - bf0[11];
  bf1[12] = bf0[4] - bf0[12];
  bf1[13] = bf0[5] - bf0[13];
  bf1[14] = bf0[6] - bf0[14];
  bf1[15] = bf0[7] - bf0[15];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 8
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = half_btf(cospi[2], bf0[0], cospi[62], bf0[1], cos_bit[stage]);
  bf1[1] = half_btf(cospi[62], bf0[0], -cospi[2], bf0[1], cos_bit[stage]);
  bf1[2] = half_btf(cospi[10], bf0[2], cospi[54], bf0[3], cos_bit[stage]);
  bf1[3] = half_btf(cospi[54], bf0[2], -cospi[10], bf0[3], cos_bit[stage]);
  bf1[4] = half_btf(cospi[18], bf0[4], cospi[46], bf0[5], cos_bit[stage]);
  bf1[5] = half_btf(cospi[46], bf0[4], -cospi[18], bf0[5], cos_bit[stage]);
  bf1[6] = half_btf(cospi[26], bf0[6], cospi[38], bf0[7], cos_bit[stage]);
  bf1[7] = half_btf(cospi[38], bf0[6], -cospi[26], bf0[7], cos_bit[stage]);
  bf1[8] = half_btf(cospi[34], bf0[8], cospi[30], bf0[9], cos_bit[stage]);
  bf1[9] = half_btf(cospi[30], bf0[8], -cospi[34], bf0[9], cos_bit[stage]);
  bf1[10] = half_btf(cospi[42], bf0[10], cospi[22], bf0[11], cos_bit[stage]);
  bf1[11] = half_btf(cospi[22], bf0[10], -cospi[42], bf0[11], cos_bit[stage]);
  bf1[12] = half_btf(cospi[50], bf0[12], cospi[14], bf0[13], cos_bit[stage]);
  bf1[13] = half_btf(cospi[14], bf0[12], -cospi[50], bf0[13], cos_bit[stage]);
  bf1[14] = half_btf(cospi[58], bf0[14], cospi[6], bf0[15], cos_bit[stage]);
  bf1[15] = half_btf(cospi[6], bf0[14], -cospi[58], bf0[15], cos_bit[stage]);
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 9
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[1];
  bf1[1] = bf0[14];
  bf1[2] = bf0[3];
  bf1[3] = bf0[12];
  bf1[4] = bf0[5];
  bf1[5] = bf0[10];
  bf1[6] = bf0[7];
  bf1[7] = bf0[8];
  bf1[8] = bf0[9];
  bf1[9] = bf0[6];
  bf1[10] = bf0[11];
  bf1[11] = bf0[4];
  bf1[12] = bf0[13];
  bf1[13] = bf0[2];
  bf1[14] = bf0[15];
  bf1[15] = bf0[0];
  range_check(stage, input, bf1, size, stage_range[stage]);
}

void av1_iadst32_new(const int32_t *input, int32_t *output,
                     const int8_t *cos_bit, const int8_t *stage_range) {
  const int32_t size = 32;
  const int32_t *cospi;

  int32_t stage = 0;
  int32_t *bf0, *bf1;
  int32_t step[32];

  // stage 0;
  range_check(stage, input, input, size, stage_range[stage]);

  // stage 1;
  stage++;
  assert(output != input);
  bf1 = output;
  bf1[0] = input[0];
  bf1[1] = -input[31];
  bf1[2] = -input[15];
  bf1[3] = input[16];
  bf1[4] = -input[7];
  bf1[5] = input[24];
  bf1[6] = input[8];
  bf1[7] = -input[23];
  bf1[8] = -input[3];
  bf1[9] = input[28];
  bf1[10] = input[12];
  bf1[11] = -input[19];
  bf1[12] = input[4];
  bf1[13] = -input[27];
  bf1[14] = -input[11];
  bf1[15] = input[20];
  bf1[16] = -input[1];
  bf1[17] = input[30];
  bf1[18] = input[14];
  bf1[19] = -input[17];
  bf1[20] = input[6];
  bf1[21] = -input[25];
  bf1[22] = -input[9];
  bf1[23] = input[22];
  bf1[24] = input[2];
  bf1[25] = -input[29];
  bf1[26] = -input[13];
  bf1[27] = input[18];
  bf1[28] = -input[5];
  bf1[29] = input[26];
  bf1[30] = input[10];
  bf1[31] = -input[21];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 2
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = half_btf(cospi[32], bf0[2], cospi[32], bf0[3], cos_bit[stage]);
  bf1[3] = half_btf(cospi[32], bf0[2], -cospi[32], bf0[3], cos_bit[stage]);
  bf1[4] = bf0[4];
  bf1[5] = bf0[5];
  bf1[6] = half_btf(cospi[32], bf0[6], cospi[32], bf0[7], cos_bit[stage]);
  bf1[7] = half_btf(cospi[32], bf0[6], -cospi[32], bf0[7], cos_bit[stage]);
  bf1[8] = bf0[8];
  bf1[9] = bf0[9];
  bf1[10] = half_btf(cospi[32], bf0[10], cospi[32], bf0[11], cos_bit[stage]);
  bf1[11] = half_btf(cospi[32], bf0[10], -cospi[32], bf0[11], cos_bit[stage]);
  bf1[12] = bf0[12];
  bf1[13] = bf0[13];
  bf1[14] = half_btf(cospi[32], bf0[14], cospi[32], bf0[15], cos_bit[stage]);
  bf1[15] = half_btf(cospi[32], bf0[14], -cospi[32], bf0[15], cos_bit[stage]);
  bf1[16] = bf0[16];
  bf1[17] = bf0[17];
  bf1[18] = half_btf(cospi[32], bf0[18], cospi[32], bf0[19], cos_bit[stage]);
  bf1[19] = half_btf(cospi[32], bf0[18], -cospi[32], bf0[19], cos_bit[stage]);
  bf1[20] = bf0[20];
  bf1[21] = bf0[21];
  bf1[22] = half_btf(cospi[32], bf0[22], cospi[32], bf0[23], cos_bit[stage]);
  bf1[23] = half_btf(cospi[32], bf0[22], -cospi[32], bf0[23], cos_bit[stage]);
  bf1[24] = bf0[24];
  bf1[25] = bf0[25];
  bf1[26] = half_btf(cospi[32], bf0[26], cospi[32], bf0[27], cos_bit[stage]);
  bf1[27] = half_btf(cospi[32], bf0[26], -cospi[32], bf0[27], cos_bit[stage]);
  bf1[28] = bf0[28];
  bf1[29] = bf0[29];
  bf1[30] = half_btf(cospi[32], bf0[30], cospi[32], bf0[31], cos_bit[stage]);
  bf1[31] = half_btf(cospi[32], bf0[30], -cospi[32], bf0[31], cos_bit[stage]);
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 3
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0] + bf0[2];
  bf1[1] = bf0[1] + bf0[3];
  bf1[2] = bf0[0] - bf0[2];
  bf1[3] = bf0[1] - bf0[3];
  bf1[4] = bf0[4] + bf0[6];
  bf1[5] = bf0[5] + bf0[7];
  bf1[6] = bf0[4] - bf0[6];
  bf1[7] = bf0[5] - bf0[7];
  bf1[8] = bf0[8] + bf0[10];
  bf1[9] = bf0[9] + bf0[11];
  bf1[10] = bf0[8] - bf0[10];
  bf1[11] = bf0[9] - bf0[11];
  bf1[12] = bf0[12] + bf0[14];
  bf1[13] = bf0[13] + bf0[15];
  bf1[14] = bf0[12] - bf0[14];
  bf1[15] = bf0[13] - bf0[15];
  bf1[16] = bf0[16] + bf0[18];
  bf1[17] = bf0[17] + bf0[19];
  bf1[18] = bf0[16] - bf0[18];
  bf1[19] = bf0[17] - bf0[19];
  bf1[20] = bf0[20] + bf0[22];
  bf1[21] = bf0[21] + bf0[23];
  bf1[22] = bf0[20] - bf0[22];
  bf1[23] = bf0[21] - bf0[23];
  bf1[24] = bf0[24] + bf0[26];
  bf1[25] = bf0[25] + bf0[27];
  bf1[26] = bf0[24] - bf0[26];
  bf1[27] = bf0[25] - bf0[27];
  bf1[28] = bf0[28] + bf0[30];
  bf1[29] = bf0[29] + bf0[31];
  bf1[30] = bf0[28] - bf0[30];
  bf1[31] = bf0[29] - bf0[31];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 4
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = half_btf(cospi[16], bf0[4], cospi[48], bf0[5], cos_bit[stage]);
  bf1[5] = half_btf(cospi[48], bf0[4], -cospi[16], bf0[5], cos_bit[stage]);
  bf1[6] = half_btf(-cospi[48], bf0[6], cospi[16], bf0[7], cos_bit[stage]);
  bf1[7] = half_btf(cospi[16], bf0[6], cospi[48], bf0[7], cos_bit[stage]);
  bf1[8] = bf0[8];
  bf1[9] = bf0[9];
  bf1[10] = bf0[10];
  bf1[11] = bf0[11];
  bf1[12] = half_btf(cospi[16], bf0[12], cospi[48], bf0[13], cos_bit[stage]);
  bf1[13] = half_btf(cospi[48], bf0[12], -cospi[16], bf0[13], cos_bit[stage]);
  bf1[14] = half_btf(-cospi[48], bf0[14], cospi[16], bf0[15], cos_bit[stage]);
  bf1[15] = half_btf(cospi[16], bf0[14], cospi[48], bf0[15], cos_bit[stage]);
  bf1[16] = bf0[16];
  bf1[17] = bf0[17];
  bf1[18] = bf0[18];
  bf1[19] = bf0[19];
  bf1[20] = half_btf(cospi[16], bf0[20], cospi[48], bf0[21], cos_bit[stage]);
  bf1[21] = half_btf(cospi[48], bf0[20], -cospi[16], bf0[21], cos_bit[stage]);
  bf1[22] = half_btf(-cospi[48], bf0[22], cospi[16], bf0[23], cos_bit[stage]);
  bf1[23] = half_btf(cospi[16], bf0[22], cospi[48], bf0[23], cos_bit[stage]);
  bf1[24] = bf0[24];
  bf1[25] = bf0[25];
  bf1[26] = bf0[26];
  bf1[27] = bf0[27];
  bf1[28] = half_btf(cospi[16], bf0[28], cospi[48], bf0[29], cos_bit[stage]);
  bf1[29] = half_btf(cospi[48], bf0[28], -cospi[16], bf0[29], cos_bit[stage]);
  bf1[30] = half_btf(-cospi[48], bf0[30], cospi[16], bf0[31], cos_bit[stage]);
  bf1[31] = half_btf(cospi[16], bf0[30], cospi[48], bf0[31], cos_bit[stage]);
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 5
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0] + bf0[4];
  bf1[1] = bf0[1] + bf0[5];
  bf1[2] = bf0[2] + bf0[6];
  bf1[3] = bf0[3] + bf0[7];
  bf1[4] = bf0[0] - bf0[4];
  bf1[5] = bf0[1] - bf0[5];
  bf1[6] = bf0[2] - bf0[6];
  bf1[7] = bf0[3] - bf0[7];
  bf1[8] = bf0[8] + bf0[12];
  bf1[9] = bf0[9] + bf0[13];
  bf1[10] = bf0[10] + bf0[14];
  bf1[11] = bf0[11] + bf0[15];
  bf1[12] = bf0[8] - bf0[12];
  bf1[13] = bf0[9] - bf0[13];
  bf1[14] = bf0[10] - bf0[14];
  bf1[15] = bf0[11] - bf0[15];
  bf1[16] = bf0[16] + bf0[20];
  bf1[17] = bf0[17] + bf0[21];
  bf1[18] = bf0[18] + bf0[22];
  bf1[19] = bf0[19] + bf0[23];
  bf1[20] = bf0[16] - bf0[20];
  bf1[21] = bf0[17] - bf0[21];
  bf1[22] = bf0[18] - bf0[22];
  bf1[23] = bf0[19] - bf0[23];
  bf1[24] = bf0[24] + bf0[28];
  bf1[25] = bf0[25] + bf0[29];
  bf1[26] = bf0[26] + bf0[30];
  bf1[27] = bf0[27] + bf0[31];
  bf1[28] = bf0[24] - bf0[28];
  bf1[29] = bf0[25] - bf0[29];
  bf1[30] = bf0[26] - bf0[30];
  bf1[31] = bf0[27] - bf0[31];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 6
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = bf0[4];
  bf1[5] = bf0[5];
  bf1[6] = bf0[6];
  bf1[7] = bf0[7];
  bf1[8] = half_btf(cospi[8], bf0[8], cospi[56], bf0[9], cos_bit[stage]);
  bf1[9] = half_btf(cospi[56], bf0[8], -cospi[8], bf0[9], cos_bit[stage]);
  bf1[10] = half_btf(cospi[40], bf0[10], cospi[24], bf0[11], cos_bit[stage]);
  bf1[11] = half_btf(cospi[24], bf0[10], -cospi[40], bf0[11], cos_bit[stage]);
  bf1[12] = half_btf(-cospi[56], bf0[12], cospi[8], bf0[13], cos_bit[stage]);
  bf1[13] = half_btf(cospi[8], bf0[12], cospi[56], bf0[13], cos_bit[stage]);
  bf1[14] = half_btf(-cospi[24], bf0[14], cospi[40], bf0[15], cos_bit[stage]);
  bf1[15] = half_btf(cospi[40], bf0[14], cospi[24], bf0[15], cos_bit[stage]);
  bf1[16] = bf0[16];
  bf1[17] = bf0[17];
  bf1[18] = bf0[18];
  bf1[19] = bf0[19];
  bf1[20] = bf0[20];
  bf1[21] = bf0[21];
  bf1[22] = bf0[22];
  bf1[23] = bf0[23];
  bf1[24] = half_btf(cospi[8], bf0[24], cospi[56], bf0[25], cos_bit[stage]);
  bf1[25] = half_btf(cospi[56], bf0[24], -cospi[8], bf0[25], cos_bit[stage]);
  bf1[26] = half_btf(cospi[40], bf0[26], cospi[24], bf0[27], cos_bit[stage]);
  bf1[27] = half_btf(cospi[24], bf0[26], -cospi[40], bf0[27], cos_bit[stage]);
  bf1[28] = half_btf(-cospi[56], bf0[28], cospi[8], bf0[29], cos_bit[stage]);
  bf1[29] = half_btf(cospi[8], bf0[28], cospi[56], bf0[29], cos_bit[stage]);
  bf1[30] = half_btf(-cospi[24], bf0[30], cospi[40], bf0[31], cos_bit[stage]);
  bf1[31] = half_btf(cospi[40], bf0[30], cospi[24], bf0[31], cos_bit[stage]);
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 7
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0] + bf0[8];
  bf1[1] = bf0[1] + bf0[9];
  bf1[2] = bf0[2] + bf0[10];
  bf1[3] = bf0[3] + bf0[11];
  bf1[4] = bf0[4] + bf0[12];
  bf1[5] = bf0[5] + bf0[13];
  bf1[6] = bf0[6] + bf0[14];
  bf1[7] = bf0[7] + bf0[15];
  bf1[8] = bf0[0] - bf0[8];
  bf1[9] = bf0[1] - bf0[9];
  bf1[10] = bf0[2] - bf0[10];
  bf1[11] = bf0[3] - bf0[11];
  bf1[12] = bf0[4] - bf0[12];
  bf1[13] = bf0[5] - bf0[13];
  bf1[14] = bf0[6] - bf0[14];
  bf1[15] = bf0[7] - bf0[15];
  bf1[16] = bf0[16] + bf0[24];
  bf1[17] = bf0[17] + bf0[25];
  bf1[18] = bf0[18] + bf0[26];
  bf1[19] = bf0[19] + bf0[27];
  bf1[20] = bf0[20] + bf0[28];
  bf1[21] = bf0[21] + bf0[29];
  bf1[22] = bf0[22] + bf0[30];
  bf1[23] = bf0[23] + bf0[31];
  bf1[24] = bf0[16] - bf0[24];
  bf1[25] = bf0[17] - bf0[25];
  bf1[26] = bf0[18] - bf0[26];
  bf1[27] = bf0[19] - bf0[27];
  bf1[28] = bf0[20] - bf0[28];
  bf1[29] = bf0[21] - bf0[29];
  bf1[30] = bf0[22] - bf0[30];
  bf1[31] = bf0[23] - bf0[31];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 8
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = bf0[4];
  bf1[5] = bf0[5];
  bf1[6] = bf0[6];
  bf1[7] = bf0[7];
  bf1[8] = bf0[8];
  bf1[9] = bf0[9];
  bf1[10] = bf0[10];
  bf1[11] = bf0[11];
  bf1[12] = bf0[12];
  bf1[13] = bf0[13];
  bf1[14] = bf0[14];
  bf1[15] = bf0[15];
  bf1[16] = half_btf(cospi[4], bf0[16], cospi[60], bf0[17], cos_bit[stage]);
  bf1[17] = half_btf(cospi[60], bf0[16], -cospi[4], bf0[17], cos_bit[stage]);
  bf1[18] = half_btf(cospi[20], bf0[18], cospi[44], bf0[19], cos_bit[stage]);
  bf1[19] = half_btf(cospi[44], bf0[18], -cospi[20], bf0[19], cos_bit[stage]);
  bf1[20] = half_btf(cospi[36], bf0[20], cospi[28], bf0[21], cos_bit[stage]);
  bf1[21] = half_btf(cospi[28], bf0[20], -cospi[36], bf0[21], cos_bit[stage]);
  bf1[22] = half_btf(cospi[52], bf0[22], cospi[12], bf0[23], cos_bit[stage]);
  bf1[23] = half_btf(cospi[12], bf0[22], -cospi[52], bf0[23], cos_bit[stage]);
  bf1[24] = half_btf(-cospi[60], bf0[24], cospi[4], bf0[25], cos_bit[stage]);
  bf1[25] = half_btf(cospi[4], bf0[24], cospi[60], bf0[25], cos_bit[stage]);
  bf1[26] = half_btf(-cospi[44], bf0[26], cospi[20], bf0[27], cos_bit[stage]);
  bf1[27] = half_btf(cospi[20], bf0[26], cospi[44], bf0[27], cos_bit[stage]);
  bf1[28] = half_btf(-cospi[28], bf0[28], cospi[36], bf0[29], cos_bit[stage]);
  bf1[29] = half_btf(cospi[36], bf0[28], cospi[28], bf0[29], cos_bit[stage]);
  bf1[30] = half_btf(-cospi[12], bf0[30], cospi[52], bf0[31], cos_bit[stage]);
  bf1[31] = half_btf(cospi[52], bf0[30], cospi[12], bf0[31], cos_bit[stage]);
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 9
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0] + bf0[16];
  bf1[1] = bf0[1] + bf0[17];
  bf1[2] = bf0[2] + bf0[18];
  bf1[3] = bf0[3] + bf0[19];
  bf1[4] = bf0[4] + bf0[20];
  bf1[5] = bf0[5] + bf0[21];
  bf1[6] = bf0[6] + bf0[22];
  bf1[7] = bf0[7] + bf0[23];
  bf1[8] = bf0[8] + bf0[24];
  bf1[9] = bf0[9] + bf0[25];
  bf1[10] = bf0[10] + bf0[26];
  bf1[11] = bf0[11] + bf0[27];
  bf1[12] = bf0[12] + bf0[28];
  bf1[13] = bf0[13] + bf0[29];
  bf1[14] = bf0[14] + bf0[30];
  bf1[15] = bf0[15] + bf0[31];
  bf1[16] = bf0[0] - bf0[16];
  bf1[17] = bf0[1] - bf0[17];
  bf1[18] = bf0[2] - bf0[18];
  bf1[19] = bf0[3] - bf0[19];
  bf1[20] = bf0[4] - bf0[20];
  bf1[21] = bf0[5] - bf0[21];
  bf1[22] = bf0[6] - bf0[22];
  bf1[23] = bf0[7] - bf0[23];
  bf1[24] = bf0[8] - bf0[24];
  bf1[25] = bf0[9] - bf0[25];
  bf1[26] = bf0[10] - bf0[26];
  bf1[27] = bf0[11] - bf0[27];
  bf1[28] = bf0[12] - bf0[28];
  bf1[29] = bf0[13] - bf0[29];
  bf1[30] = bf0[14] - bf0[30];
  bf1[31] = bf0[15] - bf0[31];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 10
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = half_btf(cospi[1], bf0[0], cospi[63], bf0[1], cos_bit[stage]);
  bf1[1] = half_btf(cospi[63], bf0[0], -cospi[1], bf0[1], cos_bit[stage]);
  bf1[2] = half_btf(cospi[5], bf0[2], cospi[59], bf0[3], cos_bit[stage]);
  bf1[3] = half_btf(cospi[59], bf0[2], -cospi[5], bf0[3], cos_bit[stage]);
  bf1[4] = half_btf(cospi[9], bf0[4], cospi[55], bf0[5], cos_bit[stage]);
  bf1[5] = half_btf(cospi[55], bf0[4], -cospi[9], bf0[5], cos_bit[stage]);
  bf1[6] = half_btf(cospi[13], bf0[6], cospi[51], bf0[7], cos_bit[stage]);
  bf1[7] = half_btf(cospi[51], bf0[6], -cospi[13], bf0[7], cos_bit[stage]);
  bf1[8] = half_btf(cospi[17], bf0[8], cospi[47], bf0[9], cos_bit[stage]);
  bf1[9] = half_btf(cospi[47], bf0[8], -cospi[17], bf0[9], cos_bit[stage]);
  bf1[10] = half_btf(cospi[21], bf0[10], cospi[43], bf0[11], cos_bit[stage]);
  bf1[11] = half_btf(cospi[43], bf0[10], -cospi[21], bf0[11], cos_bit[stage]);
  bf1[12] = half_btf(cospi[25], bf0[12], cospi[39], bf0[13], cos_bit[stage]);
  bf1[13] = half_btf(cospi[39], bf0[12], -cospi[25], bf0[13], cos_bit[stage]);
  bf1[14] = half_btf(cospi[29], bf0[14], cospi[35], bf0[15], cos_bit[stage]);
  bf1[15] = half_btf(cospi[35], bf0[14], -cospi[29], bf0[15], cos_bit[stage]);
  bf1[16] = half_btf(cospi[33], bf0[16], cospi[31], bf0[17], cos_bit[stage]);
  bf1[17] = half_btf(cospi[31], bf0[16], -cospi[33], bf0[17], cos_bit[stage]);
  bf1[18] = half_btf(cospi[37], bf0[18], cospi[27], bf0[19], cos_bit[stage]);
  bf1[19] = half_btf(cospi[27], bf0[18], -cospi[37], bf0[19], cos_bit[stage]);
  bf1[20] = half_btf(cospi[41], bf0[20], cospi[23], bf0[21], cos_bit[stage]);
  bf1[21] = half_btf(cospi[23], bf0[20], -cospi[41], bf0[21], cos_bit[stage]);
  bf1[22] = half_btf(cospi[45], bf0[22], cospi[19], bf0[23], cos_bit[stage]);
  bf1[23] = half_btf(cospi[19], bf0[22], -cospi[45], bf0[23], cos_bit[stage]);
  bf1[24] = half_btf(cospi[49], bf0[24], cospi[15], bf0[25], cos_bit[stage]);
  bf1[25] = half_btf(cospi[15], bf0[24], -cospi[49], bf0[25], cos_bit[stage]);
  bf1[26] = half_btf(cospi[53], bf0[26], cospi[11], bf0[27], cos_bit[stage]);
  bf1[27] = half_btf(cospi[11], bf0[26], -cospi[53], bf0[27], cos_bit[stage]);
  bf1[28] = half_btf(cospi[57], bf0[28], cospi[7], bf0[29], cos_bit[stage]);
  bf1[29] = half_btf(cospi[7], bf0[28], -cospi[57], bf0[29], cos_bit[stage]);
  bf1[30] = half_btf(cospi[61], bf0[30], cospi[3], bf0[31], cos_bit[stage]);
  bf1[31] = half_btf(cospi[3], bf0[30], -cospi[61], bf0[31], cos_bit[stage]);
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 11
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[1];
  bf1[1] = bf0[30];
  bf1[2] = bf0[3];
  bf1[3] = bf0[28];
  bf1[4] = bf0[5];
  bf1[5] = bf0[26];
  bf1[6] = bf0[7];
  bf1[7] = bf0[24];
  bf1[8] = bf0[9];
  bf1[9] = bf0[22];
  bf1[10] = bf0[11];
  bf1[11] = bf0[20];
  bf1[12] = bf0[13];
  bf1[13] = bf0[18];
  bf1[14] = bf0[15];
  bf1[15] = bf0[16];
  bf1[16] = bf0[17];
  bf1[17] = bf0[14];
  bf1[18] = bf0[19];
  bf1[19] = bf0[12];
  bf1[20] = bf0[21];
  bf1[21] = bf0[10];
  bf1[22] = bf0[23];
  bf1[23] = bf0[8];
  bf1[24] = bf0[25];
  bf1[25] = bf0[6];
  bf1[26] = bf0[27];
  bf1[27] = bf0[4];
  bf1[28] = bf0[29];
  bf1[29] = bf0[2];
  bf1[30] = bf0[31];
  bf1[31] = bf0[0];
  range_check(stage, input, bf1, size, stage_range[stage]);
}

#if CONFIG_EXT_TX
void av1_iidentity4_c(const int32_t *input, int32_t *output,
                      const int8_t *cos_bit, const int8_t *stage_range) {
  (void)cos_bit;
  for (int i = 0; i < 4; ++i)
    output[i] = (int32_t)dct_const_round_shift(input[i] * Sqrt2);
  range_check(0, input, output, 4, stage_range[0]);
}

void av1_iidentity8_c(const int32_t *input, int32_t *output,
                      const int8_t *cos_bit, const int8_t *stage_range) {
  (void)cos_bit;
  for (int i = 0; i < 8; ++i) output[i] = input[i] * 2;
  range_check(0, input, output, 8, stage_range[0]);
}

void av1_iidentity16_c(const int32_t *input, int32_t *output,
                       const int8_t *cos_bit, const int8_t *stage_range) {
  (void)cos_bit;
  for (int i = 0; i < 16; ++i)
    output[i] = (int32_t)dct_const_round_shift(input[i] * 2 * Sqrt2);
  range_check(0, input, output, 16, stage_range[0]);
}

void av1_iidentity32_c(const int32_t *input, int32_t *output,
                       const int8_t *cos_bit, const int8_t *stage_range) {
  (void)cos_bit;
  for (int i = 0; i < 32; ++i) output[i] = input[i] * 4;
  range_check(0, input, output, 32, stage_range[0]);
}

#if CONFIG_TX64X64
void av1_iidentity64_c(const int32_t *input, int32_t *output,
                       const int8_t *cos_bit, const int8_t *stage_range) {
  (void)cos_bit;
  for (int i = 0; i < 64; ++i)
    output[i] = (int32_t)dct_const_round_shift(input[i] * 4 * Sqrt2);
  range_check(0, input, output, 64, stage_range[0]);
}
#endif  // CONFIG_TX64X64
#endif  // CONFIG_EXT_TX

#if CONFIG_TX64X64
void av1_idct64_new(const int32_t *input, int32_t *output,
                    const int8_t *cos_bit, const int8_t *stage_range) {
  const int32_t size = 64;
  const int32_t *cospi;

  int32_t stage = 0;
  int32_t *bf0, *bf1;
  int32_t step[64];

  // stage 0;
  range_check(stage, input, input, size, stage_range[stage]);

  // stage 1;
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  assert(output != input);
  bf1 = output;
  bf1[0] = input[0];
  bf1[1] = input[32];
  bf1[2] = input[16];
  bf1[3] = input[48];
  bf1[4] = input[8];
  bf1[5] = input[40];
  bf1[6] = input[24];
  bf1[7] = input[56];
  bf1[8] = input[4];
  bf1[9] = input[36];
  bf1[10] = input[20];
  bf1[11] = input[52];
  bf1[12] = input[12];
  bf1[13] = input[44];
  bf1[14] = input[28];
  bf1[15] = input[60];
  bf1[16] = input[2];
  bf1[17] = input[34];
  bf1[18] = input[18];
  bf1[19] = input[50];
  bf1[20] = input[10];
  bf1[21] = input[42];
  bf1[22] = input[26];
  bf1[23] = input[58];
  bf1[24] = input[6];
  bf1[25] = input[38];
  bf1[26] = input[22];
  bf1[27] = input[54];
  bf1[28] = input[14];
  bf1[29] = input[46];
  bf1[30] = input[30];
  bf1[31] = input[62];
  bf1[32] = input[1];
  bf1[33] = input[33];
  bf1[34] = input[17];
  bf1[35] = input[49];
  bf1[36] = input[9];
  bf1[37] = input[41];
  bf1[38] = input[25];
  bf1[39] = input[57];
  bf1[40] = input[5];
  bf1[41] = input[37];
  bf1[42] = input[21];
  bf1[43] = input[53];
  bf1[44] = input[13];
  bf1[45] = input[45];
  bf1[46] = input[29];
  bf1[47] = input[61];
  bf1[48] = input[3];
  bf1[49] = input[35];
  bf1[50] = input[19];
  bf1[51] = input[51];
  bf1[52] = input[11];
  bf1[53] = input[43];
  bf1[54] = input[27];
  bf1[55] = input[59];
  bf1[56] = input[7];
  bf1[57] = input[39];
  bf1[58] = input[23];
  bf1[59] = input[55];
  bf1[60] = input[15];
  bf1[61] = input[47];
  bf1[62] = input[31];
  bf1[63] = input[63];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 2
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = bf0[4];
  bf1[5] = bf0[5];
  bf1[6] = bf0[6];
  bf1[7] = bf0[7];
  bf1[8] = bf0[8];
  bf1[9] = bf0[9];
  bf1[10] = bf0[10];
  bf1[11] = bf0[11];
  bf1[12] = bf0[12];
  bf1[13] = bf0[13];
  bf1[14] = bf0[14];
  bf1[15] = bf0[15];
  bf1[16] = bf0[16];
  bf1[17] = bf0[17];
  bf1[18] = bf0[18];
  bf1[19] = bf0[19];
  bf1[20] = bf0[20];
  bf1[21] = bf0[21];
  bf1[22] = bf0[22];
  bf1[23] = bf0[23];
  bf1[24] = bf0[24];
  bf1[25] = bf0[25];
  bf1[26] = bf0[26];
  bf1[27] = bf0[27];
  bf1[28] = bf0[28];
  bf1[29] = bf0[29];
  bf1[30] = bf0[30];
  bf1[31] = bf0[31];
  bf1[32] = half_btf(cospi[63], bf0[32], -cospi[1], bf0[63], cos_bit[stage]);
  bf1[33] = half_btf(cospi[31], bf0[33], -cospi[33], bf0[62], cos_bit[stage]);
  bf1[34] = half_btf(cospi[47], bf0[34], -cospi[17], bf0[61], cos_bit[stage]);
  bf1[35] = half_btf(cospi[15], bf0[35], -cospi[49], bf0[60], cos_bit[stage]);
  bf1[36] = half_btf(cospi[55], bf0[36], -cospi[9], bf0[59], cos_bit[stage]);
  bf1[37] = half_btf(cospi[23], bf0[37], -cospi[41], bf0[58], cos_bit[stage]);
  bf1[38] = half_btf(cospi[39], bf0[38], -cospi[25], bf0[57], cos_bit[stage]);
  bf1[39] = half_btf(cospi[7], bf0[39], -cospi[57], bf0[56], cos_bit[stage]);
  bf1[40] = half_btf(cospi[59], bf0[40], -cospi[5], bf0[55], cos_bit[stage]);
  bf1[41] = half_btf(cospi[27], bf0[41], -cospi[37], bf0[54], cos_bit[stage]);
  bf1[42] = half_btf(cospi[43], bf0[42], -cospi[21], bf0[53], cos_bit[stage]);
  bf1[43] = half_btf(cospi[11], bf0[43], -cospi[53], bf0[52], cos_bit[stage]);
  bf1[44] = half_btf(cospi[51], bf0[44], -cospi[13], bf0[51], cos_bit[stage]);
  bf1[45] = half_btf(cospi[19], bf0[45], -cospi[45], bf0[50], cos_bit[stage]);
  bf1[46] = half_btf(cospi[35], bf0[46], -cospi[29], bf0[49], cos_bit[stage]);
  bf1[47] = half_btf(cospi[3], bf0[47], -cospi[61], bf0[48], cos_bit[stage]);
  bf1[48] = half_btf(cospi[61], bf0[47], cospi[3], bf0[48], cos_bit[stage]);
  bf1[49] = half_btf(cospi[29], bf0[46], cospi[35], bf0[49], cos_bit[stage]);
  bf1[50] = half_btf(cospi[45], bf0[45], cospi[19], bf0[50], cos_bit[stage]);
  bf1[51] = half_btf(cospi[13], bf0[44], cospi[51], bf0[51], cos_bit[stage]);
  bf1[52] = half_btf(cospi[53], bf0[43], cospi[11], bf0[52], cos_bit[stage]);
  bf1[53] = half_btf(cospi[21], bf0[42], cospi[43], bf0[53], cos_bit[stage]);
  bf1[54] = half_btf(cospi[37], bf0[41], cospi[27], bf0[54], cos_bit[stage]);
  bf1[55] = half_btf(cospi[5], bf0[40], cospi[59], bf0[55], cos_bit[stage]);
  bf1[56] = half_btf(cospi[57], bf0[39], cospi[7], bf0[56], cos_bit[stage]);
  bf1[57] = half_btf(cospi[25], bf0[38], cospi[39], bf0[57], cos_bit[stage]);
  bf1[58] = half_btf(cospi[41], bf0[37], cospi[23], bf0[58], cos_bit[stage]);
  bf1[59] = half_btf(cospi[9], bf0[36], cospi[55], bf0[59], cos_bit[stage]);
  bf1[60] = half_btf(cospi[49], bf0[35], cospi[15], bf0[60], cos_bit[stage]);
  bf1[61] = half_btf(cospi[17], bf0[34], cospi[47], bf0[61], cos_bit[stage]);
  bf1[62] = half_btf(cospi[33], bf0[33], cospi[31], bf0[62], cos_bit[stage]);
  bf1[63] = half_btf(cospi[1], bf0[32], cospi[63], bf0[63], cos_bit[stage]);
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 3
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = bf0[4];
  bf1[5] = bf0[5];
  bf1[6] = bf0[6];
  bf1[7] = bf0[7];
  bf1[8] = bf0[8];
  bf1[9] = bf0[9];
  bf1[10] = bf0[10];
  bf1[11] = bf0[11];
  bf1[12] = bf0[12];
  bf1[13] = bf0[13];
  bf1[14] = bf0[14];
  bf1[15] = bf0[15];
  bf1[16] = half_btf(cospi[62], bf0[16], -cospi[2], bf0[31], cos_bit[stage]);
  bf1[17] = half_btf(cospi[30], bf0[17], -cospi[34], bf0[30], cos_bit[stage]);
  bf1[18] = half_btf(cospi[46], bf0[18], -cospi[18], bf0[29], cos_bit[stage]);
  bf1[19] = half_btf(cospi[14], bf0[19], -cospi[50], bf0[28], cos_bit[stage]);
  bf1[20] = half_btf(cospi[54], bf0[20], -cospi[10], bf0[27], cos_bit[stage]);
  bf1[21] = half_btf(cospi[22], bf0[21], -cospi[42], bf0[26], cos_bit[stage]);
  bf1[22] = half_btf(cospi[38], bf0[22], -cospi[26], bf0[25], cos_bit[stage]);
  bf1[23] = half_btf(cospi[6], bf0[23], -cospi[58], bf0[24], cos_bit[stage]);
  bf1[24] = half_btf(cospi[58], bf0[23], cospi[6], bf0[24], cos_bit[stage]);
  bf1[25] = half_btf(cospi[26], bf0[22], cospi[38], bf0[25], cos_bit[stage]);
  bf1[26] = half_btf(cospi[42], bf0[21], cospi[22], bf0[26], cos_bit[stage]);
  bf1[27] = half_btf(cospi[10], bf0[20], cospi[54], bf0[27], cos_bit[stage]);
  bf1[28] = half_btf(cospi[50], bf0[19], cospi[14], bf0[28], cos_bit[stage]);
  bf1[29] = half_btf(cospi[18], bf0[18], cospi[46], bf0[29], cos_bit[stage]);
  bf1[30] = half_btf(cospi[34], bf0[17], cospi[30], bf0[30], cos_bit[stage]);
  bf1[31] = half_btf(cospi[2], bf0[16], cospi[62], bf0[31], cos_bit[stage]);
  bf1[32] = bf0[32] + bf0[33];
  bf1[33] = bf0[32] - bf0[33];
  bf1[34] = -bf0[34] + bf0[35];
  bf1[35] = bf0[34] + bf0[35];
  bf1[36] = bf0[36] + bf0[37];
  bf1[37] = bf0[36] - bf0[37];
  bf1[38] = -bf0[38] + bf0[39];
  bf1[39] = bf0[38] + bf0[39];
  bf1[40] = bf0[40] + bf0[41];
  bf1[41] = bf0[40] - bf0[41];
  bf1[42] = -bf0[42] + bf0[43];
  bf1[43] = bf0[42] + bf0[43];
  bf1[44] = bf0[44] + bf0[45];
  bf1[45] = bf0[44] - bf0[45];
  bf1[46] = -bf0[46] + bf0[47];
  bf1[47] = bf0[46] + bf0[47];
  bf1[48] = bf0[48] + bf0[49];
  bf1[49] = bf0[48] - bf0[49];
  bf1[50] = -bf0[50] + bf0[51];
  bf1[51] = bf0[50] + bf0[51];
  bf1[52] = bf0[52] + bf0[53];
  bf1[53] = bf0[52] - bf0[53];
  bf1[54] = -bf0[54] + bf0[55];
  bf1[55] = bf0[54] + bf0[55];
  bf1[56] = bf0[56] + bf0[57];
  bf1[57] = bf0[56] - bf0[57];
  bf1[58] = -bf0[58] + bf0[59];
  bf1[59] = bf0[58] + bf0[59];
  bf1[60] = bf0[60] + bf0[61];
  bf1[61] = bf0[60] - bf0[61];
  bf1[62] = -bf0[62] + bf0[63];
  bf1[63] = bf0[62] + bf0[63];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 4
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = bf0[4];
  bf1[5] = bf0[5];
  bf1[6] = bf0[6];
  bf1[7] = bf0[7];
  bf1[8] = half_btf(cospi[60], bf0[8], -cospi[4], bf0[15], cos_bit[stage]);
  bf1[9] = half_btf(cospi[28], bf0[9], -cospi[36], bf0[14], cos_bit[stage]);
  bf1[10] = half_btf(cospi[44], bf0[10], -cospi[20], bf0[13], cos_bit[stage]);
  bf1[11] = half_btf(cospi[12], bf0[11], -cospi[52], bf0[12], cos_bit[stage]);
  bf1[12] = half_btf(cospi[52], bf0[11], cospi[12], bf0[12], cos_bit[stage]);
  bf1[13] = half_btf(cospi[20], bf0[10], cospi[44], bf0[13], cos_bit[stage]);
  bf1[14] = half_btf(cospi[36], bf0[9], cospi[28], bf0[14], cos_bit[stage]);
  bf1[15] = half_btf(cospi[4], bf0[8], cospi[60], bf0[15], cos_bit[stage]);
  bf1[16] = bf0[16] + bf0[17];
  bf1[17] = bf0[16] - bf0[17];
  bf1[18] = -bf0[18] + bf0[19];
  bf1[19] = bf0[18] + bf0[19];
  bf1[20] = bf0[20] + bf0[21];
  bf1[21] = bf0[20] - bf0[21];
  bf1[22] = -bf0[22] + bf0[23];
  bf1[23] = bf0[22] + bf0[23];
  bf1[24] = bf0[24] + bf0[25];
  bf1[25] = bf0[24] - bf0[25];
  bf1[26] = -bf0[26] + bf0[27];
  bf1[27] = bf0[26] + bf0[27];
  bf1[28] = bf0[28] + bf0[29];
  bf1[29] = bf0[28] - bf0[29];
  bf1[30] = -bf0[30] + bf0[31];
  bf1[31] = bf0[30] + bf0[31];
  bf1[32] = bf0[32];
  bf1[33] = half_btf(-cospi[4], bf0[33], cospi[60], bf0[62], cos_bit[stage]);
  bf1[34] = half_btf(-cospi[60], bf0[34], -cospi[4], bf0[61], cos_bit[stage]);
  bf1[35] = bf0[35];
  bf1[36] = bf0[36];
  bf1[37] = half_btf(-cospi[36], bf0[37], cospi[28], bf0[58], cos_bit[stage]);
  bf1[38] = half_btf(-cospi[28], bf0[38], -cospi[36], bf0[57], cos_bit[stage]);
  bf1[39] = bf0[39];
  bf1[40] = bf0[40];
  bf1[41] = half_btf(-cospi[20], bf0[41], cospi[44], bf0[54], cos_bit[stage]);
  bf1[42] = half_btf(-cospi[44], bf0[42], -cospi[20], bf0[53], cos_bit[stage]);
  bf1[43] = bf0[43];
  bf1[44] = bf0[44];
  bf1[45] = half_btf(-cospi[52], bf0[45], cospi[12], bf0[50], cos_bit[stage]);
  bf1[46] = half_btf(-cospi[12], bf0[46], -cospi[52], bf0[49], cos_bit[stage]);
  bf1[47] = bf0[47];
  bf1[48] = bf0[48];
  bf1[49] = half_btf(-cospi[52], bf0[46], cospi[12], bf0[49], cos_bit[stage]);
  bf1[50] = half_btf(cospi[12], bf0[45], cospi[52], bf0[50], cos_bit[stage]);
  bf1[51] = bf0[51];
  bf1[52] = bf0[52];
  bf1[53] = half_btf(-cospi[20], bf0[42], cospi[44], bf0[53], cos_bit[stage]);
  bf1[54] = half_btf(cospi[44], bf0[41], cospi[20], bf0[54], cos_bit[stage]);
  bf1[55] = bf0[55];
  bf1[56] = bf0[56];
  bf1[57] = half_btf(-cospi[36], bf0[38], cospi[28], bf0[57], cos_bit[stage]);
  bf1[58] = half_btf(cospi[28], bf0[37], cospi[36], bf0[58], cos_bit[stage]);
  bf1[59] = bf0[59];
  bf1[60] = bf0[60];
  bf1[61] = half_btf(-cospi[4], bf0[34], cospi[60], bf0[61], cos_bit[stage]);
  bf1[62] = half_btf(cospi[60], bf0[33], cospi[4], bf0[62], cos_bit[stage]);
  bf1[63] = bf0[63];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 5
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = half_btf(cospi[56], bf0[4], -cospi[8], bf0[7], cos_bit[stage]);
  bf1[5] = half_btf(cospi[24], bf0[5], -cospi[40], bf0[6], cos_bit[stage]);
  bf1[6] = half_btf(cospi[40], bf0[5], cospi[24], bf0[6], cos_bit[stage]);
  bf1[7] = half_btf(cospi[8], bf0[4], cospi[56], bf0[7], cos_bit[stage]);
  bf1[8] = bf0[8] + bf0[9];
  bf1[9] = bf0[8] - bf0[9];
  bf1[10] = -bf0[10] + bf0[11];
  bf1[11] = bf0[10] + bf0[11];
  bf1[12] = bf0[12] + bf0[13];
  bf1[13] = bf0[12] - bf0[13];
  bf1[14] = -bf0[14] + bf0[15];
  bf1[15] = bf0[14] + bf0[15];
  bf1[16] = bf0[16];
  bf1[17] = half_btf(-cospi[8], bf0[17], cospi[56], bf0[30], cos_bit[stage]);
  bf1[18] = half_btf(-cospi[56], bf0[18], -cospi[8], bf0[29], cos_bit[stage]);
  bf1[19] = bf0[19];
  bf1[20] = bf0[20];
  bf1[21] = half_btf(-cospi[40], bf0[21], cospi[24], bf0[26], cos_bit[stage]);
  bf1[22] = half_btf(-cospi[24], bf0[22], -cospi[40], bf0[25], cos_bit[stage]);
  bf1[23] = bf0[23];
  bf1[24] = bf0[24];
  bf1[25] = half_btf(-cospi[40], bf0[22], cospi[24], bf0[25], cos_bit[stage]);
  bf1[26] = half_btf(cospi[24], bf0[21], cospi[40], bf0[26], cos_bit[stage]);
  bf1[27] = bf0[27];
  bf1[28] = bf0[28];
  bf1[29] = half_btf(-cospi[8], bf0[18], cospi[56], bf0[29], cos_bit[stage]);
  bf1[30] = half_btf(cospi[56], bf0[17], cospi[8], bf0[30], cos_bit[stage]);
  bf1[31] = bf0[31];
  bf1[32] = bf0[32] + bf0[35];
  bf1[33] = bf0[33] + bf0[34];
  bf1[34] = bf0[33] - bf0[34];
  bf1[35] = bf0[32] - bf0[35];
  bf1[36] = -bf0[36] + bf0[39];
  bf1[37] = -bf0[37] + bf0[38];
  bf1[38] = bf0[37] + bf0[38];
  bf1[39] = bf0[36] + bf0[39];
  bf1[40] = bf0[40] + bf0[43];
  bf1[41] = bf0[41] + bf0[42];
  bf1[42] = bf0[41] - bf0[42];
  bf1[43] = bf0[40] - bf0[43];
  bf1[44] = -bf0[44] + bf0[47];
  bf1[45] = -bf0[45] + bf0[46];
  bf1[46] = bf0[45] + bf0[46];
  bf1[47] = bf0[44] + bf0[47];
  bf1[48] = bf0[48] + bf0[51];
  bf1[49] = bf0[49] + bf0[50];
  bf1[50] = bf0[49] - bf0[50];
  bf1[51] = bf0[48] - bf0[51];
  bf1[52] = -bf0[52] + bf0[55];
  bf1[53] = -bf0[53] + bf0[54];
  bf1[54] = bf0[53] + bf0[54];
  bf1[55] = bf0[52] + bf0[55];
  bf1[56] = bf0[56] + bf0[59];
  bf1[57] = bf0[57] + bf0[58];
  bf1[58] = bf0[57] - bf0[58];
  bf1[59] = bf0[56] - bf0[59];
  bf1[60] = -bf0[60] + bf0[63];
  bf1[61] = -bf0[61] + bf0[62];
  bf1[62] = bf0[61] + bf0[62];
  bf1[63] = bf0[60] + bf0[63];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 6
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = half_btf(cospi[32], bf0[0], cospi[32], bf0[1], cos_bit[stage]);
  bf1[1] = half_btf(cospi[32], bf0[0], -cospi[32], bf0[1], cos_bit[stage]);
  bf1[2] = half_btf(cospi[48], bf0[2], -cospi[16], bf0[3], cos_bit[stage]);
  bf1[3] = half_btf(cospi[16], bf0[2], cospi[48], bf0[3], cos_bit[stage]);
  bf1[4] = bf0[4] + bf0[5];
  bf1[5] = bf0[4] - bf0[5];
  bf1[6] = -bf0[6] + bf0[7];
  bf1[7] = bf0[6] + bf0[7];
  bf1[8] = bf0[8];
  bf1[9] = half_btf(-cospi[16], bf0[9], cospi[48], bf0[14], cos_bit[stage]);
  bf1[10] = half_btf(-cospi[48], bf0[10], -cospi[16], bf0[13], cos_bit[stage]);
  bf1[11] = bf0[11];
  bf1[12] = bf0[12];
  bf1[13] = half_btf(-cospi[16], bf0[10], cospi[48], bf0[13], cos_bit[stage]);
  bf1[14] = half_btf(cospi[48], bf0[9], cospi[16], bf0[14], cos_bit[stage]);
  bf1[15] = bf0[15];
  bf1[16] = bf0[16] + bf0[19];
  bf1[17] = bf0[17] + bf0[18];
  bf1[18] = bf0[17] - bf0[18];
  bf1[19] = bf0[16] - bf0[19];
  bf1[20] = -bf0[20] + bf0[23];
  bf1[21] = -bf0[21] + bf0[22];
  bf1[22] = bf0[21] + bf0[22];
  bf1[23] = bf0[20] + bf0[23];
  bf1[24] = bf0[24] + bf0[27];
  bf1[25] = bf0[25] + bf0[26];
  bf1[26] = bf0[25] - bf0[26];
  bf1[27] = bf0[24] - bf0[27];
  bf1[28] = -bf0[28] + bf0[31];
  bf1[29] = -bf0[29] + bf0[30];
  bf1[30] = bf0[29] + bf0[30];
  bf1[31] = bf0[28] + bf0[31];
  bf1[32] = bf0[32];
  bf1[33] = bf0[33];
  bf1[34] = half_btf(-cospi[8], bf0[34], cospi[56], bf0[61], cos_bit[stage]);
  bf1[35] = half_btf(-cospi[8], bf0[35], cospi[56], bf0[60], cos_bit[stage]);
  bf1[36] = half_btf(-cospi[56], bf0[36], -cospi[8], bf0[59], cos_bit[stage]);
  bf1[37] = half_btf(-cospi[56], bf0[37], -cospi[8], bf0[58], cos_bit[stage]);
  bf1[38] = bf0[38];
  bf1[39] = bf0[39];
  bf1[40] = bf0[40];
  bf1[41] = bf0[41];
  bf1[42] = half_btf(-cospi[40], bf0[42], cospi[24], bf0[53], cos_bit[stage]);
  bf1[43] = half_btf(-cospi[40], bf0[43], cospi[24], bf0[52], cos_bit[stage]);
  bf1[44] = half_btf(-cospi[24], bf0[44], -cospi[40], bf0[51], cos_bit[stage]);
  bf1[45] = half_btf(-cospi[24], bf0[45], -cospi[40], bf0[50], cos_bit[stage]);
  bf1[46] = bf0[46];
  bf1[47] = bf0[47];
  bf1[48] = bf0[48];
  bf1[49] = bf0[49];
  bf1[50] = half_btf(-cospi[40], bf0[45], cospi[24], bf0[50], cos_bit[stage]);
  bf1[51] = half_btf(-cospi[40], bf0[44], cospi[24], bf0[51], cos_bit[stage]);
  bf1[52] = half_btf(cospi[24], bf0[43], cospi[40], bf0[52], cos_bit[stage]);
  bf1[53] = half_btf(cospi[24], bf0[42], cospi[40], bf0[53], cos_bit[stage]);
  bf1[54] = bf0[54];
  bf1[55] = bf0[55];
  bf1[56] = bf0[56];
  bf1[57] = bf0[57];
  bf1[58] = half_btf(-cospi[8], bf0[37], cospi[56], bf0[58], cos_bit[stage]);
  bf1[59] = half_btf(-cospi[8], bf0[36], cospi[56], bf0[59], cos_bit[stage]);
  bf1[60] = half_btf(cospi[56], bf0[35], cospi[8], bf0[60], cos_bit[stage]);
  bf1[61] = half_btf(cospi[56], bf0[34], cospi[8], bf0[61], cos_bit[stage]);
  bf1[62] = bf0[62];
  bf1[63] = bf0[63];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 7
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0] + bf0[3];
  bf1[1] = bf0[1] + bf0[2];
  bf1[2] = bf0[1] - bf0[2];
  bf1[3] = bf0[0] - bf0[3];
  bf1[4] = bf0[4];
  bf1[5] = half_btf(-cospi[32], bf0[5], cospi[32], bf0[6], cos_bit[stage]);
  bf1[6] = half_btf(cospi[32], bf0[5], cospi[32], bf0[6], cos_bit[stage]);
  bf1[7] = bf0[7];
  bf1[8] = bf0[8] + bf0[11];
  bf1[9] = bf0[9] + bf0[10];
  bf1[10] = bf0[9] - bf0[10];
  bf1[11] = bf0[8] - bf0[11];
  bf1[12] = -bf0[12] + bf0[15];
  bf1[13] = -bf0[13] + bf0[14];
  bf1[14] = bf0[13] + bf0[14];
  bf1[15] = bf0[12] + bf0[15];
  bf1[16] = bf0[16];
  bf1[17] = bf0[17];
  bf1[18] = half_btf(-cospi[16], bf0[18], cospi[48], bf0[29], cos_bit[stage]);
  bf1[19] = half_btf(-cospi[16], bf0[19], cospi[48], bf0[28], cos_bit[stage]);
  bf1[20] = half_btf(-cospi[48], bf0[20], -cospi[16], bf0[27], cos_bit[stage]);
  bf1[21] = half_btf(-cospi[48], bf0[21], -cospi[16], bf0[26], cos_bit[stage]);
  bf1[22] = bf0[22];
  bf1[23] = bf0[23];
  bf1[24] = bf0[24];
  bf1[25] = bf0[25];
  bf1[26] = half_btf(-cospi[16], bf0[21], cospi[48], bf0[26], cos_bit[stage]);
  bf1[27] = half_btf(-cospi[16], bf0[20], cospi[48], bf0[27], cos_bit[stage]);
  bf1[28] = half_btf(cospi[48], bf0[19], cospi[16], bf0[28], cos_bit[stage]);
  bf1[29] = half_btf(cospi[48], bf0[18], cospi[16], bf0[29], cos_bit[stage]);
  bf1[30] = bf0[30];
  bf1[31] = bf0[31];
  bf1[32] = bf0[32] + bf0[39];
  bf1[33] = bf0[33] + bf0[38];
  bf1[34] = bf0[34] + bf0[37];
  bf1[35] = bf0[35] + bf0[36];
  bf1[36] = bf0[35] - bf0[36];
  bf1[37] = bf0[34] - bf0[37];
  bf1[38] = bf0[33] - bf0[38];
  bf1[39] = bf0[32] - bf0[39];
  bf1[40] = -bf0[40] + bf0[47];
  bf1[41] = -bf0[41] + bf0[46];
  bf1[42] = -bf0[42] + bf0[45];
  bf1[43] = -bf0[43] + bf0[44];
  bf1[44] = bf0[43] + bf0[44];
  bf1[45] = bf0[42] + bf0[45];
  bf1[46] = bf0[41] + bf0[46];
  bf1[47] = bf0[40] + bf0[47];
  bf1[48] = bf0[48] + bf0[55];
  bf1[49] = bf0[49] + bf0[54];
  bf1[50] = bf0[50] + bf0[53];
  bf1[51] = bf0[51] + bf0[52];
  bf1[52] = bf0[51] - bf0[52];
  bf1[53] = bf0[50] - bf0[53];
  bf1[54] = bf0[49] - bf0[54];
  bf1[55] = bf0[48] - bf0[55];
  bf1[56] = -bf0[56] + bf0[63];
  bf1[57] = -bf0[57] + bf0[62];
  bf1[58] = -bf0[58] + bf0[61];
  bf1[59] = -bf0[59] + bf0[60];
  bf1[60] = bf0[59] + bf0[60];
  bf1[61] = bf0[58] + bf0[61];
  bf1[62] = bf0[57] + bf0[62];
  bf1[63] = bf0[56] + bf0[63];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 8
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0] + bf0[7];
  bf1[1] = bf0[1] + bf0[6];
  bf1[2] = bf0[2] + bf0[5];
  bf1[3] = bf0[3] + bf0[4];
  bf1[4] = bf0[3] - bf0[4];
  bf1[5] = bf0[2] - bf0[5];
  bf1[6] = bf0[1] - bf0[6];
  bf1[7] = bf0[0] - bf0[7];
  bf1[8] = bf0[8];
  bf1[9] = bf0[9];
  bf1[10] = half_btf(-cospi[32], bf0[10], cospi[32], bf0[13], cos_bit[stage]);
  bf1[11] = half_btf(-cospi[32], bf0[11], cospi[32], bf0[12], cos_bit[stage]);
  bf1[12] = half_btf(cospi[32], bf0[11], cospi[32], bf0[12], cos_bit[stage]);
  bf1[13] = half_btf(cospi[32], bf0[10], cospi[32], bf0[13], cos_bit[stage]);
  bf1[14] = bf0[14];
  bf1[15] = bf0[15];
  bf1[16] = bf0[16] + bf0[23];
  bf1[17] = bf0[17] + bf0[22];
  bf1[18] = bf0[18] + bf0[21];
  bf1[19] = bf0[19] + bf0[20];
  bf1[20] = bf0[19] - bf0[20];
  bf1[21] = bf0[18] - bf0[21];
  bf1[22] = bf0[17] - bf0[22];
  bf1[23] = bf0[16] - bf0[23];
  bf1[24] = -bf0[24] + bf0[31];
  bf1[25] = -bf0[25] + bf0[30];
  bf1[26] = -bf0[26] + bf0[29];
  bf1[27] = -bf0[27] + bf0[28];
  bf1[28] = bf0[27] + bf0[28];
  bf1[29] = bf0[26] + bf0[29];
  bf1[30] = bf0[25] + bf0[30];
  bf1[31] = bf0[24] + bf0[31];
  bf1[32] = bf0[32];
  bf1[33] = bf0[33];
  bf1[34] = bf0[34];
  bf1[35] = bf0[35];
  bf1[36] = half_btf(-cospi[16], bf0[36], cospi[48], bf0[59], cos_bit[stage]);
  bf1[37] = half_btf(-cospi[16], bf0[37], cospi[48], bf0[58], cos_bit[stage]);
  bf1[38] = half_btf(-cospi[16], bf0[38], cospi[48], bf0[57], cos_bit[stage]);
  bf1[39] = half_btf(-cospi[16], bf0[39], cospi[48], bf0[56], cos_bit[stage]);
  bf1[40] = half_btf(-cospi[48], bf0[40], -cospi[16], bf0[55], cos_bit[stage]);
  bf1[41] = half_btf(-cospi[48], bf0[41], -cospi[16], bf0[54], cos_bit[stage]);
  bf1[42] = half_btf(-cospi[48], bf0[42], -cospi[16], bf0[53], cos_bit[stage]);
  bf1[43] = half_btf(-cospi[48], bf0[43], -cospi[16], bf0[52], cos_bit[stage]);
  bf1[44] = bf0[44];
  bf1[45] = bf0[45];
  bf1[46] = bf0[46];
  bf1[47] = bf0[47];
  bf1[48] = bf0[48];
  bf1[49] = bf0[49];
  bf1[50] = bf0[50];
  bf1[51] = bf0[51];
  bf1[52] = half_btf(-cospi[16], bf0[43], cospi[48], bf0[52], cos_bit[stage]);
  bf1[53] = half_btf(-cospi[16], bf0[42], cospi[48], bf0[53], cos_bit[stage]);
  bf1[54] = half_btf(-cospi[16], bf0[41], cospi[48], bf0[54], cos_bit[stage]);
  bf1[55] = half_btf(-cospi[16], bf0[40], cospi[48], bf0[55], cos_bit[stage]);
  bf1[56] = half_btf(cospi[48], bf0[39], cospi[16], bf0[56], cos_bit[stage]);
  bf1[57] = half_btf(cospi[48], bf0[38], cospi[16], bf0[57], cos_bit[stage]);
  bf1[58] = half_btf(cospi[48], bf0[37], cospi[16], bf0[58], cos_bit[stage]);
  bf1[59] = half_btf(cospi[48], bf0[36], cospi[16], bf0[59], cos_bit[stage]);
  bf1[60] = bf0[60];
  bf1[61] = bf0[61];
  bf1[62] = bf0[62];
  bf1[63] = bf0[63];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 9
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0] + bf0[15];
  bf1[1] = bf0[1] + bf0[14];
  bf1[2] = bf0[2] + bf0[13];
  bf1[3] = bf0[3] + bf0[12];
  bf1[4] = bf0[4] + bf0[11];
  bf1[5] = bf0[5] + bf0[10];
  bf1[6] = bf0[6] + bf0[9];
  bf1[7] = bf0[7] + bf0[8];
  bf1[8] = bf0[7] - bf0[8];
  bf1[9] = bf0[6] - bf0[9];
  bf1[10] = bf0[5] - bf0[10];
  bf1[11] = bf0[4] - bf0[11];
  bf1[12] = bf0[3] - bf0[12];
  bf1[13] = bf0[2] - bf0[13];
  bf1[14] = bf0[1] - bf0[14];
  bf1[15] = bf0[0] - bf0[15];
  bf1[16] = bf0[16];
  bf1[17] = bf0[17];
  bf1[18] = bf0[18];
  bf1[19] = bf0[19];
  bf1[20] = half_btf(-cospi[32], bf0[20], cospi[32], bf0[27], cos_bit[stage]);
  bf1[21] = half_btf(-cospi[32], bf0[21], cospi[32], bf0[26], cos_bit[stage]);
  bf1[22] = half_btf(-cospi[32], bf0[22], cospi[32], bf0[25], cos_bit[stage]);
  bf1[23] = half_btf(-cospi[32], bf0[23], cospi[32], bf0[24], cos_bit[stage]);
  bf1[24] = half_btf(cospi[32], bf0[23], cospi[32], bf0[24], cos_bit[stage]);
  bf1[25] = half_btf(cospi[32], bf0[22], cospi[32], bf0[25], cos_bit[stage]);
  bf1[26] = half_btf(cospi[32], bf0[21], cospi[32], bf0[26], cos_bit[stage]);
  bf1[27] = half_btf(cospi[32], bf0[20], cospi[32], bf0[27], cos_bit[stage]);
  bf1[28] = bf0[28];
  bf1[29] = bf0[29];
  bf1[30] = bf0[30];
  bf1[31] = bf0[31];
  bf1[32] = bf0[32] + bf0[47];
  bf1[33] = bf0[33] + bf0[46];
  bf1[34] = bf0[34] + bf0[45];
  bf1[35] = bf0[35] + bf0[44];
  bf1[36] = bf0[36] + bf0[43];
  bf1[37] = bf0[37] + bf0[42];
  bf1[38] = bf0[38] + bf0[41];
  bf1[39] = bf0[39] + bf0[40];
  bf1[40] = bf0[39] - bf0[40];
  bf1[41] = bf0[38] - bf0[41];
  bf1[42] = bf0[37] - bf0[42];
  bf1[43] = bf0[36] - bf0[43];
  bf1[44] = bf0[35] - bf0[44];
  bf1[45] = bf0[34] - bf0[45];
  bf1[46] = bf0[33] - bf0[46];
  bf1[47] = bf0[32] - bf0[47];
  bf1[48] = -bf0[48] + bf0[63];
  bf1[49] = -bf0[49] + bf0[62];
  bf1[50] = -bf0[50] + bf0[61];
  bf1[51] = -bf0[51] + bf0[60];
  bf1[52] = -bf0[52] + bf0[59];
  bf1[53] = -bf0[53] + bf0[58];
  bf1[54] = -bf0[54] + bf0[57];
  bf1[55] = -bf0[55] + bf0[56];
  bf1[56] = bf0[55] + bf0[56];
  bf1[57] = bf0[54] + bf0[57];
  bf1[58] = bf0[53] + bf0[58];
  bf1[59] = bf0[52] + bf0[59];
  bf1[60] = bf0[51] + bf0[60];
  bf1[61] = bf0[50] + bf0[61];
  bf1[62] = bf0[49] + bf0[62];
  bf1[63] = bf0[48] + bf0[63];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 10
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0] + bf0[31];
  bf1[1] = bf0[1] + bf0[30];
  bf1[2] = bf0[2] + bf0[29];
  bf1[3] = bf0[3] + bf0[28];
  bf1[4] = bf0[4] + bf0[27];
  bf1[5] = bf0[5] + bf0[26];
  bf1[6] = bf0[6] + bf0[25];
  bf1[7] = bf0[7] + bf0[24];
  bf1[8] = bf0[8] + bf0[23];
  bf1[9] = bf0[9] + bf0[22];
  bf1[10] = bf0[10] + bf0[21];
  bf1[11] = bf0[11] + bf0[20];
  bf1[12] = bf0[12] + bf0[19];
  bf1[13] = bf0[13] + bf0[18];
  bf1[14] = bf0[14] + bf0[17];
  bf1[15] = bf0[15] + bf0[16];
  bf1[16] = bf0[15] - bf0[16];
  bf1[17] = bf0[14] - bf0[17];
  bf1[18] = bf0[13] - bf0[18];
  bf1[19] = bf0[12] - bf0[19];
  bf1[20] = bf0[11] - bf0[20];
  bf1[21] = bf0[10] - bf0[21];
  bf1[22] = bf0[9] - bf0[22];
  bf1[23] = bf0[8] - bf0[23];
  bf1[24] = bf0[7] - bf0[24];
  bf1[25] = bf0[6] - bf0[25];
  bf1[26] = bf0[5] - bf0[26];
  bf1[27] = bf0[4] - bf0[27];
  bf1[28] = bf0[3] - bf0[28];
  bf1[29] = bf0[2] - bf0[29];
  bf1[30] = bf0[1] - bf0[30];
  bf1[31] = bf0[0] - bf0[31];
  bf1[32] = bf0[32];
  bf1[33] = bf0[33];
  bf1[34] = bf0[34];
  bf1[35] = bf0[35];
  bf1[36] = bf0[36];
  bf1[37] = bf0[37];
  bf1[38] = bf0[38];
  bf1[39] = bf0[39];
  bf1[40] = half_btf(-cospi[32], bf0[40], cospi[32], bf0[55], cos_bit[stage]);
  bf1[41] = half_btf(-cospi[32], bf0[41], cospi[32], bf0[54], cos_bit[stage]);
  bf1[42] = half_btf(-cospi[32], bf0[42], cospi[32], bf0[53], cos_bit[stage]);
  bf1[43] = half_btf(-cospi[32], bf0[43], cospi[32], bf0[52], cos_bit[stage]);
  bf1[44] = half_btf(-cospi[32], bf0[44], cospi[32], bf0[51], cos_bit[stage]);
  bf1[45] = half_btf(-cospi[32], bf0[45], cospi[32], bf0[50], cos_bit[stage]);
  bf1[46] = half_btf(-cospi[32], bf0[46], cospi[32], bf0[49], cos_bit[stage]);
  bf1[47] = half_btf(-cospi[32], bf0[47], cospi[32], bf0[48], cos_bit[stage]);
  bf1[48] = half_btf(cospi[32], bf0[47], cospi[32], bf0[48], cos_bit[stage]);
  bf1[49] = half_btf(cospi[32], bf0[46], cospi[32], bf0[49], cos_bit[stage]);
  bf1[50] = half_btf(cospi[32], bf0[45], cospi[32], bf0[50], cos_bit[stage]);
  bf1[51] = half_btf(cospi[32], bf0[44], cospi[32], bf0[51], cos_bit[stage]);
  bf1[52] = half_btf(cospi[32], bf0[43], cospi[32], bf0[52], cos_bit[stage]);
  bf1[53] = half_btf(cospi[32], bf0[42], cospi[32], bf0[53], cos_bit[stage]);
  bf1[54] = half_btf(cospi[32], bf0[41], cospi[32], bf0[54], cos_bit[stage]);
  bf1[55] = half_btf(cospi[32], bf0[40], cospi[32], bf0[55], cos_bit[stage]);
  bf1[56] = bf0[56];
  bf1[57] = bf0[57];
  bf1[58] = bf0[58];
  bf1[59] = bf0[59];
  bf1[60] = bf0[60];
  bf1[61] = bf0[61];
  bf1[62] = bf0[62];
  bf1[63] = bf0[63];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 11
  stage++;
  cospi = cospi_arr(cos_bit[stage]);
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0] + bf0[63];
  bf1[1] = bf0[1] + bf0[62];
  bf1[2] = bf0[2] + bf0[61];
  bf1[3] = bf0[3] + bf0[60];
  bf1[4] = bf0[4] + bf0[59];
  bf1[5] = bf0[5] + bf0[58];
  bf1[6] = bf0[6] + bf0[57];
  bf1[7] = bf0[7] + bf0[56];
  bf1[8] = bf0[8] + bf0[55];
  bf1[9] = bf0[9] + bf0[54];
  bf1[10] = bf0[10] + bf0[53];
  bf1[11] = bf0[11] + bf0[52];
  bf1[12] = bf0[12] + bf0[51];
  bf1[13] = bf0[13] + bf0[50];
  bf1[14] = bf0[14] + bf0[49];
  bf1[15] = bf0[15] + bf0[48];
  bf1[16] = bf0[16] + bf0[47];
  bf1[17] = bf0[17] + bf0[46];
  bf1[18] = bf0[18] + bf0[45];
  bf1[19] = bf0[19] + bf0[44];
  bf1[20] = bf0[20] + bf0[43];
  bf1[21] = bf0[21] + bf0[42];
  bf1[22] = bf0[22] + bf0[41];
  bf1[23] = bf0[23] + bf0[40];
  bf1[24] = bf0[24] + bf0[39];
  bf1[25] = bf0[25] + bf0[38];
  bf1[26] = bf0[26] + bf0[37];
  bf1[27] = bf0[27] + bf0[36];
  bf1[28] = bf0[28] + bf0[35];
  bf1[29] = bf0[29] + bf0[34];
  bf1[30] = bf0[30] + bf0[33];
  bf1[31] = bf0[31] + bf0[32];
  bf1[32] = bf0[31] - bf0[32];
  bf1[33] = bf0[30] - bf0[33];
  bf1[34] = bf0[29] - bf0[34];
  bf1[35] = bf0[28] - bf0[35];
  bf1[36] = bf0[27] - bf0[36];
  bf1[37] = bf0[26] - bf0[37];
  bf1[38] = bf0[25] - bf0[38];
  bf1[39] = bf0[24] - bf0[39];
  bf1[40] = bf0[23] - bf0[40];
  bf1[41] = bf0[22] - bf0[41];
  bf1[42] = bf0[21] - bf0[42];
  bf1[43] = bf0[20] - bf0[43];
  bf1[44] = bf0[19] - bf0[44];
  bf1[45] = bf0[18] - bf0[45];
  bf1[46] = bf0[17] - bf0[46];
  bf1[47] = bf0[16] - bf0[47];
  bf1[48] = bf0[15] - bf0[48];
  bf1[49] = bf0[14] - bf0[49];
  bf1[50] = bf0[13] - bf0[50];
  bf1[51] = bf0[12] - bf0[51];
  bf1[52] = bf0[11] - bf0[52];
  bf1[53] = bf0[10] - bf0[53];
  bf1[54] = bf0[9] - bf0[54];
  bf1[55] = bf0[8] - bf0[55];
  bf1[56] = bf0[7] - bf0[56];
  bf1[57] = bf0[6] - bf0[57];
  bf1[58] = bf0[5] - bf0[58];
  bf1[59] = bf0[4] - bf0[59];
  bf1[60] = bf0[3] - bf0[60];
  bf1[61] = bf0[2] - bf0[61];
  bf1[62] = bf0[1] - bf0[62];
  bf1[63] = bf0[0] - bf0[63];
  range_check(stage, input, bf1, size, stage_range[stage]);
}
#endif  // CONFIG_TX64X64
