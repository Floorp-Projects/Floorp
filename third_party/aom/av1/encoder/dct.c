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

#include <assert.h>
#include <math.h>

#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"
#include "./av1_rtcd.h"
#include "aom_dsp/fwd_txfm.h"
#include "aom_ports/mem.h"
#include "av1/common/blockd.h"
#include "av1/common/av1_fwd_txfm1d.h"
#include "av1/common/av1_fwd_txfm1d_cfg.h"
#include "av1/common/idct.h"

static INLINE void range_check(const tran_low_t *input, const int size,
                               const int bit) {
#if 0  // CONFIG_COEFFICIENT_RANGE_CHECKING
// TODO(angiebird): the range_check is not used because the bit range
// in fdct# is not correct. Since we are going to merge in a new version
// of fdct# from nextgenv2, we won't fix the incorrect bit range now.
  int i;
  for (i = 0; i < size; ++i) {
    assert(abs(input[i]) < (1 << bit));
  }
#else
  (void)input;
  (void)size;
  (void)bit;
#endif
}

static void fdct4(const tran_low_t *input, tran_low_t *output) {
  tran_high_t temp;
  tran_low_t step[4];

  // stage 0
  range_check(input, 4, 14);

  // stage 1
  output[0] = input[0] + input[3];
  output[1] = input[1] + input[2];
  output[2] = input[1] - input[2];
  output[3] = input[0] - input[3];

  range_check(output, 4, 15);

  // stage 2
  temp = output[0] * cospi_16_64 + output[1] * cospi_16_64;
  step[0] = (tran_low_t)fdct_round_shift(temp);
  temp = output[1] * -cospi_16_64 + output[0] * cospi_16_64;
  step[1] = (tran_low_t)fdct_round_shift(temp);
  temp = output[2] * cospi_24_64 + output[3] * cospi_8_64;
  step[2] = (tran_low_t)fdct_round_shift(temp);
  temp = output[3] * cospi_24_64 + output[2] * -cospi_8_64;
  step[3] = (tran_low_t)fdct_round_shift(temp);

  range_check(step, 4, 16);

  // stage 3
  output[0] = step[0];
  output[1] = step[2];
  output[2] = step[1];
  output[3] = step[3];

  range_check(output, 4, 16);
}

static void fdct8(const tran_low_t *input, tran_low_t *output) {
  tran_high_t temp;
  tran_low_t step[8];

  // stage 0
  range_check(input, 8, 13);

  // stage 1
  output[0] = input[0] + input[7];
  output[1] = input[1] + input[6];
  output[2] = input[2] + input[5];
  output[3] = input[3] + input[4];
  output[4] = input[3] - input[4];
  output[5] = input[2] - input[5];
  output[6] = input[1] - input[6];
  output[7] = input[0] - input[7];

  range_check(output, 8, 14);

  // stage 2
  step[0] = output[0] + output[3];
  step[1] = output[1] + output[2];
  step[2] = output[1] - output[2];
  step[3] = output[0] - output[3];
  step[4] = output[4];
  temp = output[5] * -cospi_16_64 + output[6] * cospi_16_64;
  step[5] = (tran_low_t)fdct_round_shift(temp);
  temp = output[6] * cospi_16_64 + output[5] * cospi_16_64;
  step[6] = (tran_low_t)fdct_round_shift(temp);
  step[7] = output[7];

  range_check(step, 8, 15);

  // stage 3
  temp = step[0] * cospi_16_64 + step[1] * cospi_16_64;
  output[0] = (tran_low_t)fdct_round_shift(temp);
  temp = step[1] * -cospi_16_64 + step[0] * cospi_16_64;
  output[1] = (tran_low_t)fdct_round_shift(temp);
  temp = step[2] * cospi_24_64 + step[3] * cospi_8_64;
  output[2] = (tran_low_t)fdct_round_shift(temp);
  temp = step[3] * cospi_24_64 + step[2] * -cospi_8_64;
  output[3] = (tran_low_t)fdct_round_shift(temp);
  output[4] = step[4] + step[5];
  output[5] = step[4] - step[5];
  output[6] = step[7] - step[6];
  output[7] = step[7] + step[6];

  range_check(output, 8, 16);

  // stage 4
  step[0] = output[0];
  step[1] = output[1];
  step[2] = output[2];
  step[3] = output[3];
  temp = output[4] * cospi_28_64 + output[7] * cospi_4_64;
  step[4] = (tran_low_t)fdct_round_shift(temp);
  temp = output[5] * cospi_12_64 + output[6] * cospi_20_64;
  step[5] = (tran_low_t)fdct_round_shift(temp);
  temp = output[6] * cospi_12_64 + output[5] * -cospi_20_64;
  step[6] = (tran_low_t)fdct_round_shift(temp);
  temp = output[7] * cospi_28_64 + output[4] * -cospi_4_64;
  step[7] = (tran_low_t)fdct_round_shift(temp);

  range_check(step, 8, 16);

  // stage 5
  output[0] = step[0];
  output[1] = step[4];
  output[2] = step[2];
  output[3] = step[6];
  output[4] = step[1];
  output[5] = step[5];
  output[6] = step[3];
  output[7] = step[7];

  range_check(output, 8, 16);
}

static void fdct16(const tran_low_t *input, tran_low_t *output) {
  tran_high_t temp;
  tran_low_t step[16];

  // stage 0
  range_check(input, 16, 13);

  // stage 1
  output[0] = input[0] + input[15];
  output[1] = input[1] + input[14];
  output[2] = input[2] + input[13];
  output[3] = input[3] + input[12];
  output[4] = input[4] + input[11];
  output[5] = input[5] + input[10];
  output[6] = input[6] + input[9];
  output[7] = input[7] + input[8];
  output[8] = input[7] - input[8];
  output[9] = input[6] - input[9];
  output[10] = input[5] - input[10];
  output[11] = input[4] - input[11];
  output[12] = input[3] - input[12];
  output[13] = input[2] - input[13];
  output[14] = input[1] - input[14];
  output[15] = input[0] - input[15];

  range_check(output, 16, 14);

  // stage 2
  step[0] = output[0] + output[7];
  step[1] = output[1] + output[6];
  step[2] = output[2] + output[5];
  step[3] = output[3] + output[4];
  step[4] = output[3] - output[4];
  step[5] = output[2] - output[5];
  step[6] = output[1] - output[6];
  step[7] = output[0] - output[7];
  step[8] = output[8];
  step[9] = output[9];
  temp = output[10] * -cospi_16_64 + output[13] * cospi_16_64;
  step[10] = (tran_low_t)fdct_round_shift(temp);
  temp = output[11] * -cospi_16_64 + output[12] * cospi_16_64;
  step[11] = (tran_low_t)fdct_round_shift(temp);
  temp = output[12] * cospi_16_64 + output[11] * cospi_16_64;
  step[12] = (tran_low_t)fdct_round_shift(temp);
  temp = output[13] * cospi_16_64 + output[10] * cospi_16_64;
  step[13] = (tran_low_t)fdct_round_shift(temp);
  step[14] = output[14];
  step[15] = output[15];

  range_check(step, 16, 15);

  // stage 3
  output[0] = step[0] + step[3];
  output[1] = step[1] + step[2];
  output[2] = step[1] - step[2];
  output[3] = step[0] - step[3];
  output[4] = step[4];
  temp = step[5] * -cospi_16_64 + step[6] * cospi_16_64;
  output[5] = (tran_low_t)fdct_round_shift(temp);
  temp = step[6] * cospi_16_64 + step[5] * cospi_16_64;
  output[6] = (tran_low_t)fdct_round_shift(temp);
  output[7] = step[7];
  output[8] = step[8] + step[11];
  output[9] = step[9] + step[10];
  output[10] = step[9] - step[10];
  output[11] = step[8] - step[11];
  output[12] = step[15] - step[12];
  output[13] = step[14] - step[13];
  output[14] = step[14] + step[13];
  output[15] = step[15] + step[12];

  range_check(output, 16, 16);

  // stage 4
  temp = output[0] * cospi_16_64 + output[1] * cospi_16_64;
  step[0] = (tran_low_t)fdct_round_shift(temp);
  temp = output[1] * -cospi_16_64 + output[0] * cospi_16_64;
  step[1] = (tran_low_t)fdct_round_shift(temp);
  temp = output[2] * cospi_24_64 + output[3] * cospi_8_64;
  step[2] = (tran_low_t)fdct_round_shift(temp);
  temp = output[3] * cospi_24_64 + output[2] * -cospi_8_64;
  step[3] = (tran_low_t)fdct_round_shift(temp);
  step[4] = output[4] + output[5];
  step[5] = output[4] - output[5];
  step[6] = output[7] - output[6];
  step[7] = output[7] + output[6];
  step[8] = output[8];
  temp = output[9] * -cospi_8_64 + output[14] * cospi_24_64;
  step[9] = (tran_low_t)fdct_round_shift(temp);
  temp = output[10] * -cospi_24_64 + output[13] * -cospi_8_64;
  step[10] = (tran_low_t)fdct_round_shift(temp);
  step[11] = output[11];
  step[12] = output[12];
  temp = output[13] * cospi_24_64 + output[10] * -cospi_8_64;
  step[13] = (tran_low_t)fdct_round_shift(temp);
  temp = output[14] * cospi_8_64 + output[9] * cospi_24_64;
  step[14] = (tran_low_t)fdct_round_shift(temp);
  step[15] = output[15];

  range_check(step, 16, 16);

  // stage 5
  output[0] = step[0];
  output[1] = step[1];
  output[2] = step[2];
  output[3] = step[3];
  temp = step[4] * cospi_28_64 + step[7] * cospi_4_64;
  output[4] = (tran_low_t)fdct_round_shift(temp);
  temp = step[5] * cospi_12_64 + step[6] * cospi_20_64;
  output[5] = (tran_low_t)fdct_round_shift(temp);
  temp = step[6] * cospi_12_64 + step[5] * -cospi_20_64;
  output[6] = (tran_low_t)fdct_round_shift(temp);
  temp = step[7] * cospi_28_64 + step[4] * -cospi_4_64;
  output[7] = (tran_low_t)fdct_round_shift(temp);
  output[8] = step[8] + step[9];
  output[9] = step[8] - step[9];
  output[10] = step[11] - step[10];
  output[11] = step[11] + step[10];
  output[12] = step[12] + step[13];
  output[13] = step[12] - step[13];
  output[14] = step[15] - step[14];
  output[15] = step[15] + step[14];

  range_check(output, 16, 16);

  // stage 6
  step[0] = output[0];
  step[1] = output[1];
  step[2] = output[2];
  step[3] = output[3];
  step[4] = output[4];
  step[5] = output[5];
  step[6] = output[6];
  step[7] = output[7];
  temp = output[8] * cospi_30_64 + output[15] * cospi_2_64;
  step[8] = (tran_low_t)fdct_round_shift(temp);
  temp = output[9] * cospi_14_64 + output[14] * cospi_18_64;
  step[9] = (tran_low_t)fdct_round_shift(temp);
  temp = output[10] * cospi_22_64 + output[13] * cospi_10_64;
  step[10] = (tran_low_t)fdct_round_shift(temp);
  temp = output[11] * cospi_6_64 + output[12] * cospi_26_64;
  step[11] = (tran_low_t)fdct_round_shift(temp);
  temp = output[12] * cospi_6_64 + output[11] * -cospi_26_64;
  step[12] = (tran_low_t)fdct_round_shift(temp);
  temp = output[13] * cospi_22_64 + output[10] * -cospi_10_64;
  step[13] = (tran_low_t)fdct_round_shift(temp);
  temp = output[14] * cospi_14_64 + output[9] * -cospi_18_64;
  step[14] = (tran_low_t)fdct_round_shift(temp);
  temp = output[15] * cospi_30_64 + output[8] * -cospi_2_64;
  step[15] = (tran_low_t)fdct_round_shift(temp);

  range_check(step, 16, 16);

  // stage 7
  output[0] = step[0];
  output[1] = step[8];
  output[2] = step[4];
  output[3] = step[12];
  output[4] = step[2];
  output[5] = step[10];
  output[6] = step[6];
  output[7] = step[14];
  output[8] = step[1];
  output[9] = step[9];
  output[10] = step[5];
  output[11] = step[13];
  output[12] = step[3];
  output[13] = step[11];
  output[14] = step[7];
  output[15] = step[15];

  range_check(output, 16, 16);
}

