// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/cms/tone_mapping_test.cc"
#include "lib/jxl/cms/tone_mapping.h"

#include <cstdio>
#include <hwy/foreach_target.h>

#include "lib/jxl/base/random.h"
#include "lib/jxl/cms/tone_mapping-inl.h"
#include "lib/jxl/testing.h"

// Test utils
#include <hwy/highway.h>
#include <hwy/tests/hwy_gtest.h>
HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {
namespace {

HWY_NOINLINE void TestRec2408ToneMap() {
  constexpr size_t kNumTrials = 1 << 23;
  Rng rng(1);
  float max_abs_err = 0;
  HWY_FULL(float) d;
  for (size_t i = 0; i < kNumTrials; i++) {
    float src = 11000.0 + rng.UniformF(-150.0f, 150.0f);
    float tgt = 250 + rng.UniformF(-5.0f, 5.0f);
    Vector3 luminances{rng.UniformF(0.2f, 0.4f), rng.UniformF(0.2f, 0.4f),
                       rng.UniformF(0.2f, 0.4f)};
    Color rgb{rng.UniformF(0.0f, 1.0f), rng.UniformF(0.0f, 1.0f),
              rng.UniformF(0.0f, 1.0f)};
    Rec2408ToneMapper<decltype(d)> tone_mapper({0.0f, src}, {0.0f, tgt},
                                               luminances);
    auto r = Set(d, rgb[0]);
    auto g = Set(d, rgb[1]);
    auto b = Set(d, rgb[2]);
    tone_mapper.ToneMap(&r, &g, &b);
    Rec2408ToneMapperBase tone_mapper_base({0.0f, src}, {0.0f, tgt},
                                           luminances);
    tone_mapper_base.ToneMap(rgb);
    const float actual_r = GetLane(r);
    const float expected_r = rgb[0];
    const float abs_err_r = std::abs(expected_r - actual_r);
    EXPECT_LT(abs_err_r, 2.75e-5);
    const float actual_g = GetLane(g);
    const float expected_g = rgb[1];
    const float abs_err_g = std::abs(expected_g - actual_g);
    EXPECT_LT(abs_err_g, 2.75e-5);
    const float actual_b = GetLane(b);
    const float expected_b = rgb[2];
    const float abs_err_b = std::abs(expected_b - actual_b);
    EXPECT_LT(abs_err_b, 2.75e-5);
    max_abs_err = std::max({max_abs_err, abs_err_r, abs_err_g, abs_err_b});
  }
  printf("max abs err %e\n", static_cast<double>(max_abs_err));
}

HWY_NOINLINE void TestHlgOotfApply() {
  constexpr size_t kNumTrials = 1 << 23;
  Rng rng(1);
  float max_abs_err = 0;
  HWY_FULL(float) d;
  for (size_t i = 0; i < kNumTrials; i++) {
    float src = 300.0 + rng.UniformF(-50.0f, 50.0f);
    float tgt = 80 + rng.UniformF(-5.0f, 5.0f);
    Vector3 luminances{rng.UniformF(0.2f, 0.4f), rng.UniformF(0.2f, 0.4f),
                       rng.UniformF(0.2f, 0.4f)};
    Color rgb{rng.UniformF(0.0f, 1.0f), rng.UniformF(0.0f, 1.0f),
              rng.UniformF(0.0f, 1.0f)};
    HlgOOTF ootf(src, tgt, luminances);
    auto r = Set(d, rgb[0]);
    auto g = Set(d, rgb[1]);
    auto b = Set(d, rgb[2]);
    ootf.Apply(&r, &g, &b);
    HlgOOTF_Base ootf_base(src, tgt, luminances);
    ootf_base.Apply(rgb);
    const float actual_r = GetLane(r);
    const float expected_r = rgb[0];
    const float abs_err_r = std::abs(expected_r - actual_r);
    EXPECT_LT(abs_err_r, 7.2e-7);
    const float actual_g = GetLane(g);
    const float expected_g = rgb[1];
    const float abs_err_g = std::abs(expected_g - actual_g);
    EXPECT_LT(abs_err_g, 7.2e-7);
    const float actual_b = GetLane(b);
    const float expected_b = rgb[2];
    const float abs_err_b = std::abs(expected_b - actual_b);
    EXPECT_LT(abs_err_b, 7.2e-7);
    max_abs_err = std::max({max_abs_err, abs_err_r, abs_err_g, abs_err_b});
  }
  printf("max abs err %e\n", static_cast<double>(max_abs_err));
}

HWY_NOINLINE void TestGamutMap() {
  constexpr size_t kNumTrials = 1 << 23;
  Rng rng(1);
  float max_abs_err = 0;
  HWY_FULL(float) d;
  for (size_t i = 0; i < kNumTrials; i++) {
    float preserve_saturation = rng.UniformF(0.2f, 0.4f);
    Vector3 luminances{rng.UniformF(0.2f, 0.4f), rng.UniformF(0.2f, 0.4f),
                       rng.UniformF(0.2f, 0.4f)};
    Color rgb{rng.UniformF(0.0f, 1.0f), rng.UniformF(0.0f, 1.0f),
              rng.UniformF(0.0f, 1.0f)};
    auto r = Set(d, rgb[0]);
    auto g = Set(d, rgb[1]);
    auto b = Set(d, rgb[2]);
    GamutMap(&r, &g, &b, luminances, preserve_saturation);
    GamutMapScalar(rgb, luminances, preserve_saturation);
    const float actual_r = GetLane(r);
    const float expected_r = rgb[0];
    const float abs_err_r = std::abs(expected_r - actual_r);
    EXPECT_LT(abs_err_r, 1e-10);
    const float actual_g = GetLane(g);
    const float expected_g = rgb[1];
    const float abs_err_g = std::abs(expected_g - actual_g);
    EXPECT_LT(abs_err_g, 1e-10);
    const float actual_b = GetLane(b);
    const float expected_b = rgb[2];
    const float abs_err_b = std::abs(expected_b - actual_b);
    EXPECT_LT(abs_err_b, 1e-10);
    max_abs_err = std::max({max_abs_err, abs_err_r, abs_err_g, abs_err_b});
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

class ToneMappingTargetTest : public hwy::TestWithParamTarget {};
HWY_TARGET_INSTANTIATE_TEST_SUITE_P(ToneMappingTargetTest);

HWY_EXPORT_AND_TEST_P(ToneMappingTargetTest, TestRec2408ToneMap);
HWY_EXPORT_AND_TEST_P(ToneMappingTargetTest, TestHlgOotfApply);
HWY_EXPORT_AND_TEST_P(ToneMappingTargetTest, TestGamutMap);

}  // namespace jxl
#endif  // HWY_ONCE
