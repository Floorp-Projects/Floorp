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
#include <limits.h>
#include <stdio.h>

#include "./vpx_config.h"
#if CONFIG_VP9_ENCODER
#include "./vp9_rtcd.h"
#endif
#include "vpx_mem/vpx_mem.h"

#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "third_party/googletest/src/include/gtest/gtest.h"

using libvpx_test::ACMRandom;

namespace {
class AverageTestBase : public ::testing::Test {
 public:
  AverageTestBase(int width, int height) : width_(width), height_(height) {}

  static void SetUpTestCase() {
    source_data_ = reinterpret_cast<uint8_t*>(
        vpx_memalign(kDataAlignment, kDataBlockSize));
  }

  static void TearDownTestCase() {
    vpx_free(source_data_);
    source_data_ = NULL;
  }

  virtual void TearDown() {
    libvpx_test::ClearSystemState();
  }

 protected:
  // Handle blocks up to 4 blocks 64x64 with stride up to 128
  static const int kDataAlignment = 16;
  static const int kDataBlockSize = 64 * 128;

  virtual void SetUp() {
    source_stride_ = (width_ + 31) & ~31;
    rnd_.Reset(ACMRandom::DeterministicSeed());
  }

  // Sum Pixels
  unsigned int ReferenceAverage8x8(const uint8_t* source, int pitch ) {
    unsigned int average = 0;
    for (int h = 0; h < 8; ++h)
      for (int w = 0; w < 8; ++w)
        average += source[h * source_stride_ + w];
    return ((average + 32) >> 6);
  }

  unsigned int ReferenceAverage4x4(const uint8_t* source, int pitch ) {
    unsigned int average = 0;
    for (int h = 0; h < 4; ++h)
      for (int w = 0; w < 4; ++w)
        average += source[h * source_stride_ + w];
    return ((average + 8) >> 4);
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
  static uint8_t* source_data_;
  int source_stride_;

  ACMRandom rnd_;
};
typedef unsigned int (*AverageFunction)(const uint8_t* s, int pitch);

typedef std::tr1::tuple<int, int, int, int, AverageFunction> AvgFunc;

class AverageTest
    : public AverageTestBase,
      public ::testing::WithParamInterface<AvgFunc>{
 public:
  AverageTest() : AverageTestBase(GET_PARAM(0), GET_PARAM(1)) {}

 protected:
  void CheckAverages() {
    unsigned int expected = 0;
    if (GET_PARAM(3) == 8) {
      expected = ReferenceAverage8x8(source_data_+ GET_PARAM(2),
                                     source_stride_);
    } else  if (GET_PARAM(3) == 4) {
      expected = ReferenceAverage4x4(source_data_+ GET_PARAM(2),
                                     source_stride_);
    }

    ASM_REGISTER_STATE_CHECK(GET_PARAM(4)(source_data_+ GET_PARAM(2),
                                          source_stride_));
    unsigned int actual = GET_PARAM(4)(source_data_+ GET_PARAM(2),
                                       source_stride_);

    EXPECT_EQ(expected, actual);
  }
};


uint8_t* AverageTestBase::source_data_ = NULL;

TEST_P(AverageTest, MinValue) {
  FillConstant(0);
  CheckAverages();
}

TEST_P(AverageTest, MaxValue) {
  FillConstant(255);
  CheckAverages();
}

TEST_P(AverageTest, Random) {
  // The reference frame, but not the source frame, may be unaligned for
  // certain types of searches.
  for (int i = 0; i < 1000; i++) {
    FillRandom();
    CheckAverages();
  }
}

using std::tr1::make_tuple;

INSTANTIATE_TEST_CASE_P(
    C, AverageTest,
    ::testing::Values(
        make_tuple(16, 16, 1, 8, &vp9_avg_8x8_c),
        make_tuple(16, 16, 1, 4, &vp9_avg_4x4_c)));


#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(
    SSE2, AverageTest,
    ::testing::Values(
        make_tuple(16, 16, 0, 8, &vp9_avg_8x8_sse2),
        make_tuple(16, 16, 5, 8, &vp9_avg_8x8_sse2),
        make_tuple(32, 32, 15, 8, &vp9_avg_8x8_sse2),
        make_tuple(16, 16, 0, 4, &vp9_avg_4x4_sse2),
        make_tuple(16, 16, 5, 4, &vp9_avg_4x4_sse2),
        make_tuple(32, 32, 15, 4, &vp9_avg_4x4_sse2)));

#endif

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(
    NEON, AverageTest,
    ::testing::Values(
        make_tuple(16, 16, 0, 8, &vp9_avg_8x8_neon),
        make_tuple(16, 16, 5, 8, &vp9_avg_8x8_neon),
        make_tuple(32, 32, 15, 8, &vp9_avg_8x8_neon)));

#endif

}  // namespace
