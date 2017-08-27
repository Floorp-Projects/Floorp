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

#include "av1/common/av1_fwd_txfm1d.h"
#include "test/av1_txfm_test.h"

using libaom_test::ACMRandom;
using libaom_test::input_base;
using libaom_test::reference_hybrid_1d;
using libaom_test::TYPE_TXFM;
using libaom_test::TYPE_DCT;
using libaom_test::TYPE_ADST;

namespace {
const int txfm_type_num = 2;
const TYPE_TXFM txfm_type_ls[2] = { TYPE_DCT, TYPE_ADST };

const int txfm_size_num = 5;
const int txfm_size_ls[5] = { 4, 8, 16, 32, 64 };

const TxfmFunc fwd_txfm_func_ls[2][5] = {
#if CONFIG_TX64X64
  { av1_fdct4_new, av1_fdct8_new, av1_fdct16_new, av1_fdct32_new,
    av1_fdct64_new },
#else
  { av1_fdct4_new, av1_fdct8_new, av1_fdct16_new, av1_fdct32_new, NULL },
#endif
  { av1_fadst4_new, av1_fadst8_new, av1_fadst16_new, av1_fadst32_new, NULL }
};

// the maximum stage number of fwd/inv 1d dct/adst txfm is 12
const int8_t cos_bit[12] = { 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14 };
const int8_t range_bit[12] = { 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32 };

TEST(av1_fwd_txfm1d, round_shift) {
  EXPECT_EQ(round_shift(7, 1), 4);
  EXPECT_EQ(round_shift(-7, 1), -3);

  EXPECT_EQ(round_shift(7, 2), 2);
  EXPECT_EQ(round_shift(-7, 2), -2);

  EXPECT_EQ(round_shift(8, 2), 2);
  EXPECT_EQ(round_shift(-8, 2), -2);
}

TEST(av1_fwd_txfm1d, get_max_bit) {
  int max_bit = get_max_bit(8);
  EXPECT_EQ(max_bit, 3);
}

TEST(av1_fwd_txfm1d, cospi_arr_data) {
  for (int i = 0; i < 7; i++) {
    for (int j = 0; j < 64; j++) {
      EXPECT_EQ(cospi_arr_data[i][j],
                (int32_t)round(cos(M_PI * j / 128) * (1 << (cos_bit_min + i))));
    }
  }
}

TEST(av1_fwd_txfm1d, clamp_block) {
  int16_t block[5][5] = { { 7, -5, 6, -3, 9 },
                          { 7, -5, 6, -3, 9 },
                          { 7, -5, 6, -3, 9 },
                          { 7, -5, 6, -3, 9 },
                          { 7, -5, 6, -3, 9 } };

  int16_t ref_block[5][5] = { { 7, -5, 6, -3, 9 },
                              { 7, -5, 6, -3, 9 },
                              { 7, -4, 2, -3, 9 },
                              { 7, -4, 2, -3, 9 },
                              { 7, -4, 2, -3, 9 } };

  int row = 2;
  int col = 1;
  int block_size = 3;
  int stride = 5;
  clamp_block(block[row] + col, block_size, block_size, stride, -4, 2);
  for (int r = 0; r < stride; r++) {
    for (int c = 0; c < stride; c++) {
      EXPECT_EQ(block[r][c], ref_block[r][c]);
    }
  }
}

TEST(av1_fwd_txfm1d, accuracy) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  for (int si = 0; si < txfm_size_num; ++si) {
    int txfm_size = txfm_size_ls[si];
    int32_t *input = new int32_t[txfm_size];
    int32_t *output = new int32_t[txfm_size];
    double *ref_input = new double[txfm_size];
    double *ref_output = new double[txfm_size];

    for (int ti = 0; ti < txfm_type_num; ++ti) {
      TYPE_TXFM txfm_type = txfm_type_ls[ti];
      TxfmFunc fwd_txfm_func = fwd_txfm_func_ls[ti][si];
      int max_error = 7;

      const int count_test_block = 5000;
      if (fwd_txfm_func != NULL) {
        for (int ti = 0; ti < count_test_block; ++ti) {
          for (int ni = 0; ni < txfm_size; ++ni) {
            input[ni] = rnd.Rand16() % input_base - rnd.Rand16() % input_base;
            ref_input[ni] = static_cast<double>(input[ni]);
          }

          fwd_txfm_func(input, output, cos_bit, range_bit);
          reference_hybrid_1d(ref_input, ref_output, txfm_size, txfm_type);

          for (int ni = 0; ni < txfm_size; ++ni) {
            EXPECT_LE(
                abs(output[ni] - static_cast<int32_t>(round(ref_output[ni]))),
                max_error);
          }
        }
      }
    }

    delete[] input;
    delete[] output;
    delete[] ref_input;
    delete[] ref_output;
  }
}
}  // namespace
