/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "third_party/googletest/src/include/gtest/gtest.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "./vpx_config.h"
#include "./vp8_rtcd.h"
#include "vp8/common/blockd.h"
#include "vp8/encoder/block.h"
#include "vpx_mem/vpx_mem.h"

typedef void (*SubtractBlockFunc)(BLOCK *be, BLOCKD *bd, int pitch);

namespace {

class SubtractBlockTest : public ::testing::TestWithParam<SubtractBlockFunc> {
 public:
  virtual void TearDown() {
    libvpx_test::ClearSystemState();
  }
};

using libvpx_test::ACMRandom;

TEST_P(SubtractBlockTest, SimpleSubtract) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  BLOCK be;
  BLOCKD bd;
  // in libvpx, this stride is always 16
  const int kDiffPredStride = 16;
  const int kSrcStride[] = {32, 16, 8, 4, 0};
  const int kBlockWidth = 4;
  const int kBlockHeight = 4;

  // Allocate... align to 16 for mmx/sse tests
  uint8_t *source = reinterpret_cast<uint8_t*>(
      vpx_memalign(16, kBlockHeight * kSrcStride[0] * sizeof(*source)));
  be.src_diff = reinterpret_cast<int16_t*>(
      vpx_memalign(16, kBlockHeight * kDiffPredStride * sizeof(*be.src_diff)));
  bd.predictor = reinterpret_cast<unsigned char*>(
      vpx_memalign(16, kBlockHeight * kDiffPredStride * sizeof(*bd.predictor)));

  for (int i = 0; kSrcStride[i] > 0; ++i) {
    // start at block0
    be.src = 0;
    be.base_src = &source;
    be.src_stride = kSrcStride[i];

    // set difference
    int16_t *src_diff = be.src_diff;
    for (int r = 0; r < kBlockHeight; ++r) {
      for (int c = 0; c < kBlockWidth; ++c) {
        src_diff[c] = static_cast<int16_t>(0xa5a5u);
      }
      src_diff += kDiffPredStride;
    }

    // set destination
    uint8_t *base_src = *be.base_src;
    for (int r = 0; r < kBlockHeight; ++r) {
      for (int c = 0; c < kBlockWidth; ++c) {
        base_src[c] = rnd.Rand8();
      }
      base_src += be.src_stride;
    }

    // set predictor
    uint8_t *predictor = bd.predictor;
    for (int r = 0; r < kBlockHeight; ++r) {
      for (int c = 0; c < kBlockWidth; ++c) {
        predictor[c] = rnd.Rand8();
      }
      predictor += kDiffPredStride;
    }

    ASM_REGISTER_STATE_CHECK(GetParam()(&be, &bd, kDiffPredStride));

    base_src = *be.base_src;
    src_diff = be.src_diff;
    predictor = bd.predictor;
    for (int r = 0; r < kBlockHeight; ++r) {
      for (int c = 0; c < kBlockWidth; ++c) {
        EXPECT_EQ(base_src[c], (src_diff[c] + predictor[c])) << "r = " << r
                                                             << ", c = " << c;
      }
      src_diff += kDiffPredStride;
      predictor += kDiffPredStride;
      base_src += be.src_stride;
    }
  }
  vpx_free(be.src_diff);
  vpx_free(source);
  vpx_free(bd.predictor);
}

INSTANTIATE_TEST_CASE_P(C, SubtractBlockTest,
                        ::testing::Values(vp8_subtract_b_c));

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(NEON, SubtractBlockTest,
                        ::testing::Values(vp8_subtract_b_neon));
#endif

#if HAVE_MMX
INSTANTIATE_TEST_CASE_P(MMX, SubtractBlockTest,
                        ::testing::Values(vp8_subtract_b_mmx));
#endif

#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(SSE2, SubtractBlockTest,
                        ::testing::Values(vp8_subtract_b_sse2));
#endif

}  // namespace
