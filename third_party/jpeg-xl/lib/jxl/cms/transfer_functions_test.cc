// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/cms/transfer_functions_test.cc"
#include "lib/jxl/cms/transfer_functions.h"

#include <cstdio>
#include <hwy/foreach_target.h>

#include "lib/jxl/base/random.h"
#include "lib/jxl/cms/transfer_functions-inl.h"
#include "lib/jxl/testing.h"

// Test utils
#include <hwy/highway.h>
#include <hwy/tests/hwy_gtest.h>
HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {
namespace {

HWY_NOINLINE void TestPqEncodedFromDisplay() {
  constexpr size_t kNumTrials = 1 << 23;
  Rng rng(1);
  float max_abs_err = 0;
  HWY_FULL(float) d;
  for (size_t i = 0; i < kNumTrials; i++) {
    double intensity = 11000.0 + rng.UniformF(-150.0f, 150.0f);
    TF_PQ tf_pq(intensity);
    const float f = rng.UniformF(0.0f, 1.0f);
    const float actual = GetLane(tf_pq.EncodedFromDisplay(d, Set(d, f)));
    const float expected = TF_PQ_Base::EncodedFromDisplay(intensity, f);
    const float abs_err = std::abs(expected - actual);
    EXPECT_LT(abs_err, 6e-7) << "f = " << f;
    max_abs_err = std::max(max_abs_err, abs_err);
  }
  printf("max abs err %e\n", static_cast<double>(max_abs_err));
}

HWY_NOINLINE void TestHlgEncodedFromDisplay() {
  constexpr size_t kNumTrials = 1 << 23;
  Rng rng(1);
  float max_abs_err = 0;
  HWY_FULL(float) d;
  for (size_t i = 0; i < kNumTrials; i++) {
    const float f = rng.UniformF(0.0f, 1.0f);
    const float actual = GetLane(TF_HLG().EncodedFromDisplay(d, Set(d, f)));
    const float expected = TF_HLG_Base::EncodedFromDisplay(f);
    const float abs_err = std::abs(expected - actual);
    EXPECT_LT(abs_err, 4e-7) << "f = " << f;
    max_abs_err = std::max(max_abs_err, abs_err);
  }
  printf("max abs err %e\n", static_cast<double>(max_abs_err));
}

HWY_NOINLINE void TestPqDisplayFromEncoded() {
  constexpr size_t kNumTrials = 1 << 23;
  Rng rng(1);
  float max_abs_err = 0;
  HWY_FULL(float) d;
  for (size_t i = 0; i < kNumTrials; i++) {
    double intensity = 11000.0 + rng.UniformF(-150.0f, 150.0f);
    TF_PQ tf_pq(intensity);
    const float f = rng.UniformF(0.0f, 1.0f);
    const float actual = GetLane(tf_pq.DisplayFromEncoded(d, Set(d, f)));
    const float expected = TF_PQ_Base::DisplayFromEncoded(intensity, f);
    const float abs_err = std::abs(expected - actual);
    EXPECT_LT(abs_err, 3E-6) << "f = " << f;
    max_abs_err = std::max(max_abs_err, abs_err);
  }
  printf("max abs err %e\n", static_cast<double>(max_abs_err));
}

}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {

class TransferFunctionsTargetTest : public hwy::TestWithParamTarget {};
HWY_TARGET_INSTANTIATE_TEST_SUITE_P(TransferFunctionsTargetTest);

HWY_EXPORT_AND_TEST_P(TransferFunctionsTargetTest, TestPqEncodedFromDisplay);
HWY_EXPORT_AND_TEST_P(TransferFunctionsTargetTest, TestHlgEncodedFromDisplay);
HWY_EXPORT_AND_TEST_P(TransferFunctionsTargetTest, TestPqDisplayFromEncoded);

}  // namespace jxl
#endif  // HWY_ONCE
