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

#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"
#include "aom/aom_codec.h"
#include "aom/aom_integer.h"
#include "aom_dsp/aom_filter.h"
#include "aom_mem/aom_mem.h"

using libaom_test::ACMRandom;

namespace {
const int number_of_iterations = 500;

typedef unsigned int (*MaskedVarianceFunc)(const uint8_t *a, int a_stride,
                                           const uint8_t *b, int b_stride,
                                           const uint8_t *m, int m_stride,
                                           unsigned int *sse);

typedef std::tr1::tuple<MaskedVarianceFunc, MaskedVarianceFunc>
    MaskedVarianceParam;

class MaskedVarianceTest
    : public ::testing::TestWithParam<MaskedVarianceParam> {
 public:
  virtual ~MaskedVarianceTest() {}
  virtual void SetUp() {
    opt_func_ = GET_PARAM(0);
    ref_func_ = GET_PARAM(1);
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  MaskedVarianceFunc opt_func_;
  MaskedVarianceFunc ref_func_;
};

TEST_P(MaskedVarianceTest, OperationCheck) {
  unsigned int ref_ret, opt_ret;
  unsigned int ref_sse, opt_sse;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint8_t, src_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);
  DECLARE_ALIGNED(16, uint8_t, ref_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);
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
      msk_ptr[j] = rnd(65);
    }

    ref_ret = ref_func_(src_ptr, src_stride, ref_ptr, ref_stride, msk_ptr,
                        msk_stride, &ref_sse);
    ASM_REGISTER_STATE_CHECK(opt_ret = opt_func_(src_ptr, src_stride, ref_ptr,
                                                 ref_stride, msk_ptr,
                                                 msk_stride, &opt_sse));

    if (opt_ret != ref_ret || opt_sse != ref_sse) {
      err_count++;
      if (first_failure == -1) first_failure = i;
    }
  }

  EXPECT_EQ(0, err_count) << "Error: Masked Variance Test OperationCheck,"
                          << "C output doesn't match SSSE3 output. "
                          << "First failed at test case " << first_failure;
}

TEST_P(MaskedVarianceTest, ExtremeValues) {
  unsigned int ref_ret, opt_ret;
  unsigned int ref_sse, opt_sse;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint8_t, src_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);
  DECLARE_ALIGNED(16, uint8_t, ref_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);
  DECLARE_ALIGNED(16, uint8_t, msk_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);
  int err_count = 0;
  int first_failure = -1;
  int src_stride = MAX_SB_SIZE;
  int ref_stride = MAX_SB_SIZE;
  int msk_stride = MAX_SB_SIZE;

  for (int i = 0; i < 8; ++i) {
    memset(src_ptr, (i & 0x1) ? 255 : 0, MAX_SB_SIZE * MAX_SB_SIZE);
    memset(ref_ptr, (i & 0x2) ? 255 : 0, MAX_SB_SIZE * MAX_SB_SIZE);
    memset(msk_ptr, (i & 0x4) ? 64 : 0, MAX_SB_SIZE * MAX_SB_SIZE);

    ref_ret = ref_func_(src_ptr, src_stride, ref_ptr, ref_stride, msk_ptr,
                        msk_stride, &ref_sse);
    ASM_REGISTER_STATE_CHECK(opt_ret = opt_func_(src_ptr, src_stride, ref_ptr,
                                                 ref_stride, msk_ptr,
                                                 msk_stride, &opt_sse));

    if (opt_ret != ref_ret || opt_sse != ref_sse) {
      err_count++;
      if (first_failure == -1) first_failure = i;
    }
  }

  EXPECT_EQ(0, err_count) << "Error: Masked Variance Test ExtremeValues,"
                          << "C output doesn't match SSSE3 output. "
                          << "First failed at test case " << first_failure;
}

typedef unsigned int (*MaskedSubPixelVarianceFunc)(
    const uint8_t *a, int a_stride, int xoffset, int yoffset, const uint8_t *b,
    int b_stride, const uint8_t *m, int m_stride, unsigned int *sse);

typedef std::tr1::tuple<MaskedSubPixelVarianceFunc, MaskedSubPixelVarianceFunc>
    MaskedSubPixelVarianceParam;

