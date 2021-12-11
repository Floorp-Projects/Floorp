/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/aom_config.h"

#include "av1/common/blockd.h"
#include "av1/common/enums.h"
#include "av1/common/mv.h"
#include "av1/common/mvref_common.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/tile_common.h"

namespace {
TEST(IntrabcTest, DvValidation) {
  struct DvTestCase {
    MV dv;
    int mi_row_offset;
    int mi_col_offset;
    BLOCK_SIZE bsize;
    bool valid;
  };
  const int kSubPelScale = 8;
  const int kTileMaxMibWidth = 8;
  const DvTestCase kDvCases[] = {
    { { 0, 0 }, 0, 0, BLOCK_128X128, false },
    { { 0, 0 }, 0, 0, BLOCK_64X64, false },
    { { 0, 0 }, 0, 0, BLOCK_32X32, false },
    { { 0, 0 }, 0, 0, BLOCK_16X16, false },
    { { 0, 0 }, 0, 0, BLOCK_8X8, false },
    { { 0, 0 }, 0, 0, BLOCK_4X4, false },
    { { -MAX_SB_SIZE * kSubPelScale, -MAX_SB_SIZE * kSubPelScale },
      MAX_SB_SIZE / MI_SIZE,
      MAX_SB_SIZE / MI_SIZE,
      BLOCK_16X16,
      true },
    { { 0, -MAX_SB_SIZE * kSubPelScale },
      MAX_SB_SIZE / MI_SIZE,
      MAX_SB_SIZE / MI_SIZE,
      BLOCK_16X16,
      false },
    { { -MAX_SB_SIZE * kSubPelScale, 0 },
      MAX_SB_SIZE / MI_SIZE,
      MAX_SB_SIZE / MI_SIZE,
      BLOCK_16X16,
      true },
    { { MAX_SB_SIZE * kSubPelScale, 0 },
      MAX_SB_SIZE / MI_SIZE,
      MAX_SB_SIZE / MI_SIZE,
      BLOCK_16X16,
      false },
    { { 0, MAX_SB_SIZE * kSubPelScale },
      MAX_SB_SIZE / MI_SIZE,
      MAX_SB_SIZE / MI_SIZE,
      BLOCK_16X16,
      false },
    { { -32 * kSubPelScale, -32 * kSubPelScale },
      MAX_SB_SIZE / MI_SIZE,
      MAX_SB_SIZE / MI_SIZE,
      BLOCK_32X32,
      true },
    { { -32 * kSubPelScale, -32 * kSubPelScale },
      32 / MI_SIZE,
      32 / MI_SIZE,
      BLOCK_32X32,
      false },
    { { -32 * kSubPelScale - kSubPelScale / 2, -32 * kSubPelScale },
      MAX_SB_SIZE / MI_SIZE,
      MAX_SB_SIZE / MI_SIZE,
      BLOCK_32X32,
      false },
    { { -33 * kSubPelScale, -32 * kSubPelScale },
      MAX_SB_SIZE / MI_SIZE,
      MAX_SB_SIZE / MI_SIZE,
      BLOCK_32X32,
      true },
    { { -32 * kSubPelScale, -32 * kSubPelScale - kSubPelScale / 2 },
      MAX_SB_SIZE / MI_SIZE,
      MAX_SB_SIZE / MI_SIZE,
      BLOCK_32X32,
      false },
    { { -32 * kSubPelScale, -33 * kSubPelScale },
      MAX_SB_SIZE / MI_SIZE,
      MAX_SB_SIZE / MI_SIZE,
      BLOCK_32X32,
      true },
    { { -MAX_SB_SIZE * kSubPelScale, -MAX_SB_SIZE * kSubPelScale },
      MAX_SB_SIZE / MI_SIZE,
      MAX_SB_SIZE / MI_SIZE,
      BLOCK_LARGEST,
      true },
    { { -(MAX_SB_SIZE + 1) * kSubPelScale, -MAX_SB_SIZE * kSubPelScale },
      MAX_SB_SIZE / MI_SIZE,
      MAX_SB_SIZE / MI_SIZE,
      BLOCK_LARGEST,
      false },
    { { -MAX_SB_SIZE * kSubPelScale, -(MAX_SB_SIZE + 1) * kSubPelScale },
      MAX_SB_SIZE / MI_SIZE,
      MAX_SB_SIZE / MI_SIZE,
      BLOCK_LARGEST,
      false },
    { { -(MAX_SB_SIZE - 1) * kSubPelScale, -MAX_SB_SIZE * kSubPelScale },
      MAX_SB_SIZE / MI_SIZE,
      MAX_SB_SIZE / MI_SIZE,
      BLOCK_LARGEST,
      false },
    { { -MAX_SB_SIZE * kSubPelScale, -(MAX_SB_SIZE - 1) * kSubPelScale },
      MAX_SB_SIZE / MI_SIZE,
      MAX_SB_SIZE / MI_SIZE,
      BLOCK_LARGEST,
      true },
    { { -(MAX_SB_SIZE - 1) * kSubPelScale, -(MAX_SB_SIZE - 1) * kSubPelScale },
      MAX_SB_SIZE / MI_SIZE,
      MAX_SB_SIZE / MI_SIZE,
      BLOCK_LARGEST,
      false },
    { { -MAX_SB_SIZE * kSubPelScale, MAX_SB_SIZE * kSubPelScale },
      MAX_SB_SIZE / MI_SIZE,
      MAX_SB_SIZE / MI_SIZE,
      BLOCK_LARGEST,
      false },
    { { -MAX_SB_SIZE * kSubPelScale,
        (kTileMaxMibWidth - 2) * MAX_SB_SIZE * kSubPelScale },
      MAX_SB_SIZE / MI_SIZE,
      MAX_SB_SIZE / MI_SIZE,
      BLOCK_LARGEST,
      false },
    { { -MAX_SB_SIZE * kSubPelScale,
        ((kTileMaxMibWidth - 2) * MAX_SB_SIZE + 1) * kSubPelScale },
      MAX_SB_SIZE / MI_SIZE,
      MAX_SB_SIZE / MI_SIZE,
      BLOCK_LARGEST,
      false },
  };

  MACROBLOCKD xd;
  memset(&xd, 0, sizeof(xd));
  xd.tile.mi_row_start = 8 * MAX_MIB_SIZE;
  xd.tile.mi_row_end = 16 * MAX_MIB_SIZE;
  xd.tile.mi_col_start = 24 * MAX_MIB_SIZE;
  xd.tile.mi_col_end = xd.tile.mi_col_start + kTileMaxMibWidth * MAX_MIB_SIZE;
  xd.plane[1].subsampling_x = 1;
  xd.plane[1].subsampling_y = 1;
  xd.plane[2].subsampling_x = 1;
  xd.plane[2].subsampling_y = 1;

  AV1_COMMON cm;
  memset(&cm, 0, sizeof(cm));

  for (int i = 0; i < static_cast<int>(GTEST_ARRAY_SIZE_(kDvCases)); ++i) {
    EXPECT_EQ(static_cast<int>(kDvCases[i].valid),
              av1_is_dv_valid(kDvCases[i].dv, &cm, &xd,
                              xd.tile.mi_row_start + kDvCases[i].mi_row_offset,
                              xd.tile.mi_col_start + kDvCases[i].mi_col_offset,
                              kDvCases[i].bsize, MAX_MIB_SIZE_LOG2))
        << "DvCases[" << i << "]";
  }
}
}  // namespace
