/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <string.h>
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "third_party/googletest/src/include/gtest/gtest.h"

#include "./vpx_config.h"
#include "./vp8_rtcd.h"
#include "vp8/common/blockd.h"
#include "vpx_mem/vpx_mem.h"

namespace {

using libvpx_test::ACMRandom;

class IntraPredBase {
 public:
  virtual ~IntraPredBase() { libvpx_test::ClearSystemState(); }

 protected:
  void SetupMacroblock(MACROBLOCKD *mbptr,
                       MODE_INFO *miptr,
                       uint8_t *data,
                       int block_size,
                       int stride,
                       int num_planes) {
    mbptr_ = mbptr;
    miptr_ = miptr;
    mbptr_->up_available = 1;
    mbptr_->left_available = 1;
    mbptr_->mode_info_context = miptr_;
    stride_ = stride;
    block_size_ = block_size;
    num_planes_ = num_planes;
    for (int p = 0; p < num_planes; p++)
      data_ptr_[p] = data + stride * (block_size + 1) * p +
                     stride + block_size;
  }

  void FillRandom() {
    // Fill edges with random data
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    for (int p = 0; p < num_planes_; p++) {
      for (int x = -1 ; x <= block_size_; x++)
        data_ptr_[p][x - stride_] = rnd.Rand8();
      for (int y = 0; y < block_size_; y++)
        data_ptr_[p][y * stride_ - 1] = rnd.Rand8();
    }
  }

  virtual void Predict(MB_PREDICTION_MODE mode) = 0;

  void SetLeftUnavailable() {
    mbptr_->left_available = 0;
    for (int p = 0; p < num_planes_; p++)
      for (int i = -1; i < block_size_; ++i)
        data_ptr_[p][stride_ * i - 1] = 129;
  }

  void SetTopUnavailable() {
    mbptr_->up_available = 0;
    for (int p = 0; p < num_planes_; p++)
      memset(&data_ptr_[p][-1 - stride_], 127, block_size_ + 2);
  }

  void SetTopLeftUnavailable() {
    SetLeftUnavailable();
    SetTopUnavailable();
  }

  int BlockSizeLog2Min1() const {
    switch (block_size_) {
      case 16:
        return 3;
      case 8:
        return 2;
      default:
        return 0;
    }
  }

  // check DC prediction output against a reference
  void CheckDCPrediction() const {
    for (int p = 0; p < num_planes_; p++) {
      // calculate expected DC
      int expected;
      if (mbptr_->up_available || mbptr_->left_available) {
        int sum = 0, shift = BlockSizeLog2Min1() + mbptr_->up_available +
                             mbptr_->left_available;
        if (mbptr_->up_available)
          for (int x = 0; x < block_size_; x++)
            sum += data_ptr_[p][x - stride_];
        if (mbptr_->left_available)
          for (int y = 0; y < block_size_; y++)
            sum += data_ptr_[p][y * stride_ - 1];
        expected = (sum + (1 << (shift - 1))) >> shift;
      } else {
        expected = 0x80;
      }
      // check that all subsequent lines are equal to the first
      for (int y = 1; y < block_size_; ++y)
        ASSERT_EQ(0, memcmp(data_ptr_[p], &data_ptr_[p][y * stride_],
                            block_size_));
      // within the first line, ensure that each pixel has the same value
      for (int x = 1; x < block_size_; ++x)
        ASSERT_EQ(data_ptr_[p][0], data_ptr_[p][x]);
      // now ensure that that pixel has the expected (DC) value
      ASSERT_EQ(expected, data_ptr_[p][0]);
    }
  }

  // check V prediction output against a reference
  void CheckVPrediction() const {
    // check that all lines equal the top border
    for (int p = 0; p < num_planes_; p++)
      for (int y = 0; y < block_size_; y++)
        ASSERT_EQ(0, memcmp(&data_ptr_[p][-stride_],
                            &data_ptr_[p][y * stride_], block_size_));
  }

  // check H prediction output against a reference
  void CheckHPrediction() const {
    // for each line, ensure that each pixel is equal to the left border
    for (int p = 0; p < num_planes_; p++)
      for (int y = 0; y < block_size_; y++)
        for (int x = 0; x < block_size_; x++)
          ASSERT_EQ(data_ptr_[p][-1 + y * stride_],
                    data_ptr_[p][x + y * stride_]);
  }

  static int ClipByte(int value) {
    if (value > 255)
      return 255;
    else if (value < 0)
      return 0;
    return value;
  }