class MaskedSubPixelVarianceTest
    : public ::testing::TestWithParam<MaskedSubPixelVarianceParam> {
 public:
  virtual ~MaskedSubPixelVarianceTest() {}
  virtual void SetUp() {
    opt_func_ = GET_PARAM(0);
    ref_func_ = GET_PARAM(1);
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  MaskedSubPixelVarianceFunc opt_func_;
  MaskedSubPixelVarianceFunc ref_func_;
};

TEST_P(MaskedSubPixelVarianceTest, OperationCheck) {
  unsigned int ref_ret, opt_ret;
  unsigned int ref_sse, opt_sse;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint8_t, src_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 1)]);
  DECLARE_ALIGNED(16, uint8_t, ref_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 1)]);
  DECLARE_ALIGNED(16, uint8_t, msk_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 1)]);
  int err_count = 0;
  int first_failure = -1;
  int src_stride = (MAX_SB_SIZE + 1);
  int ref_stride = (MAX_SB_SIZE + 1);
  int msk_stride = (MAX_SB_SIZE + 1);
  int xoffset;
  int yoffset;

  for (int i = 0; i < number_of_iterations; ++i) {
    int xoffsets[] = { 0, 4, rnd(BIL_SUBPEL_SHIFTS) };
    int yoffsets[] = { 0, 4, rnd(BIL_SUBPEL_SHIFTS) };
    for (int j = 0; j < (MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 1); j++) {
      src_ptr[j] = rnd.Rand8();
      ref_ptr[j] = rnd.Rand8();
      msk_ptr[j] = rnd(65);
    }
    for (int k = 0; k < 3; k++) {
      xoffset = xoffsets[k];
      for (int l = 0; l < 3; l++) {
        xoffset = xoffsets[k];
        yoffset = yoffsets[l];

        ref_ret = ref_func_(src_ptr, src_stride, xoffset, yoffset, ref_ptr,
                            ref_stride, msk_ptr, msk_stride, &ref_sse);
        ASM_REGISTER_STATE_CHECK(
            opt_ret = opt_func_(src_ptr, src_stride, xoffset, yoffset, ref_ptr,
                                ref_stride, msk_ptr, msk_stride, &opt_sse));

        if (opt_ret != ref_ret || opt_sse != ref_sse) {
          err_count++;
          if (first_failure == -1) first_failure = i;
        }
      }
    }
  }

  EXPECT_EQ(0, err_count)
      << "Error: Masked Sub Pixel Variance Test OperationCheck,"
      << "C output doesn't match SSSE3 output. "
      << "First failed at test case " << first_failure;
}

TEST_P(MaskedSubPixelVarianceTest, ExtremeValues) {
  unsigned int ref_ret, opt_ret;
  unsigned int ref_sse, opt_sse;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint8_t, src_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 1)]);
  DECLARE_ALIGNED(16, uint8_t, ref_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 1)]);
  DECLARE_ALIGNED(16, uint8_t, msk_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 1)]);
  int first_failure_x = -1;
  int first_failure_y = -1;
  int err_count = 0;
  int first_failure = -1;
  int src_stride = (MAX_SB_SIZE + 1);
  int ref_stride = (MAX_SB_SIZE + 1);
  int msk_stride = (MAX_SB_SIZE + 1);

  for (int xoffset = 0; xoffset < BIL_SUBPEL_SHIFTS; xoffset++) {
    for (int yoffset = 0; yoffset < BIL_SUBPEL_SHIFTS; yoffset++) {
      for (int i = 0; i < 8; ++i) {
        memset(src_ptr, (i & 0x1) ? 255 : 0,
               (MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 1));
        memset(ref_ptr, (i & 0x2) ? 255 : 0,
               (MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 1));
        memset(msk_ptr, (i & 0x4) ? 64 : 0,
               (MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 1));

        ref_ret = ref_func_(src_ptr, src_stride, xoffset, yoffset, ref_ptr,
                            ref_stride, msk_ptr, msk_stride, &ref_sse);
        ASM_REGISTER_STATE_CHECK(
            opt_ret = opt_func_(src_ptr, src_stride, xoffset, yoffset, ref_ptr,
                                ref_stride, msk_ptr, msk_stride, &opt_sse));

        if (opt_ret != ref_ret || opt_sse != ref_sse) {
          err_count++;
          if (first_failure == -1) {
            first_failure = i;
            first_failure_x = xoffset;
            first_failure_y = yoffset;
          }
        }
      }
    }
  }

  EXPECT_EQ(0, err_count) << "Error: Masked Variance Test ExtremeValues,"
                          << "C output doesn't match SSSE3 output. "
                          << "First failed at test case " << first_failure
                          << " x_offset = " << first_failure_x
                          << " y_offset = " << first_failure_y;
}

#if CONFIG_HIGHBITDEPTH
typedef std::tr1::tuple<MaskedVarianceFunc, MaskedVarianceFunc, aom_bit_depth_t>
    HighbdMaskedVarianceParam;

