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

#include "av1/common/common_data.h"
#include "av1/common/scan.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {

TEST(ScanTest, av1_augment_prob) {
  const TX_SIZE tx_size = TX_4X4;
  const TX_TYPE tx_type = DCT_DCT;
  const int tx1d_size = tx_size_wide[tx_size];
  uint32_t prob[16] = { 8, 8, 7, 7, 8, 8, 4, 2, 3, 3, 2, 2, 2, 2, 2, 2 };
  const uint32_t ref_prob[16] = {
    8, 8, 7, 7, 8, 8, 4, 2, 3, 3, 2, 2, 2, 2, 2, 2
  };
  av1_augment_prob(tx_size, tx_type, prob);
  for (int r = 0; r < tx1d_size; ++r) {
    for (int c = 0; c < tx1d_size; ++c) {
      const uint32_t idx = r * tx1d_size + c;
      EXPECT_EQ(ref_prob[idx], prob[idx] >> 16);
    }
  }

  const SCAN_ORDER *sc = get_default_scan(tx_size, tx_type, 0);
  const uint32_t mask = (1 << 16) - 1;
  for (int r = 0; r < tx1d_size; ++r) {
    for (int c = 0; c < tx1d_size; ++c) {
      const uint32_t ref_idx = r * tx1d_size + c;
      const uint32_t scan_idx = mask ^ (prob[r * tx1d_size + c] & mask);
      const uint32_t idx = sc->scan[scan_idx];
      EXPECT_EQ(ref_idx, idx);
    }
  }
}

#if USE_TOPOLOGICAL_SORT
TEST(ScanTest, av1_update_sort_order) {
  const TX_SIZE tx_size = TX_4X4;
  const TX_TYPE tx_type = DCT_DCT;
  const uint32_t prob[16] = { 15, 14, 11, 10, 13, 12, 9, 5,
                              8,  7,  4,  2,  6,  3,  1, 0 };
  const int16_t ref_sort_order[16] = { 0, 1,  4, 5,  2,  3,  6,  8,
                                       9, 12, 7, 10, 13, 11, 14, 15 };
  int16_t sort_order[16];
  av1_update_sort_order(tx_size, tx_type, prob, sort_order);
  for (int i = 0; i < 16; ++i) EXPECT_EQ(ref_sort_order[i], sort_order[i]);
}
#endif

#if USE_TOPOLOGICAL_SORT
TEST(ScanTest, av1_update_scan_order) {
  TX_SIZE tx_size = TX_4X4;
  const TX_TYPE tx_type = DCT_DCT;
  const uint32_t prob[16] = { 10, 12, 14, 9, 11, 13, 15, 5,
                              8,  7,  4,  2, 6,  3,  1,  0 };
  int16_t sort_order[16];
  int16_t scan[16];
  int16_t iscan[16];
  const int16_t ref_iscan[16] = { 0, 1, 2,  6,  3, 4,  5,  10,
                                  7, 8, 11, 13, 9, 12, 14, 15 };

  av1_update_sort_order(tx_size, tx_type, prob, sort_order);
  av1_update_scan_order(tx_size, sort_order, scan, iscan);

  for (int i = 0; i < 16; ++i) {
    EXPECT_EQ(ref_iscan[i], iscan[i]);
    EXPECT_EQ(i, scan[ref_iscan[i]]);
  }
}
#endif

TEST(ScanTest, av1_update_neighbors) {
  TX_SIZE tx_size = TX_4X4;
  // raster order
  const int16_t scan[16] = { 0, 1, 2,  3,  4,  5,  6,  7,
                             8, 9, 10, 11, 12, 13, 14, 15 };
  int16_t nb[(16 + 1) * 2];
  const int16_t ref_nb[(16 + 1) * 2] = { 0,  0,  0,  0,  1,  1,  2, 2, 0,
                                         1,  1,  4,  2,  5,  3,  6, 4, 5,
                                         5,  8,  6,  9,  7,  10, 8, 9, 9,
                                         12, 10, 13, 11, 14, 0,  0 };

  // raster order's scan and iscan are the same
  av1_update_neighbors(tx_size, scan, scan, nb);

  for (int i = 0; i < (16 + 1) * 2; ++i) {
    EXPECT_EQ(ref_nb[i], nb[i]);
  }
}

#if USE_2X2_PROB
TEST(ScanTest, av1_down_sample_scan_count) {
  const uint32_t non_zero_count[256] = {
    13, 12, 11, 10, 0,  0, 0, 0,  0, 0, 0,  0,  0, 0, 0, 0, 13, 9, 10, 8, 0, 0,
    0,  0,  0,  0,  0,  0, 0, 0,  0, 0, 11, 12, 9, 8, 0, 0, 0,  0, 0,  0, 0, 0,
    0,  0,  0,  0,  13, 9, 9, 10, 0, 0, 0,  0,  0, 0, 0, 0, 0,  0, 0,  0, 0, 0,
    0,  0,  0,  0,  0,  0, 0, 0,  0, 0, 0,  0,  0, 0, 0, 0, 0,  0, 0,  0, 0, 0,
    0,  0,  0,  0,  0,  0, 0, 0,  0, 0, 0,  0,  0, 0, 0, 0, 0,  0, 0,  0, 0, 0,
    0,  0,  0,  0,  0,  0, 0, 0,  0, 0, 0,  0,  0, 0, 0, 0, 0,  0, 0,  0, 0, 0,
    0,  0,  0,  0,  0,  0, 0, 0,  0, 0, 0,  0,  0, 0, 0, 0, 0,  0, 0,  0, 0, 0,
    0,  0,  0,  0,  0,  0, 0, 0,  0, 0, 0,  0,  0, 0, 0, 0, 0,  0, 0,  0, 0, 0,
    0,  0,  0,  0,  0,  0, 0, 0,  0, 0, 0,  0,  0, 0, 0, 0, 0,  0, 0,  0, 0, 0,
    0,  0,  0,  0,  0,  0, 0, 0,  0, 0, 0,  0,  0, 0, 0, 0, 0,  0, 0,  0, 0, 0,
    0,  0,  0,  0,  0,  0, 0, 0,  0, 0, 0,  0,  0, 0, 0, 0, 0,  0, 0,  0, 0, 0,
    0,  0,  0,  0,  0,  0, 0, 0,  0, 0, 0,  0,  0, 0,
  };
  const uint32_t ref_non_zero_count_ds[64] = {
    13, 11, 0, 0, 0, 0, 0, 0, 11, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,  0,  0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,  0,  0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  };
  uint32_t non_zero_count_ds[64];
  av1_down_sample_scan_count(non_zero_count_ds, non_zero_count, TX_16X16);
  for (int i = 0; i < 64; ++i) {
    EXPECT_EQ(ref_non_zero_count_ds[i], non_zero_count_ds[i]);
  }
}
#endif

}  // namespace
