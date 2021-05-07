// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>

#include <random>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/fast_math_test.cc"
#include <hwy/foreach_target.h>

#include "lib/jxl/dec_xyb-inl.h"
#include "lib/jxl/enc_xyb.h"
#include "lib/jxl/fast_math-inl.h"
#include "lib/jxl/transfer_functions-inl.h"

// Test utils
#include <hwy/highway.h>
#include <hwy/tests/test_util-inl.h>
HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {
namespace {

HWY_NOINLINE void TestFastLog2() {
  constexpr size_t kNumTrials = 1 << 23;
  std::mt19937 rng(1);
  std::uniform_real_distribution<float> dist(1e-7f, 1e3f);
  float max_abs_err = 0;
  HWY_FULL(float) d;
  for (size_t i = 0; i < kNumTrials; i++) {
    const float f = dist(rng);
    const auto actual_v = FastLog2f(d, Set(d, f));
    const float actual = GetLane(actual_v);
    const float abs_err = std::abs(std::log2(f) - actual);
    EXPECT_LT(abs_err, 2.9E-6) << "f = " << f;
    max_abs_err = std::max(max_abs_err, abs_err);
  }
  printf("max abs err %e\n", static_cast<double>(max_abs_err));
}

HWY_NOINLINE void TestFastPow2() {
  constexpr size_t kNumTrials = 1 << 23;
  std::mt19937 rng(1);
  std::uniform_real_distribution<float> dist(-100, 100);
  float max_rel_err = 0;
  HWY_FULL(float) d;
  for (size_t i = 0; i < kNumTrials; i++) {
    const float f = dist(rng);
    const auto actual_v = FastPow2f(d, Set(d, f));
    const float actual = GetLane(actual_v);
    const float expected = std::pow(2, f);
    const float rel_err = std::abs(expected - actual) / expected;
    EXPECT_LT(rel_err, 3.1E-7) << "f = " << f;
    max_rel_err = std::max(max_rel_err, rel_err);
  }
  printf("max rel err %e\n", static_cast<double>(max_rel_err));
}

HWY_NOINLINE void TestFastPow() {
  constexpr size_t kNumTrials = 1 << 23;
  std::mt19937 rng(1);
  std::uniform_real_distribution<float> distb(1e-3f, 1e3f);
  std::uniform_real_distribution<float> diste(-10, 10);
  float max_rel_err = 0;
  HWY_FULL(float) d;
  for (size_t i = 0; i < kNumTrials; i++) {
    const float b = distb(rng);
    const float e = diste(rng);
    const auto actual_v = FastPowf(d, Set(d, b), Set(d, e));
    const float actual = GetLane(actual_v);
    const float expected = std::pow(b, e);
    const float rel_err = std::abs(expected - actual) / expected;
    EXPECT_LT(rel_err, 3E-5) << "b = " << b << " e = " << e;
    max_rel_err = std::max(max_rel_err, rel_err);
  }
  printf("max rel err %e\n", static_cast<double>(max_rel_err));
}

HWY_NOINLINE void TestFastCos() {
  constexpr size_t kNumTrials = 1 << 23;
  std::mt19937 rng(1);
  std::uniform_real_distribution<float> dist(-1e3f, 1e3f);
  float max_abs_err = 0;
  HWY_FULL(float) d;
  for (size_t i = 0; i < kNumTrials; i++) {
    const float f = dist(rng);
    const auto actual_v = FastCosf(d, Set(d, f));
    const float actual = GetLane(actual_v);
    const float abs_err = std::abs(std::cos(f) - actual);
    EXPECT_LT(abs_err, 7E-5) << "f = " << f;
    max_abs_err = std::max(max_abs_err, abs_err);
  }
  printf("max abs err %e\n", static_cast<double>(max_abs_err));
}

HWY_NOINLINE void TestFastErf() {
  constexpr size_t kNumTrials = 1 << 23;
  std::mt19937 rng(1);
  std::uniform_real_distribution<float> dist(-5.f, 5.f);
  float max_abs_err = 0;
  HWY_FULL(float) d;
  for (size_t i = 0; i < kNumTrials; i++) {
    const float f = dist(rng);
    const auto actual_v = FastErff(d, Set(d, f));
    const float actual = GetLane(actual_v);
    const float abs_err = std::abs(std::erf(f) - actual);
    EXPECT_LT(abs_err, 7E-4) << "f = " << f;
    max_abs_err = std::max(max_abs_err, abs_err);
  }
  printf("max abs err %e\n", static_cast<double>(max_abs_err));
}

HWY_NOINLINE void TestFastSRGB() {
  constexpr size_t kNumTrials = 1 << 23;
  std::mt19937 rng(1);
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  float max_abs_err = 0;
  HWY_FULL(float) d;
  for (size_t i = 0; i < kNumTrials; i++) {
    const float f = dist(rng);
    const auto actual_v = FastLinearToSRGB(d, Set(d, f));
    const float actual = GetLane(actual_v);
    const float expected = GetLane(TF_SRGB().EncodedFromDisplay(d, Set(d, f)));
    const float abs_err = std::abs(expected - actual);
    EXPECT_LT(abs_err, 1.2E-4) << "f = " << f;
    max_abs_err = std::max(max_abs_err, abs_err);
  }
  printf("max abs err %e\n", static_cast<double>(max_abs_err));
}

HWY_NOINLINE void TestFastPQEFD() {
  constexpr size_t kNumTrials = 1 << 23;
  std::mt19937 rng(1);
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  float max_abs_err = 0;
  HWY_FULL(float) d;
  for (size_t i = 0; i < kNumTrials; i++) {
    const float f = dist(rng);
    const float actual = GetLane(TF_PQ().EncodedFromDisplay(d, Set(d, f)));
    const float expected = TF_PQ().EncodedFromDisplay(f);
    const float abs_err = std::abs(expected - actual);
    EXPECT_LT(abs_err, 7e-7) << "f = " << f;
    max_abs_err = std::max(max_abs_err, abs_err);
  }
  printf("max abs err %e\n", static_cast<double>(max_abs_err));
}

HWY_NOINLINE void TestFastHLGEFD() {
  constexpr size_t kNumTrials = 1 << 23;
  std::mt19937 rng(1);
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  float max_abs_err = 0;
  HWY_FULL(float) d;
  for (size_t i = 0; i < kNumTrials; i++) {
    const float f = dist(rng);
    const float actual = GetLane(TF_HLG().EncodedFromDisplay(d, Set(d, f)));
    const float expected = TF_HLG().EncodedFromDisplay(f);
    const float abs_err = std::abs(expected - actual);
    EXPECT_LT(abs_err, 5e-7) << "f = " << f;
    max_abs_err = std::max(max_abs_err, abs_err);
  }
  printf("max abs err %e\n", static_cast<double>(max_abs_err));
}

HWY_NOINLINE void TestFast709EFD() {
  constexpr size_t kNumTrials = 1 << 23;
  std::mt19937 rng(1);
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  float max_abs_err = 0;
  HWY_FULL(float) d;
  for (size_t i = 0; i < kNumTrials; i++) {
    const float f = dist(rng);
    const float actual = GetLane(TF_709().EncodedFromDisplay(d, Set(d, f)));
    const float expected = TF_709().EncodedFromDisplay(f);
    const float abs_err = std::abs(expected - actual);
    EXPECT_LT(abs_err, 2e-6) << "f = " << f;
    max_abs_err = std::max(max_abs_err, abs_err);
  }
  printf("max abs err %e\n", static_cast<double>(max_abs_err));
}

HWY_NOINLINE void TestFastPQDFE() {
  constexpr size_t kNumTrials = 1 << 23;
  std::mt19937 rng(1);
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  float max_abs_err = 0;
  HWY_FULL(float) d;
  for (size_t i = 0; i < kNumTrials; i++) {
    const float f = dist(rng);
    const float actual = GetLane(TF_PQ().DisplayFromEncoded(d, Set(d, f)));
    const float expected = TF_PQ().DisplayFromEncoded(f);
    const float abs_err = std::abs(expected - actual);
    EXPECT_LT(abs_err, 3E-6) << "f = " << f;
    max_abs_err = std::max(max_abs_err, abs_err);
  }
  printf("max abs err %e\n", static_cast<double>(max_abs_err));
}

HWY_NOINLINE void TestFastXYB() {
  if (!HasFastXYBTosRGB8()) return;
  ImageMetadata metadata;
  ImageBundle ib(&metadata);
  int scaling = 1;
  int n = 256 * scaling;
  float inv_scaling = 1.0f / scaling;
  int kChunk = 32;
  // The image is divided in chunks to reduce total memory usage.
  for (int cr = 0; cr < n; cr += kChunk) {
    for (int cg = 0; cg < n; cg += kChunk) {
      for (int cb = 0; cb < n; cb += kChunk) {
        Image3F chunk(kChunk * kChunk, kChunk);
        for (int ir = 0; ir < kChunk; ir++) {
          for (int ig = 0; ig < kChunk; ig++) {
            for (int ib = 0; ib < kChunk; ib++) {
              float r = (cr + ir) * inv_scaling;
              float g = (cg + ig) * inv_scaling;
              float b = (cb + ib) * inv_scaling;
              chunk.PlaneRow(0, ir)[ig * kChunk + ib] = r * (1.0f / 255);
              chunk.PlaneRow(1, ir)[ig * kChunk + ib] = g * (1.0f / 255);
              chunk.PlaneRow(2, ir)[ig * kChunk + ib] = b * (1.0f / 255);
            }
          }
        }
        ib.SetFromImage(std::move(chunk), ColorEncoding::SRGB());
        Image3F xyb(kChunk * kChunk, kChunk);
        std::vector<uint8_t> roundtrip(kChunk * kChunk * kChunk * 3);
        ToXYB(ib, nullptr, &xyb);
        jxl::HWY_NAMESPACE::FastXYBTosRGB8(xyb, Rect(xyb), Rect(xyb),
                                           roundtrip.data(), xyb.xsize());
        for (int ir = 0; ir < kChunk; ir++) {
          for (int ig = 0; ig < kChunk; ig++) {
            for (int ib = 0; ib < kChunk; ib++) {
              float r = (cr + ir) * inv_scaling;
              float g = (cg + ig) * inv_scaling;
              float b = (cb + ib) * inv_scaling;
              size_t idx = ir * kChunk * kChunk + ig * kChunk + ib;
              int rr = roundtrip[3 * idx];
              int rg = roundtrip[3 * idx + 1];
              int rb = roundtrip[3 * idx + 2];
              EXPECT_LT(abs(r - rr), 2) << "expected " << r << " got " << rr;
              EXPECT_LT(abs(g - rg), 2) << "expected " << g << " got " << rg;
              EXPECT_LT(abs(b - rb), 2) << "expected " << b << " got " << rb;
            }
          }
        }
      }
    }
  }
}

}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {

class FastMathTargetTest : public hwy::TestWithParamTarget {};
HWY_TARGET_INSTANTIATE_TEST_SUITE_P(FastMathTargetTest);

HWY_EXPORT_AND_TEST_P(FastMathTargetTest, TestFastLog2);
HWY_EXPORT_AND_TEST_P(FastMathTargetTest, TestFastPow2);
HWY_EXPORT_AND_TEST_P(FastMathTargetTest, TestFastPow);
HWY_EXPORT_AND_TEST_P(FastMathTargetTest, TestFastCos);
HWY_EXPORT_AND_TEST_P(FastMathTargetTest, TestFastErf);
HWY_EXPORT_AND_TEST_P(FastMathTargetTest, TestFastSRGB);
HWY_EXPORT_AND_TEST_P(FastMathTargetTest, TestFastPQDFE);
HWY_EXPORT_AND_TEST_P(FastMathTargetTest, TestFastPQEFD);
HWY_EXPORT_AND_TEST_P(FastMathTargetTest, TestFastHLGEFD);
HWY_EXPORT_AND_TEST_P(FastMathTargetTest, TestFast709EFD);
HWY_EXPORT_AND_TEST_P(FastMathTargetTest, TestFastXYB);

}  // namespace jxl
#endif  // HWY_ONCE