class HighbdMaskedVarianceTest
    : public ::testing::TestWithParam<HighbdMaskedVarianceParam> {
 public:
  virtual ~HighbdMaskedVarianceTest() {}
  virtual void SetUp() {
    opt_func_ = GET_PARAM(0);
    ref_func_ = GET_PARAM(1);
    bit_depth_ = GET_PARAM(2);
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  MaskedVarianceFunc opt_func_;
  MaskedVarianceFunc ref_func_;
  aom_bit_depth_t bit_depth_;
};

TEST_P(HighbdMaskedVarianceTest, OperationCheck) {
  unsigned int ref_ret, opt_ret;
  unsigned int ref_sse, opt_sse;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint16_t, src_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);
  DECLARE_ALIGNED(16, uint16_t, ref_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);
  DECLARE_ALIGNED(16, uint8_t, msk_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);
  uint8_t *src8_ptr = CONVERT_TO_BYTEPTR(src_ptr);
  uint8_t *ref8_ptr = CONVERT_TO_BYTEPTR(ref_ptr);
  int err_count = 0;
  int first_failure = -1;
  int src_stride = MAX_SB_SIZE;
  int ref_stride = MAX_SB_SIZE;
  int msk_stride = MAX_SB_SIZE;

  for (int i = 0; i < number_of_iterations; ++i) {
    for (int j = 0; j < MAX_SB_SIZE * MAX_SB_SIZE; j++) {
      src_ptr[j] = rnd.Rand16() & ((1 << bit_depth_) - 1);
      ref_ptr[j] = rnd.Rand16() & ((1 << bit_depth_) - 1);
      msk_ptr[j] = rnd(65);
    }

    ref_ret = ref_func_(src8_ptr, src_stride, ref8_ptr, ref_stride, msk_ptr,
                        msk_stride, &ref_sse);
    ASM_REGISTER_STATE_CHECK(opt_ret = opt_func_(src8_ptr, src_stride, ref8_ptr,
                                                 ref_stride, msk_ptr,
                                                 msk_stride, &opt_sse));

    if (opt_ret != ref_ret || opt_sse != ref_sse) {
      err_count++;
      if (first_failure == -1) first_failure = i;
    }
  }

  EXPECT_EQ(0, err_count) << "Error: Masked Variance Test OperationCheck,"
                          << "C output doesn't match SSSE3 output. "
                          << "First failed at test case " << first_failure;
}

TEST_P(HighbdMaskedVarianceTest, ExtremeValues) {
  unsigned int ref_ret, opt_ret;
  unsigned int ref_sse, opt_sse;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint16_t, src_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);
  DECLARE_ALIGNED(16, uint16_t, ref_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);
  DECLARE_ALIGNED(16, uint8_t, msk_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);
  uint8_t *src8_ptr = CONVERT_TO_BYTEPTR(src_ptr);
  uint8_t *ref8_ptr = CONVERT_TO_BYTEPTR(ref_ptr);
  int err_count = 0;
  int first_failure = -1;
  int src_stride = MAX_SB_SIZE;
  int ref_stride = MAX_SB_SIZE;
  int msk_stride = MAX_SB_SIZE;

  for (int i = 0; i < 8; ++i) {
    aom_memset16(src_ptr, (i & 0x1) ? ((1 << bit_depth_) - 1) : 0,
                 MAX_SB_SIZE * MAX_SB_SIZE);
    aom_memset16(ref_ptr, (i & 0x2) ? ((1 << bit_depth_) - 1) : 0,
                 MAX_SB_SIZE * MAX_SB_SIZE);
    memset(msk_ptr, (i & 0x4) ? 64 : 0, MAX_SB_SIZE * MAX_SB_SIZE);

    ref_ret = ref_func_(src8_ptr, src_stride, ref8_ptr, ref_stride, msk_ptr,
                        msk_stride, &ref_sse);
    ASM_REGISTER_STATE_CHECK(opt_ret = opt_func_(src8_ptr, src_stride, ref8_ptr,
                                                 ref_stride, msk_ptr,
                                                 msk_stride, &opt_sse));

    if (opt_ret != ref_ret || opt_sse != ref_sse) {
      err_count++;
      if (first_failure == -1) first_failure = i;
    }
  }

  EXPECT_EQ(0, err_count) << "Error: Masked Variance Test ExtremeValues,"
                          << "C output doesn't match SSSE3 output. "
                          << "First failed at test case " << first_failure;
}

typedef std::tr1::tuple<MaskedSubPixelVarianceFunc, MaskedSubPixelVarianceFunc,
                        aom_bit_depth_t>
    HighbdMaskedSubPixelVarianceParam;