static void fdct32(const tran_low_t *input, tran_low_t *output) {
  tran_high_t temp;
  tran_low_t step[32];

  // stage 0
  range_check(input, 32, 14);

  // stage 1
  output[0] = input[0] + input[31];
  output[1] = input[1] + input[30];
  output[2] = input[2] + input[29];
  output[3] = input[3] + input[28];
  output[4] = input[4] + input[27];
  output[5] = input[5] + input[26];
  output[6] = input[6] + input[25];
  output[7] = input[7] + input[24];
  output[8] = input[8] + input[23];
  output[9] = input[9] + input[22];
  output[10] = input[10] + input[21];
  output[11] = input[11] + input[20];
  output[12] = input[12] + input[19];
  output[13] = input[13] + input[18];
  output[14] = input[14] + input[17];
  output[15] = input[15] + input[16];
  output[16] = input[15] - input[16];
  output[17] = input[14] - input[17];
  output[18] = input[13] - input[18];
  output[19] = input[12] - input[19];
  output[20] = input[11] - input[20];
  output[21] = input[10] - input[21];
  output[22] = input[9] - input[22];
  output[23] = input[8] - input[23];
  output[24] = input[7] - input[24];
  output[25] = input[6] - input[25];
  output[26] = input[5] - input[26];
  output[27] = input[4] - input[27];
  output[28] = input[3] - input[28];
  output[29] = input[2] - input[29];
  output[30] = input[1] - input[30];
  output[31] = input[0] - input[31];

  range_check(output, 32, 15);

  // stage 2
  step[0] = output[0] + output[15];
  step[1] = output[1] + output[14];
  step[2] = output[2] + output[13];
  step[3] = output[3] + output[12];
  step[4] = output[4] + output[11];
  step[5] = output[5] + output[10];
  step[6] = output[6] + output[9];
  step[7] = output[7] + output[8];
  step[8] = output[7] - output[8];
  step[9] = output[6] - output[9];
  step[10] = output[5] - output[10];
  step[11] = output[4] - output[11];
  step[12] = output[3] - output[12];
  step[13] = output[2] - output[13];
  step[14] = output[1] - output[14];
  step[15] = output[0] - output[15];
  step[16] = output[16];
  step[17] = output[17];
  step[18] = output[18];
  step[19] = output[19];
  temp = output[20] * -cospi_16_64 + output[27] * cospi_16_64;
  step[20] = (tran_low_t)fdct_round_shift(temp);
  temp = output[21] * -cospi_16_64 + output[26] * cospi_16_64;
  step[21] = (tran_low_t)fdct_round_shift(temp);
  temp = output[22] * -cospi_16_64 + output[25] * cospi_16_64;
  step[22] = (tran_low_t)fdct_round_shift(temp);
  temp = output[23] * -cospi_16_64 + output[24] * cospi_16_64;
  step[23] = (tran_low_t)fdct_round_shift(temp);
  temp = output[24] * cospi_16_64 + output[23] * cospi_16_64;
  step[24] = (tran_low_t)fdct_round_shift(temp);
  temp = output[25] * cospi_16_64 + output[22] * cospi_16_64;
  step[25] = (tran_low_t)fdct_round_shift(temp);
  temp = output[26] * cospi_16_64 + output[21] * cospi_16_64;
  step[26] = (tran_low_t)fdct_round_shift(temp);
  temp = output[27] * cospi_16_64 + output[20] * cospi_16_64;
  step[27] = (tran_low_t)fdct_round_shift(temp);
  step[28] = output[28];
  step[29] = output[29];
  step[30] = output[30];
  step[31] = output[31];

  range_check(step, 32, 16);

  // stage 3
  output[0] = step[0] + step[7];
  output[1] = step[1] + step[6];
  output[2] = step[2] + step[5];
  output[3] = step[3] + step[4];
  output[4] = step[3] - step[4];
  output[5] = step[2] - step[5];
  output[6] = step[1] - step[6];
  output[7] = step[0] - step[7];
  output[8] = step[8];
  output[9] = step[9];
  temp = step[10] * -cospi_16_64 + step[13] * cospi_16_64;
  output[10] = (tran_low_t)fdct_round_shift(temp);
  temp = step[11] * -cospi_16_64 + step[12] * cospi_16_64;
  output[11] = (tran_low_t)fdct_round_shift(temp);
  temp = step[12] * cospi_16_64 + step[11] * cospi_16_64;
  output[12] = (tran_low_t)fdct_round_shift(temp);
  temp = step[13] * cospi_16_64 + step[10] * cospi_16_64;
  output[13] = (tran_low_t)fdct_round_shift(temp);
  output[14] = step[14];
  output[15] = step[15];
  output[16] = step[16] + step[23];
  output[17] = step[17] + step[22];
  output[18] = step[18] + step[21];
  output[19] = step[19] + step[20];
  output[20] = step[19] - step[20];
  output[21] = step[18] - step[21];
  output[22] = step[17] - step[22];
  output[23] = step[16] - step[23];
  output[24] = step[31] - step[24];
  output[25] = step[30] - step[25];
  output[26] = step[29] - step[26];
  output[27] = step[28] - step[27];
  output[28] = step[28] + step[27];
  output[29] = step[29] + step[26];
  output[30] = step[30] + step[25];
  output[31] = step[31] + step[24];

  range_check(output, 32, 17);

  // stage 4
  step[0] = output[0] + output[3];
  step[1] = output[1] + output[2];
  step[2] = output[1] - output[2];
  step[3] = output[0] - output[3];
  step[4] = output[4];
  temp = output[5] * -cospi_16_64 + output[6] * cospi_16_64;
  step[5] = (tran_low_t)fdct_round_shift(temp);
  temp = output[6] * cospi_16_64 + output[5] * cospi_16_64;
  step[6] = (tran_low_t)fdct_round_shift(temp);
  step[7] = output[7];
  step[8] = output[8] + output[11];
  step[9] = output[9] + output[10];
  step[10] = output[9] - output[10];
  step[11] = output[8] - output[11];
  step[12] = output[15] - output[12];
  step[13] = output[14] - output[13];
  step[14] = output[14] + output[13];
  step[15] = output[15] + output[12];
  step[16] = output[16];
  step[17] = output[17];
  temp = output[18] * -cospi_8_64 + output[29] * cospi_24_64;
  step[18] = (tran_low_t)fdct_round_shift(temp);
  temp = output[19] * -cospi_8_64 + output[28] * cospi_24_64;
  step[19] = (tran_low_t)fdct_round_shift(temp);
  temp = output[20] * -cospi_24_64 + output[27] * -cospi_8_64;
  step[20] = (tran_low_t)fdct_round_shift(temp);
  temp = output[21] * -cospi_24_64 + output[26] * -cospi_8_64;
  step[21] = (tran_low_t)fdct_round_shift(temp);
  step[22] = output[22];
  step[23] = output[23];
  step[24] = output[24];
  step[25] = output[25];
  temp = output[26] * cospi_24_64 + output[21] * -cospi_8_64;
  step[26] = (tran_low_t)fdct_round_shift(temp);
  temp = output[27] * cospi_24_64 + output[20] * -cospi_8_64;
  step[27] = (tran_low_t)fdct_round_shift(temp);
  temp = output[28] * cospi_8_64 + output[19] * cospi_24_64;
  step[28] = (tran_low_t)fdct_round_shift(temp);
  temp = output[29] * cospi_8_64 + output[18] * cospi_24_64;
  step[29] = (tran_low_t)fdct_round_shift(temp);
  step[30] = output[30];
  step[31] = output[31];

  range_check(step, 32, 18);

  // stage 5
  temp = step[0] * cospi_16_64 + step[1] * cospi_16_64;
  output[0] = (tran_low_t)fdct_round_shift(temp);
  temp = step[1] * -cospi_16_64 + step[0] * cospi_16_64;
  output[1] = (tran_low_t)fdct_round_shift(temp);
  temp = step[2] * cospi_24_64 + step[3] * cospi_8_64;
  output[2] = (tran_low_t)fdct_round_shift(temp);
  temp = step[3] * cospi_24_64 + step[2] * -cospi_8_64;
  output[3] = (tran_low_t)fdct_round_shift(temp);
  output[4] = step[4] + step[5];
  output[5] = step[4] - step[5];
  output[6] = step[7] - step[6];
  output[7] = step[7] + step[6];
  output[8] = step[8];
  temp = step[9] * -cospi_8_64 + step[14] * cospi_24_64;
  output[9] = (tran_low_t)fdct_round_shift(temp);
  temp = step[10] * -cospi_24_64 + step[13] * -cospi_8_64;
  output[10] = (tran_low_t)fdct_round_shift(temp);
  output[11] = step[11];
  output[12] = step[12];
  temp = step[13] * cospi_24_64 + step[10] * -cospi_8_64;
  output[13] = (tran_low_t)fdct_round_shift(temp);
  temp = step[14] * cospi_8_64 + step[9] * cospi_24_64;
  output[14] = (tran_low_t)fdct_round_shift(temp);
  output[15] = step[15];
  output[16] = step[16] + step[19];
  output[17] = step[17] + step[18];
  output[18] = step[17] - step[18];
  output[19] = step[16] - step[19];
  output[20] = step[23] - step[20];
  output[21] = step[22] - step[21];
  output[22] = step[22] + step[21];
  output[23] = step[23] + step[20];
  output[24] = step[24] + step[27];
  output[25] = step[25] + step[26];
  output[26] = step[25] - step[26];
  output[27] = step[24] - step[27];
  output[28] = step[31] - step[28];
  output[29] = step[30] - step[29];
  output[30] = step[30] + step[29];
  output[31] = step[31] + step[28];

  range_check(output, 32, 18);

  // stage 6
  step[0] = output[0];
  step[1] = output[1];
  step[2] = output[2];
  step[3] = output[3];
  temp = output[4] * cospi_28_64 + output[7] * cospi_4_64;
  step[4] = (tran_low_t)fdct_round_shift(temp);
  temp = output[5] * cospi_12_64 + output[6] * cospi_20_64;
  step[5] = (tran_low_t)fdct_round_shift(temp);
  temp = output[6] * cospi_12_64 + output[5] * -cospi_20_64;
  step[6] = (tran_low_t)fdct_round_shift(temp);
  temp = output[7] * cospi_28_64 + output[4] * -cospi_4_64;
  step[7] = (tran_low_t)fdct_round_shift(temp);
  step[8] = output[8] + output[9];
  step[9] = output[8] - output[9];
  step[10] = output[11] - output[10];
  step[11] = output[11] + output[10];
  step[12] = output[12] + output[13];
  step[13] = output[12] - output[13];
  step[14] = output[15] - output[14];
  step[15] = output[15] + output[14];
  step[16] = output[16];
  temp = output[17] * -cospi_4_64 + output[30] * cospi_28_64;
  step[17] = (tran_low_t)fdct_round_shift(temp);
  temp = output[18] * -cospi_28_64 + output[29] * -cospi_4_64;
  step[18] = (tran_low_t)fdct_round_shift(temp);
  step[19] = output[19];
  step[20] = output[20];
  temp = output[21] * -cospi_20_64 + output[26] * cospi_12_64;
  step[21] = (tran_low_t)fdct_round_shift(temp);
  temp = output[22] * -cospi_12_64 + output[25] * -cospi_20_64;
  step[22] = (tran_low_t)fdct_round_shift(temp);
  step[23] = output[23];
  step[24] = output[24];
  temp = output[25] * cospi_12_64 + output[22] * -cospi_20_64;
  step[25] = (tran_low_t)fdct_round_shift(temp);
  temp = output[26] * cospi_20_64 + output[21] * cospi_12_64;
  step[26] = (tran_low_t)fdct_round_shift(temp);
  step[27] = output[27];
  step[28] = output[28];
  temp = output[29] * cospi_28_64 + output[18] * -cospi_4_64;
  step[29] = (tran_low_t)fdct_round_shift(temp);
  temp = output[30] * cospi_4_64 + output[17] * cospi_28_64;
  step[30] = (tran_low_t)fdct_round_shift(temp);
  step[31] = output[31];

  range_check(step, 32, 18);

  // stage 7
  output[0] = step[0];
  output[1] = step[1];
  output[2] = step[2];
  output[3] = step[3];
  output[4] = step[4];
  output[5] = step[5];
  output[6] = step[6];
  output[7] = step[7];
  temp = step[8] * cospi_30_64 + step[15] * cospi_2_64;
  output[8] = (tran_low_t)fdct_round_shift(temp);
  temp = step[9] * cospi_14_64 + step[14] * cospi_18_64;
  output[9] = (tran_low_t)fdct_round_shift(temp);
  temp = step[10] * cospi_22_64 + step[13] * cospi_10_64;
  output[10] = (tran_low_t)fdct_round_shift(temp);
  temp = step[11] * cospi_6_64 + step[12] * cospi_26_64;
  output[11] = (tran_low_t)fdct_round_shift(temp);
  temp = step[12] * cospi_6_64 + step[11] * -cospi_26_64;
  output[12] = (tran_low_t)fdct_round_shift(temp);
  temp = step[13] * cospi_22_64 + step[10] * -cospi_10_64;
  output[13] = (tran_low_t)fdct_round_shift(temp);
  temp = step[14] * cospi_14_64 + step[9] * -cospi_18_64;
  output[14] = (tran_low_t)fdct_round_shift(temp);
  temp = step[15] * cospi_30_64 + step[8] * -cospi_2_64;
  output[15] = (tran_low_t)fdct_round_shift(temp);
  output[16] = step[16] + step[17];
  output[17] = step[16] - step[17];
  output[18] = step[19] - step[18];
  output[19] = step[19] + step[18];
  output[20] = step[20] + step[21];
  output[21] = step[20] - step[21];
  output[22] = step[23] - step[22];
  output[23] = step[23] + step[22];
  output[24] = step[24] + step[25];
  output[25] = step[24] - step[25];
  output[26] = step[27] - step[26];
  output[27] = step[27] + step[26];
  output[28] = step[28] + step[29];
  output[29] = step[28] - step[29];
  output[30] = step[31] - step[30];
  output[31] = step[31] + step[30];

  range_check(output, 32, 18);

  // stage 8
  step[0] = output[0];
  step[1] = output[1];
  step[2] = output[2];
  step[3] = output[3];
  step[4] = output[4];
  step[5] = output[5];
  step[6] = output[6];
  step[7] = output[7];
  step[8] = output[8];
  step[9] = output[9];
  step[10] = output[10];
  step[11] = output[11];
  step[12] = output[12];
  step[13] = output[13];
  step[14] = output[14];
  step[15] = output[15];
  temp = output[16] * cospi_31_64 + output[31] * cospi_1_64;
  step[16] = (tran_low_t)fdct_round_shift(temp);
  temp = output[17] * cospi_15_64 + output[30] * cospi_17_64;
  step[17] = (tran_low_t)fdct_round_shift(temp);
  temp = output[18] * cospi_23_64 + output[29] * cospi_9_64;
  step[18] = (tran_low_t)fdct_round_shift(temp);
  temp = output[19] * cospi_7_64 + output[28] * cospi_25_64;
  step[19] = (tran_low_t)fdct_round_shift(temp);
  temp = output[20] * cospi_27_64 + output[27] * cospi_5_64;
  step[20] = (tran_low_t)fdct_round_shift(temp);
  temp = output[21] * cospi_11_64 + output[26] * cospi_21_64;
  step[21] = (tran_low_t)fdct_round_shift(temp);
  temp = output[22] * cospi_19_64 + output[25] * cospi_13_64;
  step[22] = (tran_low_t)fdct_round_shift(temp);
  temp = output[23] * cospi_3_64 + output[24] * cospi_29_64;
  step[23] = (tran_low_t)fdct_round_shift(temp);
  temp = output[24] * cospi_3_64 + output[23] * -cospi_29_64;
  step[24] = (tran_low_t)fdct_round_shift(temp);
  temp = output[25] * cospi_19_64 + output[22] * -cospi_13_64;
  step[25] = (tran_low_t)fdct_round_shift(temp);
  temp = output[26] * cospi_11_64 + output[21] * -cospi_21_64;
  step[26] = (tran_low_t)fdct_round_shift(temp);
  temp = output[27] * cospi_27_64 + output[20] * -cospi_5_64;
  step[27] = (tran_low_t)fdct_round_shift(temp);
  temp = output[28] * cospi_7_64 + output[19] * -cospi_25_64;
  step[28] = (tran_low_t)fdct_round_shift(temp);
  temp = output[29] * cospi_23_64 + output[18] * -cospi_9_64;
  step[29] = (tran_low_t)fdct_round_shift(temp);
  temp = output[30] * cospi_15_64 + output[17] * -cospi_17_64;
  step[30] = (tran_low_t)fdct_round_shift(temp);
  temp = output[31] * cospi_31_64 + output[16] * -cospi_1_64;
  step[31] = (tran_low_t)fdct_round_shift(temp);

  range_check(step, 32, 18);

  // stage 9
  output[0] = step[0];
  output[1] = step[16];
  output[2] = step[8];
  output[3] = step[24];
  output[4] = step[4];
  output[5] = step[20];
  output[6] = step[12];
  output[7] = step[28];
  output[8] = step[2];
  output[9] = step[18];
  output[10] = step[10];
  output[11] = step[26];
  output[12] = step[6];
  output[13] = step[22];
  output[14] = step[14];
  output[15] = step[30];
  output[16] = step[1];
  output[17] = step[17];
  output[18] = step[9];
  output[19] = step[25];
  output[20] = step[5];
  output[21] = step[21];
  output[22] = step[13];
  output[23] = step[29];
  output[24] = step[3];
  output[25] = step[19];
  output[26] = step[11];
  output[27] = step[27];
  output[28] = step[7];
  output[29] = step[23];
  output[30] = step[15];
  output[31] = step[31];

  range_check(output, 32, 18);
}

