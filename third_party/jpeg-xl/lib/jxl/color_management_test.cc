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

#include "lib/jxl/color_management.h"

#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <new>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/file_io.h"
#include "lib/jxl/base/thread_pool_internal.h"
#include "lib/jxl/enc_color_management.h"
#include "lib/jxl/image_test_utils.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testdata.h"

namespace jxl {

std::ostream& operator<<(std::ostream& os, const CIExy& xy) {
  return os << "{x=" << xy.x << ", y=" << xy.y << "}";
}

std::ostream& operator<<(std::ostream& os, const PrimariesCIExy& primaries) {
  return os << "{r=" << primaries.r << ", g=" << primaries.g
            << ", b=" << primaries.b << "}";
}

namespace {

using ::testing::ElementsAre;
using ::testing::FloatNear;

// Small enough to be fast. If changed, must update Generate*.
static constexpr size_t kWidth = 16;

struct Globals {
  // TODO(deymo): Make this a const.
  static Globals* GetInstance() {
    static Globals ret;
    return &ret;
  }

 private:
  static constexpr size_t kNumThreads = 0;  // only have a single row.

  Globals() : pool(kNumThreads) {
    in_gray = GenerateGray();
    in_color = GenerateColor();
    out_gray = ImageF(kWidth, 1);
    out_color = ImageF(kWidth * 3, 1);

    c_native = ColorEncoding::LinearSRGB(/*is_gray=*/false);
    c_gray = ColorEncoding::LinearSRGB(/*is_gray=*/true);
  }

  static ImageF GenerateGray() {
    ImageF gray(kWidth, 1);
    float* JXL_RESTRICT row = gray.Row(0);
    // Increasing left to right
    for (uint32_t x = 0; x < kWidth; ++x) {
      row[x] = x * 1.0f / (kWidth - 1);  // [0, 1]
    }
    return gray;
  }

  static ImageF GenerateColor() {
    ImageF image(kWidth * 3, 1);
    float* JXL_RESTRICT interleaved = image.Row(0);
    std::fill(interleaved, interleaved + kWidth * 3, 0.0f);

    // [0, 4): neutral
    for (int32_t x = 0; x < 4; ++x) {
      interleaved[3 * x + 0] = x * 1.0f / 3;  // [0, 1]
      interleaved[3 * x + 2] = interleaved[3 * x + 1] = interleaved[3 * x + 0];
    }

    // [4, 13): pure RGB with low/medium/high saturation
    for (int32_t c = 0; c < 3; ++c) {
      interleaved[3 * (4 + c) + c] = 0.08f + c * 0.01f;
      interleaved[3 * (7 + c) + c] = 0.75f + c * 0.01f;
      interleaved[3 * (10 + c) + c] = 1.0f;
    }

    // [13, 16): impure, not quite saturated RGB
    interleaved[3 * 13 + 0] = 0.86f;
    interleaved[3 * 13 + 2] = interleaved[3 * 13 + 1] = 0.16f;
    interleaved[3 * 14 + 1] = 0.87f;
    interleaved[3 * 14 + 2] = interleaved[3 * 14 + 0] = 0.16f;
    interleaved[3 * 15 + 2] = 0.88f;
    interleaved[3 * 15 + 1] = interleaved[3 * 15 + 0] = 0.16f;

    return image;
  }

 public:
  ThreadPoolInternal pool;

