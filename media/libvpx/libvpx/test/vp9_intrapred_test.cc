/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "third_party/googletest/src/include/gtest/gtest.h"

#include "./vpx_config.h"
#include "./vp9_rtcd.h"
#include "vp9/common/vp9_blockd.h"
#include "vp9/common/vp9_pred_common.h"
#include "vpx_mem/vpx_mem.h"
#include "test/util.h"

namespace {

using libvpx_test::ACMRandom;

const int count_test_block = 100000;

// Base class for VP9 intra prediction tests.
class VP9IntraPredBase {
 public:
  virtual ~VP9IntraPredBase() { libvpx_test::ClearSystemState(); }

 protected:
  virtual void Predict(PREDICTION_MODE mode) = 0;

  void CheckPrediction(int test_case_number, int *error_count) const {
    // For each pixel ensure that the calculated value is the same as reference.
    for (int y = 0; y < block_size_; y++) {
      for (int x = 0; x < block_size_; x++) {
        *error_count += ref_dst_[x + y * stride_] != dst_[x + y * stride_];
        if (*error_count == 1) {
          ASSERT_EQ(ref_dst_[x + y * stride_], dst_[x + y * stride_])
              << " Failed on Test Case Number "<< test_case_number;
        }
      }
    }
  }

  void RunTest(uint16_t* left_col, uint16_t* above_data,
               uint16_t* dst, uint16_t* ref_dst) {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    left_col_ = left_col;
    dst_ = dst;
    ref_dst_ = ref_dst;
    above_row_ = above_data + 16;
    int error_count = 0;
    for (int i = 0; i < count_test_block; ++i) {
      // Fill edges with random data, try first with saturated values.
      for (int x = -1; x <= block_size_*2; x++) {
        if (i == 0) {
          above_row_[x] = mask_;
        } else {
          above_row_[x] = rnd.Rand16() & mask_;
        }
      }
      for (int y = 0; y < block_size_; y++) {
        if (i == 0) {
          left_col_[y] = mask_;
        } else {
          left_col_[y] = rnd.Rand16() & mask_;
        }
      }
      Predict(DC_PRED);
      CheckPrediction(i, &error_count);
    }
    ASSERT_EQ(0, error_count);
  }