#ifndef AV1_DCT_GTEST

static void fadst4(const tran_low_t *input, tran_low_t *output) {
  tran_high_t x0, x1, x2, x3;
  tran_high_t s0, s1, s2, s3, s4, s5, s6, s7;

  x0 = input[0];
  x1 = input[1];
  x2 = input[2];
  x3 = input[3];

  if (!(x0 | x1 | x2 | x3)) {
    output[0] = output[1] = output[2] = output[3] = 0;
    return;
  }

  s0 = sinpi_1_9 * x0;
  s1 = sinpi_4_9 * x0;
  s2 = sinpi_2_9 * x1;
  s3 = sinpi_1_9 * x1;
  s4 = sinpi_3_9 * x2;
  s5 = sinpi_4_9 * x3;
  s6 = sinpi_2_9 * x3;
  s7 = x0 + x1 - x3;

  x0 = s0 + s2 + s5;
  x1 = sinpi_3_9 * s7;
  x2 = s1 - s3 + s6;
  x3 = s4;

  s0 = x0 + x3;
  s1 = x1;
  s2 = x2 - x3;
  s3 = x2 - x0 + x3;

  // 1-D transform scaling factor is sqrt(2).
  output[0] = (tran_low_t)fdct_round_shift(s0);
  output[1] = (tran_low_t)fdct_round_shift(s1);
  output[2] = (tran_low_t)fdct_round_shift(s2);
  output[3] = (tran_low_t)fdct_round_shift(s3);
}

static void fadst8(const tran_low_t *input, tran_low_t *output) {
  tran_high_t s0, s1, s2, s3, s4, s5, s6, s7;

  tran_high_t x0 = input[7];
  tran_high_t x1 = input[0];
  tran_high_t x2 = input[5];
  tran_high_t x3 = input[2];
  tran_high_t x4 = input[3];
  tran_high_t x5 = input[4];
  tran_high_t x6 = input[1];
  tran_high_t x7 = input[6];

  // stage 1
  s0 = cospi_2_64 * x0 + cospi_30_64 * x1;
  s1 = cospi_30_64 * x0 - cospi_2_64 * x1;
  s2 = cospi_10_64 * x2 + cospi_22_64 * x3;
  s3 = cospi_22_64 * x2 - cospi_10_64 * x3;
  s4 = cospi_18_64 * x4 + cospi_14_64 * x5;
  s5 = cospi_14_64 * x4 - cospi_18_64 * x5;
  s6 = cospi_26_64 * x6 + cospi_6_64 * x7;
  s7 = cospi_6_64 * x6 - cospi_26_64 * x7;

  x0 = s0 + s4;
  x1 = s1 + s5;
  x2 = s2 + s6;
  x3 = s3 + s7;
  x4 = fdct_round_shift(s0 - s4);
  x5 = fdct_round_shift(s1 - s5);
  x6 = fdct_round_shift(s2 - s6);
  x7 = fdct_round_shift(s3 - s7);

  // stage 2
  s0 = x0;
  s1 = x1;
  s2 = x2;
  s3 = x3;
  s4 = cospi_8_64 * x4 + cospi_24_64 * x5;
  s5 = cospi_24_64 * x4 - cospi_8_64 * x5;
  s6 = -cospi_24_64 * x6 + cospi_8_64 * x7;
  s7 = cospi_8_64 * x6 + cospi_24_64 * x7;

  x0 = fdct_round_shift(s0 + s2);
  x1 = fdct_round_shift(s1 + s3);
  x2 = fdct_round_shift(s0 - s2);
  x3 = fdct_round_shift(s1 - s3);
  x4 = fdct_round_shift(s4 + s6);
  x5 = fdct_round_shift(s5 + s7);
  x6 = fdct_round_shift(s4 - s6);
  x7 = fdct_round_shift(s5 - s7);

  // stage 3
  s2 = cospi_16_64 * (x2 + x3);
  s3 = cospi_16_64 * (x2 - x3);
  s6 = cospi_16_64 * (x6 + x7);
  s7 = cospi_16_64 * (x6 - x7);

  x2 = fdct_round_shift(s2);
  x3 = fdct_round_shift(s3);
  x6 = fdct_round_shift(s6);
  x7 = fdct_round_shift(s7);

  output[0] = (tran_low_t)x0;
  output[1] = (tran_low_t)-x4;
  output[2] = (tran_low_t)x6;
  output[3] = (tran_low_t)-x2;
  output[4] = (tran_low_t)x3;
  output[5] = (tran_low_t)-x7;
  output[6] = (tran_low_t)x5;
  output[7] = (tran_low_t)-x1;
}