class HighbdMaskedSubPixelVarianceTest
    : public ::testing::TestWithParam<HighbdMaskedSubPixelVarianceParam> {
 public:
  virtual ~HighbdMaskedSubPixelVarianceTest() {}
  virtual void SetUp() {
    opt_func_ = GET_PARAM(0);
    ref_func_ = GET_PARAM(1);
    bit_depth_ = GET_PARAM(2);
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  MaskedSubPixelVarianceFunc opt_func_;
  MaskedSubPixelVarianceFunc ref_func_;
  aom_bit_depth_t bit_depth_;
};

TEST_P(HighbdMaskedSubPixelVarianceTest, OperationCheck) {
  unsigned int ref_ret, opt_ret;
  unsigned int ref_sse, opt_sse;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint16_t, src_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 1)]);
  DECLARE_ALIGNED(16, uint16_t, ref_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 1)]);
  DECLARE_ALIGNED(16, uint8_t, msk_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 1)]);
  uint8_t *src8_ptr = CONVERT_TO_BYTEPTR(src_ptr);
  uint8_t *ref8_ptr = CONVERT_TO_BYTEPTR(ref_ptr);
  int err_count = 0;
  int first_failure = -1;
  int first_failure_x = -1;
  int first_failure_y = -1;
  int src_stride = (MAX_SB_SIZE + 1);
  int ref_stride = (MAX_SB_SIZE + 1);
  int msk_stride = (MAX_SB_SIZE + 1);
  int xoffset, yoffset;

  for (int i = 0; i < number_of_iterations; ++i) {
    for (xoffset = 0; xoffset < BIL_SUBPEL_SHIFTS; xoffset++) {
      for (yoffset = 0; yoffset < BIL_SUBPEL_SHIFTS; yoffset++) {
        for (int j = 0; j < (MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 1); j++) {
          src_ptr[j] = rnd.Rand16() & ((1 << bit_depth_) - 1);
          ref_ptr[j] = rnd.Rand16() & ((1 << bit_depth_) - 1);
          msk_ptr[j] = rnd(65);
        }

        ref_ret = ref_func_(src8_ptr, src_stride, xoffset, yoffset, ref8_ptr,
                            ref_stride, msk_ptr, msk_stride, &ref_sse);
        ASM_REGISTER_STATE_CHECK(opt_ret =
                                     opt_func_(src8_ptr, src_stride, xoffset,
                                               yoffset, ref8_ptr, ref_stride,
                                               msk_ptr, msk_stride, &opt_sse));

        if (opt_ret != ref_ret || opt_sse != ref_sse) {
          err_count++;
          if (first_failure == -1) {
            first_failure = i;
            first_failure_x = xoffset;
            first_failure_y = yoffset;
          }
        }
      }
    }
  }

  EXPECT_EQ(0, err_count)
      << "Error: Masked Sub Pixel Variance Test OperationCheck,"
      << "C output doesn't match SSSE3 output. "
      << "First failed at test case " << first_failure
      << " x_offset = " << first_failure_x << " y_offset = " << first_failure_y;
}

TEST_P(HighbdMaskedSubPixelVarianceTest, ExtremeValues) {
  unsigned int ref_ret, opt_ret;
  unsigned int ref_sse, opt_sse;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint16_t, src_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 1)]);
  DECLARE_ALIGNED(16, uint16_t, ref_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 1)]);
  DECLARE_ALIGNED(16, uint8_t, msk_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 1)]);
  uint8_t *src8_ptr = CONVERT_TO_BYTEPTR(src_ptr);
  uint8_t *ref8_ptr = CONVERT_TO_BYTEPTR(ref_ptr);
  int first_failure_x = -1;
  int first_failure_y = -1;
  int err_count = 0;
  int first_failure = -1;
  int src_stride = (MAX_SB_SIZE + 1);
  int ref_stride = (MAX_SB_SIZE + 1);
  int msk_stride = (MAX_SB_SIZE + 1);

  for (int xoffset = 0; xoffset < BIL_SUBPEL_SHIFTS; xoffset++) {
    for (int yoffset = 0; yoffset < BIL_SUBPEL_SHIFTS; yoffset++) {
      for (int i = 0; i < 8; ++i) {
        aom_memset16(src_ptr, (i & 0x1) ? ((1 << bit_depth_) - 1) : 0,
                     (MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 1));
        aom_memset16(ref_ptr, (i & 0x2) ? ((1 << bit_depth_) - 1) : 0,
                     (MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 1));
        memset(msk_ptr, (i & 0x4) ? 64 : 0,
               (MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 1));

        ref_ret = ref_func_(src8_ptr, src_stride, xoffset, yoffset, ref8_ptr,
                            ref_stride, msk_ptr, msk_stride, &ref_sse);
        ASM_REGISTER_STATE_CHECK(opt_ret =
                                     opt_func_(src8_ptr, src_stride, xoffset,
                                               yoffset, ref8_ptr, ref_stride,
                                               msk_ptr, msk_stride, &opt_sse));

        if (opt_ret != ref_ret || opt_sse != ref_sse) {
          err_count++;
          if (first_failure == -1) {
            first_failure = i;
            first_failure_x = xoffset;
            first_failure_y = yoffset;
          }
        }
      }
    }
  }

  EXPECT_EQ(0, err_count) << "Error: Masked Variance Test ExtremeValues,"
                          << "C output doesn't match SSSE3 output. "
                          << "First failed at test case " << first_failure
                          << " x_offset = " << first_failure_x
                          << " y_offset = " << first_failure_y;
}
#endif  // CONFIG_HIGHBITDEPTH