  // check TM prediction output against a reference
  void CheckTMPrediction() const {
    for (int p = 0; p < num_planes_; p++)
      for (int y = 0; y < block_size_; y++)
        for (int x = 0; x < block_size_; x++) {
          const int expected = ClipByte(data_ptr_[p][x - stride_]
                                      + data_ptr_[p][stride_ * y - 1]
                                      - data_ptr_[p][-1 - stride_]);
          ASSERT_EQ(expected, data_ptr_[p][y * stride_ + x]);
       }
  }

  // Actual test
  void RunTest() {
    {
      SCOPED_TRACE("DC_PRED");
      FillRandom();
      Predict(DC_PRED);
      CheckDCPrediction();
    }
    {
      SCOPED_TRACE("DC_PRED LEFT");
      FillRandom();
      SetLeftUnavailable();
      Predict(DC_PRED);
      CheckDCPrediction();
    }
    {
      SCOPED_TRACE("DC_PRED TOP");
      FillRandom();
      SetTopUnavailable();
      Predict(DC_PRED);
      CheckDCPrediction();
    }
    {
      SCOPED_TRACE("DC_PRED TOP_LEFT");
      FillRandom();
      SetTopLeftUnavailable();
      Predict(DC_PRED);
      CheckDCPrediction();
    }
    {
      SCOPED_TRACE("H_PRED");
      FillRandom();
      Predict(H_PRED);
      CheckHPrediction();
    }
    {
      SCOPED_TRACE("V_PRED");
      FillRandom();
      Predict(V_PRED);
      CheckVPrediction();
    }
    {
      SCOPED_TRACE("TM_PRED");
      FillRandom();
      Predict(TM_PRED);
      CheckTMPrediction();
    }
  }

  MACROBLOCKD *mbptr_;
  MODE_INFO *miptr_;
  uint8_t *data_ptr_[2];  // in the case of Y, only [0] is used
  int stride_;
  int block_size_;
  int num_planes_;
};

typedef void (*IntraPredYFunc)(MACROBLOCKD *x,
                               uint8_t *yabove_row,
                               uint8_t *yleft,
                               int left_stride,
                               uint8_t *ypred_ptr,
                               int y_stride);

class IntraPredYTest
    : public IntraPredBase,
      public ::testing::TestWithParam<IntraPredYFunc> {
 public:
  static void SetUpTestCase() {
    mb_ = reinterpret_cast<MACROBLOCKD*>(
        vpx_memalign(32, sizeof(MACROBLOCKD)));
    mi_ = reinterpret_cast<MODE_INFO*>(
        vpx_memalign(32, sizeof(MODE_INFO)));
    data_array_ = reinterpret_cast<uint8_t*>(
        vpx_memalign(kDataAlignment, kDataBufferSize));
  }

  static void TearDownTestCase() {
    vpx_free(data_array_);
    vpx_free(mi_);
    vpx_free(mb_);
    data_array_ = NULL;
  }

 protected:
  static const int kBlockSize = 16;
  static const int kDataAlignment = 16;
  static const int kStride = kBlockSize * 3;
  // We use 48 so that the data pointer of the first pixel in each row of
  // each macroblock is 16-byte aligned, and this gives us access to the
  // top-left and top-right corner pixels belonging to the top-left/right
  // macroblocks.
  // We use 17 lines so we have one line above us for top-prediction.
  static const int kDataBufferSize = kStride * (kBlockSize + 1);

  virtual void SetUp() {
    pred_fn_ = GetParam();
    SetupMacroblock(mb_, mi_, data_array_, kBlockSize, kStride, 1);
  }

  virtual void Predict(MB_PREDICTION_MODE mode) {
    mbptr_->mode_info_context->mbmi.mode = mode;
    ASM_REGISTER_STATE_CHECK(pred_fn_(mbptr_,
                                      data_ptr_[0] - kStride,
                                      data_ptr_[0] - 1, kStride,
                                      data_ptr_[0], kStride));
  }

  IntraPredYFunc pred_fn_;
  static uint8_t* data_array_;
  static MACROBLOCKD * mb_;
  static MODE_INFO *mi_;
};

MACROBLOCKD* IntraPredYTest::mb_ = NULL;
MODE_INFO* IntraPredYTest::mi_ = NULL;
uint8_t* IntraPredYTest::data_array_ = NULL;

TEST_P(IntraPredYTest, IntraPredTests) {
  RunTest();
}

INSTANTIATE_TEST_CASE_P(C, IntraPredYTest,
                        ::testing::Values(
                            vp8_build_intra_predictors_mby_s_c));
#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(SSE2, IntraPredYTest,
                        ::testing::Values(
                            vp8_build_intra_predictors_mby_s_sse2));