static void fadst16(const tran_low_t *input, tran_low_t *output) {
  tran_high_t s0, s1, s2, s3, s4, s5, s6, s7, s8;
  tran_high_t s9, s10, s11, s12, s13, s14, s15;

  tran_high_t x0 = input[15];
  tran_high_t x1 = input[0];
  tran_high_t x2 = input[13];
  tran_high_t x3 = input[2];
  tran_high_t x4 = input[11];
  tran_high_t x5 = input[4];
  tran_high_t x6 = input[9];
  tran_high_t x7 = input[6];
  tran_high_t x8 = input[7];
  tran_high_t x9 = input[8];
  tran_high_t x10 = input[5];
  tran_high_t x11 = input[10];
  tran_high_t x12 = input[3];
  tran_high_t x13 = input[12];
  tran_high_t x14 = input[1];
  tran_high_t x15 = input[14];

  // stage 1
  s0 = x0 * cospi_1_64 + x1 * cospi_31_64;
  s1 = x0 * cospi_31_64 - x1 * cospi_1_64;
  s2 = x2 * cospi_5_64 + x3 * cospi_27_64;
  s3 = x2 * cospi_27_64 - x3 * cospi_5_64;
  s4 = x4 * cospi_9_64 + x5 * cospi_23_64;
  s5 = x4 * cospi_23_64 - x5 * cospi_9_64;
  s6 = x6 * cospi_13_64 + x7 * cospi_19_64;
  s7 = x6 * cospi_19_64 - x7 * cospi_13_64;
  s8 = x8 * cospi_17_64 + x9 * cospi_15_64;
  s9 = x8 * cospi_15_64 - x9 * cospi_17_64;
  s10 = x10 * cospi_21_64 + x11 * cospi_11_64;
  s11 = x10 * cospi_11_64 - x11 * cospi_21_64;
  s12 = x12 * cospi_25_64 + x13 * cospi_7_64;
  s13 = x12 * cospi_7_64 - x13 * cospi_25_64;
  s14 = x14 * cospi_29_64 + x15 * cospi_3_64;
  s15 = x14 * cospi_3_64 - x15 * cospi_29_64;

  x0 = s0 + s8;
  x1 = s1 + s9;
  x2 = s2 + s10;
  x3 = s3 + s11;
  x4 = s4 + s12;
  x5 = s5 + s13;
  x6 = s6 + s14;
  x7 = s7 + s15;

  x8 = fdct_round_shift(s0 - s8);
  x9 = fdct_round_shift(s1 - s9);
  x10 = fdct_round_shift(s2 - s10);
  x11 = fdct_round_shift(s3 - s11);
  x12 = fdct_round_shift(s4 - s12);
  x13 = fdct_round_shift(s5 - s13);
  x14 = fdct_round_shift(s6 - s14);
  x15 = fdct_round_shift(s7 - s15);

  // stage 2
  s0 = x0;
  s1 = x1;
  s2 = x2;
  s3 = x3;
  s4 = x4;
  s5 = x5;
  s6 = x6;
  s7 = x7;
  s8 = x8 * cospi_4_64 + x9 * cospi_28_64;
  s9 = x8 * cospi_28_64 - x9 * cospi_4_64;
  s10 = x10 * cospi_20_64 + x11 * cospi_12_64;
  s11 = x10 * cospi_12_64 - x11 * cospi_20_64;
  s12 = -x12 * cospi_28_64 + x13 * cospi_4_64;
  s13 = x12 * cospi_4_64 + x13 * cospi_28_64;
  s14 = -x14 * cospi_12_64 + x15 * cospi_20_64;
  s15 = x14 * cospi_20_64 + x15 * cospi_12_64;

  x0 = s0 + s4;
  x1 = s1 + s5;
  x2 = s2 + s6;
  x3 = s3 + s7;
  x4 = fdct_round_shift(s0 - s4);
  x5 = fdct_round_shift(s1 - s5);
  x6 = fdct_round_shift(s2 - s6);
  x7 = fdct_round_shift(s3 - s7);

  x8 = s8 + s12;
  x9 = s9 + s13;
  x10 = s10 + s14;
  x11 = s11 + s15;
  x12 = fdct_round_shift(s8 - s12);
  x13 = fdct_round_shift(s9 - s13);
  x14 = fdct_round_shift(s10 - s14);
  x15 = fdct_round_shift(s11 - s15);

  // stage 3
  s0 = x0;
  s1 = x1;
  s2 = x2;
  s3 = x3;
  s4 = x4 * cospi_8_64 + x5 * cospi_24_64;
  s5 = x4 * cospi_24_64 - x5 * cospi_8_64;
  s6 = -x6 * cospi_24_64 + x7 * cospi_8_64;
  s7 = x6 * cospi_8_64 + x7 * cospi_24_64;
  s8 = x8;
  s9 = x9;
  s10 = x10;
  s11 = x11;
  s12 = x12 * cospi_8_64 + x13 * cospi_24_64;
  s13 = x12 * cospi_24_64 - x13 * cospi_8_64;
  s14 = -x14 * cospi_24_64 + x15 * cospi_8_64;
  s15 = x14 * cospi_8_64 + x15 * cospi_24_64;

  x0 = fdct_round_shift(s0 + s2);
  x1 = fdct_round_shift(s1 + s3);
  x2 = fdct_round_shift(s0 - s2);
  x3 = fdct_round_shift(s1 - s3);

  x4 = fdct_round_shift(s4 + s6);
  x5 = fdct_round_shift(s5 + s7);
  x6 = fdct_round_shift(s4 - s6);
  x7 = fdct_round_shift(s5 - s7);

  x8 = fdct_round_shift(s8 + s10);
  x9 = fdct_round_shift(s9 + s11);
  x10 = fdct_round_shift(s8 - s10);
  x11 = fdct_round_shift(s9 - s11);

  x12 = fdct_round_shift(s12 + s14);
  x13 = fdct_round_shift(s13 + s15);
  x14 = fdct_round_shift(s12 - s14);
  x15 = fdct_round_shift(s13 - s15);

  // stage 4
  s2 = (-cospi_16_64) * (x2 + x3);
  s3 = cospi_16_64 * (x2 - x3);
  s6 = cospi_16_64 * (x6 + x7);
  s7 = cospi_16_64 * (-x6 + x7);
  s10 = cospi_16_64 * (x10 + x11);
  s11 = cospi_16_64 * (-x10 + x11);
  s14 = (-cospi_16_64) * (x14 + x15);
  s15 = cospi_16_64 * (x14 - x15);

  x2 = fdct_round_shift(s2);
  x3 = fdct_round_shift(s3);
  x6 = fdct_round_shift(s6);
  x7 = fdct_round_shift(s7);
  x10 = fdct_round_shift(s10);
  x11 = fdct_round_shift(s11);
  x14 = fdct_round_shift(s14);
  x15 = fdct_round_shift(s15);

  output[0] = (tran_low_t)x0;
  output[1] = (tran_low_t)-x8;
  output[2] = (tran_low_t)x12;
  output[3] = (tran_low_t)-x4;
  output[4] = (tran_low_t)x6;
  output[5] = (tran_low_t)x14;
  output[6] = (tran_low_t)x10;
  output[7] = (tran_low_t)x2;
  output[8] = (tran_low_t)x3;
  output[9] = (tran_low_t)x11;
  output[10] = (tran_low_t)x15;
  output[11] = (tran_low_t)x7;
  output[12] = (tran_low_t)x5;
  output[13] = (tran_low_t)-x13;
  output[14] = (tran_low_t)x9;
  output[15] = (tran_low_t)-x1;
}

// For use in lieu of ADST
static void fhalfright32(const tran_low_t *input, tran_low_t *output) {
  int i;
  tran_low_t inputhalf[16];
  for (i = 0; i < 16; ++i) {
    output[16 + i] = input[i] * 4;
  }
  // Multiply input by sqrt(2)
  for (i = 0; i < 16; ++i) {
    inputhalf[i] = (tran_low_t)fdct_round_shift(input[i + 16] * Sqrt2);
  }
  fdct16(inputhalf, output);
  // Note overall scaling factor is 4 times orthogonal
}

#if CONFIG_EXT_TX
// TODO(sarahparker) these functions will be removed once the highbitdepth
// codepath works properly for rectangular transforms. They have almost
// identical versions in av1_fwd_txfm1d.c, but those are currently only
// being used for square transforms.
static void fidtx4(const tran_low_t *input, tran_low_t *output) {
  int i;
  for (i = 0; i < 4; ++i)
    output[i] = (tran_low_t)fdct_round_shift(input[i] * Sqrt2);
}

static void fidtx8(const tran_low_t *input, tran_low_t *output) {
  int i;
  for (i = 0; i < 8; ++i) output[i] = input[i] * 2;
}

static void fidtx16(const tran_low_t *input, tran_low_t *output) {
  int i;
  for (i = 0; i < 16; ++i)
    output[i] = (tran_low_t)fdct_round_shift(input[i] * 2 * Sqrt2);
}

static void fidtx32(const tran_low_t *input, tran_low_t *output) {
  int i;
  for (i = 0; i < 32; ++i) output[i] = input[i] * 4;
}

static void copy_block(const int16_t *src, int src_stride, int l, int w,
                       int16_t *dest, int dest_stride) {
  int i;
  for (i = 0; i < l; ++i) {
    memcpy(dest + dest_stride * i, src + src_stride * i, w * sizeof(int16_t));
  }
}

static void fliplr(int16_t *dest, int stride, int l, int w) {
  int i, j;
  for (i = 0; i < l; ++i) {
    for (j = 0; j < w / 2; ++j) {
      const int16_t tmp = dest[i * stride + j];
      dest[i * stride + j] = dest[i * stride + w - 1 - j];
      dest[i * stride + w - 1 - j] = tmp;
    }
  }
}

static void flipud(int16_t *dest, int stride, int l, int w) {
  int i, j;
  for (j = 0; j < w; ++j) {
    for (i = 0; i < l / 2; ++i) {
      const int16_t tmp = dest[i * stride + j];
      dest[i * stride + j] = dest[(l - 1 - i) * stride + j];
      dest[(l - 1 - i) * stride + j] = tmp;
    }
  }
}

static void fliplrud(int16_t *dest, int stride, int l, int w) {
  int i, j;
  for (i = 0; i < l / 2; ++i) {
    for (j = 0; j < w; ++j) {
      const int16_t tmp = dest[i * stride + j];
      dest[i * stride + j] = dest[(l - 1 - i) * stride + w - 1 - j];
      dest[(l - 1 - i) * stride + w - 1 - j] = tmp;
    }
  }
}

static void copy_fliplr(const int16_t *src, int src_stride, int l, int w,
                        int16_t *dest, int dest_stride) {
  copy_block(src, src_stride, l, w, dest, dest_stride);
  fliplr(dest, dest_stride, l, w);
}

static void copy_flipud(const int16_t *src, int src_stride, int l, int w,
                        int16_t *dest, int dest_stride) {
  copy_block(src, src_stride, l, w, dest, dest_stride);
  flipud(dest, dest_stride, l, w);
}

static void copy_fliplrud(const int16_t *src, int src_stride, int l, int w,
                          int16_t *dest, int dest_stride) {
  copy_block(src, src_stride, l, w, dest, dest_stride);
  fliplrud(dest, dest_stride, l, w);
}

static void maybe_flip_input(const int16_t **src, int *src_stride, int l, int w,
                             int16_t *buff, int tx_type) {
  switch (tx_type) {
    case DCT_DCT:
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
    case IDTX:
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST: break;
    case FLIPADST_DCT:
    case FLIPADST_ADST:
    case V_FLIPADST:
      copy_flipud(*src, *src_stride, l, w, buff, w);
      *src = buff;
      *src_stride = w;
      break;
    case DCT_FLIPADST:
    case ADST_FLIPADST:
    case H_FLIPADST:
      copy_fliplr(*src, *src_stride, l, w, buff, w);
      *src = buff;
      *src_stride = w;
      break;
    case FLIPADST_FLIPADST:
      copy_fliplrud(*src, *src_stride, l, w, buff, w);
      *src = buff;
      *src_stride = w;
      break;
    default: assert(0); break;
  }
}
#endif  // CONFIG_EXT_TX

void av1_fht4x4_c(const int16_t *input, tran_low_t *output, int stride,
                  int tx_type) {
  if (tx_type == DCT_DCT) {
    aom_fdct4x4_c(input, output, stride);
  } else {
    static const transform_2d FHT[] = {
      { fdct4, fdct4 },    // DCT_DCT
      { fadst4, fdct4 },   // ADST_DCT
      { fdct4, fadst4 },   // DCT_ADST
      { fadst4, fadst4 },  // ADST_ADST
#if CONFIG_EXT_TX
      { fadst4, fdct4 },   // FLIPADST_DCT
      { fdct4, fadst4 },   // DCT_FLIPADST
      { fadst4, fadst4 },  // FLIPADST_FLIPADST
      { fadst4, fadst4 },  // ADST_FLIPADST
      { fadst4, fadst4 },  // FLIPADST_ADST
      { fidtx4, fidtx4 },  // IDTX
      { fdct4, fidtx4 },   // V_DCT
      { fidtx4, fdct4 },   // H_DCT
      { fadst4, fidtx4 },  // V_ADST
      { fidtx4, fadst4 },  // H_ADST
      { fadst4, fidtx4 },  // V_FLIPADST
      { fidtx4, fadst4 },  // H_FLIPADST
#endif                     // CONFIG_EXT_TX
    };
    const transform_2d ht = FHT[tx_type];
    tran_low_t out[4 * 4];
    int i, j;
    tran_low_t temp_in[4], temp_out[4];

#if CONFIG_EXT_TX
    int16_t flipped_input[4 * 4];
    maybe_flip_input(&input, &stride, 4, 4, flipped_input, tx_type);
#endif

    // Columns
    for (i = 0; i < 4; ++i) {
      for (j = 0; j < 4; ++j) temp_in[j] = input[j * stride + i] * 16;
      if (i == 0 && temp_in[0]) temp_in[0] += 1;
      ht.cols(temp_in, temp_out);
      for (j = 0; j < 4; ++j) out[j * 4 + i] = temp_out[j];
    }

    // Rows
    for (i = 0; i < 4; ++i) {
      for (j = 0; j < 4; ++j) temp_in[j] = out[j + i * 4];
      ht.rows(temp_in, temp_out);
      for (j = 0; j < 4; ++j) output[j + i * 4] = (temp_out[j] + 1) >> 2;
    }
  }
}