using std::tr1::make_tuple;

#if HAVE_SSSE3
INSTANTIATE_TEST_CASE_P(
    SSSE3_C_COMPARE, MaskedVarianceTest,
    ::testing::Values(
#if CONFIG_EXT_PARTITION
        make_tuple(&aom_masked_variance128x128_ssse3,
                   &aom_masked_variance128x128_c),
        make_tuple(&aom_masked_variance128x64_ssse3,
                   &aom_masked_variance128x64_c),
        make_tuple(&aom_masked_variance64x128_ssse3,
                   &aom_masked_variance64x128_c),
#endif  // CONFIG_EXT_PARTITION
        make_tuple(&aom_masked_variance64x64_ssse3,
                   &aom_masked_variance64x64_c),
        make_tuple(&aom_masked_variance64x32_ssse3,
                   &aom_masked_variance64x32_c),
        make_tuple(&aom_masked_variance32x64_ssse3,
                   &aom_masked_variance32x64_c),
        make_tuple(&aom_masked_variance32x32_ssse3,
                   &aom_masked_variance32x32_c),
        make_tuple(&aom_masked_variance32x16_ssse3,
                   &aom_masked_variance32x16_c),
        make_tuple(&aom_masked_variance16x32_ssse3,
                   &aom_masked_variance16x32_c),
        make_tuple(&aom_masked_variance16x16_ssse3,
                   &aom_masked_variance16x16_c),
        make_tuple(&aom_masked_variance16x8_ssse3, &aom_masked_variance16x8_c),
        make_tuple(&aom_masked_variance8x16_ssse3, &aom_masked_variance8x16_c),
        make_tuple(&aom_masked_variance8x8_ssse3, &aom_masked_variance8x8_c),
        make_tuple(&aom_masked_variance8x4_ssse3, &aom_masked_variance8x4_c),
        make_tuple(&aom_masked_variance4x8_ssse3, &aom_masked_variance4x8_c),
        make_tuple(&aom_masked_variance4x4_ssse3, &aom_masked_variance4x4_c)));

INSTANTIATE_TEST_CASE_P(
    SSSE3_C_COMPARE, MaskedSubPixelVarianceTest,
    ::testing::Values(
#if CONFIG_EXT_PARTITION
        make_tuple(&aom_masked_sub_pixel_variance128x128_ssse3,
                   &aom_masked_sub_pixel_variance128x128_c),
        make_tuple(&aom_masked_sub_pixel_variance128x64_ssse3,
                   &aom_masked_sub_pixel_variance128x64_c),
        make_tuple(&aom_masked_sub_pixel_variance64x128_ssse3,
                   &aom_masked_sub_pixel_variance64x128_c),
#endif  // CONFIG_EXT_PARTITION
        make_tuple(&aom_masked_sub_pixel_variance64x64_ssse3,
                   &aom_masked_sub_pixel_variance64x64_c),
        make_tuple(&aom_masked_sub_pixel_variance64x32_ssse3,
                   &aom_masked_sub_pixel_variance64x32_c),
        make_tuple(&aom_masked_sub_pixel_variance32x64_ssse3,
                   &aom_masked_sub_pixel_variance32x64_c),
        make_tuple(&aom_masked_sub_pixel_variance32x32_ssse3,
                   &aom_masked_sub_pixel_variance32x32_c),
        make_tuple(&aom_masked_sub_pixel_variance32x16_ssse3,
                   &aom_masked_sub_pixel_variance32x16_c),
        make_tuple(&aom_masked_sub_pixel_variance16x32_ssse3,
                   &aom_masked_sub_pixel_variance16x32_c),
        make_tuple(&aom_masked_sub_pixel_variance16x16_ssse3,
                   &aom_masked_sub_pixel_variance16x16_c),
        make_tuple(&aom_masked_sub_pixel_variance16x8_ssse3,
                   &aom_masked_sub_pixel_variance16x8_c),
        make_tuple(&aom_masked_sub_pixel_variance8x16_ssse3,
                   &aom_masked_sub_pixel_variance8x16_c),
        make_tuple(&aom_masked_sub_pixel_variance8x8_ssse3,
                   &aom_masked_sub_pixel_variance8x8_c),
        make_tuple(&aom_masked_sub_pixel_variance8x4_ssse3,
                   &aom_masked_sub_pixel_variance8x4_c),
        make_tuple(&aom_masked_sub_pixel_variance4x8_ssse3,
                   &aom_masked_sub_pixel_variance4x8_c),
        make_tuple(&aom_masked_sub_pixel_variance4x4_ssse3,
                   &aom_masked_sub_pixel_variance4x4_c)));

