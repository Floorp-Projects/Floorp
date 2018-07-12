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

#include <cmath>
#include <cstdlib>
#include <string>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "av1/common/av1_loopfilter.h"
#include "av1/common/entropy.h"
#include "aom/aom_integer.h"

using libaom_test::ACMRandom;

namespace {
// Horizontally and Vertically need 32x32: 8  Coeffs preceeding filtered section
//                                         16 Coefs within filtered section
//                                         8  Coeffs following filtered section
const int kNumCoeffs = 1024;

const int number_of_iterations = 10000;

const int kSpeedTestNum = 500000;

#if CONFIG_HIGHBITDEPTH
typedef void (*loop_op_t)(uint16_t *s, int p, const uint8_t *blimit,
                          const uint8_t *limit, const uint8_t *thresh, int bd);
typedef void (*dual_loop_op_t)(uint16_t *s, int p, const uint8_t *blimit0,
                               const uint8_t *limit0, const uint8_t *thresh0,
                               const uint8_t *blimit1, const uint8_t *limit1,
                               const uint8_t *thresh1, int bd);
#else
typedef void (*loop_op_t)(uint8_t *s, int p, const uint8_t *blimit,
                          const uint8_t *limit, const uint8_t *thresh);
typedef void (*dual_loop_op_t)(uint8_t *s, int p, const uint8_t *blimit0,
                               const uint8_t *limit0, const uint8_t *thresh0,
                               const uint8_t *blimit1, const uint8_t *limit1,
                               const uint8_t *thresh1);
#endif  // CONFIG_HIGHBITDEPTH

typedef std::tr1::tuple<loop_op_t, loop_op_t, int> loop8_param_t;
typedef std::tr1::tuple<dual_loop_op_t, dual_loop_op_t, int> dualloop8_param_t;

class Loop8Test6Param : public ::testing::TestWithParam<loop8_param_t> {
 public:
  virtual ~Loop8Test6Param() {}
  virtual void SetUp() {
    loopfilter_op_ = GET_PARAM(0);
    ref_loopfilter_op_ = GET_PARAM(1);
    bit_depth_ = GET_PARAM(2);
    mask_ = (1 << bit_depth_) - 1;
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  int bit_depth_;
  int mask_;
  loop_op_t loopfilter_op_;
  loop_op_t ref_loopfilter_op_;
};

class Loop8Test9Param : public ::testing::TestWithParam<dualloop8_param_t> {
 public:
  virtual ~Loop8Test9Param() {}
  virtual void SetUp() {
    loopfilter_op_ = GET_PARAM(0);
    ref_loopfilter_op_ = GET_PARAM(1);
    bit_depth_ = GET_PARAM(2);
    mask_ = (1 << bit_depth_) - 1;
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  int bit_depth_;
  int mask_;
  dual_loop_op_t loopfilter_op_;
  dual_loop_op_t ref_loopfilter_op_;
};

TEST_P(Loop8Test6Param, OperationCheck) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  const int count_test_block = number_of_iterations;
#if CONFIG_HIGHBITDEPTH
  int32_t bd = bit_depth_;
  DECLARE_ALIGNED(16, uint16_t, s[kNumCoeffs]);
  DECLARE_ALIGNED(16, uint16_t, ref_s[kNumCoeffs]);
#else
  DECLARE_ALIGNED(8, uint8_t, s[kNumCoeffs]);
  DECLARE_ALIGNED(8, uint8_t, ref_s[kNumCoeffs]);
#endif  // CONFIG_HIGHBITDEPTH
  int err_count_total = 0;
  int first_failure = -1;
  for (int i = 0; i < count_test_block; ++i) {
    int err_count = 0;
    uint8_t tmp = static_cast<uint8_t>(rnd(3 * MAX_LOOP_FILTER + 4));
    DECLARE_ALIGNED(16, const uint8_t,
                    blimit[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                    tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = static_cast<uint8_t>(rnd(MAX_LOOP_FILTER));
    DECLARE_ALIGNED(16, const uint8_t,
                    limit[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                   tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = rnd.Rand8();
    DECLARE_ALIGNED(16, const uint8_t,
                    thresh[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                    tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    int32_t p = kNumCoeffs / 32;

    uint16_t tmp_s[kNumCoeffs];
    int j = 0;
    while (j < kNumCoeffs) {
      uint8_t val = rnd.Rand8();
      if (val & 0x80) {  // 50% chance to choose a new value.
        tmp_s[j] = rnd.Rand16();
        j++;
      } else {  // 50% chance to repeat previous value in row X times
        int k = 0;
        while (k++ < ((val & 0x1f) + 1) && j < kNumCoeffs) {
          if (j < 1) {
            tmp_s[j] = rnd.Rand16();
          } else if (val & 0x20) {  // Increment by an value within the limit
            tmp_s[j] = (tmp_s[j - 1] + (*limit - 1));
          } else {  // Decrement by an value within the limit
            tmp_s[j] = (tmp_s[j - 1] - (*limit - 1));
          }
          j++;
        }
      }
    }
    for (j = 0; j < kNumCoeffs; j++) {
      if (i % 2) {
        s[j] = tmp_s[j] & mask_;
      } else {
        s[j] = tmp_s[p * (j % p) + j / p] & mask_;
      }
      ref_s[j] = s[j];
    }
#if CONFIG_HIGHBITDEPTH
    ref_loopfilter_op_(ref_s + 8 + p * 8, p, blimit, limit, thresh, bd);
    ASM_REGISTER_STATE_CHECK(
        loopfilter_op_(s + 8 + p * 8, p, blimit, limit, thresh, bd));
#else
    ref_loopfilter_op_(ref_s + 8 + p * 8, p, blimit, limit, thresh);
    ASM_REGISTER_STATE_CHECK(
        loopfilter_op_(s + 8 + p * 8, p, blimit, limit, thresh));
#endif  // CONFIG_HIGHBITDEPTH

    for (j = 0; j < kNumCoeffs; ++j) {
      err_count += ref_s[j] != s[j];
    }
    if (err_count && !err_count_total) {
      first_failure = i;
    }
    err_count_total += err_count;
  }
  EXPECT_EQ(0, err_count_total)
      << "Error: Loop8Test6Param, C output doesn't match SSE2 "
         "loopfilter output. "
      << "First failed at test case " << first_failure;
}

TEST_P(Loop8Test6Param, ValueCheck) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  const int count_test_block = number_of_iterations;
#if CONFIG_HIGHBITDEPTH
  const int32_t bd = bit_depth_;
  DECLARE_ALIGNED(16, uint16_t, s[kNumCoeffs]);
  DECLARE_ALIGNED(16, uint16_t, ref_s[kNumCoeffs]);
#else
  DECLARE_ALIGNED(8, uint8_t, s[kNumCoeffs]);
  DECLARE_ALIGNED(8, uint8_t, ref_s[kNumCoeffs]);
#endif  // CONFIG_HIGHBITDEPTH
  int err_count_total = 0;
  int first_failure = -1;

  // NOTE: The code in av1_loopfilter.c:update_sharpness computes mblim as a
  // function of sharpness_lvl and the loopfilter lvl as:
  // block_inside_limit = lvl >> ((sharpness_lvl > 0) + (sharpness_lvl > 4));
  // ...
  // memset(lfi->lfthr[lvl].mblim, (2 * (lvl + 2) + block_inside_limit),
  //        SIMD_WIDTH);
  // This means that the largest value for mblim will occur when sharpness_lvl
  // is equal to 0, and lvl is equal to its greatest value (MAX_LOOP_FILTER).
  // In this case block_inside_limit will be equal to MAX_LOOP_FILTER and
  // therefore mblim will be equal to (2 * (lvl + 2) + block_inside_limit) =
  // 2 * (MAX_LOOP_FILTER + 2) + MAX_LOOP_FILTER = 3 * MAX_LOOP_FILTER + 4

  for (int i = 0; i < count_test_block; ++i) {
    int err_count = 0;
    uint8_t tmp = static_cast<uint8_t>(rnd(3 * MAX_LOOP_FILTER + 4));
    DECLARE_ALIGNED(16, const uint8_t,
                    blimit[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                    tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = static_cast<uint8_t>(rnd(MAX_LOOP_FILTER));
    DECLARE_ALIGNED(16, const uint8_t,
                    limit[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                   tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = rnd.Rand8();
    DECLARE_ALIGNED(16, const uint8_t,
                    thresh[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                    tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    int32_t p = kNumCoeffs / 32;
    for (int j = 0; j < kNumCoeffs; ++j) {
      s[j] = rnd.Rand16() & mask_;
      ref_s[j] = s[j];
    }
#if CONFIG_HIGHBITDEPTH
    ref_loopfilter_op_(ref_s + 8 + p * 8, p, blimit, limit, thresh, bd);
    ASM_REGISTER_STATE_CHECK(
        loopfilter_op_(s + 8 + p * 8, p, blimit, limit, thresh, bd));
#else
    ref_loopfilter_op_(ref_s + 8 + p * 8, p, blimit, limit, thresh);
    ASM_REGISTER_STATE_CHECK(
        loopfilter_op_(s + 8 + p * 8, p, blimit, limit, thresh));
#endif  // CONFIG_HIGHBITDEPTH
    for (int j = 0; j < kNumCoeffs; ++j) {
      err_count += ref_s[j] != s[j];
    }
    if (err_count && !err_count_total) {
      first_failure = i;
    }
    err_count_total += err_count;
  }
  EXPECT_EQ(0, err_count_total)
      << "Error: Loop8Test6Param, C output doesn't match SSE2 "
         "loopfilter output. "
      << "First failed at test case " << first_failure;
}

TEST_P(Loop8Test6Param, DISABLED_Speed) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  const int count_test_block = kSpeedTestNum;
#if CONFIG_HIGHBITDEPTH
  const int32_t bd = bit_depth_;
  DECLARE_ALIGNED(16, uint16_t, s[kNumCoeffs]);
#else
  DECLARE_ALIGNED(8, uint8_t, s[kNumCoeffs]);
#endif  // CONFIG_HIGHBITDEPTH

  uint8_t tmp = static_cast<uint8_t>(rnd(3 * MAX_LOOP_FILTER + 4));
  DECLARE_ALIGNED(16, const uint8_t,
                  blimit[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                  tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
  tmp = static_cast<uint8_t>(rnd(MAX_LOOP_FILTER));
  DECLARE_ALIGNED(16, const uint8_t,
                  limit[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                 tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
  tmp = rnd.Rand8();
  DECLARE_ALIGNED(16, const uint8_t,
                  thresh[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                  tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };

  int32_t p = kNumCoeffs / 32;
  for (int j = 0; j < kNumCoeffs; ++j) {
    s[j] = rnd.Rand16() & mask_;
  }

  for (int i = 0; i < count_test_block; ++i) {
#if CONFIG_HIGHBITDEPTH
    loopfilter_op_(s + 8 + p * 8, p, blimit, limit, thresh, bd);
#else
    loopfilter_op_(s + 8 + p * 8, p, blimit, limit, thresh);
#endif  // CONFIG_HIGHBITDEPTH
  }
}

TEST_P(Loop8Test9Param, OperationCheck) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  const int count_test_block = number_of_iterations;
#if CONFIG_HIGHBITDEPTH
  const int32_t bd = bit_depth_;
  DECLARE_ALIGNED(16, uint16_t, s[kNumCoeffs]);
  DECLARE_ALIGNED(16, uint16_t, ref_s[kNumCoeffs]);
#else
  DECLARE_ALIGNED(8, uint8_t, s[kNumCoeffs]);
  DECLARE_ALIGNED(8, uint8_t, ref_s[kNumCoeffs]);
#endif  // CONFIG_HIGHBITDEPTH
  int err_count_total = 0;
  int first_failure = -1;
  for (int i = 0; i < count_test_block; ++i) {
    int err_count = 0;
    uint8_t tmp = static_cast<uint8_t>(rnd(3 * MAX_LOOP_FILTER + 4));
    DECLARE_ALIGNED(16, const uint8_t,
                    blimit0[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                     tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = static_cast<uint8_t>(rnd(MAX_LOOP_FILTER));
    DECLARE_ALIGNED(16, const uint8_t,
                    limit0[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                    tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = rnd.Rand8();
    DECLARE_ALIGNED(16, const uint8_t,
                    thresh0[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                     tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = static_cast<uint8_t>(rnd(3 * MAX_LOOP_FILTER + 4));
    DECLARE_ALIGNED(16, const uint8_t,
                    blimit1[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                     tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = static_cast<uint8_t>(rnd(MAX_LOOP_FILTER));
    DECLARE_ALIGNED(16, const uint8_t,
                    limit1[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                    tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = rnd.Rand8();
    DECLARE_ALIGNED(16, const uint8_t,
                    thresh1[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                     tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    int32_t p = kNumCoeffs / 32;
    uint16_t tmp_s[kNumCoeffs];
    int j = 0;
    const uint8_t limit = *limit0 < *limit1 ? *limit0 : *limit1;
    while (j < kNumCoeffs) {
      uint8_t val = rnd.Rand8();
      if (val & 0x80) {  // 50% chance to choose a new value.
        tmp_s[j] = rnd.Rand16();
        j++;
      } else {  // 50% chance to repeat previous value in row X times.
        int k = 0;
        while (k++ < ((val & 0x1f) + 1) && j < kNumCoeffs) {
          if (j < 1) {
            tmp_s[j] = rnd.Rand16();
          } else if (val & 0x20) {  // Increment by a value within the limit.
            tmp_s[j] = (tmp_s[j - 1] + (limit - 1));
          } else {  // Decrement by an value within the limit.
            tmp_s[j] = (tmp_s[j - 1] - (limit - 1));
          }
          j++;
        }
      }
    }
    for (j = 0; j < kNumCoeffs; j++) {
      if (i % 2) {
        s[j] = tmp_s[j] & mask_;
      } else {
        s[j] = tmp_s[p * (j % p) + j / p] & mask_;
      }
      ref_s[j] = s[j];
    }
#if CONFIG_HIGHBITDEPTH
    ref_loopfilter_op_(ref_s + 8 + p * 8, p, blimit0, limit0, thresh0, blimit1,
                       limit1, thresh1, bd);
    ASM_REGISTER_STATE_CHECK(loopfilter_op_(s + 8 + p * 8, p, blimit0, limit0,
                                            thresh0, blimit1, limit1, thresh1,
                                            bd));
#else
    ref_loopfilter_op_(ref_s + 8 + p * 8, p, blimit0, limit0, thresh0, blimit1,
                       limit1, thresh1);
    ASM_REGISTER_STATE_CHECK(loopfilter_op_(s + 8 + p * 8, p, blimit0, limit0,
                                            thresh0, blimit1, limit1, thresh1));
#endif  // CONFIG_HIGHBITDEPTH
    for (j = 0; j < kNumCoeffs; ++j) {
      err_count += ref_s[j] != s[j];
    }
    if (err_count && !err_count_total) {
      first_failure = i;
    }
    err_count_total += err_count;
  }
  EXPECT_EQ(0, err_count_total)
      << "Error: Loop8Test9Param, C output doesn't match SSE2 "
         "loopfilter output. "
      << "First failed at test case " << first_failure;
}

TEST_P(Loop8Test9Param, ValueCheck) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  const int count_test_block = number_of_iterations;
#if CONFIG_HIGHBITDEPTH
  DECLARE_ALIGNED(16, uint16_t, s[kNumCoeffs]);
  DECLARE_ALIGNED(16, uint16_t, ref_s[kNumCoeffs]);
#else
  DECLARE_ALIGNED(8, uint8_t, s[kNumCoeffs]);
  DECLARE_ALIGNED(8, uint8_t, ref_s[kNumCoeffs]);
#endif  // CONFIG_HIGHBITDEPTH
  int err_count_total = 0;
  int first_failure = -1;
  for (int i = 0; i < count_test_block; ++i) {
    int err_count = 0;
    uint8_t tmp = static_cast<uint8_t>(rnd(3 * MAX_LOOP_FILTER + 4));
    DECLARE_ALIGNED(16, const uint8_t,
                    blimit0[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                     tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = static_cast<uint8_t>(rnd(MAX_LOOP_FILTER));
    DECLARE_ALIGNED(16, const uint8_t,
                    limit0[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                    tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = rnd.Rand8();
    DECLARE_ALIGNED(16, const uint8_t,
                    thresh0[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                     tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = static_cast<uint8_t>(rnd(3 * MAX_LOOP_FILTER + 4));
    DECLARE_ALIGNED(16, const uint8_t,
                    blimit1[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                     tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = static_cast<uint8_t>(rnd(MAX_LOOP_FILTER));
    DECLARE_ALIGNED(16, const uint8_t,
                    limit1[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                    tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = rnd.Rand8();
    DECLARE_ALIGNED(16, const uint8_t,
                    thresh1[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                     tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    int32_t p = kNumCoeffs / 32;  // TODO(pdlf) can we have non-square here?
    for (int j = 0; j < kNumCoeffs; ++j) {
      s[j] = rnd.Rand16() & mask_;
      ref_s[j] = s[j];
    }
#if CONFIG_HIGHBITDEPTH
    const int32_t bd = bit_depth_;
    ref_loopfilter_op_(ref_s + 8 + p * 8, p, blimit0, limit0, thresh0, blimit1,
                       limit1, thresh1, bd);
    ASM_REGISTER_STATE_CHECK(loopfilter_op_(s + 8 + p * 8, p, blimit0, limit0,
                                            thresh0, blimit1, limit1, thresh1,
                                            bd));
#else
    ref_loopfilter_op_(ref_s + 8 + p * 8, p, blimit0, limit0, thresh0, blimit1,
                       limit1, thresh1);
    ASM_REGISTER_STATE_CHECK(loopfilter_op_(s + 8 + p * 8, p, blimit0, limit0,
                                            thresh0, blimit1, limit1, thresh1));
#endif  // CONFIG_HIGHBITDEPTH
    for (int j = 0; j < kNumCoeffs; ++j) {
      err_count += ref_s[j] != s[j];
    }
    if (err_count && !err_count_total) {
      first_failure = i;
    }
    err_count_total += err_count;
  }
  EXPECT_EQ(0, err_count_total)
      << "Error: Loop8Test9Param, C output doesn't match SSE2"
         "loopfilter output. "
      << "First failed at test case " << first_failure;
}

TEST_P(Loop8Test9Param, DISABLED_Speed) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  const int count_test_block = kSpeedTestNum;
#if CONFIG_HIGHBITDEPTH
  DECLARE_ALIGNED(16, uint16_t, s[kNumCoeffs]);
#else
  DECLARE_ALIGNED(8, uint8_t, s[kNumCoeffs]);
#endif  // CONFIG_HIGHBITDEPTH

  uint8_t tmp = static_cast<uint8_t>(rnd(3 * MAX_LOOP_FILTER + 4));
  DECLARE_ALIGNED(16, const uint8_t,
                  blimit0[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                   tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
  tmp = static_cast<uint8_t>(rnd(MAX_LOOP_FILTER));
  DECLARE_ALIGNED(16, const uint8_t,
                  limit0[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                  tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
  tmp = rnd.Rand8();
  DECLARE_ALIGNED(16, const uint8_t,
                  thresh0[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                   tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
  tmp = static_cast<uint8_t>(rnd(3 * MAX_LOOP_FILTER + 4));
  DECLARE_ALIGNED(16, const uint8_t,
                  blimit1[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                   tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
  tmp = static_cast<uint8_t>(rnd(MAX_LOOP_FILTER));
  DECLARE_ALIGNED(16, const uint8_t,
                  limit1[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                  tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
  tmp = rnd.Rand8();
  DECLARE_ALIGNED(16, const uint8_t,
                  thresh1[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                   tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
  int32_t p = kNumCoeffs / 32;  // TODO(pdlf) can we have non-square here?
  for (int j = 0; j < kNumCoeffs; ++j) {
    s[j] = rnd.Rand16() & mask_;
  }

  for (int i = 0; i < count_test_block; ++i) {
#if CONFIG_HIGHBITDEPTH
    const int32_t bd = bit_depth_;
    loopfilter_op_(s + 8 + p * 8, p, blimit0, limit0, thresh0, blimit1, limit1,
                   thresh1, bd);
#else
    loopfilter_op_(s + 8 + p * 8, p, blimit0, limit0, thresh0, blimit1, limit1,
                   thresh1);
#endif  // CONFIG_HIGHBITDEPTH
  }
}

using std::tr1::make_tuple;

#if HAVE_SSE2
#if CONFIG_HIGHBITDEPTH

const loop8_param_t kHbdLoop8Test6[] = {
  make_tuple(&aom_highbd_lpf_horizontal_4_sse2, &aom_highbd_lpf_horizontal_4_c,
             8),
  make_tuple(&aom_highbd_lpf_vertical_4_sse2, &aom_highbd_lpf_vertical_4_c, 8),
  make_tuple(&aom_highbd_lpf_horizontal_8_sse2, &aom_highbd_lpf_horizontal_8_c,
             8),
  make_tuple(&aom_highbd_lpf_horizontal_edge_8_sse2,
             &aom_highbd_lpf_horizontal_edge_8_c, 8),
  make_tuple(&aom_highbd_lpf_horizontal_edge_16_sse2,
             &aom_highbd_lpf_horizontal_edge_16_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_8_sse2, &aom_highbd_lpf_vertical_8_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_16_sse2, &aom_highbd_lpf_vertical_16_c,
             8),
  make_tuple(&aom_highbd_lpf_horizontal_4_sse2, &aom_highbd_lpf_horizontal_4_c,
             10),
  make_tuple(&aom_highbd_lpf_vertical_4_sse2, &aom_highbd_lpf_vertical_4_c, 10),
  make_tuple(&aom_highbd_lpf_horizontal_8_sse2, &aom_highbd_lpf_horizontal_8_c,
             10),
  make_tuple(&aom_highbd_lpf_horizontal_edge_8_sse2,
             &aom_highbd_lpf_horizontal_edge_8_c, 10),
  make_tuple(&aom_highbd_lpf_horizontal_edge_16_sse2,
             &aom_highbd_lpf_horizontal_edge_16_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_8_sse2, &aom_highbd_lpf_vertical_8_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_16_sse2, &aom_highbd_lpf_vertical_16_c,
             10),
  make_tuple(&aom_highbd_lpf_horizontal_4_sse2, &aom_highbd_lpf_horizontal_4_c,
             12),
  make_tuple(&aom_highbd_lpf_vertical_4_sse2, &aom_highbd_lpf_vertical_4_c, 12),
  make_tuple(&aom_highbd_lpf_horizontal_8_sse2, &aom_highbd_lpf_horizontal_8_c,
             12),
  make_tuple(&aom_highbd_lpf_horizontal_edge_8_sse2,
             &aom_highbd_lpf_horizontal_edge_8_c, 12),
  make_tuple(&aom_highbd_lpf_horizontal_edge_16_sse2,
             &aom_highbd_lpf_horizontal_edge_16_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_8_sse2, &aom_highbd_lpf_vertical_8_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_16_sse2, &aom_highbd_lpf_vertical_16_c,
             12),
  make_tuple(&aom_highbd_lpf_vertical_16_dual_sse2,
             &aom_highbd_lpf_vertical_16_dual_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_16_dual_sse2,
             &aom_highbd_lpf_vertical_16_dual_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_16_dual_sse2,
             &aom_highbd_lpf_vertical_16_dual_c, 12)
};

INSTANTIATE_TEST_CASE_P(SSE2, Loop8Test6Param,
                        ::testing::ValuesIn(kHbdLoop8Test6));
#else
const loop8_param_t kLoop8Test6[] = {
  make_tuple(&aom_lpf_horizontal_4_sse2, &aom_lpf_horizontal_4_c, 8),
  make_tuple(&aom_lpf_horizontal_8_sse2, &aom_lpf_horizontal_8_c, 8),
  make_tuple(&aom_lpf_horizontal_edge_8_sse2, &aom_lpf_horizontal_edge_8_c, 8),
  make_tuple(&aom_lpf_horizontal_edge_16_sse2, &aom_lpf_horizontal_edge_16_c,
             8),
  make_tuple(&aom_lpf_vertical_4_sse2, &aom_lpf_vertical_4_c, 8),
  make_tuple(&aom_lpf_vertical_8_sse2, &aom_lpf_vertical_8_c, 8),
  make_tuple(&aom_lpf_vertical_16_sse2, &aom_lpf_vertical_16_c, 8),
#if !CONFIG_PARALLEL_DEBLOCKING
  make_tuple(&aom_lpf_vertical_16_dual_sse2, &aom_lpf_vertical_16_dual_c, 8)
#endif
};

INSTANTIATE_TEST_CASE_P(SSE2, Loop8Test6Param,
                        ::testing::ValuesIn(kLoop8Test6));
#endif  // CONFIG_HIGHBITDEPTH
#endif  // HAVE_SSE2

#if HAVE_AVX2
#if CONFIG_HIGHBITDEPTH

const loop8_param_t kHbdLoop8Test6Avx2[] = {
  make_tuple(&aom_highbd_lpf_horizontal_edge_16_avx2,
             &aom_highbd_lpf_horizontal_edge_16_c, 8),
  make_tuple(&aom_highbd_lpf_horizontal_edge_16_avx2,
             &aom_highbd_lpf_horizontal_edge_16_c, 10),
  make_tuple(&aom_highbd_lpf_horizontal_edge_16_avx2,
             &aom_highbd_lpf_horizontal_edge_16_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_16_dual_avx2,
             &aom_highbd_lpf_vertical_16_dual_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_16_dual_avx2,
             &aom_highbd_lpf_vertical_16_dual_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_16_dual_avx2,
             &aom_highbd_lpf_vertical_16_dual_c, 12)
};

INSTANTIATE_TEST_CASE_P(AVX2, Loop8Test6Param,
                        ::testing::ValuesIn(kHbdLoop8Test6Avx2));

#endif
#endif

#if HAVE_AVX2 && (!CONFIG_HIGHBITDEPTH) && (!CONFIG_PARALLEL_DEBLOCKING)
INSTANTIATE_TEST_CASE_P(
    AVX2, Loop8Test6Param,
    ::testing::Values(make_tuple(&aom_lpf_horizontal_edge_8_avx2,
                                 &aom_lpf_horizontal_edge_8_c, 8),
                      make_tuple(&aom_lpf_horizontal_edge_16_avx2,
                                 &aom_lpf_horizontal_edge_16_c, 8)));
#endif

#if HAVE_SSE2
#if CONFIG_HIGHBITDEPTH
const dualloop8_param_t kHbdLoop8Test9[] = {
  make_tuple(&aom_highbd_lpf_horizontal_4_dual_sse2,
             &aom_highbd_lpf_horizontal_4_dual_c, 8),
  make_tuple(&aom_highbd_lpf_horizontal_8_dual_sse2,
             &aom_highbd_lpf_horizontal_8_dual_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_4_dual_sse2,
             &aom_highbd_lpf_vertical_4_dual_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_8_dual_sse2,
             &aom_highbd_lpf_vertical_8_dual_c, 8),
  make_tuple(&aom_highbd_lpf_horizontal_4_dual_sse2,
             &aom_highbd_lpf_horizontal_4_dual_c, 10),
  make_tuple(&aom_highbd_lpf_horizontal_8_dual_sse2,
             &aom_highbd_lpf_horizontal_8_dual_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_4_dual_sse2,
             &aom_highbd_lpf_vertical_4_dual_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_8_dual_sse2,
             &aom_highbd_lpf_vertical_8_dual_c, 10),
  make_tuple(&aom_highbd_lpf_horizontal_4_dual_sse2,
             &aom_highbd_lpf_horizontal_4_dual_c, 12),
  make_tuple(&aom_highbd_lpf_horizontal_8_dual_sse2,
             &aom_highbd_lpf_horizontal_8_dual_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_4_dual_sse2,
             &aom_highbd_lpf_vertical_4_dual_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_8_dual_sse2,
             &aom_highbd_lpf_vertical_8_dual_c, 12)
};

INSTANTIATE_TEST_CASE_P(SSE2, Loop8Test9Param,
                        ::testing::ValuesIn(kHbdLoop8Test9));
#else
#if !CONFIG_PARALLEL_DEBLOCKING
const dualloop8_param_t kLoop8Test9[] = {
  make_tuple(&aom_lpf_horizontal_4_dual_sse2, &aom_lpf_horizontal_4_dual_c, 8),
  make_tuple(&aom_lpf_horizontal_8_dual_sse2, &aom_lpf_horizontal_8_dual_c, 8),
  make_tuple(&aom_lpf_vertical_4_dual_sse2, &aom_lpf_vertical_4_dual_c, 8),
  make_tuple(&aom_lpf_vertical_8_dual_sse2, &aom_lpf_vertical_8_dual_c, 8)
};

INSTANTIATE_TEST_CASE_P(SSE2, Loop8Test9Param,
                        ::testing::ValuesIn(kLoop8Test9));
#endif
#endif  // CONFIG_HIGHBITDEPTH
#endif  // HAVE_SSE2

#if HAVE_AVX2
#if CONFIG_HIGHBITDEPTH
const dualloop8_param_t kHbdLoop8Test9Avx2[] = {
  make_tuple(&aom_highbd_lpf_horizontal_4_dual_avx2,
             &aom_highbd_lpf_horizontal_4_dual_c, 8),
  make_tuple(&aom_highbd_lpf_horizontal_4_dual_avx2,
             &aom_highbd_lpf_horizontal_4_dual_c, 10),
  make_tuple(&aom_highbd_lpf_horizontal_4_dual_avx2,
             &aom_highbd_lpf_horizontal_4_dual_c, 12),
  make_tuple(&aom_highbd_lpf_horizontal_8_dual_avx2,
             &aom_highbd_lpf_horizontal_8_dual_c, 8),
  make_tuple(&aom_highbd_lpf_horizontal_8_dual_avx2,
             &aom_highbd_lpf_horizontal_8_dual_c, 10),
  make_tuple(&aom_highbd_lpf_horizontal_8_dual_avx2,
             &aom_highbd_lpf_horizontal_8_dual_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_4_dual_avx2,
             &aom_highbd_lpf_vertical_4_dual_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_4_dual_avx2,
             &aom_highbd_lpf_vertical_4_dual_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_4_dual_avx2,
             &aom_highbd_lpf_vertical_4_dual_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_8_dual_avx2,
             &aom_highbd_lpf_vertical_8_dual_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_8_dual_avx2,
             &aom_highbd_lpf_vertical_8_dual_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_8_dual_avx2,
             &aom_highbd_lpf_vertical_8_dual_c, 12),
};

INSTANTIATE_TEST_CASE_P(AVX2, Loop8Test9Param,
                        ::testing::ValuesIn(kHbdLoop8Test9Avx2));
#endif
#endif

#if HAVE_NEON && (!CONFIG_PARALLEL_DEBLOCKING)
#if CONFIG_HIGHBITDEPTH
// No neon high bitdepth functions.
#else
INSTANTIATE_TEST_CASE_P(
    NEON, Loop8Test6Param,
    ::testing::Values(
#if HAVE_NEON_ASM
        // Using #if inside the macro is unsupported on MSVS but the tests are
        // not
        // currently built for MSVS with ARM and NEON.
        make_tuple(&aom_lpf_horizontal_edge_8_neon,
                   &aom_lpf_horizontal_edge_8_c, 8),
        make_tuple(&aom_lpf_horizontal_edge_16_neon,
                   &aom_lpf_horizontal_edge_16_c, 8),
        make_tuple(&aom_lpf_vertical_16_neon, &aom_lpf_vertical_16_c, 8),
        make_tuple(&aom_lpf_vertical_16_dual_neon, &aom_lpf_vertical_16_dual_c,
                   8),
#endif  // HAVE_NEON_ASM
        make_tuple(&aom_lpf_horizontal_8_neon, &aom_lpf_horizontal_8_c, 8),
        make_tuple(&aom_lpf_vertical_8_neon, &aom_lpf_vertical_8_c, 8),
        make_tuple(&aom_lpf_horizontal_4_neon, &aom_lpf_horizontal_4_c, 8),
        make_tuple(&aom_lpf_vertical_4_neon, &aom_lpf_vertical_4_c, 8)));
INSTANTIATE_TEST_CASE_P(NEON, Loop8Test9Param,
                        ::testing::Values(
#if HAVE_NEON_ASM
                            make_tuple(&aom_lpf_horizontal_8_dual_neon,
                                       &aom_lpf_horizontal_8_dual_c, 8),
                            make_tuple(&aom_lpf_vertical_8_dual_neon,
                                       &aom_lpf_vertical_8_dual_c, 8),
#endif  // HAVE_NEON_ASM
                            make_tuple(&aom_lpf_horizontal_4_dual_neon,
                                       &aom_lpf_horizontal_4_dual_c, 8),
                            make_tuple(&aom_lpf_vertical_4_dual_neon,
                                       &aom_lpf_vertical_4_dual_c, 8)));
#endif  // CONFIG_HIGHBITDEPTH
#endif  // HAVE_NEON && (!CONFIG_PARALLEL_DEBLOCKING)

#if HAVE_DSPR2 && !CONFIG_HIGHBITDEPTH && (!CONFIG_PARALLEL_DEBLOCKING)
INSTANTIATE_TEST_CASE_P(
    DSPR2, Loop8Test6Param,
    ::testing::Values(
        make_tuple(&aom_lpf_horizontal_4_dspr2, &aom_lpf_horizontal_4_c, 8),
        make_tuple(&aom_lpf_horizontal_8_dspr2, &aom_lpf_horizontal_8_c, 8),
        make_tuple(&aom_lpf_horizontal_edge_8, &aom_lpf_horizontal_edge_8, 8),
        make_tuple(&aom_lpf_horizontal_edge_16, &aom_lpf_horizontal_edge_16, 8),
        make_tuple(&aom_lpf_vertical_4_dspr2, &aom_lpf_vertical_4_c, 8),
        make_tuple(&aom_lpf_vertical_8_dspr2, &aom_lpf_vertical_8_c, 8),
        make_tuple(&aom_lpf_vertical_16_dspr2, &aom_lpf_vertical_16_c, 8),
        make_tuple(&aom_lpf_vertical_16_dual_dspr2, &aom_lpf_vertical_16_dual_c,
                   8)));

INSTANTIATE_TEST_CASE_P(
    DSPR2, Loop8Test9Param,
    ::testing::Values(make_tuple(&aom_lpf_horizontal_4_dual_dspr2,
                                 &aom_lpf_horizontal_4_dual_c, 8),
                      make_tuple(&aom_lpf_horizontal_8_dual_dspr2,
                                 &aom_lpf_horizontal_8_dual_c, 8),
                      make_tuple(&aom_lpf_vertical_4_dual_dspr2,
                                 &aom_lpf_vertical_4_dual_c, 8),
                      make_tuple(&aom_lpf_vertical_8_dual_dspr2,
                                 &aom_lpf_vertical_8_dual_c, 8)));
#endif  // HAVE_DSPR2 && !CONFIG_HIGHBITDEPTH && (!CONFIG_PARALLEL_DEBLOCKING)

#if HAVE_MSA && (!CONFIG_HIGHBITDEPTH) && (!CONFIG_PARALLEL_DEBLOCKING)
INSTANTIATE_TEST_CASE_P(
    MSA, Loop8Test6Param,
    ::testing::Values(
        make_tuple(&aom_lpf_horizontal_4_msa, &aom_lpf_horizontal_4_c, 8),
        make_tuple(&aom_lpf_horizontal_8_msa, &aom_lpf_horizontal_8_c, 8),
        make_tuple(&aom_lpf_horizontal_edge_8_msa, &aom_lpf_horizontal_edge_8_c,
                   8),
        make_tuple(&aom_lpf_horizontal_edge_16_msa,
                   &aom_lpf_horizontal_edge_16_c, 8),
        make_tuple(&aom_lpf_vertical_4_msa, &aom_lpf_vertical_4_c, 8),
        make_tuple(&aom_lpf_vertical_8_msa, &aom_lpf_vertical_8_c, 8),
        make_tuple(&aom_lpf_vertical_16_msa, &aom_lpf_vertical_16_c, 8)));

INSTANTIATE_TEST_CASE_P(
    MSA, Loop8Test9Param,
    ::testing::Values(make_tuple(&aom_lpf_horizontal_4_dual_msa,
                                 &aom_lpf_horizontal_4_dual_c, 8),
                      make_tuple(&aom_lpf_horizontal_8_dual_msa,
                                 &aom_lpf_horizontal_8_dual_c, 8),
                      make_tuple(&aom_lpf_vertical_4_dual_msa,
                                 &aom_lpf_vertical_4_dual_c, 8),
                      make_tuple(&aom_lpf_vertical_8_dual_msa,
                                 &aom_lpf_vertical_8_dual_c, 8)));
#endif  // HAVE_MSA && (!CONFIG_HIGHBITDEPTH) && (!CONFIG_PARALLEL_DEBLOCKING)

}  // namespace