void av1_fht4x8_c(const int16_t *input, tran_low_t *output, int stride,
                  int tx_type) {
  static const transform_2d FHT[] = {
    { fdct8, fdct4 },    // DCT_DCT
    { fadst8, fdct4 },   // ADST_DCT
    { fdct8, fadst4 },   // DCT_ADST
    { fadst8, fadst4 },  // ADST_ADST
#if CONFIG_EXT_TX
    { fadst8, fdct4 },   // FLIPADST_DCT
    { fdct8, fadst4 },   // DCT_FLIPADST
    { fadst8, fadst4 },  // FLIPADST_FLIPADST
    { fadst8, fadst4 },  // ADST_FLIPADST
    { fadst8, fadst4 },  // FLIPADST_ADST
    { fidtx8, fidtx4 },  // IDTX
    { fdct8, fidtx4 },   // V_DCT
    { fidtx8, fdct4 },   // H_DCT
    { fadst8, fidtx4 },  // V_ADST
    { fidtx8, fadst4 },  // H_ADST
    { fadst8, fidtx4 },  // V_FLIPADST
    { fidtx8, fadst4 },  // H_FLIPADST
#endif
  };
  const transform_2d ht = FHT[tx_type];
  const int n = 4;
  const int n2 = 8;
  tran_low_t out[8 * 4];
  tran_low_t temp_in[8], temp_out[8];
  int i, j;
#if CONFIG_EXT_TX
  int16_t flipped_input[8 * 4];
  maybe_flip_input(&input, &stride, n2, n, flipped_input, tx_type);
#endif

  // Rows
  for (i = 0; i < n2; ++i) {
    for (j = 0; j < n; ++j)
      temp_in[j] =
          (tran_low_t)fdct_round_shift(input[i * stride + j] * 4 * Sqrt2);
    ht.rows(temp_in, temp_out);
    for (j = 0; j < n; ++j) out[j * n2 + i] = temp_out[j];
  }

  // Columns
  for (i = 0; i < n; ++i) {
    for (j = 0; j < n2; ++j) temp_in[j] = out[j + i * n2];
    ht.cols(temp_in, temp_out);
    for (j = 0; j < n2; ++j)
      output[i + j * n] = (temp_out[j] + (temp_out[j] < 0)) >> 1;
  }
  // Note: overall scale factor of transform is 8 times unitary
}

void av1_fht8x4_c(const int16_t *input, tran_low_t *output, int stride,
                  int tx_type) {
  static const transform_2d FHT[] = {
    { fdct4, fdct8 },    // DCT_DCT
    { fadst4, fdct8 },   // ADST_DCT
    { fdct4, fadst8 },   // DCT_ADST
    { fadst4, fadst8 },  // ADST_ADST
#if CONFIG_EXT_TX
    { fadst4, fdct8 },   // FLIPADST_DCT
    { fdct4, fadst8 },   // DCT_FLIPADST
    { fadst4, fadst8 },  // FLIPADST_FLIPADST
    { fadst4, fadst8 },  // ADST_FLIPADST
    { fadst4, fadst8 },  // FLIPADST_ADST
    { fidtx4, fidtx8 },  // IDTX
    { fdct4, fidtx8 },   // V_DCT
    { fidtx4, fdct8 },   // H_DCT
    { fadst4, fidtx8 },  // V_ADST
    { fidtx4, fadst8 },  // H_ADST
    { fadst4, fidtx8 },  // V_FLIPADST
    { fidtx4, fadst8 },  // H_FLIPADST
#endif
  };
  const transform_2d ht = FHT[tx_type];
  const int n = 4;
  const int n2 = 8;
  tran_low_t out[8 * 4];
  tran_low_t temp_in[8], temp_out[8];
  int i, j;
#if CONFIG_EXT_TX
  int16_t flipped_input[8 * 4];
  maybe_flip_input(&input, &stride, n, n2, flipped_input, tx_type);
#endif

  // Columns
  for (i = 0; i < n2; ++i) {
    for (j = 0; j < n; ++j)
      temp_in[j] =
          (tran_low_t)fdct_round_shift(input[j * stride + i] * 4 * Sqrt2);
    ht.cols(temp_in, temp_out);
    for (j = 0; j < n; ++j) out[j * n2 + i] = temp_out[j];
  }

  // Rows
  for (i = 0; i < n; ++i) {
    for (j = 0; j < n2; ++j) temp_in[j] = out[j + i * n2];
    ht.rows(temp_in, temp_out);
    for (j = 0; j < n2; ++j)
      output[j + i * n2] = (temp_out[j] + (temp_out[j] < 0)) >> 1;
  }
  // Note: overall scale factor of transform is 8 times unitary
}

void av1_fht4x16_c(const int16_t *input, tran_low_t *output, int stride,
                   int tx_type) {
  static const transform_2d FHT[] = {
    { fdct16, fdct4 },    // DCT_DCT
    { fadst16, fdct4 },   // ADST_DCT
    { fdct16, fadst4 },   // DCT_ADST
    { fadst16, fadst4 },  // ADST_ADST
#if CONFIG_EXT_TX
    { fadst16, fdct4 },   // FLIPADST_DCT
    { fdct16, fadst4 },   // DCT_FLIPADST
    { fadst16, fadst4 },  // FLIPADST_FLIPADST
    { fadst16, fadst4 },  // ADST_FLIPADST
    { fadst16, fadst4 },  // FLIPADST_ADST
    { fidtx16, fidtx4 },  // IDTX
    { fdct16, fidtx4 },   // V_DCT
    { fidtx16, fdct4 },   // H_DCT
    { fadst16, fidtx4 },  // V_ADST
    { fidtx16, fadst4 },  // H_ADST
    { fadst16, fidtx4 },  // V_FLIPADST
    { fidtx16, fadst4 },  // H_FLIPADST
#endif
  };
  const transform_2d ht = FHT[tx_type];
  const int n = 4;
  const int n4 = 16;
  tran_low_t out[16 * 4];
  tran_low_t temp_in[16], temp_out[16];
  int i, j;
#if CONFIG_EXT_TX
  int16_t flipped_input[16 * 4];
  maybe_flip_input(&input, &stride, n4, n, flipped_input, tx_type);
#endif

  // Rows
  for (i = 0; i < n4; ++i) {
    for (j = 0; j < n; ++j) temp_in[j] = input[i * stride + j] * 4;
    ht.rows(temp_in, temp_out);
    for (j = 0; j < n; ++j) out[j * n4 + i] = temp_out[j];
  }

  // Columns
  for (i = 0; i < n; ++i) {
    for (j = 0; j < n4; ++j) temp_in[j] = out[j + i * n4];
    ht.cols(temp_in, temp_out);
    for (j = 0; j < n4; ++j)
      output[i + j * n] = (temp_out[j] + (temp_out[j] < 0)) >> 1;
  }
  // Note: overall scale factor of transform is 8 times unitary
}

void av1_fht16x4_c(const int16_t *input, tran_low_t *output, int stride,
                   int tx_type) {
  static const transform_2d FHT[] = {
    { fdct4, fdct16 },    // DCT_DCT
    { fadst4, fdct16 },   // ADST_DCT
    { fdct4, fadst16 },   // DCT_ADST
    { fadst4, fadst16 },  // ADST_ADST
#if CONFIG_EXT_TX
    { fadst4, fdct16 },   // FLIPADST_DCT
    { fdct4, fadst16 },   // DCT_FLIPADST
    { fadst4, fadst16 },  // FLIPADST_FLIPADST
    { fadst4, fadst16 },  // ADST_FLIPADST
    { fadst4, fadst16 },  // FLIPADST_ADST
    { fidtx4, fidtx16 },  // IDTX
    { fdct4, fidtx16 },   // V_DCT
    { fidtx4, fdct16 },   // H_DCT
    { fadst4, fidtx16 },  // V_ADST
    { fidtx4, fadst16 },  // H_ADST
    { fadst4, fidtx16 },  // V_FLIPADST
    { fidtx4, fadst16 },  // H_FLIPADST
#endif
  };
  const transform_2d ht = FHT[tx_type];
  const int n = 4;
  const int n4 = 16;
  tran_low_t out[16 * 4];
  tran_low_t temp_in[16], temp_out[16];
  int i, j;
#if CONFIG_EXT_TX
  int16_t flipped_input[16 * 4];
  maybe_flip_input(&input, &stride, n, n4, flipped_input, tx_type);
#endif

  // Columns
  for (i = 0; i < n4; ++i) {
    for (j = 0; j < n; ++j) temp_in[j] = input[j * stride + i] * 4;
    ht.cols(temp_in, temp_out);
    for (j = 0; j < n; ++j) out[j * n4 + i] = temp_out[j];
  }

  // Rows
  for (i = 0; i < n; ++i) {
    for (j = 0; j < n4; ++j) temp_in[j] = out[j + i * n4];
    ht.rows(temp_in, temp_out);
    for (j = 0; j < n4; ++j)
      output[j + i * n4] = (temp_out[j] + (temp_out[j] < 0)) >> 1;
  }
  // Note: overall scale factor of transform is 8 times unitary
}

void av1_fht8x16_c(const int16_t *input, tran_low_t *output, int stride,
                   int tx_type) {
  static const transform_2d FHT[] = {
    { fdct16, fdct8 },    // DCT_DCT
    { fadst16, fdct8 },   // ADST_DCT
    { fdct16, fadst8 },   // DCT_ADST
    { fadst16, fadst8 },  // ADST_ADST
#if CONFIG_EXT_TX
    { fadst16, fdct8 },   // FLIPADST_DCT
    { fdct16, fadst8 },   // DCT_FLIPADST
    { fadst16, fadst8 },  // FLIPADST_FLIPADST
    { fadst16, fadst8 },  // ADST_FLIPADST
    { fadst16, fadst8 },  // FLIPADST_ADST
    { fidtx16, fidtx8 },  // IDTX
    { fdct16, fidtx8 },   // V_DCT
    { fidtx16, fdct8 },   // H_DCT
    { fadst16, fidtx8 },  // V_ADST
    { fidtx16, fadst8 },  // H_ADST
    { fadst16, fidtx8 },  // V_FLIPADST
    { fidtx16, fadst8 },  // H_FLIPADST
#endif
  };
  const transform_2d ht = FHT[tx_type];
  const int n = 8;
  const int n2 = 16;
  tran_low_t out[16 * 8];
  tran_low_t temp_in[16], temp_out[16];
  int i, j;
#if CONFIG_EXT_TX
  int16_t flipped_input[16 * 8];
  maybe_flip_input(&input, &stride, n2, n, flipped_input, tx_type);
#endif

  // Rows
  for (i = 0; i < n2; ++i) {
    for (j = 0; j < n; ++j)
      temp_in[j] =
          (tran_low_t)fdct_round_shift(input[i * stride + j] * 4 * Sqrt2);
    ht.rows(temp_in, temp_out);
    for (j = 0; j < n; ++j)
      out[j * n2 + i] = ROUND_POWER_OF_TWO_SIGNED(temp_out[j], 2);
  }

  // Columns
  for (i = 0; i < n; ++i) {
    for (j = 0; j < n2; ++j) temp_in[j] = out[j + i * n2];
    ht.cols(temp_in, temp_out);
    for (j = 0; j < n2; ++j) output[i + j * n] = temp_out[j];
  }
  // Note: overall scale factor of transform is 8 times unitary
}

