/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "av1/common/blockd.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

// Verify the optimized implementation of get_partition_subsize() produces the
// same results as the Partition_Subsize lookup table in the spec.
TEST(BlockdTest, GetPartitionSubsize) {
  // The Partition_Subsize table in the spec (Section 9.3. Conversion tables).
  /* clang-format off */
  static const BLOCK_SIZE kPartitionSubsize[10][BLOCK_SIZES_ALL] = {
    {
                                    BLOCK_4X4,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X8,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X16,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X32,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X64,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_128X128,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID
    }, {
                                    BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X4,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X8,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X16,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X32,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_128X64,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID
    }, {
                                    BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_4X8,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X16,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X32,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X64,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X128,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID
    }, {
                                    BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_4X4,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X8,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X16,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X32,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X64,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID
    }, {
                                    BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X4,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X8,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X16,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X32,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_128X64,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID
    }, {
                                    BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X4,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X8,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X16,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X32,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_128X64,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID
    }, {
                                    BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_4X8,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X16,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X32,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X64,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X128,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID
    }, {
                                    BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_4X8,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X16,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X32,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X64,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X128,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID
    }, {
                                    BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X4,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X8,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X16,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID
    }, {
                                    BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_4X16,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X32,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X64,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID
    }
  };
  /* clang-format on */

  for (int partition = 0; partition < 10; partition++) {
    for (int bsize = BLOCK_4X4; bsize < BLOCK_SIZES_ALL; bsize++) {
      EXPECT_EQ(kPartitionSubsize[partition][bsize],
                get_partition_subsize(static_cast<BLOCK_SIZE>(bsize),
                                      static_cast<PARTITION_TYPE>(partition)));
    }
  }
}
