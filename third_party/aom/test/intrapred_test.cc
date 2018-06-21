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

#include <string>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "av1/common/blockd.h"
#include "av1/common/common.h"
#include "av1/common/pred_common.h"
#include "aom_mem/aom_mem.h"

namespace {

using libaom_test::ACMRandom;

const int count_test_block = 100000;

typedef void (*HighbdIntraPred)(uint16_t *dst, ptrdiff_t stride,
                                const uint16_t *above, const uint16_t *left,
                                int bps);
typedef void (*IntraPred)(uint8_t *dst, ptrdiff_t stride, const uint8_t *above,
                          const uint8_t *left);

template <typename FuncType>
struct IntraPredFunc {
  IntraPredFunc(FuncType pred = NULL, FuncType ref = NULL,
                int block_width_value = 0, int block_height_value = 0,
                int bit_depth_value = 0)
      : pred_fn(pred), ref_fn(ref), block_width(block_width_value),
        block_height(block_height_value), bit_depth(bit_depth_value) {}

  FuncType pred_fn;
  FuncType ref_fn;
  int block_width;
  int block_height;
  int bit_depth;
};

template <typename FuncType, typename Pixel>
class AV1IntraPredTest
    : public ::testing::TestWithParam<IntraPredFunc<FuncType> > {
 public:
  void RunTest(Pixel *left_col, Pixel *above_data, Pixel *dst, Pixel *ref_dst) {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int block_width = params_.block_width;
    const int block_height = params_.block_height;
    above_row_ = above_data + 16;
    left_col_ = left_col;
    dst_ = dst;
    ref_dst_ = ref_dst;
    int error_count = 0;
    for (int i = 0; i < count_test_block; ++i) {
      // Fill edges with random data, try first with saturated values.
      for (int x = -1; x <= block_width * 2; x++) {
        if (i == 0) {
          above_row_[x] = mask_;
        } else {
          above_row_[x] = rnd.Rand16() & mask_;
        }
      }
      for (int y = 0; y < block_height; y++) {
        if (i == 0) {
          left_col_[y] = mask_;
        } else {
          left_col_[y] = rnd.Rand16() & mask_;
        }
      }
      Predict();
      CheckPrediction(i, &error_count);
    }
    ASSERT_EQ(0, error_count);
  }

 protected:
  virtual void SetUp() {
    params_ = this->GetParam();
    stride_ = params_.block_width * 3;
    mask_ = (1 << params_.bit_depth) - 1;
  }

  virtual void Predict() = 0;

  void CheckPrediction(int test_case_number, int *error_count) const {
    // For each pixel ensure that the calculated value is the same as reference.
    const int block_width = params_.block_width;
    const int block_height = params_.block_height;
    for (int y = 0; y < block_height; y++) {
      for (int x = 0; x < block_width; x++) {
        *error_count += ref_dst_[x + y * stride_] != dst_[x + y * stride_];
        if (*error_count == 1) {
          ASSERT_EQ(ref_dst_[x + y * stride_], dst_[x + y * stride_])
              << " Failed on Test Case Number " << test_case_number
              << " location: x = " << x << " y = " << y;
        }
      }
    }
  }

  Pixel *above_row_;
  Pixel *left_col_;
  Pixel *dst_;
  Pixel *ref_dst_;
  ptrdiff_t stride_;
  int mask_;

  IntraPredFunc<FuncType> params_;
};

class HighbdIntraPredTest : public AV1IntraPredTest<HighbdIntraPred, uint16_t> {
 protected:
  void Predict() {
    const int bit_depth = params_.bit_depth;
    params_.ref_fn(ref_dst_, stride_, above_row_, left_col_, bit_depth);
    ASM_REGISTER_STATE_CHECK(
        params_.pred_fn(dst_, stride_, above_row_, left_col_, bit_depth));
  }
};

class LowbdIntraPredTest : public AV1IntraPredTest<IntraPred, uint8_t> {
 protected:
  void Predict() {
    params_.ref_fn(ref_dst_, stride_, above_row_, left_col_);
    ASM_REGISTER_STATE_CHECK(
        params_.pred_fn(dst_, stride_, above_row_, left_col_));
  }
};

// Suppress an unitialized warning. Once there are implementations to test then
// this can be restored.
TEST_P(HighbdIntraPredTest, Bitexact) {
  // max block size is 64
  DECLARE_ALIGNED(16, uint16_t, left_col[2 * 64]);
  DECLARE_ALIGNED(16, uint16_t, above_data[2 * 64 + 64]);
  DECLARE_ALIGNED(16, uint16_t, dst[3 * 64 * 64]);
  DECLARE_ALIGNED(16, uint16_t, ref_dst[3 * 64 * 64]);
  av1_zero(left_col);
  av1_zero(above_data);
  RunTest(left_col, above_data, dst, ref_dst);
}

// Same issue as above but for arm.
#if !HAVE_NEON
TEST_P(LowbdIntraPredTest, Bitexact) {
  // max block size is 32
  DECLARE_ALIGNED(16, uint8_t, left_col[2 * 32]);
  DECLARE_ALIGNED(16, uint8_t, above_data[2 * 32 + 32]);
  DECLARE_ALIGNED(16, uint8_t, dst[3 * 32 * 32]);
  DECLARE_ALIGNED(16, uint8_t, ref_dst[3 * 32 * 32]);
  av1_zero(left_col);
  av1_zero(above_data);
  RunTest(left_col, above_data, dst, ref_dst);
}
#endif  // !HAVE_NEON

// -----------------------------------------------------------------------------
// High Bit Depth Tests
#define highbd_entry(type, width, height, opt, bd)                          \
  IntraPredFunc<HighbdIntraPred>(                                           \
      &aom_highbd_##type##_predictor_##width##x##height##_##opt,            \
      &aom_highbd_##type##_predictor_##width##x##height##_c, width, height, \
      bd)

#if 0
#define highbd_intrapred(type, opt, bd)                                       \
  highbd_entry(type, 4, 4, opt, bd), highbd_entry(type, 4, 8, opt, bd),       \
      highbd_entry(type, 8, 4, opt, bd), highbd_entry(type, 8, 8, opt, bd),   \
      highbd_entry(type, 8, 16, opt, bd), highbd_entry(type, 16, 8, opt, bd), \
      highbd_entry(type, 16, 16, opt, bd),                                    \
      highbd_entry(type, 16, 32, opt, bd),                                    \
      highbd_entry(type, 32, 16, opt, bd), highbd_entry(type, 32, 32, opt, bd)
#endif