void av1_fht16x8_c(const int16_t *input, tran_low_t *output, int stride,
                   int tx_type) {
  static const transform_2d FHT[] = {
    { fdct8, fdct16 },    // DCT_DCT
    { fadst8, fdct16 },   // ADST_DCT
    { fdct8, fadst16 },   // DCT_ADST
    { fadst8, fadst16 },  // ADST_ADST
#if CONFIG_EXT_TX
    { fadst8, fdct16 },   // FLIPADST_DCT
    { fdct8, fadst16 },   // DCT_FLIPADST
    { fadst8, fadst16 },  // FLIPADST_FLIPADST
    { fadst8, fadst16 },  // ADST_FLIPADST
    { fadst8, fadst16 },  // FLIPADST_ADST
    { fidtx8, fidtx16 },  // IDTX
    { fdct8, fidtx16 },   // V_DCT
    { fidtx8, fdct16 },   // H_DCT
    { fadst8, fidtx16 },  // V_ADST
    { fidtx8, fadst16 },  // H_ADST
    { fadst8, fidtx16 },  // V_FLIPADST
    { fidtx8, fadst16 },  // H_FLIPADST
#endif
  };
  const transform_2d ht = FHT[tx_type];
  const int n = 8;
  const int n2 = 16;
  tran_low_t out[16 * 8];
  tran_low_t temp_in[16], temp_out[16];
  int i, j;
#if CONFIG_EXT_TX
  int16_t flipped_input[16 * 8];
  maybe_flip_input(&input, &stride, n, n2, flipped_input, tx_type);
#endif

  // Columns
  for (i = 0; i < n2; ++i) {
    for (j = 0; j < n; ++j)
      temp_in[j] =
          (tran_low_t)fdct_round_shift(input[j * stride + i] * 4 * Sqrt2);
    ht.cols(temp_in, temp_out);
    for (j = 0; j < n; ++j)
      out[j * n2 + i] = ROUND_POWER_OF_TWO_SIGNED(temp_out[j], 2);
  }

  // Rows
  for (i = 0; i < n; ++i) {
    for (j = 0; j < n2; ++j) temp_in[j] = out[j + i * n2];
    ht.rows(temp_in, temp_out);
    for (j = 0; j < n2; ++j) output[j + i * n2] = temp_out[j];
  }
  // Note: overall scale factor of transform is 8 times unitary
}

void av1_fht8x32_c(const int16_t *input, tran_low_t *output, int stride,
                   int tx_type) {
  static const transform_2d FHT[] = {
    { fdct32, fdct8 },         // DCT_DCT
    { fhalfright32, fdct8 },   // ADST_DCT
    { fdct32, fadst8 },        // DCT_ADST
    { fhalfright32, fadst8 },  // ADST_ADST
#if CONFIG_EXT_TX
    { fhalfright32, fdct8 },   // FLIPADST_DCT
    { fdct32, fadst8 },        // DCT_FLIPADST
    { fhalfright32, fadst8 },  // FLIPADST_FLIPADST
    { fhalfright32, fadst8 },  // ADST_FLIPADST
    { fhalfright32, fadst8 },  // FLIPADST_ADST
    { fidtx32, fidtx8 },       // IDTX
    { fdct32, fidtx8 },        // V_DCT
    { fidtx32, fdct8 },        // H_DCT
    { fhalfright32, fidtx8 },  // V_ADST
    { fidtx32, fadst8 },       // H_ADST
    { fhalfright32, fidtx8 },  // V_FLIPADST
    { fidtx32, fadst8 },       // H_FLIPADST
#endif
  };
  const transform_2d ht = FHT[tx_type];
  const int n = 8;
  const int n4 = 32;
  tran_low_t out[32 * 8];
  tran_low_t temp_in[32], temp_out[32];
  int i, j;
#if CONFIG_EXT_TX
  int16_t flipped_input[32 * 8];
  maybe_flip_input(&input, &stride, n4, n, flipped_input, tx_type);
#endif

  // Rows
  for (i = 0; i < n4; ++i) {
    for (j = 0; j < n; ++j) temp_in[j] = input[i * stride + j] * 4;
    ht.rows(temp_in, temp_out);
    for (j = 0; j < n; ++j) out[j * n4 + i] = temp_out[j];
  }

  // Columns
  for (i = 0; i < n; ++i) {
    for (j = 0; j < n4; ++j) temp_in[j] = out[j + i * n4];
    ht.cols(temp_in, temp_out);
    for (j = 0; j < n4; ++j)
      output[i + j * n] = ROUND_POWER_OF_TWO_SIGNED(temp_out[j], 2);
  }
  // Note: overall scale factor of transform is 4 times unitary
}

void av1_fht32x8_c(const int16_t *input, tran_low_t *output, int stride,
                   int tx_type) {
  static const transform_2d FHT[] = {
    { fdct8, fdct32 },         // DCT_DCT
    { fadst8, fdct32 },        // ADST_DCT
    { fdct8, fhalfright32 },   // DCT_ADST
    { fadst8, fhalfright32 },  // ADST_ADST
#if CONFIG_EXT_TX
    { fadst8, fdct32 },        // FLIPADST_DCT
    { fdct8, fhalfright32 },   // DCT_FLIPADST
    { fadst8, fhalfright32 },  // FLIPADST_FLIPADST
    { fadst8, fhalfright32 },  // ADST_FLIPADST
    { fadst8, fhalfright32 },  // FLIPADST_ADST
    { fidtx8, fidtx32 },       // IDTX
    { fdct8, fidtx32 },        // V_DCT
    { fidtx8, fdct32 },        // H_DCT
    { fadst8, fidtx32 },       // V_ADST
    { fidtx8, fhalfright32 },  // H_ADST
    { fadst8, fidtx32 },       // V_FLIPADST
    { fidtx8, fhalfright32 },  // H_FLIPADST
#endif
  };
  const transform_2d ht = FHT[tx_type];
  const int n = 8;
  const int n4 = 32;
  tran_low_t out[32 * 8];
  tran_low_t temp_in[32], temp_out[32];
  int i, j;
#if CONFIG_EXT_TX
  int16_t flipped_input[32 * 8];
  maybe_flip_input(&input, &stride, n, n4, flipped_input, tx_type);
#endif

  // Columns
  for (i = 0; i < n4; ++i) {
    for (j = 0; j < n; ++j) temp_in[j] = input[j * stride + i] * 4;
    ht.cols(temp_in, temp_out);
    for (j = 0; j < n; ++j) out[j * n4 + i] = temp_out[j];
  }

  // Rows
  for (i = 0; i < n; ++i) {
    for (j = 0; j < n4; ++j) temp_in[j] = out[j + i * n4];
    ht.rows(temp_in, temp_out);
    for (j = 0; j < n4; ++j)
      output[j + i * n4] = ROUND_POWER_OF_TWO_SIGNED(temp_out[j], 2);
  }
  // Note: overall scale factor of transform is 4 times unitary
}

void av1_fht16x32_c(const int16_t *input, tran_low_t *output, int stride,
                    int tx_type) {
  static const transform_2d FHT[] = {
    { fdct32, fdct16 },         // DCT_DCT
    { fhalfright32, fdct16 },   // ADST_DCT
    { fdct32, fadst16 },        // DCT_ADST
    { fhalfright32, fadst16 },  // ADST_ADST
#if CONFIG_EXT_TX
    { fhalfright32, fdct16 },   // FLIPADST_DCT
    { fdct32, fadst16 },        // DCT_FLIPADST
    { fhalfright32, fadst16 },  // FLIPADST_FLIPADST
    { fhalfright32, fadst16 },  // ADST_FLIPADST
    { fhalfright32, fadst16 },  // FLIPADST_ADST
    { fidtx32, fidtx16 },       // IDTX
    { fdct32, fidtx16 },        // V_DCT
    { fidtx32, fdct16 },        // H_DCT
    { fhalfright32, fidtx16 },  // V_ADST
    { fidtx32, fadst16 },       // H_ADST
    { fhalfright32, fidtx16 },  // V_FLIPADST
    { fidtx32, fadst16 },       // H_FLIPADST
#endif
  };
  const transform_2d ht = FHT[tx_type];
  const int n = 16;
  const int n2 = 32;
  tran_low_t out[32 * 16];
  tran_low_t temp_in[32], temp_out[32];
  int i, j;
#if CONFIG_EXT_TX
  int16_t flipped_input[32 * 16];
  maybe_flip_input(&input, &stride, n2, n, flipped_input, tx_type);
#endif

  // Rows
  for (i = 0; i < n2; ++i) {
    for (j = 0; j < n; ++j)
      temp_in[j] =
          (tran_low_t)fdct_round_shift(input[i * stride + j] * 4 * Sqrt2);
    ht.rows(temp_in, temp_out);
    for (j = 0; j < n; ++j)
      out[j * n2 + i] = ROUND_POWER_OF_TWO_SIGNED(temp_out[j], 4);
  }

  // Columns
  for (i = 0; i < n; ++i) {
    for (j = 0; j < n2; ++j) temp_in[j] = out[j + i * n2];
    ht.cols(temp_in, temp_out);
    for (j = 0; j < n2; ++j) output[i + j * n] = temp_out[j];
  }
  // Note: overall scale factor of transform is 4 times unitary
}

void av1_fht32x16_c(const int16_t *input, tran_low_t *output, int stride,
                    int tx_type) {
  static const transform_2d FHT[] = {
    { fdct16, fdct32 },         // DCT_DCT
    { fadst16, fdct32 },        // ADST_DCT
    { fdct16, fhalfright32 },   // DCT_ADST
    { fadst16, fhalfright32 },  // ADST_ADST
#if CONFIG_EXT_TX
    { fadst16, fdct32 },        // FLIPADST_DCT
    { fdct16, fhalfright32 },   // DCT_FLIPADST
    { fadst16, fhalfright32 },  // FLIPADST_FLIPADST
    { fadst16, fhalfright32 },  // ADST_FLIPADST
    { fadst16, fhalfright32 },  // FLIPADST_ADST
    { fidtx16, fidtx32 },       // IDTX
    { fdct16, fidtx32 },        // V_DCT
    { fidtx16, fdct32 },        // H_DCT
    { fadst16, fidtx32 },       // V_ADST
    { fidtx16, fhalfright32 },  // H_ADST
    { fadst16, fidtx32 },       // V_FLIPADST
    { fidtx16, fhalfright32 },  // H_FLIPADST
#endif
  };
  const transform_2d ht = FHT[tx_type];
  const int n = 16;
  const int n2 = 32;
  tran_low_t out[32 * 16];
  tran_low_t temp_in[32], temp_out[32];
  int i, j;
#if CONFIG_EXT_TX
  int16_t flipped_input[32 * 16];
  maybe_flip_input(&input, &stride, n, n2, flipped_input, tx_type);
#endif

  // Columns
  for (i = 0; i < n2; ++i) {
    for (j = 0; j < n; ++j)
      temp_in[j] =
          (tran_low_t)fdct_round_shift(input[j * stride + i] * 4 * Sqrt2);
    ht.cols(temp_in, temp_out);
    for (j = 0; j < n; ++j)
      out[j * n2 + i] = ROUND_POWER_OF_TWO_SIGNED(temp_out[j], 4);
  }

  // Rows
  for (i = 0; i < n; ++i) {
    for (j = 0; j < n2; ++j) temp_in[j] = out[j + i * n2];
    ht.rows(temp_in, temp_out);
    for (j = 0; j < n2; ++j) output[j + i * n2] = temp_out[j];
  }
  // Note: overall scale factor of transform is 4 times unitary
}

