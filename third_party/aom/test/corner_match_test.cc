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
#include "config/av1_rtcd.h"

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/acm_random.h"
#include "test/util.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"

#include "av1/encoder/corner_match.h"

namespace test_libaom {

namespace AV1CornerMatch {

using libaom_test::ACMRandom;

using ::testing::make_tuple;
using ::testing::tuple;
typedef tuple<int> CornerMatchParam;

class AV1CornerMatchTest : public ::testing::TestWithParam<CornerMatchParam> {
 public:
  virtual ~AV1CornerMatchTest();
  virtual void SetUp();

  virtual void TearDown();

 protected:
  void RunCheckOutput();

  libaom_test::ACMRandom rnd_;
};

AV1CornerMatchTest::~AV1CornerMatchTest() {}
void AV1CornerMatchTest::SetUp() { rnd_.Reset(ACMRandom::DeterministicSeed()); }
void AV1CornerMatchTest::TearDown() { libaom_test::ClearSystemState(); }

void AV1CornerMatchTest::RunCheckOutput() {
  const int w = 128, h = 128;
  const int num_iters = 10000;
  int i, j;

  uint8_t *input1 = new uint8_t[w * h];
  uint8_t *input2 = new uint8_t[w * h];

  // Test the two extreme cases:
  // i) Random data, should have correlation close to 0
  // ii) Linearly related data + noise, should have correlation close to 1
  int mode = GET_PARAM(0);
  if (mode == 0) {
    for (i = 0; i < h; ++i)
      for (j = 0; j < w; ++j) {
        input1[i * w + j] = rnd_.Rand8();
        input2[i * w + j] = rnd_.Rand8();
      }
  } else if (mode == 1) {
    for (i = 0; i < h; ++i)
      for (j = 0; j < w; ++j) {
        int v = rnd_.Rand8();
        input1[i * w + j] = v;
        input2[i * w + j] = (v / 2) + (rnd_.Rand8() & 15);
      }
  }

  for (i = 0; i < num_iters; ++i) {
    int x1 = MATCH_SZ_BY2 + rnd_.PseudoUniform(w - 2 * MATCH_SZ_BY2);
    int y1 = MATCH_SZ_BY2 + rnd_.PseudoUniform(h - 2 * MATCH_SZ_BY2);
    int x2 = MATCH_SZ_BY2 + rnd_.PseudoUniform(w - 2 * MATCH_SZ_BY2);
    int y2 = MATCH_SZ_BY2 + rnd_.PseudoUniform(h - 2 * MATCH_SZ_BY2);

    double res_c =
        compute_cross_correlation_c(input1, w, x1, y1, input2, w, x2, y2);
    double res_sse4 =
        compute_cross_correlation_sse4_1(input1, w, x1, y1, input2, w, x2, y2);

    ASSERT_EQ(res_sse4, res_c);
  }

  delete[] input1;
  delete[] input2;
}

TEST_P(AV1CornerMatchTest, CheckOutput) { RunCheckOutput(); }

INSTANTIATE_TEST_CASE_P(SSE4_1, AV1CornerMatchTest,
                        ::testing::Values(make_tuple(0), make_tuple(1)));

}  // namespace AV1CornerMatch

}  // namespace test_libaom