#if CONFIG_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(
    SSSE3_C_COMPARE, HighbdMaskedVarianceTest,
    ::testing::Values(
#if CONFIG_EXT_PARTITION
        make_tuple(&aom_highbd_masked_variance128x128_ssse3,
                   &aom_highbd_masked_variance128x128_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_variance128x64_ssse3,
                   &aom_highbd_masked_variance128x64_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_variance64x128_ssse3,
                   &aom_highbd_masked_variance64x128_c, AOM_BITS_8),
#endif  // CONFIG_EXT_PARTITION
        make_tuple(&aom_highbd_masked_variance64x64_ssse3,
                   &aom_highbd_masked_variance64x64_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_variance64x32_ssse3,
                   &aom_highbd_masked_variance64x32_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_variance32x64_ssse3,
                   &aom_highbd_masked_variance32x64_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_variance32x32_ssse3,
                   &aom_highbd_masked_variance32x32_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_variance32x16_ssse3,
                   &aom_highbd_masked_variance32x16_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_variance16x32_ssse3,
                   &aom_highbd_masked_variance16x32_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_variance16x16_ssse3,
                   &aom_highbd_masked_variance16x16_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_variance16x8_ssse3,
                   &aom_highbd_masked_variance16x8_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_variance8x16_ssse3,
                   &aom_highbd_masked_variance8x16_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_variance8x8_ssse3,
                   &aom_highbd_masked_variance8x8_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_variance8x4_ssse3,
                   &aom_highbd_masked_variance8x4_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_variance4x8_ssse3,
                   &aom_highbd_masked_variance4x8_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_variance4x4_ssse3,
                   &aom_highbd_masked_variance4x4_c, AOM_BITS_8),
#if CONFIG_EXT_PARTITION
        make_tuple(&aom_highbd_10_masked_variance128x128_ssse3,
                   &aom_highbd_10_masked_variance128x128_c, AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_variance128x64_ssse3,
                   &aom_highbd_10_masked_variance128x64_c, AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_variance64x128_ssse3,
                   &aom_highbd_10_masked_variance64x128_c, AOM_BITS_10),
#endif  // CONFIG_EXT_PARTITION
        make_tuple(&aom_highbd_10_masked_variance64x64_ssse3,
                   &aom_highbd_10_masked_variance64x64_c, AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_variance64x32_ssse3,
                   &aom_highbd_10_masked_variance64x32_c, AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_variance32x64_ssse3,
                   &aom_highbd_10_masked_variance32x64_c, AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_variance32x32_ssse3,
                   &aom_highbd_10_masked_variance32x32_c, AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_variance32x16_ssse3,
                   &aom_highbd_10_masked_variance32x16_c, AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_variance16x32_ssse3,
                   &aom_highbd_10_masked_variance16x32_c, AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_variance16x16_ssse3,
                   &aom_highbd_10_masked_variance16x16_c, AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_variance16x8_ssse3,
                   &aom_highbd_10_masked_variance16x8_c, AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_variance8x16_ssse3,
                   &aom_highbd_10_masked_variance8x16_c, AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_variance8x8_ssse3,
                   &aom_highbd_10_masked_variance8x8_c, AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_variance8x4_ssse3,
                   &aom_highbd_10_masked_variance8x4_c, AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_variance4x8_ssse3,
                   &aom_highbd_10_masked_variance4x8_c, AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_variance4x4_ssse3,
                   &aom_highbd_10_masked_variance4x4_c, AOM_BITS_10),
#if CONFIG_EXT_PARTITION
        make_tuple(&aom_highbd_12_masked_variance128x128_ssse3,
                   &aom_highbd_12_masked_variance128x128_c, AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_variance128x64_ssse3,
                   &aom_highbd_12_masked_variance128x64_c, AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_variance64x128_ssse3,
                   &aom_highbd_12_masked_variance64x128_c, AOM_BITS_12),
#endif  // CONFIG_EXT_PARTITION
        make_tuple(&aom_highbd_12_masked_variance64x64_ssse3,
                   &aom_highbd_12_masked_variance64x64_c, AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_variance64x32_ssse3,
                   &aom_highbd_12_masked_variance64x32_c, AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_variance32x64_ssse3,
                   &aom_highbd_12_masked_variance32x64_c, AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_variance32x32_ssse3,
                   &aom_highbd_12_masked_variance32x32_c, AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_variance32x16_ssse3,
                   &aom_highbd_12_masked_variance32x16_c, AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_variance16x32_ssse3,
                   &aom_highbd_12_masked_variance16x32_c, AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_variance16x16_ssse3,
                   &aom_highbd_12_masked_variance16x16_c, AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_variance16x8_ssse3,
                   &aom_highbd_12_masked_variance16x8_c, AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_variance8x16_ssse3,
                   &aom_highbd_12_masked_variance8x16_c, AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_variance8x8_ssse3,
                   &aom_highbd_12_masked_variance8x8_c, AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_variance8x4_ssse3,
                   &aom_highbd_12_masked_variance8x4_c, AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_variance4x8_ssse3,
                   &aom_highbd_12_masked_variance4x8_c, AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_variance4x4_ssse3,
                   &aom_highbd_12_masked_variance4x4_c, AOM_BITS_12)));