void av1_fdct8x8_quant_c(const int16_t *input, int stride,
                         tran_low_t *coeff_ptr, intptr_t n_coeffs,
                         int skip_block, const int16_t *zbin_ptr,
                         const int16_t *round_ptr, const int16_t *quant_ptr,
                         const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
                         tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr,
                         uint16_t *eob_ptr, const int16_t *scan,
                         const int16_t *iscan
#if CONFIG_AOM_QM
                         ,
                         const qm_val_t *qm_ptr, const qm_val_t *iqm_ptr
#endif
                         ) {
  int eob = -1;

  int i, j;
  tran_low_t intermediate[64];

  // Transform columns
  {
    tran_low_t *output = intermediate;
    tran_high_t s0, s1, s2, s3, s4, s5, s6, s7;  // canbe16
    tran_high_t t0, t1, t2, t3;                  // needs32
    tran_high_t x0, x1, x2, x3;                  // canbe16

    for (i = 0; i < 8; i++) {
      // stage 1
      s0 = (input[0 * stride] + input[7 * stride]) * 4;
      s1 = (input[1 * stride] + input[6 * stride]) * 4;
      s2 = (input[2 * stride] + input[5 * stride]) * 4;
      s3 = (input[3 * stride] + input[4 * stride]) * 4;
      s4 = (input[3 * stride] - input[4 * stride]) * 4;
      s5 = (input[2 * stride] - input[5 * stride]) * 4;
      s6 = (input[1 * stride] - input[6 * stride]) * 4;
      s7 = (input[0 * stride] - input[7 * stride]) * 4;

      // fdct4(step, step);
      x0 = s0 + s3;
      x1 = s1 + s2;
      x2 = s1 - s2;
      x3 = s0 - s3;
      t0 = (x0 + x1) * cospi_16_64;
      t1 = (x0 - x1) * cospi_16_64;
      t2 = x2 * cospi_24_64 + x3 * cospi_8_64;
      t3 = -x2 * cospi_8_64 + x3 * cospi_24_64;
      output[0 * 8] = (tran_low_t)fdct_round_shift(t0);
      output[2 * 8] = (tran_low_t)fdct_round_shift(t2);
      output[4 * 8] = (tran_low_t)fdct_round_shift(t1);
      output[6 * 8] = (tran_low_t)fdct_round_shift(t3);

      // stage 2
      t0 = (s6 - s5) * cospi_16_64;
      t1 = (s6 + s5) * cospi_16_64;
      t2 = fdct_round_shift(t0);
      t3 = fdct_round_shift(t1);

      // stage 3
      x0 = s4 + t2;
      x1 = s4 - t2;
      x2 = s7 - t3;
      x3 = s7 + t3;

      // stage 4
      t0 = x0 * cospi_28_64 + x3 * cospi_4_64;
      t1 = x1 * cospi_12_64 + x2 * cospi_20_64;
      t2 = x2 * cospi_12_64 + x1 * -cospi_20_64;
      t3 = x3 * cospi_28_64 + x0 * -cospi_4_64;
      output[1 * 8] = (tran_low_t)fdct_round_shift(t0);
      output[3 * 8] = (tran_low_t)fdct_round_shift(t2);
      output[5 * 8] = (tran_low_t)fdct_round_shift(t1);
      output[7 * 8] = (tran_low_t)fdct_round_shift(t3);
      input++;
      output++;
    }
  }

  // Rows
  for (i = 0; i < 8; ++i) {
    fdct8(&intermediate[i * 8], &coeff_ptr[i * 8]);
    for (j = 0; j < 8; ++j) coeff_ptr[j + i * 8] /= 2;
  }

  // TODO(jingning) Decide the need of these arguments after the
  // quantization process is completed.
  (void)zbin_ptr;
  (void)quant_shift_ptr;
  (void)iscan;

  memset(qcoeff_ptr, 0, n_coeffs * sizeof(*qcoeff_ptr));
  memset(dqcoeff_ptr, 0, n_coeffs * sizeof(*dqcoeff_ptr));

  if (!skip_block) {
    // Quantization pass: All coefficients with index >= zero_flag are
    // skippable. Note: zero_flag can be zero.
    for (i = 0; i < n_coeffs; i++) {
      const int rc = scan[i];
      const int coeff = coeff_ptr[rc];
#if CONFIG_AOM_QM
      const qm_val_t wt = qm_ptr[rc];
      const qm_val_t iwt = iqm_ptr[rc];
      const int dequant =
          (dequant_ptr[rc != 0] * iwt + (1 << (AOM_QM_BITS - 1))) >>
          AOM_QM_BITS;
#endif
      const int coeff_sign = (coeff >> 31);
      const int abs_coeff = (coeff ^ coeff_sign) - coeff_sign;

      int64_t tmp = clamp(abs_coeff + round_ptr[rc != 0], INT16_MIN, INT16_MAX);
      int tmp32;
#if CONFIG_AOM_QM
      tmp32 = (int)((tmp * quant_ptr[rc != 0] * wt) >> (16 + AOM_QM_BITS));
      qcoeff_ptr[rc] = (tmp32 ^ coeff_sign) - coeff_sign;
      dqcoeff_ptr[rc] = qcoeff_ptr[rc] * dequant;
#else
      tmp32 = (int)((tmp * quant_ptr[rc != 0]) >> 16);
      qcoeff_ptr[rc] = (tmp32 ^ coeff_sign) - coeff_sign;
      dqcoeff_ptr[rc] = qcoeff_ptr[rc] * dequant_ptr[rc != 0];
#endif

      if (tmp32) eob = i;
    }
  }
  *eob_ptr = eob + 1;
}

void av1_fht8x8_c(const int16_t *input, tran_low_t *output, int stride,
                  int tx_type) {
  if (tx_type == DCT_DCT) {
    aom_fdct8x8_c(input, output, stride);
  } else {
    static const transform_2d FHT[] = {
      { fdct8, fdct8 },    // DCT_DCT
      { fadst8, fdct8 },   // ADST_DCT
      { fdct8, fadst8 },   // DCT_ADST
      { fadst8, fadst8 },  // ADST_ADST
#if CONFIG_EXT_TX
      { fadst8, fdct8 },   // FLIPADST_DCT
      { fdct8, fadst8 },   // DCT_FLIPADST
      { fadst8, fadst8 },  // FLIPADST_FLIPADST
      { fadst8, fadst8 },  // ADST_FLIPADST
      { fadst8, fadst8 },  // FLIPADST_ADST
      { fidtx8, fidtx8 },  // IDTX
      { fdct8, fidtx8 },   // V_DCT
      { fidtx8, fdct8 },   // H_DCT
      { fadst8, fidtx8 },  // V_ADST
      { fidtx8, fadst8 },  // H_ADST
      { fadst8, fidtx8 },  // V_FLIPADST
      { fidtx8, fadst8 },  // H_FLIPADST
#endif                     // CONFIG_EXT_TX
    };
    const transform_2d ht = FHT[tx_type];
    tran_low_t out[64];
    int i, j;
    tran_low_t temp_in[8], temp_out[8];

#if CONFIG_EXT_TX
    int16_t flipped_input[8 * 8];
    maybe_flip_input(&input, &stride, 8, 8, flipped_input, tx_type);
#endif

    // Columns
    for (i = 0; i < 8; ++i) {
      for (j = 0; j < 8; ++j) temp_in[j] = input[j * stride + i] * 4;
      ht.cols(temp_in, temp_out);
      for (j = 0; j < 8; ++j) out[j * 8 + i] = temp_out[j];
    }

    // Rows
    for (i = 0; i < 8; ++i) {
      for (j = 0; j < 8; ++j) temp_in[j] = out[j + i * 8];
      ht.rows(temp_in, temp_out);
      for (j = 0; j < 8; ++j)
        output[j + i * 8] = (temp_out[j] + (temp_out[j] < 0)) >> 1;
    }
  }
}

/* 4-point reversible, orthonormal Walsh-Hadamard in 3.5 adds, 0.5 shifts per
   pixel. */
void av1_fwht4x4_c(const int16_t *input, tran_low_t *output, int stride) {
  int i;
  tran_high_t a1, b1, c1, d1, e1;
  const int16_t *ip_pass0 = input;
  const tran_low_t *ip = NULL;
  tran_low_t *op = output;

  for (i = 0; i < 4; i++) {
    a1 = ip_pass0[0 * stride];
    b1 = ip_pass0[1 * stride];
    c1 = ip_pass0[2 * stride];
    d1 = ip_pass0[3 * stride];

    a1 += b1;
    d1 = d1 - c1;
    e1 = (a1 - d1) >> 1;
    b1 = e1 - b1;
    c1 = e1 - c1;
    a1 -= c1;
    d1 += b1;
    op[0] = (tran_low_t)a1;
    op[4] = (tran_low_t)c1;
    op[8] = (tran_low_t)d1;
    op[12] = (tran_low_t)b1;

    ip_pass0++;
    op++;
  }
  ip = output;
  op = output;

  for (i = 0; i < 4; i++) {
    a1 = ip[0];
    b1 = ip[1];
    c1 = ip[2];
    d1 = ip[3];

    a1 += b1;
    d1 -= c1;
    e1 = (a1 - d1) >> 1;
    b1 = e1 - b1;
    c1 = e1 - c1;
    a1 -= c1;
    d1 += b1;
    op[0] = (tran_low_t)(a1 * UNIT_QUANT_FACTOR);
    op[1] = (tran_low_t)(c1 * UNIT_QUANT_FACTOR);
    op[2] = (tran_low_t)(d1 * UNIT_QUANT_FACTOR);
    op[3] = (tran_low_t)(b1 * UNIT_QUANT_FACTOR);

    ip += 4;
    op += 4;
  }
}

void av1_fht16x16_c(const int16_t *input, tran_low_t *output, int stride,
                    int tx_type) {
  static const transform_2d FHT[] = {
    { fdct16, fdct16 },    // DCT_DCT
    { fadst16, fdct16 },   // ADST_DCT
    { fdct16, fadst16 },   // DCT_ADST
    { fadst16, fadst16 },  // ADST_ADST
#if CONFIG_EXT_TX
    { fadst16, fdct16 },   // FLIPADST_DCT
    { fdct16, fadst16 },   // DCT_FLIPADST
    { fadst16, fadst16 },  // FLIPADST_FLIPADST
    { fadst16, fadst16 },  // ADST_FLIPADST
    { fadst16, fadst16 },  // FLIPADST_ADST
    { fidtx16, fidtx16 },  // IDTX
    { fdct16, fidtx16 },   // V_DCT
    { fidtx16, fdct16 },   // H_DCT
    { fadst16, fidtx16 },  // V_ADST
    { fidtx16, fadst16 },  // H_ADST
    { fadst16, fidtx16 },  // V_FLIPADST
    { fidtx16, fadst16 },  // H_FLIPADST
#endif                     // CONFIG_EXT_TX
  };

  const transform_2d ht = FHT[tx_type];
  tran_low_t out[256];
  int i, j;
  tran_low_t temp_in[16], temp_out[16];

#if CONFIG_EXT_TX
  int16_t flipped_input[16 * 16];
  maybe_flip_input(&input, &stride, 16, 16, flipped_input, tx_type);
#endif

  // Columns
  for (i = 0; i < 16; ++i) {
    for (j = 0; j < 16; ++j) temp_in[j] = input[j * stride + i] * 4;
    ht.cols(temp_in, temp_out);
    for (j = 0; j < 16; ++j)
      out[j * 16 + i] = (temp_out[j] + 1 + (temp_out[j] < 0)) >> 2;
  }

  // Rows
  for (i = 0; i < 16; ++i) {
    for (j = 0; j < 16; ++j) temp_in[j] = out[j + i * 16];
    ht.rows(temp_in, temp_out);
    for (j = 0; j < 16; ++j) output[j + i * 16] = temp_out[j];
  }
}

#if CONFIG_HIGHBITDEPTH
void av1_highbd_fht4x4_c(const int16_t *input, tran_low_t *output, int stride,
                         int tx_type) {
  av1_fht4x4_c(input, output, stride, tx_type);
}

void av1_highbd_fht4x8_c(const int16_t *input, tran_low_t *output, int stride,
                         int tx_type) {
  av1_fht4x8_c(input, output, stride, tx_type);
}

