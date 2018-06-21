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
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"

using libaom_test::ACMRandom;

namespace {
const int number_of_iterations = 200;

typedef unsigned int (*MaskedSADFunc)(const uint8_t *src, int src_stride,
                                      const uint8_t *ref, int ref_stride,
                                      const uint8_t *second_pred,
                                      const uint8_t *msk, int msk_stride,
                                      int invert_mask);
typedef ::testing::tuple<MaskedSADFunc, MaskedSADFunc> MaskedSADParam;

class MaskedSADTest : public ::testing::TestWithParam<MaskedSADParam> {
 public:
  virtual ~MaskedSADTest() {}
  virtual void SetUp() {
    maskedSAD_op_ = GET_PARAM(0);
    ref_maskedSAD_op_ = GET_PARAM(1);
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  MaskedSADFunc maskedSAD_op_;
  MaskedSADFunc ref_maskedSAD_op_;
};

TEST_P(MaskedSADTest, OperationCheck) {
  unsigned int ref_ret, ret;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint8_t, src_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);
  DECLARE_ALIGNED(16, uint8_t, ref_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);
  DECLARE_ALIGNED(16, uint8_t, second_pred_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);
  DECLARE_ALIGNED(16, uint8_t, msk_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);
  int err_count = 0;
  int first_failure = -1;
  int src_stride = MAX_SB_SIZE;
  int ref_stride = MAX_SB_SIZE;
  int msk_stride = MAX_SB_SIZE;
  for (int i = 0; i < number_of_iterations; ++i) {
    for (int j = 0; j < MAX_SB_SIZE * MAX_SB_SIZE; j++) {
      src_ptr[j] = rnd.Rand8();
      ref_ptr[j] = rnd.Rand8();
      second_pred_ptr[j] = rnd.Rand8();
      msk_ptr[j] = ((rnd.Rand8() & 0x7f) > 64) ? rnd.Rand8() & 0x3f : 64;
      assert(msk_ptr[j] <= 64);
    }

    for (int invert_mask = 0; invert_mask < 2; ++invert_mask) {
      ref_ret =
          ref_maskedSAD_op_(src_ptr, src_stride, ref_ptr, ref_stride,
                            second_pred_ptr, msk_ptr, msk_stride, invert_mask);
      ASM_REGISTER_STATE_CHECK(ret = maskedSAD_op_(src_ptr, src_stride, ref_ptr,
                                                   ref_stride, second_pred_ptr,
                                                   msk_ptr, msk_stride,
                                                   invert_mask));
      if (ret != ref_ret) {
        err_count++;
        if (first_failure == -1) first_failure = i;
      }
    }
  }
  EXPECT_EQ(0, err_count)
      << "Error: Masked SAD Test, C output doesn't match SSSE3 output. "
      << "First failed at test case " << first_failure;
}

typedef unsigned int (*HighbdMaskedSADFunc)(const uint8_t *src, int src_stride,
                                            const uint8_t *ref, int ref_stride,
                                            const uint8_t *second_pred,
                                            const uint8_t *msk, int msk_stride,
                                            int invert_mask);
typedef ::testing::tuple<HighbdMaskedSADFunc, HighbdMaskedSADFunc>
    HighbdMaskedSADParam;

class HighbdMaskedSADTest
    : public ::testing::TestWithParam<HighbdMaskedSADParam> {
 public:
  virtual ~HighbdMaskedSADTest() {}
  virtual void SetUp() {
    maskedSAD_op_ = GET_PARAM(0);
    ref_maskedSAD_op_ = GET_PARAM(1);
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  HighbdMaskedSADFunc maskedSAD_op_;
  HighbdMaskedSADFunc ref_maskedSAD_op_;
};

TEST_P(HighbdMaskedSADTest, OperationCheck) {
  unsigned int ref_ret, ret;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint16_t, src_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);
  DECLARE_ALIGNED(16, uint16_t, ref_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);
  DECLARE_ALIGNED(16, uint16_t, second_pred_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);
  DECLARE_ALIGNED(16, uint8_t, msk_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);
  uint8_t *src8_ptr = CONVERT_TO_BYTEPTR(src_ptr);
  uint8_t *ref8_ptr = CONVERT_TO_BYTEPTR(ref_ptr);
  uint8_t *second_pred8_ptr = CONVERT_TO_BYTEPTR(second_pred_ptr);
  int err_count = 0;
  int first_failure = -1;
  int src_stride = MAX_SB_SIZE;
  int ref_stride = MAX_SB_SIZE;
  int msk_stride = MAX_SB_SIZE;
  for (int i = 0; i < number_of_iterations; ++i) {
    for (int j = 0; j < MAX_SB_SIZE * MAX_SB_SIZE; j++) {
      src_ptr[j] = rnd.Rand16() & 0xfff;
      ref_ptr[j] = rnd.Rand16() & 0xfff;
      second_pred_ptr[j] = rnd.Rand16() & 0xfff;
      msk_ptr[j] = ((rnd.Rand8() & 0x7f) > 64) ? rnd.Rand8() & 0x3f : 64;
    }

    for (int invert_mask = 0; invert_mask < 2; ++invert_mask) {
      ref_ret =
          ref_maskedSAD_op_(src8_ptr, src_stride, ref8_ptr, ref_stride,
                            second_pred8_ptr, msk_ptr, msk_stride, invert_mask);
      ASM_REGISTER_STATE_CHECK(ret = maskedSAD_op_(src8_ptr, src_stride,
                                                   ref8_ptr, ref_stride,
                                                   second_pred8_ptr, msk_ptr,
                                                   msk_stride, invert_mask));
      if (ret != ref_ret) {
        err_count++;
        if (first_failure == -1) first_failure = i;
      }
    }
  }
  EXPECT_EQ(0, err_count)
      << "Error: High BD Masked SAD Test, C output doesn't match SSSE3 output. "
      << "First failed at test case " << first_failure;
}

using ::testing::make_tuple;

#if HAVE_SSSE3
const MaskedSADParam msad_test[] = {
  make_tuple(&aom_masked_sad128x128_ssse3, &aom_masked_sad128x128_c),
  make_tuple(&aom_masked_sad128x64_ssse3, &aom_masked_sad128x64_c),
  make_tuple(&aom_masked_sad64x128_ssse3, &aom_masked_sad64x128_c),
  make_tuple(&aom_masked_sad64x64_ssse3, &aom_masked_sad64x64_c),
  make_tuple(&aom_masked_sad64x32_ssse3, &aom_masked_sad64x32_c),
  make_tuple(&aom_masked_sad32x64_ssse3, &aom_masked_sad32x64_c),
  make_tuple(&aom_masked_sad32x32_ssse3, &aom_masked_sad32x32_c),
  make_tuple(&aom_masked_sad32x16_ssse3, &aom_masked_sad32x16_c),
  make_tuple(&aom_masked_sad16x32_ssse3, &aom_masked_sad16x32_c),
  make_tuple(&aom_masked_sad16x16_ssse3, &aom_masked_sad16x16_c),
  make_tuple(&aom_masked_sad16x8_ssse3, &aom_masked_sad16x8_c),
  make_tuple(&aom_masked_sad8x16_ssse3, &aom_masked_sad8x16_c),
  make_tuple(&aom_masked_sad8x8_ssse3, &aom_masked_sad8x8_c),
  make_tuple(&aom_masked_sad8x4_ssse3, &aom_masked_sad8x4_c),
  make_tuple(&aom_masked_sad4x8_ssse3, &aom_masked_sad4x8_c),
  make_tuple(&aom_masked_sad4x4_ssse3, &aom_masked_sad4x4_c)
};

INSTANTIATE_TEST_CASE_P(SSSE3_C_COMPARE, MaskedSADTest,
                        ::testing::ValuesIn(msad_test));
const HighbdMaskedSADParam hbd_msad_test[] = {
  make_tuple(&aom_highbd_masked_sad128x128_ssse3,
             &aom_highbd_masked_sad128x128_c),
  make_tuple(&aom_highbd_masked_sad128x64_ssse3,
             &aom_highbd_masked_sad128x64_c),
  make_tuple(&aom_highbd_masked_sad64x128_ssse3,
             &aom_highbd_masked_sad64x128_c),
  make_tuple(&aom_highbd_masked_sad64x64_ssse3, &aom_highbd_masked_sad64x64_c),
  make_tuple(&aom_highbd_masked_sad64x32_ssse3, &aom_highbd_masked_sad64x32_c),
  make_tuple(&aom_highbd_masked_sad32x64_ssse3, &aom_highbd_masked_sad32x64_c),
  make_tuple(&aom_highbd_masked_sad32x32_ssse3, &aom_highbd_masked_sad32x32_c),
  make_tuple(&aom_highbd_masked_sad32x16_ssse3, &aom_highbd_masked_sad32x16_c),
  make_tuple(&aom_highbd_masked_sad16x32_ssse3, &aom_highbd_masked_sad16x32_c),
  make_tuple(&aom_highbd_masked_sad16x16_ssse3, &aom_highbd_masked_sad16x16_c),
  make_tuple(&aom_highbd_masked_sad16x8_ssse3, &aom_highbd_masked_sad16x8_c),
  make_tuple(&aom_highbd_masked_sad8x16_ssse3, &aom_highbd_masked_sad8x16_c),
  make_tuple(&aom_highbd_masked_sad8x8_ssse3, &aom_highbd_masked_sad8x8_c),
  make_tuple(&aom_highbd_masked_sad8x4_ssse3, &aom_highbd_masked_sad8x4_c),
  make_tuple(&aom_highbd_masked_sad4x8_ssse3, &aom_highbd_masked_sad4x8_c),
  make_tuple(&aom_highbd_masked_sad4x4_ssse3, &aom_highbd_masked_sad4x4_c)
};

INSTANTIATE_TEST_CASE_P(SSSE3_C_COMPARE, HighbdMaskedSADTest,
                        ::testing::ValuesIn(hbd_msad_test));
#endif  // HAVE_SSSE3
}  // namespace