INSTANTIATE_TEST_CASE_P(
    SSSE3_C_COMPARE, HighbdMaskedSubPixelVarianceTest,
    ::testing::Values(
#if CONFIG_EXT_PARTITION
        make_tuple(&aom_highbd_masked_sub_pixel_variance128x128_ssse3,
                   &aom_highbd_masked_sub_pixel_variance128x128_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_sub_pixel_variance128x64_ssse3,
                   &aom_highbd_masked_sub_pixel_variance128x64_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_sub_pixel_variance64x128_ssse3,
                   &aom_highbd_masked_sub_pixel_variance64x128_c, AOM_BITS_8),
#endif  // CONFIG_EXT_PARTITION
        make_tuple(&aom_highbd_masked_sub_pixel_variance64x64_ssse3,
                   &aom_highbd_masked_sub_pixel_variance64x64_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_sub_pixel_variance64x32_ssse3,
                   &aom_highbd_masked_sub_pixel_variance64x32_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_sub_pixel_variance32x64_ssse3,
                   &aom_highbd_masked_sub_pixel_variance32x64_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_sub_pixel_variance32x32_ssse3,
                   &aom_highbd_masked_sub_pixel_variance32x32_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_sub_pixel_variance32x16_ssse3,
                   &aom_highbd_masked_sub_pixel_variance32x16_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_sub_pixel_variance16x32_ssse3,
                   &aom_highbd_masked_sub_pixel_variance16x32_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_sub_pixel_variance16x16_ssse3,
                   &aom_highbd_masked_sub_pixel_variance16x16_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_sub_pixel_variance16x8_ssse3,
                   &aom_highbd_masked_sub_pixel_variance16x8_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_sub_pixel_variance8x16_ssse3,
                   &aom_highbd_masked_sub_pixel_variance8x16_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_sub_pixel_variance8x8_ssse3,
                   &aom_highbd_masked_sub_pixel_variance8x8_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_sub_pixel_variance8x4_ssse3,
                   &aom_highbd_masked_sub_pixel_variance8x4_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_sub_pixel_variance4x8_ssse3,
                   &aom_highbd_masked_sub_pixel_variance4x8_c, AOM_BITS_8),
        make_tuple(&aom_highbd_masked_sub_pixel_variance4x4_ssse3,
                   &aom_highbd_masked_sub_pixel_variance4x4_c, AOM_BITS_8),
#if CONFIG_EXT_PARTITION
        make_tuple(&aom_highbd_10_masked_sub_pixel_variance128x128_ssse3,
                   &aom_highbd_10_masked_sub_pixel_variance128x128_c,
                   AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_sub_pixel_variance128x64_ssse3,
                   &aom_highbd_10_masked_sub_pixel_variance128x64_c,
                   AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_sub_pixel_variance64x128_ssse3,
                   &aom_highbd_10_masked_sub_pixel_variance64x128_c,
                   AOM_BITS_10),
#endif  // CONFIG_EXT_PARTITION
        make_tuple(&aom_highbd_10_masked_sub_pixel_variance64x64_ssse3,
                   &aom_highbd_10_masked_sub_pixel_variance64x64_c,
                   AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_sub_pixel_variance64x32_ssse3,
                   &aom_highbd_10_masked_sub_pixel_variance64x32_c,
                   AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_sub_pixel_variance32x64_ssse3,
                   &aom_highbd_10_masked_sub_pixel_variance32x64_c,
                   AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_sub_pixel_variance32x32_ssse3,
                   &aom_highbd_10_masked_sub_pixel_variance32x32_c,
                   AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_sub_pixel_variance32x16_ssse3,
                   &aom_highbd_10_masked_sub_pixel_variance32x16_c,
                   AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_sub_pixel_variance16x32_ssse3,
                   &aom_highbd_10_masked_sub_pixel_variance16x32_c,
                   AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_sub_pixel_variance16x16_ssse3,
                   &aom_highbd_10_masked_sub_pixel_variance16x16_c,
                   AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_sub_pixel_variance16x8_ssse3,
                   &aom_highbd_10_masked_sub_pixel_variance16x8_c, AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_sub_pixel_variance8x16_ssse3,
                   &aom_highbd_10_masked_sub_pixel_variance8x16_c, AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_sub_pixel_variance8x8_ssse3,
                   &aom_highbd_10_masked_sub_pixel_variance8x8_c, AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_sub_pixel_variance8x4_ssse3,
                   &aom_highbd_10_masked_sub_pixel_variance8x4_c, AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_sub_pixel_variance4x8_ssse3,
                   &aom_highbd_10_masked_sub_pixel_variance4x8_c, AOM_BITS_10),
        make_tuple(&aom_highbd_10_masked_sub_pixel_variance4x4_ssse3,
                   &aom_highbd_10_masked_sub_pixel_variance4x4_c, AOM_BITS_10),
#if CONFIG_EXT_PARTITION
        make_tuple(&aom_highbd_12_masked_sub_pixel_variance128x128_ssse3,
                   &aom_highbd_12_masked_sub_pixel_variance128x128_c,
                   AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_sub_pixel_variance128x64_ssse3,
                   &aom_highbd_12_masked_sub_pixel_variance128x64_c,
                   AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_sub_pixel_variance64x128_ssse3,
                   &aom_highbd_12_masked_sub_pixel_variance64x128_c,
                   AOM_BITS_12),
#endif  // CONFIG_EXT_PARTITION
        make_tuple(&aom_highbd_12_masked_sub_pixel_variance64x64_ssse3,
                   &aom_highbd_12_masked_sub_pixel_variance64x64_c,
                   AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_sub_pixel_variance64x32_ssse3,
                   &aom_highbd_12_masked_sub_pixel_variance64x32_c,
                   AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_sub_pixel_variance32x64_ssse3,
                   &aom_highbd_12_masked_sub_pixel_variance32x64_c,
                   AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_sub_pixel_variance32x32_ssse3,
                   &aom_highbd_12_masked_sub_pixel_variance32x32_c,
                   AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_sub_pixel_variance32x16_ssse3,
                   &aom_highbd_12_masked_sub_pixel_variance32x16_c,
                   AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_sub_pixel_variance16x32_ssse3,
                   &aom_highbd_12_masked_sub_pixel_variance16x32_c,
                   AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_sub_pixel_variance16x16_ssse3,
                   &aom_highbd_12_masked_sub_pixel_variance16x16_c,
                   AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_sub_pixel_variance16x8_ssse3,
                   &aom_highbd_12_masked_sub_pixel_variance16x8_c, AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_sub_pixel_variance8x16_ssse3,
                   &aom_highbd_12_masked_sub_pixel_variance8x16_c, AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_sub_pixel_variance8x8_ssse3,
                   &aom_highbd_12_masked_sub_pixel_variance8x8_c, AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_sub_pixel_variance8x4_ssse3,
                   &aom_highbd_12_masked_sub_pixel_variance8x4_c, AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_sub_pixel_variance4x8_ssse3,
                   &aom_highbd_12_masked_sub_pixel_variance4x8_c, AOM_BITS_12),
        make_tuple(&aom_highbd_12_masked_sub_pixel_variance4x4_ssse3,
                   &aom_highbd_12_masked_sub_pixel_variance4x4_c,
                   AOM_BITS_12)));
#endif  // CONFIG_HIGHBITDEPTH

#endif  // HAVE_SSSE3
}  // namespace
