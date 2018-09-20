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

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/av1_rtcd.h"

#include "test/acm_random.h"
#include "test/av1_txfm_test.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "av1/common/enums.h"
#include "av1/common/scan.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_ports/mem.h"

namespace {

using ::testing::tuple;
using libaom_test::ACMRandom;

typedef void (*HbdHtFunc)(const int16_t *input, int32_t *output, int stride,
                          TX_TYPE tx_type, int bd);

typedef void (*IHbdHtFunc)(const int32_t *coeff, uint16_t *output, int stride,
                           TX_TYPE tx_type, int bd);

// Test parameter argument list:
//   <transform reference function,
//    optimized inverse transform function,
//    inverse transform reference function,
//    num_coeffs,
//    tx_type,
//    bit_depth>
typedef tuple<HbdHtFunc, IHbdHtFunc, IHbdHtFunc, int, TX_TYPE, int> IHbdHtParam;

class AV1HighbdInvHTNxN : public ::testing::TestWithParam<IHbdHtParam> {
 public:
  virtual ~AV1HighbdInvHTNxN() {}

  virtual void SetUp() {
    txfm_ref_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    inv_txfm_ref_ = GET_PARAM(2);
    num_coeffs_ = GET_PARAM(3);
    tx_type_ = GET_PARAM(4);
    bit_depth_ = GET_PARAM(5);

    input_ = reinterpret_cast<int16_t *>(
        aom_memalign(16, sizeof(input_[0]) * num_coeffs_));

    // Note:
    // Inverse transform input buffer is 32-byte aligned
    // Refer to <root>/av1/encoder/context_tree.c, function,
    // void alloc_mode_context().
    coeffs_ = reinterpret_cast<int32_t *>(
        aom_memalign(32, sizeof(coeffs_[0]) * num_coeffs_));
    output_ = reinterpret_cast<uint16_t *>(
        aom_memalign(32, sizeof(output_[0]) * num_coeffs_));
    output_ref_ = reinterpret_cast<uint16_t *>(
        aom_memalign(32, sizeof(output_ref_[0]) * num_coeffs_));
  }

  virtual void TearDown() {
    aom_free(input_);
    aom_free(coeffs_);
    aom_free(output_);
    aom_free(output_ref_);
    libaom_test::ClearSystemState();
  }

 protected:
  void RunBitexactCheck();

 private:
  int GetStride() const {
    if (16 == num_coeffs_) {
      return 4;
    } else if (64 == num_coeffs_) {
      return 8;
    } else if (256 == num_coeffs_) {
      return 16;
    } else if (1024 == num_coeffs_) {
      return 32;
    } else if (4096 == num_coeffs_) {
      return 64;
    } else {
      return 0;
    }
  }

  HbdHtFunc txfm_ref_;
  IHbdHtFunc inv_txfm_;
  IHbdHtFunc inv_txfm_ref_;
  int num_coeffs_;
  TX_TYPE tx_type_;
  int bit_depth_;