#endif
#if HAVE_SSSE3
INSTANTIATE_TEST_CASE_P(SSSE3, IntraPredYTest,
                        ::testing::Values(
                            vp8_build_intra_predictors_mby_s_ssse3));
#endif
#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(NEON, IntraPredYTest,
                        ::testing::Values(
                            vp8_build_intra_predictors_mby_s_neon));
#endif

typedef void (*IntraPredUvFunc)(MACROBLOCKD *x,
                                uint8_t *uabove_row,
                                uint8_t *vabove_row,
                                uint8_t *uleft,
                                uint8_t *vleft,
                                int left_stride,
                                uint8_t *upred_ptr,
                                uint8_t *vpred_ptr,
                                int pred_stride);

class IntraPredUVTest
    : public IntraPredBase,
      public ::testing::TestWithParam<IntraPredUvFunc> {
 public:
  static void SetUpTestCase() {
    mb_ = reinterpret_cast<MACROBLOCKD*>(
        vpx_memalign(32, sizeof(MACROBLOCKD)));
    mi_ = reinterpret_cast<MODE_INFO*>(
        vpx_memalign(32, sizeof(MODE_INFO)));
    data_array_ = reinterpret_cast<uint8_t*>(
        vpx_memalign(kDataAlignment, kDataBufferSize));
  }

  static void TearDownTestCase() {
    vpx_free(data_array_);
    vpx_free(mi_);
    vpx_free(mb_);
    data_array_ = NULL;
  }

 protected:
  static const int kBlockSize = 8;
  static const int kDataAlignment = 8;
  static const int kStride = kBlockSize * 3;
  // We use 24 so that the data pointer of the first pixel in each row of
  // each macroblock is 8-byte aligned, and this gives us access to the
  // top-left and top-right corner pixels belonging to the top-left/right
  // macroblocks.
  // We use 9 lines so we have one line above us for top-prediction.
  // [0] = U, [1] = V
  static const int kDataBufferSize = 2 * kStride * (kBlockSize + 1);

  virtual void SetUp() {
    pred_fn_ = GetParam();
    SetupMacroblock(mb_, mi_, data_array_, kBlockSize, kStride, 2);
  }

  virtual void Predict(MB_PREDICTION_MODE mode) {
    mbptr_->mode_info_context->mbmi.uv_mode = mode;
    pred_fn_(mbptr_, data_ptr_[0] - kStride, data_ptr_[1] - kStride,
             data_ptr_[0] - 1, data_ptr_[1] - 1, kStride,
             data_ptr_[0], data_ptr_[1], kStride);
  }

  IntraPredUvFunc pred_fn_;
  // We use 24 so that the data pointer of the first pixel in each row of
  // each macroblock is 8-byte aligned, and this gives us access to the
  // top-left and top-right corner pixels belonging to the top-left/right
  // macroblocks.
  // We use 9 lines so we have one line above us for top-prediction.
  // [0] = U, [1] = V
  static uint8_t* data_array_;
  static MACROBLOCKD* mb_;
  static MODE_INFO* mi_;
};

MACROBLOCKD* IntraPredUVTest::mb_ = NULL;
MODE_INFO* IntraPredUVTest::mi_ = NULL;
uint8_t* IntraPredUVTest::data_array_ = NULL;

TEST_P(IntraPredUVTest, IntraPredTests) {
  RunTest();
}

INSTANTIATE_TEST_CASE_P(C, IntraPredUVTest,
                        ::testing::Values(
                            vp8_build_intra_predictors_mbuv_s_c));
#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(SSE2, IntraPredUVTest,
                        ::testing::Values(
                            vp8_build_intra_predictors_mbuv_s_sse2));
#endif
#if HAVE_SSSE3
INSTANTIATE_TEST_CASE_P(SSSE3, IntraPredUVTest,
                        ::testing::Values(
                            vp8_build_intra_predictors_mbuv_s_ssse3));
#endif
#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(NEON, IntraPredUVTest,
                        ::testing::Values(
                            vp8_build_intra_predictors_mbuv_s_neon));
#endif

}  // namespace