void av1_highbd_fht8x4_c(const int16_t *input, tran_low_t *output, int stride,
                         int tx_type) {
  av1_fht8x4_c(input, output, stride, tx_type);
}

void av1_highbd_fht8x16_c(const int16_t *input, tran_low_t *output, int stride,
                          int tx_type) {
  av1_fht8x16_c(input, output, stride, tx_type);
}

void av1_highbd_fht16x8_c(const int16_t *input, tran_low_t *output, int stride,
                          int tx_type) {
  av1_fht16x8_c(input, output, stride, tx_type);
}

void av1_highbd_fht16x32_c(const int16_t *input, tran_low_t *output, int stride,
                           int tx_type) {
  av1_fht16x32_c(input, output, stride, tx_type);
}

void av1_highbd_fht32x16_c(const int16_t *input, tran_low_t *output, int stride,
                           int tx_type) {
  av1_fht32x16_c(input, output, stride, tx_type);
}

void av1_highbd_fht4x16_c(const int16_t *input, tran_low_t *output, int stride,
                          int tx_type) {
  av1_fht4x16_c(input, output, stride, tx_type);
}

void av1_highbd_fht16x4_c(const int16_t *input, tran_low_t *output, int stride,
                          int tx_type) {
  av1_fht16x4_c(input, output, stride, tx_type);
}

void av1_highbd_fht8x32_c(const int16_t *input, tran_low_t *output, int stride,
                          int tx_type) {
  av1_fht8x32_c(input, output, stride, tx_type);
}

void av1_highbd_fht32x8_c(const int16_t *input, tran_low_t *output, int stride,
                          int tx_type) {
  av1_fht32x8_c(input, output, stride, tx_type);
}

void av1_highbd_fht8x8_c(const int16_t *input, tran_low_t *output, int stride,
                         int tx_type) {
  av1_fht8x8_c(input, output, stride, tx_type);
}

void av1_highbd_fwht4x4_c(const int16_t *input, tran_low_t *output,
                          int stride) {
  av1_fwht4x4_c(input, output, stride);
}

void av1_highbd_fht16x16_c(const int16_t *input, tran_low_t *output, int stride,
                           int tx_type) {
  av1_fht16x16_c(input, output, stride, tx_type);
}
#endif  // CONFIG_HIGHBITDEPTH

void av1_fht32x32_c(const int16_t *input, tran_low_t *output, int stride,
                    int tx_type) {
  static const transform_2d FHT[] = {
    { fdct32, fdct32 },  // DCT_DCT
#if CONFIG_EXT_TX
    { fhalfright32, fdct32 },        // ADST_DCT
    { fdct32, fhalfright32 },        // DCT_ADST
    { fhalfright32, fhalfright32 },  // ADST_ADST
    { fhalfright32, fdct32 },        // FLIPADST_DCT
    { fdct32, fhalfright32 },        // DCT_FLIPADST
    { fhalfright32, fhalfright32 },  // FLIPADST_FLIPADST
    { fhalfright32, fhalfright32 },  // ADST_FLIPADST
    { fhalfright32, fhalfright32 },  // FLIPADST_ADST
    { fidtx32, fidtx32 },            // IDTX
    { fdct32, fidtx32 },             // V_DCT
    { fidtx32, fdct32 },             // H_DCT
    { fhalfright32, fidtx32 },       // V_ADST
    { fidtx32, fhalfright32 },       // H_ADST
    { fhalfright32, fidtx32 },       // V_FLIPADST
    { fidtx32, fhalfright32 },       // H_FLIPADST
#endif
  };
  const transform_2d ht = FHT[tx_type];
  tran_low_t out[1024];
  int i, j;
  tran_low_t temp_in[32], temp_out[32];

#if CONFIG_EXT_TX
  int16_t flipped_input[32 * 32];
  maybe_flip_input(&input, &stride, 32, 32, flipped_input, tx_type);
#endif

  // Columns
  for (i = 0; i < 32; ++i) {
    for (j = 0; j < 32; ++j) temp_in[j] = input[j * stride + i] * 4;
    ht.cols(temp_in, temp_out);
    for (j = 0; j < 32; ++j)
      out[j * 32 + i] = ROUND_POWER_OF_TWO_SIGNED(temp_out[j], 4);
  }

  // Rows
  for (i = 0; i < 32; ++i) {
    for (j = 0; j < 32; ++j) temp_in[j] = out[j + i * 32];
    ht.rows(temp_in, temp_out);
    for (j = 0; j < 32; ++j) output[j + i * 32] = temp_out[j];
  }
}

#if CONFIG_TX64X64
#if CONFIG_EXT_TX
static void fidtx64(const tran_low_t *input, tran_low_t *output) {
  int i;
  for (i = 0; i < 64; ++i)
    output[i] = (tran_low_t)fdct_round_shift(input[i] * 4 * Sqrt2);
}

// For use in lieu of ADST
static void fhalfright64(const tran_low_t *input, tran_low_t *output) {
  int i;
  tran_low_t inputhalf[32];
  for (i = 0; i < 32; ++i) {
    output[32 + i] = (tran_low_t)fdct_round_shift(input[i] * 4 * Sqrt2);
  }
  // Multiply input by sqrt(2)
  for (i = 0; i < 32; ++i) {
    inputhalf[i] = (tran_low_t)fdct_round_shift(input[i + 32] * Sqrt2);
  }
  fdct32(inputhalf, output);
  // Note overall scaling factor is 2 times unitary
}
#endif  // CONFIG_EXT_TX

static void fdct64_col(const tran_low_t *input, tran_low_t *output) {
  int32_t in[64], out[64];
  int i;
  for (i = 0; i < 64; ++i) in[i] = (int32_t)input[i];
  av1_fdct64_new(in, out, fwd_cos_bit_col_dct_64, fwd_stage_range_col_dct_64);
  for (i = 0; i < 64; ++i) output[i] = (tran_low_t)out[i];
}

static void fdct64_row(const tran_low_t *input, tran_low_t *output) {
  int32_t in[64], out[64];
  int i;
  for (i = 0; i < 64; ++i) in[i] = (int32_t)input[i];
  av1_fdct64_new(in, out, fwd_cos_bit_row_dct_64, fwd_stage_range_row_dct_64);
  for (i = 0; i < 64; ++i) output[i] = (tran_low_t)out[i];
}

void av1_fht64x64_c(const int16_t *input, tran_low_t *output, int stride,
                    int tx_type) {
  static const transform_2d FHT[] = {
    { fdct64_col, fdct64_row },  // DCT_DCT
#if CONFIG_EXT_TX
    { fhalfright64, fdct64_row },    // ADST_DCT
    { fdct64_col, fhalfright64 },    // DCT_ADST
    { fhalfright64, fhalfright64 },  // ADST_ADST
    { fhalfright64, fdct64_row },    // FLIPADST_DCT
    { fdct64_col, fhalfright64 },    // DCT_FLIPADST
    { fhalfright64, fhalfright64 },  // FLIPADST_FLIPADST
    { fhalfright64, fhalfright64 },  // ADST_FLIPADST
    { fhalfright64, fhalfright64 },  // FLIPADST_ADST
    { fidtx64, fidtx64 },            // IDTX
    { fdct64_col, fidtx64 },         // V_DCT
    { fidtx64, fdct64_row },         // H_DCT
    { fhalfright64, fidtx64 },       // V_ADST
    { fidtx64, fhalfright64 },       // H_ADST
    { fhalfright64, fidtx64 },       // V_FLIPADST
    { fidtx64, fhalfright64 },       // H_FLIPADST
#endif
  };
  const transform_2d ht = FHT[tx_type];
  tran_low_t out[4096];
  int i, j;
  tran_low_t temp_in[64], temp_out[64];
#if CONFIG_EXT_TX
  int16_t flipped_input[64 * 64];
  maybe_flip_input(&input, &stride, 64, 64, flipped_input, tx_type);
#endif
  // Columns
  for (i = 0; i < 64; ++i) {
    for (j = 0; j < 64; ++j) temp_in[j] = input[j * stride + i];
    ht.cols(temp_in, temp_out);
    for (j = 0; j < 64; ++j)
      out[j * 64 + i] = (temp_out[j] + 1 + (temp_out[j] > 0)) >> 2;
  }

  // Rows
  for (i = 0; i < 64; ++i) {
    for (j = 0; j < 64; ++j) temp_in[j] = out[j + i * 64];
    ht.rows(temp_in, temp_out);
    for (j = 0; j < 64; ++j)
      output[j + i * 64] =
          (tran_low_t)((temp_out[j] + 1 + (temp_out[j] < 0)) >> 2);
  }
}
#endif  // CONFIG_TX64X64

#if CONFIG_EXT_TX
// Forward identity transform.
void av1_fwd_idtx_c(const int16_t *src_diff, tran_low_t *coeff, int stride,
                    int bs, int tx_type) {
  int r, c;
  const int shift = bs < 32 ? 3 : (bs < 64 ? 2 : 1);
  if (tx_type == IDTX) {
    for (r = 0; r < bs; ++r) {
      for (c = 0; c < bs; ++c) coeff[c] = src_diff[c] * (1 << shift);
      src_diff += stride;
      coeff += bs;
    }
  }
}
#endif  // CONFIG_EXT_TX

#if CONFIG_HIGHBITDEPTH
void av1_highbd_fht32x32_c(const int16_t *input, tran_low_t *output, int stride,
                           int tx_type) {
  av1_fht32x32_c(input, output, stride, tx_type);
}

#if CONFIG_TX64X64
void av1_highbd_fht64x64_c(const int16_t *input, tran_low_t *output, int stride,
                           int tx_type) {
  av1_fht64x64_c(input, output, stride, tx_type);
}
#endif  // CONFIG_TX64X64
#endif  // CONFIG_HIGHBITDEPTH

#if CONFIG_DPCM_INTRA
void av1_dpcm_ft4_c(const int16_t *input, int stride, TX_TYPE_1D tx_type,
                    tran_low_t *output) {
  assert(tx_type < TX_TYPES_1D);
  static const transform_1d FHT[] = { fdct4, fadst4, fadst4, fidtx4 };
  const transform_1d ft = FHT[tx_type];
  tran_low_t temp_in[4];
  for (int i = 0; i < 4; ++i)
    temp_in[i] = (tran_low_t)fdct_round_shift(input[i * stride] * 4 * Sqrt2);
  ft(temp_in, output);
}

void av1_dpcm_ft8_c(const int16_t *input, int stride, TX_TYPE_1D tx_type,
                    tran_low_t *output) {
  assert(tx_type < TX_TYPES_1D);
  static const transform_1d FHT[] = { fdct8, fadst8, fadst8, fidtx8 };
  const transform_1d ft = FHT[tx_type];
  tran_low_t temp_in[8];
  for (int i = 0; i < 8; ++i) temp_in[i] = input[i * stride] * 4;
  ft(temp_in, output);
}

void av1_dpcm_ft16_c(const int16_t *input, int stride, TX_TYPE_1D tx_type,
                     tran_low_t *output) {
  assert(tx_type < TX_TYPES_1D);
  static const transform_1d FHT[] = { fdct16, fadst16, fadst16, fidtx16 };
  const transform_1d ft = FHT[tx_type];
  tran_low_t temp_in[16];
  for (int i = 0; i < 16; ++i)
    temp_in[i] = (tran_low_t)fdct_round_shift(input[i * stride] * 2 * Sqrt2);
  ft(temp_in, output);
}

void av1_dpcm_ft32_c(const int16_t *input, int stride, TX_TYPE_1D tx_type,
                     tran_low_t *output) {
  assert(tx_type < TX_TYPES_1D);
  static const transform_1d FHT[] = { fdct32, fhalfright32, fhalfright32,
                                      fidtx32 };
  const transform_1d ft = FHT[tx_type];
  tran_low_t temp_in[32];
  for (int i = 0; i < 32; ++i) temp_in[i] = input[i * stride];
  ft(temp_in, output);
}
#endif  // CONFIG_DPCM_INTRA
#endif  // !AV1_DCT_GTEST