  int16_t *input_;
  int32_t *coeffs_;
  uint16_t *output_;
  uint16_t *output_ref_;
};

void AV1HighbdInvHTNxN::RunBitexactCheck() {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  const int stride = GetStride();
  const int num_tests = 20000;
  const uint16_t mask = (1 << bit_depth_) - 1;

  for (int i = 0; i < num_tests; ++i) {
    for (int j = 0; j < num_coeffs_; ++j) {
      input_[j] = (rnd.Rand16() & mask) - (rnd.Rand16() & mask);
      output_ref_[j] = rnd.Rand16() & mask;
      output_[j] = output_ref_[j];
    }

    txfm_ref_(input_, coeffs_, stride, tx_type_, bit_depth_);
    inv_txfm_ref_(coeffs_, output_ref_, stride, tx_type_, bit_depth_);
    ASM_REGISTER_STATE_CHECK(
        inv_txfm_(coeffs_, output_, stride, tx_type_, bit_depth_));

    for (int j = 0; j < num_coeffs_; ++j) {
      EXPECT_EQ(output_ref_[j], output_[j])
          << "Not bit-exact result at index: " << j << " At test block: " << i;
    }
  }
}

TEST_P(AV1HighbdInvHTNxN, InvTransResultCheck) { RunBitexactCheck(); }

using ::testing::make_tuple;

#if HAVE_SSE4_1
#define PARAM_LIST_4X4                                   \
  &av1_fwd_txfm2d_4x4_c, &av1_inv_txfm2d_add_4x4_sse4_1, \
      &av1_inv_txfm2d_add_4x4_c, 16

const IHbdHtParam kArrayIhtParam[] = {
  // 4x4
  make_tuple(PARAM_LIST_4X4, DCT_DCT, 10),
  make_tuple(PARAM_LIST_4X4, DCT_DCT, 12),
  make_tuple(PARAM_LIST_4X4, ADST_DCT, 10),
  make_tuple(PARAM_LIST_4X4, ADST_DCT, 12),
  make_tuple(PARAM_LIST_4X4, DCT_ADST, 10),
  make_tuple(PARAM_LIST_4X4, DCT_ADST, 12),
  make_tuple(PARAM_LIST_4X4, ADST_ADST, 10),
  make_tuple(PARAM_LIST_4X4, ADST_ADST, 12),
  make_tuple(PARAM_LIST_4X4, FLIPADST_DCT, 10),
  make_tuple(PARAM_LIST_4X4, FLIPADST_DCT, 12),
  make_tuple(PARAM_LIST_4X4, DCT_FLIPADST, 10),
  make_tuple(PARAM_LIST_4X4, DCT_FLIPADST, 12),
  make_tuple(PARAM_LIST_4X4, FLIPADST_FLIPADST, 10),
  make_tuple(PARAM_LIST_4X4, FLIPADST_FLIPADST, 12),
  make_tuple(PARAM_LIST_4X4, ADST_FLIPADST, 10),
  make_tuple(PARAM_LIST_4X4, ADST_FLIPADST, 12),
  make_tuple(PARAM_LIST_4X4, FLIPADST_ADST, 10),
  make_tuple(PARAM_LIST_4X4, FLIPADST_ADST, 12),
};

INSTANTIATE_TEST_CASE_P(SSE4_1, AV1HighbdInvHTNxN,
                        ::testing::ValuesIn(kArrayIhtParam));
#endif  // HAVE_SSE4_1

typedef void (*HighbdInvTxfm2dFunc)(const int32_t *input, uint8_t *output,
                                    int stride, const TxfmParam *txfm_param);

typedef ::testing::tuple<const HighbdInvTxfm2dFunc> AV1HighbdInvTxfm2dParam;
class AV1HighbdInvTxfm2d
    : public ::testing::TestWithParam<AV1HighbdInvTxfm2dParam> {
 public:
  virtual void SetUp() { target_func_ = GET_PARAM(0); }
  void RunAV1InvTxfm2dTest(TX_TYPE tx_type, TX_SIZE tx_size, int run_times,
                           int bit_depth);

 private:
  HighbdInvTxfm2dFunc target_func_;
};

void AV1HighbdInvTxfm2d::RunAV1InvTxfm2dTest(TX_TYPE tx_type_, TX_SIZE tx_size_,
                                             int run_times, int bit_depth_) {
  FwdTxfm2dFunc fwd_func_ = libaom_test::fwd_txfm_func_ls[tx_size_];
  TxfmParam txfm_param;
  const int BLK_WIDTH = 64;
  const int BLK_SIZE = BLK_WIDTH * BLK_WIDTH;
  DECLARE_ALIGNED(16, int16_t, input[BLK_SIZE]) = { 0 };
  DECLARE_ALIGNED(32, int32_t, inv_input[BLK_SIZE]) = { 0 };
  DECLARE_ALIGNED(32, uint16_t, output[BLK_SIZE]) = { 0 };
  DECLARE_ALIGNED(32, uint16_t, ref_output[BLK_SIZE]) = { 0 };
  int stride = BLK_WIDTH;
  int rows = tx_size_high[tx_size_];
  int cols = tx_size_wide[tx_size_];
  const int rows_nonezero = AOMMIN(32, rows);
  const int cols_nonezero = AOMMIN(32, cols);
  const uint16_t mask = (1 << bit_depth_) - 1;
  run_times /= (rows * cols);
  run_times = AOMMAX(1, run_times);
  const SCAN_ORDER *scan_order = get_default_scan(tx_size_, tx_type_);
  const int16_t *scan = scan_order->scan;
  const int16_t eobmax = rows_nonezero * cols_nonezero;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  int randTimes = run_times == 1 ? (eobmax) : 1;

  txfm_param.tx_type = tx_type_;
  txfm_param.tx_size = tx_size_;
  txfm_param.lossless = 0;
  txfm_param.bd = bit_depth_;
  txfm_param.is_hbd = 1;
  txfm_param.tx_set_type = EXT_TX_SET_ALL16;

  for (int cnt = 0; cnt < randTimes; ++cnt) {
    for (int r = 0; r < BLK_WIDTH; ++r) {
      for (int c = 0; c < BLK_WIDTH; ++c) {
        input[r * cols + c] = (rnd.Rand16() & mask) - (rnd.Rand16() & mask);
        output[r * stride + c] = rnd.Rand16() & mask;

        ref_output[r * stride + c] = output[r * stride + c];
      }
    }
    fwd_func_(input, inv_input, stride, tx_type_, bit_depth_);

    // produce eob input by setting high freq coeffs to zero
    const int eob = AOMMIN(cnt + 1, eobmax);
    for (int i = eob; i < eobmax; i++) {
      inv_input[scan[i]] = 0;
    }
    txfm_param.eob = eob;
    aom_usec_timer ref_timer, test_timer;

    aom_usec_timer_start(&ref_timer);
    for (int i = 0; i < run_times; ++i) {
      av1_highbd_inv_txfm_add_c(inv_input, CONVERT_TO_BYTEPTR(ref_output),
                                stride, &txfm_param);
    }
    aom_usec_timer_mark(&ref_timer);
    const int elapsed_time_c =
        static_cast<int>(aom_usec_timer_elapsed(&ref_timer));

    aom_usec_timer_start(&test_timer);
    for (int i = 0; i < run_times; ++i) {
      target_func_(inv_input, CONVERT_TO_BYTEPTR(output), stride, &txfm_param);
    }
    aom_usec_timer_mark(&test_timer);
    const int elapsed_time_simd =
        static_cast<int>(aom_usec_timer_elapsed(&test_timer));
    if (run_times > 10) {
      printf(
          "txfm_size[%d] \t txfm_type[%d] \t c_time=%d \t simd_time=%d \t "
          "gain=%d \n",
          tx_size_, tx_type_, elapsed_time_c, elapsed_time_simd,
          (elapsed_time_c / elapsed_time_simd));
    } else {
      for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
          ASSERT_EQ(ref_output[r * stride + c], output[r * stride + c])
              << "[" << r << "," << c << "] " << cnt
              << " tx_size: " << static_cast<int>(tx_size_)
              << " tx_type: " << tx_type_ << " eob " << eob;
        }
      }
    }
  }
}