  int block_size_;
  uint16_t *above_row_;
  uint16_t *left_col_;
  uint16_t *dst_;
  uint16_t *ref_dst_;
  ptrdiff_t stride_;
  int mask_;
};

typedef void (*intra_pred_fn_t)(
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above,
      const uint16_t *left, int bps);
typedef std::tr1::tuple<intra_pred_fn_t,
                        intra_pred_fn_t, int, int> intra_pred_params_t;
class VP9IntraPredTest
    : public VP9IntraPredBase,
      public ::testing::TestWithParam<intra_pred_params_t> {

  virtual void SetUp() {
    pred_fn_    = GET_PARAM(0);
    ref_fn_     = GET_PARAM(1);
    block_size_ = GET_PARAM(2);
    bit_depth_  = GET_PARAM(3);
    stride_     = block_size_ * 3;
    mask_       = (1 << bit_depth_) - 1;
  }

  virtual void Predict(PREDICTION_MODE mode) {
    const uint16_t *const_above_row = above_row_;
    const uint16_t *const_left_col = left_col_;
    ref_fn_(ref_dst_, stride_, const_above_row, const_left_col, bit_depth_);
    ASM_REGISTER_STATE_CHECK(pred_fn_(dst_, stride_, const_above_row,
                                      const_left_col, bit_depth_));
  }
  intra_pred_fn_t pred_fn_;
  intra_pred_fn_t ref_fn_;
  int bit_depth_;
};

TEST_P(VP9IntraPredTest, IntraPredTests) {
  // max block size is 32
  DECLARE_ALIGNED(16, uint16_t, left_col[2*32]);
  DECLARE_ALIGNED(16, uint16_t, above_data[2*32+32]);
  DECLARE_ALIGNED(16, uint16_t, dst[3 * 32 * 32]);
  DECLARE_ALIGNED(16, uint16_t, ref_dst[3 * 32 * 32]);
  RunTest(left_col, above_data, dst, ref_dst);
}

using std::tr1::make_tuple;

#if HAVE_SSE2
#if CONFIG_VP9_HIGHBITDEPTH
#if ARCH_X86_64
INSTANTIATE_TEST_CASE_P(SSE2_TO_C_8, VP9IntraPredTest,
                        ::testing::Values(
                            make_tuple(&vp9_highbd_dc_predictor_32x32_sse2,
                                       &vp9_highbd_dc_predictor_32x32_c, 32, 8),
                            make_tuple(&vp9_highbd_tm_predictor_16x16_sse2,
                                       &vp9_highbd_tm_predictor_16x16_c, 16, 8),
                            make_tuple(&vp9_highbd_tm_predictor_32x32_sse2,
                                       &vp9_highbd_tm_predictor_32x32_c, 32, 8),
                            make_tuple(&vp9_highbd_dc_predictor_4x4_sse,
                                       &vp9_highbd_dc_predictor_4x4_c, 4, 8),
                            make_tuple(&vp9_highbd_dc_predictor_8x8_sse2,
                                       &vp9_highbd_dc_predictor_8x8_c, 8, 8),
                            make_tuple(&vp9_highbd_dc_predictor_16x16_sse2,
                                       &vp9_highbd_dc_predictor_16x16_c, 16, 8),
                            make_tuple(&vp9_highbd_v_predictor_4x4_sse,
                                       &vp9_highbd_v_predictor_4x4_c, 4, 8),
                            make_tuple(&vp9_highbd_v_predictor_8x8_sse2,
                                       &vp9_highbd_v_predictor_8x8_c, 8, 8),
                            make_tuple(&vp9_highbd_v_predictor_16x16_sse2,
                                       &vp9_highbd_v_predictor_16x16_c, 16, 8),
                            make_tuple(&vp9_highbd_v_predictor_32x32_sse2,
                                       &vp9_highbd_v_predictor_32x32_c, 32, 8),
                            make_tuple(&vp9_highbd_tm_predictor_4x4_sse,
                                       &vp9_highbd_tm_predictor_4x4_c, 4, 8),
                            make_tuple(&vp9_highbd_tm_predictor_8x8_sse2,
                                       &vp9_highbd_tm_predictor_8x8_c, 8, 8)));
#else
INSTANTIATE_TEST_CASE_P(SSE2_TO_C_8, VP9IntraPredTest,
                        ::testing::Values(
                            make_tuple(&vp9_highbd_dc_predictor_4x4_sse,
                                       &vp9_highbd_dc_predictor_4x4_c, 4, 8),
                            make_tuple(&vp9_highbd_dc_predictor_8x8_sse2,
                                       &vp9_highbd_dc_predictor_8x8_c, 8, 8),
                            make_tuple(&vp9_highbd_dc_predictor_16x16_sse2,
                                       &vp9_highbd_dc_predictor_16x16_c, 16, 8),
                            make_tuple(&vp9_highbd_v_predictor_4x4_sse,
                                       &vp9_highbd_v_predictor_4x4_c, 4, 8),
                            make_tuple(&vp9_highbd_v_predictor_8x8_sse2,
                                       &vp9_highbd_v_predictor_8x8_c, 8, 8),
                            make_tuple(&vp9_highbd_v_predictor_16x16_sse2,
                                       &vp9_highbd_v_predictor_16x16_c, 16, 8),
                            make_tuple(&vp9_highbd_v_predictor_32x32_sse2,
                                       &vp9_highbd_v_predictor_32x32_c, 32, 8),
                            make_tuple(&vp9_highbd_tm_predictor_4x4_sse,
                                       &vp9_highbd_tm_predictor_4x4_c, 4, 8),
                            make_tuple(&vp9_highbd_tm_predictor_8x8_sse2,
                                       &vp9_highbd_tm_predictor_8x8_c, 8, 8)));
#endif
#if ARCH_X86_64
INSTANTIATE_TEST_CASE_P(SSE2_TO_C_10, VP9IntraPredTest,
                        ::testing::Values(
                            make_tuple(&vp9_highbd_dc_predictor_32x32_sse2,
                                       &vp9_highbd_dc_predictor_32x32_c, 32,
                                       10),
                            make_tuple(&vp9_highbd_tm_predictor_16x16_sse2,
                                       &vp9_highbd_tm_predictor_16x16_c, 16,
                                       10),
                            make_tuple(&vp9_highbd_tm_predictor_32x32_sse2,
                                       &vp9_highbd_tm_predictor_32x32_c, 32,
                                       10),
                            make_tuple(&vp9_highbd_dc_predictor_4x4_sse,
                                       &vp9_highbd_dc_predictor_4x4_c, 4, 10),
                            make_tuple(&vp9_highbd_dc_predictor_8x8_sse2,
                                       &vp9_highbd_dc_predictor_8x8_c, 8, 10),
                            make_tuple(&vp9_highbd_dc_predictor_16x16_sse2,
                                       &vp9_highbd_dc_predictor_16x16_c, 16,
                                       10),
                            make_tuple(&vp9_highbd_v_predictor_4x4_sse,
                                       &vp9_highbd_v_predictor_4x4_c, 4, 10),
                            make_tuple(&vp9_highbd_v_predictor_8x8_sse2,
                                       &vp9_highbd_v_predictor_8x8_c, 8, 10),
                            make_tuple(&vp9_highbd_v_predictor_16x16_sse2,
                                       &vp9_highbd_v_predictor_16x16_c, 16,
                                       10),
                            make_tuple(&vp9_highbd_v_predictor_32x32_sse2,
                                       &vp9_highbd_v_predictor_32x32_c, 32,
                                       10),
                            make_tuple(&vp9_highbd_tm_predictor_4x4_sse,
                                       &vp9_highbd_tm_predictor_4x4_c, 4, 10),
                            make_tuple(&vp9_highbd_tm_predictor_8x8_sse2,
                                       &vp9_highbd_tm_predictor_8x8_c, 8, 10)));
#else
INSTANTIATE_TEST_CASE_P(SSE2_TO_C_10, VP9IntraPredTest,
                        ::testing::Values(
                            make_tuple(&vp9_highbd_dc_predictor_4x4_sse,
                                       &vp9_highbd_dc_predictor_4x4_c, 4, 10),
                            make_tuple(&vp9_highbd_dc_predictor_8x8_sse2,
                                       &vp9_highbd_dc_predictor_8x8_c, 8, 10),
                            make_tuple(&vp9_highbd_dc_predictor_16x16_sse2,
                                       &vp9_highbd_dc_predictor_16x16_c, 16,
                                       10),
                            make_tuple(&vp9_highbd_v_predictor_4x4_sse,
                                       &vp9_highbd_v_predictor_4x4_c, 4, 10),
                            make_tuple(&vp9_highbd_v_predictor_8x8_sse2,
                                       &vp9_highbd_v_predictor_8x8_c, 8, 10),
                            make_tuple(&vp9_highbd_v_predictor_16x16_sse2,
                                       &vp9_highbd_v_predictor_16x16_c, 16, 10),
                            make_tuple(&vp9_highbd_v_predictor_32x32_sse2,
                                       &vp9_highbd_v_predictor_32x32_c, 32, 10),
                            make_tuple(&vp9_highbd_tm_predictor_4x4_sse,
                                       &vp9_highbd_tm_predictor_4x4_c, 4, 10),
                            make_tuple(&vp9_highbd_tm_predictor_8x8_sse2,
                                       &vp9_highbd_tm_predictor_8x8_c, 8, 10)));
#endif

#if ARCH_X86_64
INSTANTIATE_TEST_CASE_P(SSE2_TO_C_12, VP9IntraPredTest,
                        ::testing::Values(
                            make_tuple(&vp9_highbd_dc_predictor_32x32_sse2,
                                       &vp9_highbd_dc_predictor_32x32_c, 32,
                                       12),
                            make_tuple(&vp9_highbd_tm_predictor_16x16_sse2,
                                       &vp9_highbd_tm_predictor_16x16_c, 16,
                                       12),
                            make_tuple(&vp9_highbd_tm_predictor_32x32_sse2,
                                       &vp9_highbd_tm_predictor_32x32_c, 32,
                                       12),
                            make_tuple(&vp9_highbd_dc_predictor_4x4_sse,
                                       &vp9_highbd_dc_predictor_4x4_c, 4, 12),
                            make_tuple(&vp9_highbd_dc_predictor_8x8_sse2,
                                       &vp9_highbd_dc_predictor_8x8_c, 8, 12),
                            make_tuple(&vp9_highbd_dc_predictor_16x16_sse2,
                                       &vp9_highbd_dc_predictor_16x16_c, 16,
                                       12),
                            make_tuple(&vp9_highbd_v_predictor_4x4_sse,
                                       &vp9_highbd_v_predictor_4x4_c, 4, 12),
                            make_tuple(&vp9_highbd_v_predictor_8x8_sse2,
                                       &vp9_highbd_v_predictor_8x8_c, 8, 12),
                            make_tuple(&vp9_highbd_v_predictor_16x16_sse2,
                                       &vp9_highbd_v_predictor_16x16_c, 16,
                                       12),
                            make_tuple(&vp9_highbd_v_predictor_32x32_sse2,
                                       &vp9_highbd_v_predictor_32x32_c, 32,
                                       12),
                            make_tuple(&vp9_highbd_tm_predictor_4x4_sse,
                                       &vp9_highbd_tm_predictor_4x4_c, 4, 12),
                            make_tuple(&vp9_highbd_tm_predictor_8x8_sse2,
                                       &vp9_highbd_tm_predictor_8x8_c, 8, 12)));
#else
INSTANTIATE_TEST_CASE_P(SSE2_TO_C_12, VP9IntraPredTest,
                        ::testing::Values(
                            make_tuple(&vp9_highbd_dc_predictor_4x4_sse,
                                       &vp9_highbd_dc_predictor_4x4_c, 4, 12),
                            make_tuple(&vp9_highbd_dc_predictor_8x8_sse2,
                                       &vp9_highbd_dc_predictor_8x8_c, 8, 12),
                            make_tuple(&vp9_highbd_dc_predictor_16x16_sse2,
                                       &vp9_highbd_dc_predictor_16x16_c, 16,
                                       12),
                            make_tuple(&vp9_highbd_v_predictor_4x4_sse,
                                       &vp9_highbd_v_predictor_4x4_c, 4, 12),
                            make_tuple(&vp9_highbd_v_predictor_8x8_sse2,
                                       &vp9_highbd_v_predictor_8x8_c, 8, 12),
                            make_tuple(&vp9_highbd_v_predictor_16x16_sse2,
                                       &vp9_highbd_v_predictor_16x16_c, 16, 12),
                            make_tuple(&vp9_highbd_v_predictor_32x32_sse2,
                                       &vp9_highbd_v_predictor_32x32_c, 32, 12),
                            make_tuple(&vp9_highbd_tm_predictor_4x4_sse,
                                       &vp9_highbd_tm_predictor_4x4_c, 4, 12),
                            make_tuple(&vp9_highbd_tm_predictor_8x8_sse2,
                                       &vp9_highbd_tm_predictor_8x8_c, 8, 12)));
#endif
#endif  // CONFIG_VP9_HIGHBITDEPTH
#endif  // HAVE_SSE2
}  // namespace
