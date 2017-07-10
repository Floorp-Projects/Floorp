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

#include "test/av1_txfm_test.h"
#include "test/util.h"
#include "av1/common/av1_fwd_txfm1d.h"
#include "av1/common/av1_inv_txfm1d.h"

using libaom_test::ACMRandom;
using libaom_test::input_base;

namespace {
const int txfm_type_num = 2;
const int txfm_size_ls[5] = { 4, 8, 16, 32, 64 };

const TxfmFunc fwd_txfm_func_ls[][2] = {
  { av1_fdct4_new, av1_fadst4_new },
  { av1_fdct8_new, av1_fadst8_new },
  { av1_fdct16_new, av1_fadst16_new },
  { av1_fdct32_new, av1_fadst32_new },
#if CONFIG_TX64X64
  { av1_fdct64_new, NULL },
#endif
};

const TxfmFunc inv_txfm_func_ls[][2] = {
  { av1_idct4_new, av1_iadst4_new },
  { av1_idct8_new, av1_iadst8_new },
  { av1_idct16_new, av1_iadst16_new },
  { av1_idct32_new, av1_iadst32_new },
#if CONFIG_TX64X64
  { av1_idct64_new, NULL },
#endif
};

// the maximum stage number of fwd/inv 1d dct/adst txfm is 12
const int8_t cos_bit[12] = { 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13 };
const int8_t range_bit[12] = { 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32 };

TEST(av1_inv_txfm1d, round_trip) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  for (int si = 0; si < NELEMENTS(fwd_txfm_func_ls); ++si) {
    int txfm_size = txfm_size_ls[si];

    for (int ti = 0; ti < txfm_type_num; ++ti) {
      TxfmFunc fwd_txfm_func = fwd_txfm_func_ls[si][ti];
      TxfmFunc inv_txfm_func = inv_txfm_func_ls[si][ti];
      int max_error = 2;

      if (!fwd_txfm_func) continue;

      const int count_test_block = 5000;
      for (int ci = 0; ci < count_test_block; ++ci) {
        int32_t input[64];
        int32_t output[64];
        int32_t round_trip_output[64];

        ASSERT_LE(txfm_size, NELEMENTS(input));

        for (int ni = 0; ni < txfm_size; ++ni) {
          input[ni] = rnd.Rand16() % input_base - rnd.Rand16() % input_base;
        }

        fwd_txfm_func(input, output, cos_bit, range_bit);
        inv_txfm_func(output, round_trip_output, cos_bit, range_bit);

        for (int ni = 0; ni < txfm_size; ++ni) {
          int node_err =
              abs(input[ni] - round_shift(round_trip_output[ni],
                                          get_max_bit(txfm_size) - 1));
          EXPECT_LE(node_err, max_error);
        }
      }
    }
  }
}

}  // namespace
