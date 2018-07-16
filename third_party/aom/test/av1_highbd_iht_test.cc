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
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "av1/common/enums.h"
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
#define PARAM_LIST_8X8                                   \
  &av1_fwd_txfm2d_8x8_c, &av1_inv_txfm2d_add_8x8_sse4_1, \
      &av1_inv_txfm2d_add_8x8_c, 64
#define PARAM_LIST_16X16                                     \
  &av1_fwd_txfm2d_16x16_c, &av1_inv_txfm2d_add_16x16_sse4_1, \
      &av1_inv_txfm2d_add_16x16_c, 256
#define PARAM_LIST_64X64                                     \
  &av1_fwd_txfm2d_64x64_c, &av1_inv_txfm2d_add_64x64_sse4_1, \
      &av1_inv_txfm2d_add_64x64_c, 4096

const IHbdHtParam kArrayIhtParam[] = {
  // 16x16
  make_tuple(PARAM_LIST_16X16, DCT_DCT, 10),
  make_tuple(PARAM_LIST_16X16, DCT_DCT, 12),
  make_tuple(PARAM_LIST_16X16, ADST_DCT, 10),
  make_tuple(PARAM_LIST_16X16, ADST_DCT, 12),
  make_tuple(PARAM_LIST_16X16, DCT_ADST, 10),
  make_tuple(PARAM_LIST_16X16, DCT_ADST, 12),
  make_tuple(PARAM_LIST_16X16, ADST_ADST, 10),
  make_tuple(PARAM_LIST_16X16, ADST_ADST, 12),
  make_tuple(PARAM_LIST_16X16, FLIPADST_DCT, 10),
  make_tuple(PARAM_LIST_16X16, FLIPADST_DCT, 12),
  make_tuple(PARAM_LIST_16X16, DCT_FLIPADST, 10),
  make_tuple(PARAM_LIST_16X16, DCT_FLIPADST, 12),
  make_tuple(PARAM_LIST_16X16, FLIPADST_FLIPADST, 10),
  make_tuple(PARAM_LIST_16X16, FLIPADST_FLIPADST, 12),
  make_tuple(PARAM_LIST_16X16, ADST_FLIPADST, 10),
  make_tuple(PARAM_LIST_16X16, ADST_FLIPADST, 12),
  make_tuple(PARAM_LIST_16X16, FLIPADST_ADST, 10),
  make_tuple(PARAM_LIST_16X16, FLIPADST_ADST, 12),
  // 8x8
  make_tuple(PARAM_LIST_8X8, DCT_DCT, 10),
  make_tuple(PARAM_LIST_8X8, DCT_DCT, 12),
  make_tuple(PARAM_LIST_8X8, ADST_DCT, 10),
  make_tuple(PARAM_LIST_8X8, ADST_DCT, 12),
  make_tuple(PARAM_LIST_8X8, DCT_ADST, 10),
  make_tuple(PARAM_LIST_8X8, DCT_ADST, 12),
  make_tuple(PARAM_LIST_8X8, ADST_ADST, 10),
  make_tuple(PARAM_LIST_8X8, ADST_ADST, 12),
  make_tuple(PARAM_LIST_8X8, FLIPADST_DCT, 10),
  make_tuple(PARAM_LIST_8X8, FLIPADST_DCT, 12),
  make_tuple(PARAM_LIST_8X8, DCT_FLIPADST, 10),
  make_tuple(PARAM_LIST_8X8, DCT_FLIPADST, 12),
  make_tuple(PARAM_LIST_8X8, FLIPADST_FLIPADST, 10),
  make_tuple(PARAM_LIST_8X8, FLIPADST_FLIPADST, 12),
  make_tuple(PARAM_LIST_8X8, ADST_FLIPADST, 10),
  make_tuple(PARAM_LIST_8X8, ADST_FLIPADST, 12),
  make_tuple(PARAM_LIST_8X8, FLIPADST_ADST, 10),
  make_tuple(PARAM_LIST_8X8, FLIPADST_ADST, 12),
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
  make_tuple(PARAM_LIST_64X64, DCT_DCT, 10),
  make_tuple(PARAM_LIST_64X64, DCT_DCT, 12),
};

INSTANTIATE_TEST_CASE_P(SSE4_1, AV1HighbdInvHTNxN,
                        ::testing::ValuesIn(kArrayIhtParam));
#endif  // HAVE_SSE4_1

#if HAVE_AVX2
#define PARAM_LIST_32X32                                   \
  &av1_fwd_txfm2d_32x32_c, &av1_inv_txfm2d_add_32x32_avx2, \
      &av1_inv_txfm2d_add_32x32_c, 1024

const IHbdHtParam kArrayIhtParam32x32[] = {
  // 32x32
  make_tuple(PARAM_LIST_32X32, DCT_DCT, 10),
  make_tuple(PARAM_LIST_32X32, DCT_DCT, 12),
};

INSTANTIATE_TEST_CASE_P(AVX2, AV1HighbdInvHTNxN,
                        ::testing::ValuesIn(kArrayIhtParam32x32));

#endif  // HAVE_AVX2
}  // namespace