  // ---------------------------------------------------------------------------
  // Low Bit Depth Tests

#define lowbd_entry(type, width, height, opt)                                  \
  IntraPredFunc<IntraPred>(&aom_##type##_predictor_##width##x##height##_##opt, \
                           &aom_##type##_predictor_##width##x##height##_c,     \
                           width, height, 8)

#define lowbd_intrapred(type, opt)                                    \
  lowbd_entry(type, 4, 4, opt), lowbd_entry(type, 4, 8, opt),         \
      lowbd_entry(type, 8, 4, opt), lowbd_entry(type, 8, 8, opt),     \
      lowbd_entry(type, 8, 16, opt), lowbd_entry(type, 16, 8, opt),   \
      lowbd_entry(type, 16, 16, opt), lowbd_entry(type, 16, 32, opt), \
      lowbd_entry(type, 32, 16, opt), lowbd_entry(type, 32, 32, opt)

#if HAVE_SSE2
const IntraPredFunc<IntraPred> LowbdIntraPredTestVector[] = {
  lowbd_intrapred(dc, sse2),      lowbd_intrapred(dc_top, sse2),
  lowbd_intrapred(dc_left, sse2), lowbd_intrapred(dc_128, sse2),
  lowbd_intrapred(v, sse2),       lowbd_intrapred(h, sse2),
};

INSTANTIATE_TEST_CASE_P(SSE2, LowbdIntraPredTest,
                        ::testing::ValuesIn(LowbdIntraPredTestVector));

#endif  // HAVE_SSE2

#if HAVE_SSSE3
const IntraPredFunc<IntraPred> LowbdIntraPredTestVectorSsse3[] = {
  lowbd_intrapred(paeth, ssse3),
  lowbd_intrapred(smooth, ssse3),
};

INSTANTIATE_TEST_CASE_P(SSSE3, LowbdIntraPredTest,
                        ::testing::ValuesIn(LowbdIntraPredTestVectorSsse3));

#endif  // HAVE_SSSE3

#if HAVE_AVX2
const IntraPredFunc<IntraPred> LowbdIntraPredTestVectorAvx2[] = {
  lowbd_entry(dc, 32, 32, avx2),      lowbd_entry(dc_top, 32, 32, avx2),
  lowbd_entry(dc_left, 32, 32, avx2), lowbd_entry(dc_128, 32, 32, avx2),
  lowbd_entry(v, 32, 32, avx2),       lowbd_entry(h, 32, 32, avx2),
  lowbd_entry(dc, 32, 16, avx2),      lowbd_entry(dc_top, 32, 16, avx2),
  lowbd_entry(dc_left, 32, 16, avx2), lowbd_entry(dc_128, 32, 16, avx2),
  lowbd_entry(v, 32, 16, avx2),       lowbd_entry(paeth, 16, 8, avx2),
  lowbd_entry(paeth, 16, 16, avx2),   lowbd_entry(paeth, 16, 32, avx2),
  lowbd_entry(paeth, 32, 16, avx2),   lowbd_entry(paeth, 32, 32, avx2),
};

INSTANTIATE_TEST_CASE_P(AVX2, LowbdIntraPredTest,
                        ::testing::ValuesIn(LowbdIntraPredTestVectorAvx2));

#endif  // HAVE_AVX2

#if HAVE_NEON
const IntraPredFunc<HighbdIntraPred> HighbdIntraPredTestVectorNeon[] = {
  highbd_entry(dc, 4, 4, neon, 8),   highbd_entry(dc, 8, 8, neon, 8),
  highbd_entry(dc, 16, 16, neon, 8), highbd_entry(dc, 32, 32, neon, 8),
  highbd_entry(dc, 64, 64, neon, 8),
};

INSTANTIATE_TEST_CASE_P(NEON, HighbdIntraPredTest,
                        ::testing::ValuesIn(HighbdIntraPredTestVectorNeon));

#endif  // HAVE_NEON
}  // namespace