  // ImageF so we can use VerifyRelativeError; all are interleaved RGB.
  ImageF in_gray;
  ImageF in_color;
  ImageF out_gray;
  ImageF out_color;
  ColorEncoding c_native;
  ColorEncoding c_gray;
};

class ColorManagementTest
    : public ::testing::TestWithParam<test::ColorEncodingDescriptor> {
 public:
  static void VerifySameFields(const ColorEncoding& c,
                               const ColorEncoding& c2) {
    ASSERT_EQ(c.rendering_intent, c2.rendering_intent);
    ASSERT_EQ(c.GetColorSpace(), c2.GetColorSpace());
    ASSERT_EQ(c.white_point, c2.white_point);
    if (c.HasPrimaries()) {
      ASSERT_EQ(c.primaries, c2.primaries);
    }
    ASSERT_TRUE(c.tf.IsSame(c2.tf));
  }

  // "Same" pixels after converting g->c_native -> c -> g->c_native.
  static void VerifyPixelRoundTrip(const ColorEncoding& c) {
    Globals* g = Globals::GetInstance();
    const ColorEncoding& c_native = c.IsGray() ? g->c_gray : g->c_native;
    ColorSpaceTransform xform_fwd;
    ColorSpaceTransform xform_rev;
    ASSERT_TRUE(xform_fwd.Init(c_native, c, kDefaultIntensityTarget, kWidth,
                               g->pool.NumThreads()));
    ASSERT_TRUE(xform_rev.Init(c, c_native, kDefaultIntensityTarget, kWidth,
                               g->pool.NumThreads()));

    const size_t thread = 0;
    const ImageF& in = c.IsGray() ? g->in_gray : g->in_color;
    ImageF* JXL_RESTRICT out = c.IsGray() ? &g->out_gray : &g->out_color;
    DoColorSpaceTransform(&xform_fwd, thread, in.Row(0),
                          xform_fwd.BufDst(thread));
    DoColorSpaceTransform(&xform_rev, thread, xform_fwd.BufDst(thread),
                          out->Row(0));

#if JPEGXL_ENABLE_SKCMS
    double max_l1 = 7E-4;
    double max_rel = 4E-7;
#else
    double max_l1 = 5E-5;
    // Most are lower; reached 3E-7 with D60 AP0.
    double max_rel = 4E-7;
#endif
    if (c.IsGray()) max_rel = 2E-5;
    VerifyRelativeError(in, *out, max_l1, max_rel);
  }
};
JXL_GTEST_INSTANTIATE_TEST_SUITE_P(ColorManagementTestInstantiation,
                                   ColorManagementTest,
                                   ::testing::ValuesIn(test::AllEncodings()));

// Exercises the ColorManagement interface for ALL ColorEncoding synthesizable
// via enums.
TEST_P(ColorManagementTest, VerifyAllProfiles) {
  ColorEncoding c = ColorEncodingFromDescriptor(GetParam());
  printf("%s\n", Description(c).c_str());

  // Can create profile.
  ASSERT_TRUE(c.CreateICC());

  // Can set an equivalent ColorEncoding from the generated ICC profile.
  ColorEncoding c3;
  ASSERT_TRUE(c3.SetICC(PaddedBytes(c.ICC())));
  VerifySameFields(c, c3);

  VerifyPixelRoundTrip(c);
}

testing::Matcher<CIExy> CIExyIs(const double x, const double y) {
  static constexpr double kMaxError = 1e-4;
  return testing::AllOf(
      testing::Field(&CIExy::x, testing::DoubleNear(x, kMaxError)),
      testing::Field(&CIExy::y, testing::DoubleNear(y, kMaxError)));
}

testing::Matcher<PrimariesCIExy> PrimariesAre(
    const testing::Matcher<CIExy>& r, const testing::Matcher<CIExy>& g,
    const testing::Matcher<CIExy>& b) {
  return testing::AllOf(testing::Field(&PrimariesCIExy::r, r),
                        testing::Field(&PrimariesCIExy::g, g),
                        testing::Field(&PrimariesCIExy::b, b));
}

TEST_F(ColorManagementTest, sRGBChromaticity) {
  const ColorEncoding sRGB = ColorEncoding::SRGB();
  EXPECT_THAT(sRGB.GetWhitePoint(), CIExyIs(0.3127, 0.3290));
  EXPECT_THAT(sRGB.GetPrimaries(),
              PrimariesAre(CIExyIs(0.64, 0.33), CIExyIs(0.30, 0.60),
                           CIExyIs(0.15, 0.06)));
}

TEST_F(ColorManagementTest, D2700Chromaticity) {
  PaddedBytes icc = ReadTestData("jxl/color_management/sRGB-D2700.icc");
  ColorEncoding sRGB_D2700;
  ASSERT_TRUE(sRGB_D2700.SetICC(std::move(icc)));

  EXPECT_THAT(sRGB_D2700.GetWhitePoint(), CIExyIs(0.45986, 0.41060));
  // The illuminant-relative chromaticities of this profile's primaries are the
  // same as for sRGB. It is the PCS-relative chromaticities that would be
  // different.
  EXPECT_THAT(sRGB_D2700.GetPrimaries(),
              PrimariesAre(CIExyIs(0.64, 0.33), CIExyIs(0.30, 0.60),
                           CIExyIs(0.15, 0.06)));
}

TEST_F(ColorManagementTest, D2700ToSRGB) {
  PaddedBytes icc = ReadTestData("jxl/color_management/sRGB-D2700.icc");
  ColorEncoding sRGB_D2700;
  ASSERT_TRUE(sRGB_D2700.SetICC(std::move(icc)));

  ColorSpaceTransform transform;
  ASSERT_TRUE(transform.Init(sRGB_D2700, ColorEncoding::SRGB(),
                             kDefaultIntensityTarget, 1, 1));
  const float sRGB_D2700_values[3] = {0.863, 0.737, 0.490};
  float sRGB_values[3];
  DoColorSpaceTransform(&transform, 0, sRGB_D2700_values, sRGB_values);
  EXPECT_THAT(sRGB_values,
              ElementsAre(FloatNear(0.914, 1e-3), FloatNear(0.745, 1e-3),
                          FloatNear(0.601, 1e-3)));
}

}  // namespace
}  // namespace jxl
