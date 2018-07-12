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

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"

#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "aom_mem/aom_mem.h"

using libaom_test::ACMRandom;

namespace {
class AverageTestBase : public ::testing::Test {
 public:
  AverageTestBase(int width, int height) : width_(width), height_(height) {}

  static void SetUpTestCase() {
    source_data_ = reinterpret_cast<uint8_t *>(
        aom_memalign(kDataAlignment, kDataBlockSize));
  }

  static void TearDownTestCase() {
    aom_free(source_data_);
    source_data_ = NULL;
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  // Handle blocks up to 4 blocks 64x64 with stride up to 128
  static const int kDataAlignment = 16;
  static const int kDataBlockSize = 64 * 128;

  virtual void SetUp() {
    source_stride_ = (width_ + 31) & ~31;
    rnd_.Reset(ACMRandom::DeterministicSeed());
  }

  void FillConstant(uint8_t fill_constant) {
    for (int i = 0; i < width_ * height_; ++i) {
      source_data_[i] = fill_constant;
    }
  }

  void FillRandom() {
    for (int i = 0; i < width_ * height_; ++i) {
      source_data_[i] = rnd_.Rand8();
    }
  }

  int width_, height_;
  static uint8_t *source_data_;
  int source_stride_;

  ACMRandom rnd_;
};

typedef void (*IntProRowFunc)(int16_t hbuf[16], uint8_t const *ref,
                              const int ref_stride, const int height);

typedef std::tr1::tuple<int, IntProRowFunc, IntProRowFunc> IntProRowParam;

class IntProRowTest : public AverageTestBase,
                      public ::testing::WithParamInterface<IntProRowParam> {
 public:
  IntProRowTest()
      : AverageTestBase(16, GET_PARAM(0)), hbuf_asm_(NULL), hbuf_c_(NULL) {
    asm_func_ = GET_PARAM(1);
    c_func_ = GET_PARAM(2);
  }

 protected:
  virtual void SetUp() {
    hbuf_asm_ = reinterpret_cast<int16_t *>(
        aom_memalign(kDataAlignment, sizeof(*hbuf_asm_) * 16));
    hbuf_c_ = reinterpret_cast<int16_t *>(
        aom_memalign(kDataAlignment, sizeof(*hbuf_c_) * 16));
  }

  virtual void TearDown() {
    aom_free(hbuf_c_);
    hbuf_c_ = NULL;
    aom_free(hbuf_asm_);
    hbuf_asm_ = NULL;
  }

  void RunComparison() {
    ASM_REGISTER_STATE_CHECK(c_func_(hbuf_c_, source_data_, 0, height_));
    ASM_REGISTER_STATE_CHECK(asm_func_(hbuf_asm_, source_data_, 0, height_));
    EXPECT_EQ(0, memcmp(hbuf_c_, hbuf_asm_, sizeof(*hbuf_c_) * 16))
        << "Output mismatch";
  }

 private:
  IntProRowFunc asm_func_;
  IntProRowFunc c_func_;
  int16_t *hbuf_asm_;
  int16_t *hbuf_c_;
};

typedef int16_t (*IntProColFunc)(uint8_t const *ref, const int width);

typedef std::tr1::tuple<int, IntProColFunc, IntProColFunc> IntProColParam;

class IntProColTest : public AverageTestBase,
                      public ::testing::WithParamInterface<IntProColParam> {
 public:
  IntProColTest() : AverageTestBase(GET_PARAM(0), 1), sum_asm_(0), sum_c_(0) {
    asm_func_ = GET_PARAM(1);
    c_func_ = GET_PARAM(2);
  }

 protected:
  void RunComparison() {
    ASM_REGISTER_STATE_CHECK(sum_c_ = c_func_(source_data_, width_));
    ASM_REGISTER_STATE_CHECK(sum_asm_ = asm_func_(source_data_, width_));
    EXPECT_EQ(sum_c_, sum_asm_) << "Output mismatch";
  }

 private:
  IntProColFunc asm_func_;
  IntProColFunc c_func_;
  int16_t sum_asm_;
  int16_t sum_c_;
};

typedef int (*SatdFunc)(const int16_t *coeffs, int length);
typedef std::tr1::tuple<int, SatdFunc> SatdTestParam;

class SatdTest : public ::testing::Test,
                 public ::testing::WithParamInterface<SatdTestParam> {
 protected:
  virtual void SetUp() {
    satd_size_ = GET_PARAM(0);
    satd_func_ = GET_PARAM(1);
    rnd_.Reset(ACMRandom::DeterministicSeed());
    src_ = reinterpret_cast<int16_t *>(
        aom_memalign(16, sizeof(*src_) * satd_size_));
    ASSERT_TRUE(src_ != NULL);
  }

  virtual void TearDown() {
    libaom_test::ClearSystemState();
    aom_free(src_);
  }

  void FillConstant(const int16_t val) {
    for (int i = 0; i < satd_size_; ++i) src_[i] = val;
  }

  void FillRandom() {
    for (int i = 0; i < satd_size_; ++i) src_[i] = rnd_.Rand16();
  }

  void Check(int expected) {
    int total;
    ASM_REGISTER_STATE_CHECK(total = satd_func_(src_, satd_size_));
    EXPECT_EQ(expected, total);
  }

  int satd_size_;

 private:
  int16_t *src_;
  SatdFunc satd_func_;
  ACMRandom rnd_;
};

uint8_t *AverageTestBase::source_data_ = NULL;

TEST_P(IntProRowTest, MinValue) {
  FillConstant(0);
  RunComparison();
}

TEST_P(IntProRowTest, MaxValue) {
  FillConstant(255);
  RunComparison();
}

TEST_P(IntProRowTest, Random) {
  FillRandom();
  RunComparison();
}

TEST_P(IntProColTest, MinValue) {
  FillConstant(0);
  RunComparison();
}

TEST_P(IntProColTest, MaxValue) {
  FillConstant(255);
  RunComparison();
}

TEST_P(IntProColTest, Random) {
  FillRandom();
  RunComparison();
}

TEST_P(SatdTest, MinValue) {
  const int kMin = -32640;
  const int expected = -kMin * satd_size_;
  FillConstant(kMin);
  Check(expected);
}

TEST_P(SatdTest, MaxValue) {
  const int kMax = 32640;
  const int expected = kMax * satd_size_;
  FillConstant(kMax);
  Check(expected);
}

TEST_P(SatdTest, Random) {
  int expected;
  switch (satd_size_) {
    case 16: expected = 205298; break;
    case 64: expected = 1113950; break;
    case 256: expected = 4268415; break;
    case 1024: expected = 16954082; break;
    default:
      FAIL() << "Invalid satd size (" << satd_size_
             << ") valid: 16/64/256/1024";
  }
  FillRandom();
  Check(expected);
}

using std::tr1::make_tuple;

INSTANTIATE_TEST_CASE_P(C, SatdTest,
                        ::testing::Values(make_tuple(16, &aom_satd_c),
                                          make_tuple(64, &aom_satd_c),
                                          make_tuple(256, &aom_satd_c),
                                          make_tuple(1024, &aom_satd_c)));

#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(
    SSE2, IntProRowTest,
    ::testing::Values(make_tuple(16, &aom_int_pro_row_sse2, &aom_int_pro_row_c),
                      make_tuple(32, &aom_int_pro_row_sse2, &aom_int_pro_row_c),
                      make_tuple(64, &aom_int_pro_row_sse2,
                                 &aom_int_pro_row_c)));

INSTANTIATE_TEST_CASE_P(
    SSE2, IntProColTest,
    ::testing::Values(make_tuple(16, &aom_int_pro_col_sse2, &aom_int_pro_col_c),
                      make_tuple(32, &aom_int_pro_col_sse2, &aom_int_pro_col_c),
                      make_tuple(64, &aom_int_pro_col_sse2,
                                 &aom_int_pro_col_c)));

INSTANTIATE_TEST_CASE_P(SSE2, SatdTest,
                        ::testing::Values(make_tuple(16, &aom_satd_sse2),
                                          make_tuple(64, &aom_satd_sse2),
                                          make_tuple(256, &aom_satd_sse2),
                                          make_tuple(1024, &aom_satd_sse2)));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(
    NEON, IntProRowTest,
    ::testing::Values(make_tuple(16, &aom_int_pro_row_neon, &aom_int_pro_row_c),
                      make_tuple(32, &aom_int_pro_row_neon, &aom_int_pro_row_c),
                      make_tuple(64, &aom_int_pro_row_neon,
                                 &aom_int_pro_row_c)));

INSTANTIATE_TEST_CASE_P(
    NEON, IntProColTest,
    ::testing::Values(make_tuple(16, &aom_int_pro_col_neon, &aom_int_pro_col_c),
                      make_tuple(32, &aom_int_pro_col_neon, &aom_int_pro_col_c),
                      make_tuple(64, &aom_int_pro_col_neon,
                                 &aom_int_pro_col_c)));

INSTANTIATE_TEST_CASE_P(NEON, SatdTest,
                        ::testing::Values(make_tuple(16, &aom_satd_neon),
                                          make_tuple(64, &aom_satd_neon),
                                          make_tuple(256, &aom_satd_neon),
                                          make_tuple(1024, &aom_satd_neon)));
#endif

}  // namespace