TEST_P(AV1HighbdInvTxfm2d, match) {
  int bitdepth_ar[2] = { 10, 12 };
  for (int k = 0; k < 2; ++k) {
    int bd = bitdepth_ar[k];
    for (int j = 0; j < (int)(TX_SIZES_ALL); ++j) {
      for (int i = 0; i < (int)TX_TYPES; ++i) {
        if (libaom_test::IsTxSizeTypeValid(static_cast<TX_SIZE>(j),
                                           static_cast<TX_TYPE>(i))) {
          RunAV1InvTxfm2dTest(static_cast<TX_TYPE>(i), static_cast<TX_SIZE>(j),
                              1, bd);
        }
      }
    }
  }
}

TEST_P(AV1HighbdInvTxfm2d, DISABLED_Speed) {
  int bitdepth_ar[2] = { 10, 12 };
  for (int k = 0; k < 2; ++k) {
    int bd = bitdepth_ar[k];
    for (int j = 0; j < (int)(TX_SIZES_ALL); ++j) {
      for (int i = 0; i < (int)TX_TYPES; ++i) {
        if (libaom_test::IsTxSizeTypeValid(static_cast<TX_SIZE>(j),
                                           static_cast<TX_TYPE>(i))) {
          RunAV1InvTxfm2dTest(static_cast<TX_TYPE>(i), static_cast<TX_SIZE>(j),
                              1000000, bd);
        }
      }
    }
  }
}

#if HAVE_SSE4_1
INSTANTIATE_TEST_CASE_P(SSE4_1, AV1HighbdInvTxfm2d,
                        ::testing::Values(av1_highbd_inv_txfm_add_sse4_1));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_CASE_P(AVX2, AV1HighbdInvTxfm2d,
                        ::testing::Values(av1_highbd_inv_txfm_add_avx2));
#endif
}  // namespace
