// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <jxl/cms.h>
#include <jxl/cms_interface.h>
#include <jxl/memory_manager.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "lib/jxl/base/common.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/cms/color_encoding_cms.h"
#include "lib/jxl/cms/opsin_params.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/enc_xyb.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/image_metadata.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/image_test_utils.h"
#include "lib/jxl/test_memory_manager.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testing.h"

namespace jxl {

std::ostream& operator<<(std::ostream& os, const CIExy& xy) {
  return os << "{x=" << xy.x << ", y=" << xy.y << "}";
}

std::ostream& operator<<(std::ostream& os, const PrimariesCIExy& primaries) {
  return os << "{r=" << primaries.r << ", g=" << primaries.g
            << ", b=" << primaries.b << "}";
}

namespace {

// Small enough to be fast. If changed, must update Generate*.
constexpr size_t kWidth = 16;

constexpr size_t kNumThreads = 1;  // only have a single row.

struct Globals {
  // TODO(deymo): Make this a const.
  static Globals* GetInstance() {
    static Globals ret;
    return &ret;
  }

 private:
  Globals() {
    JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
    in_gray = GenerateGray();
    in_color = GenerateColor();
    JXL_ASSIGN_OR_DIE(out_gray, ImageF::Create(memory_manager, kWidth, 1));
    JXL_ASSIGN_OR_DIE(out_color, ImageF::Create(memory_manager, kWidth * 3, 1));

    c_native = ColorEncoding::LinearSRGB(/*is_gray=*/false);
    c_gray = ColorEncoding::LinearSRGB(/*is_gray=*/true);
  }

  static ImageF GenerateGray() {
    JXL_ASSIGN_OR_DIE(ImageF gray,
                      ImageF::Create(jxl::test::MemoryManager(), kWidth, 1));
    float* JXL_RESTRICT row = gray.Row(0);
    // Increasing left to right
    for (uint32_t x = 0; x < kWidth; ++x) {
      row[x] = x * 1.0f / (kWidth - 1);  // [0, 1]
    }
    return gray;
  }

  static ImageF GenerateColor() {
    JXL_ASSIGN_OR_DIE(ImageF image, ImageF::Create(jxl::test::MemoryManager(),
                                                   kWidth * 3, 1));
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
  // "Same" pixels after converting g->c_native -> c -> g->c_native.
  static void VerifyPixelRoundTrip(const ColorEncoding& c) {
    Globals* g = Globals::GetInstance();
    const ColorEncoding& c_native = c.IsGray() ? g->c_gray : g->c_native;
    const JxlCmsInterface& cms = *JxlGetDefaultCms();
    ColorSpaceTransform xform_fwd(cms);
    ColorSpaceTransform xform_rev(cms);
    const float intensity_target =
        c.Tf().IsHLG() ? 1000 : kDefaultIntensityTarget;
    ASSERT_TRUE(
        xform_fwd.Init(c_native, c, intensity_target, kWidth, kNumThreads));
    ASSERT_TRUE(
        xform_rev.Init(c, c_native, intensity_target, kWidth, kNumThreads));

    const size_t thread = 0;
    const ImageF& in = c.IsGray() ? g->in_gray : g->in_color;
    ImageF* JXL_RESTRICT out = c.IsGray() ? &g->out_gray : &g->out_color;
    ASSERT_TRUE(
        xform_fwd.Run(thread, in.Row(0), xform_fwd.BufDst(thread), kWidth));
    ASSERT_TRUE(
        xform_rev.Run(thread, xform_fwd.BufDst(thread), out->Row(0), kWidth));

    // With lcms2, this value is lower: 5E-5
    double max_l1 = 7E-4;
    // Most are lower; reached 3E-7 with D60 AP0.
    double max_rel = 4E-7;
    if (c.IsGray()) max_rel = 2E-5;
    JXL_ASSERT_OK(VerifyRelativeError(in, *out, max_l1, max_rel, _));
  }
};
JXL_GTEST_INSTANTIATE_TEST_SUITE_P(ColorManagementTestInstantiation,
                                   ColorManagementTest,
                                   ::testing::ValuesIn(test::AllEncodings()));

// Exercises the ColorManagement interface for ALL ColorEncoding synthesizable
// via enums.
TEST_P(ColorManagementTest, VerifyAllProfiles) {
  ColorEncoding actual = ColorEncodingFromDescriptor(GetParam());
  printf("%s\n", Description(actual).c_str());

  // Can create profile.
  ASSERT_TRUE(actual.CreateICC());

  // Can set an equivalent ColorEncoding from the generated ICC profile.
  ColorEncoding expected;
  ASSERT_TRUE(expected.SetICC(IccBytes(actual.ICC()), JxlGetDefaultCms()));

  EXPECT_EQ(actual.GetRenderingIntent(), expected.GetRenderingIntent())
      << "different rendering intent: " << ToString(actual.GetRenderingIntent())
      << " instead of " << ToString(expected.GetRenderingIntent());
  EXPECT_EQ(actual.GetColorSpace(), expected.GetColorSpace())
      << "different color space: " << ToString(actual.GetColorSpace())
      << " instead of " << ToString(expected.GetColorSpace());
  EXPECT_EQ(actual.GetWhitePointType(), expected.GetWhitePointType())
      << "different white point: " << ToString(actual.GetWhitePointType())
      << " instead of " << ToString(expected.GetWhitePointType());
  EXPECT_EQ(actual.HasPrimaries(), expected.HasPrimaries());
  if (actual.HasPrimaries()) {
    EXPECT_EQ(actual.GetPrimariesType(), expected.GetPrimariesType())
        << "different primaries: " << ToString(actual.GetPrimariesType())
        << " instead of " << ToString(expected.GetPrimariesType());
  }

  static const auto tf_to_string =
      [](const jxl::cms::CustomTransferFunction& tf) {
        if (tf.have_gamma) {
          return "g" + ToString(tf.GetGamma());
        }
        return ToString(tf.transfer_function);
      };
  EXPECT_TRUE(actual.Tf().IsSame(expected.Tf()))
      << "different transfer function: " << tf_to_string(actual.Tf())
      << " instead of " << tf_to_string(expected.Tf());

  VerifyPixelRoundTrip(actual);
}

#define EXPECT_CIEXY_NEAR(A, E, T)                                       \
  {                                                                      \
    CIExy _actual = (A);                                                 \
    CIExy _expected = (E);                                               \
    double _tolerance = (T);                                             \
    EXPECT_NEAR(_actual.x, _expected.x, _tolerance) << "x is different"; \
    EXPECT_NEAR(_actual.y, _expected.y, _tolerance) << "y is different"; \
  }

#define EXPECT_PRIMARIES_NEAR(A, E, T)                                         \
  {                                                                            \
    PrimariesCIExy _actual = (A);                                              \
    PrimariesCIExy _expected = (E);                                            \
    double _tolerance = (T);                                                   \
    EXPECT_NEAR(_actual.r.x, _expected.r.x, _tolerance) << "r.x is different"; \
    EXPECT_NEAR(_actual.r.y, _expected.r.y, _tolerance) << "r.y is different"; \
    EXPECT_NEAR(_actual.g.x, _expected.g.x, _tolerance) << "g.x is different"; \
    EXPECT_NEAR(_actual.g.y, _expected.g.y, _tolerance) << "g.y is different"; \
    EXPECT_NEAR(_actual.b.x, _expected.b.x, _tolerance) << "b.x is different"; \
    EXPECT_NEAR(_actual.b.y, _expected.b.y, _tolerance) << "b.y is different"; \
  }

TEST_F(ColorManagementTest, sRGBChromaticity) {
  const ColorEncoding sRGB = ColorEncoding::SRGB();
  EXPECT_CIEXY_NEAR(sRGB.GetWhitePoint(), CIExy(0.3127, 0.3290), 1e-4);
  PrimariesCIExy srgb_primaries = {{0.64, 0.33}, {0.30, 0.60}, {0.15, 0.06}};
  EXPECT_PRIMARIES_NEAR(sRGB.GetPrimaries(), srgb_primaries, 1e-4);
}

TEST_F(ColorManagementTest, D2700Chromaticity) {
  std::vector<uint8_t> icc_data =
      jxl::test::ReadTestData("jxl/color_management/sRGB-D2700.icc");
  IccBytes icc;
  Bytes(icc_data).AppendTo(icc);
  ColorEncoding sRGB_D2700;
  ASSERT_TRUE(sRGB_D2700.SetICC(std::move(icc), JxlGetDefaultCms()));

  EXPECT_CIEXY_NEAR(sRGB_D2700.GetWhitePoint(), CIExy(0.45986, 0.41060), 1e-4);
  // The illuminant-relative chromaticities of this profile's primaries are the
  // same as for sRGB. It is the PCS-relative chromaticities that would be
  // different.
  PrimariesCIExy srgb_primaries = {{0.64, 0.33}, {0.30, 0.60}, {0.15, 0.06}};
  EXPECT_PRIMARIES_NEAR(sRGB_D2700.GetPrimaries(), srgb_primaries, 1e-4);
}

TEST_F(ColorManagementTest, D2700ToSRGB) {
  std::vector<uint8_t> icc_data =
      jxl::test::ReadTestData("jxl/color_management/sRGB-D2700.icc");
  IccBytes icc;
  Bytes(icc_data).AppendTo(icc);
  ColorEncoding sRGB_D2700;
  ASSERT_TRUE(sRGB_D2700.SetICC(std::move(icc), JxlGetDefaultCms()));

  ColorSpaceTransform transform(*JxlGetDefaultCms());
  ASSERT_TRUE(transform.Init(sRGB_D2700, ColorEncoding::SRGB(),
                             kDefaultIntensityTarget, 1, 1));
  Color sRGB_D2700_values{0.863, 0.737, 0.490};
  Color sRGB_values;
  ASSERT_TRUE(
      transform.Run(0, sRGB_D2700_values.data(), sRGB_values.data(), 1));
  Color sRGB_expected{0.914, 0.745, 0.601};
  EXPECT_ARRAY_NEAR(sRGB_values, sRGB_expected, 1e-3);
}

TEST_F(ColorManagementTest, P3HlgTo2020Hlg) {
  ColorEncoding p3_hlg;
  p3_hlg.SetColorSpace(ColorSpace::kRGB);
  ASSERT_TRUE(p3_hlg.SetWhitePointType(WhitePoint::kD65));
  ASSERT_TRUE(p3_hlg.SetPrimariesType(Primaries::kP3));
  p3_hlg.Tf().SetTransferFunction(TransferFunction::kHLG);
  ASSERT_TRUE(p3_hlg.CreateICC());

  ColorEncoding rec2020_hlg = p3_hlg;
  ASSERT_TRUE(rec2020_hlg.SetPrimariesType(Primaries::k2100));
  ASSERT_TRUE(rec2020_hlg.CreateICC());

  ColorSpaceTransform transform(*JxlGetDefaultCms());
  ASSERT_TRUE(transform.Init(p3_hlg, rec2020_hlg, 1000, 1, 1));
  Color p3_hlg_values{0., 0.75, 0.};
  Color rec2020_hlg_values;
  ASSERT_TRUE(
      transform.Run(0, p3_hlg_values.data(), rec2020_hlg_values.data(), 1));
  Color rec2020_hlg_expected{0.3973, 0.7382, 0.1183};
  EXPECT_ARRAY_NEAR(rec2020_hlg_values, rec2020_hlg_expected, 1e-4);
}

TEST_F(ColorManagementTest, HlgOotf) {
  ColorEncoding p3_hlg;
  p3_hlg.SetColorSpace(ColorSpace::kRGB);
  ASSERT_TRUE(p3_hlg.SetWhitePointType(WhitePoint::kD65));
  ASSERT_TRUE(p3_hlg.SetPrimariesType(Primaries::kP3));
  p3_hlg.Tf().SetTransferFunction(TransferFunction::kHLG);
  ASSERT_TRUE(p3_hlg.CreateICC());

  ColorSpaceTransform transform_to_1000(*JxlGetDefaultCms());
  ASSERT_TRUE(
      transform_to_1000.Init(p3_hlg, ColorEncoding::LinearSRGB(), 1000, 1, 1));
  // HDR reference white: https://www.itu.int/pub/R-REP-BT.2408-4-2021
  Color p3_hlg_values{0.75, 0.75, 0.75};
  Color linear_srgb_values;
  ASSERT_TRUE(transform_to_1000.Run(0, p3_hlg_values.data(),
                                    linear_srgb_values.data(), 1));
  // On a 1000-nit display, HDR reference white should be 203 cd/m² which is
  // 0.203 times the maximum.
  EXPECT_ARRAY_NEAR(linear_srgb_values, (Color{0.203, 0.203, 0.203}), 1e-3);

  ColorSpaceTransform transform_to_400(*JxlGetDefaultCms());
  ASSERT_TRUE(
      transform_to_400.Init(p3_hlg, ColorEncoding::LinearSRGB(), 400, 1, 1));
  ASSERT_TRUE(transform_to_400.Run(0, p3_hlg_values.data(),
                                   linear_srgb_values.data(), 1));
  // On a 400-nit display, it should be 100 cd/m².
  EXPECT_ARRAY_NEAR(linear_srgb_values, (Color{0.250, 0.250, 0.250}), 1e-3);

  p3_hlg_values[2] = 0.50;
  ASSERT_TRUE(transform_to_1000.Run(0, p3_hlg_values.data(),
                                    linear_srgb_values.data(), 1));
  EXPECT_ARRAY_NEAR(linear_srgb_values, (Color{0.201, 0.201, 0.050}), 1e-3);

  ColorSpaceTransform transform_from_400(*JxlGetDefaultCms());
  ASSERT_TRUE(
      transform_from_400.Init(ColorEncoding::LinearSRGB(), p3_hlg, 400, 1, 1));
  linear_srgb_values[0] = linear_srgb_values[1] = linear_srgb_values[2] = 0.250;
  ASSERT_TRUE(transform_from_400.Run(0, linear_srgb_values.data(),
                                     p3_hlg_values.data(), 1));
  EXPECT_ARRAY_NEAR(p3_hlg_values, (Color{0.75, 0.75, 0.75}), 1e-3);

  ColorEncoding grayscale_hlg;
  grayscale_hlg.SetColorSpace(ColorSpace::kGray);
  ASSERT_TRUE(grayscale_hlg.SetWhitePointType(WhitePoint::kD65));
  grayscale_hlg.Tf().SetTransferFunction(TransferFunction::kHLG);
  ASSERT_TRUE(grayscale_hlg.CreateICC());

  ColorSpaceTransform grayscale_transform(*JxlGetDefaultCms());
  ASSERT_TRUE(grayscale_transform.Init(
      grayscale_hlg, ColorEncoding::LinearSRGB(/*is_gray=*/true), 1000, 1, 1));
  const float grayscale_hlg_value = 0.75;
  float linear_grayscale_value;
  ASSERT_TRUE(grayscale_transform.Run(0, &grayscale_hlg_value,
                                      &linear_grayscale_value, 1));
  EXPECT_NEAR(linear_grayscale_value, 0.203, 1e-3);
}

TEST_F(ColorManagementTest, XYBProfile) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  ColorEncoding c_xyb;
  c_xyb.SetColorSpace(ColorSpace::kXYB);
  c_xyb.SetRenderingIntent(RenderingIntent::kPerceptual);
  ASSERT_TRUE(c_xyb.CreateICC());
  ColorEncoding c_native = ColorEncoding::LinearSRGB(false);

  static const size_t kGridDim = 17;
  static const size_t kNumColors = kGridDim * kGridDim * kGridDim;
  const JxlCmsInterface& cms = *JxlGetDefaultCms();
  ColorSpaceTransform xform(cms);
  ASSERT_TRUE(
      xform.Init(c_xyb, c_native, kDefaultIntensityTarget, kNumColors, 1));

  ImageMetadata metadata;
  metadata.color_encoding = c_native;
  ImageBundle ib(memory_manager, &metadata);
  JXL_ASSIGN_OR_DIE(Image3F native,
                    Image3F::Create(memory_manager, kNumColors, 1));
  float mul = 1.0f / (kGridDim - 1);
  for (size_t ir = 0, x = 0; ir < kGridDim; ++ir) {
    for (size_t ig = 0; ig < kGridDim; ++ig) {
      for (size_t ib = 0; ib < kGridDim; ++ib, ++x) {
        native.PlaneRow(0, 0)[x] = ir * mul;
        native.PlaneRow(1, 0)[x] = ig * mul;
        native.PlaneRow(2, 0)[x] = ib * mul;
      }
    }
  }
  ib.SetFromImage(std::move(native), c_native);
  const Image3F& in = *ib.color();
  JXL_ASSIGN_OR_DIE(Image3F opsin,
                    Image3F::Create(memory_manager, kNumColors, 1));
  JXL_CHECK(ToXYB(ib, nullptr, &opsin, cms, nullptr));

  JXL_ASSIGN_OR_DIE(Image3F opsin2,
                    Image3F::Create(memory_manager, kNumColors, 1));
  CopyImageTo(opsin, &opsin2);
  ScaleXYB(&opsin2);

  float* src = xform.BufSrc(0);
  for (size_t i = 0; i < kNumColors; ++i) {
    for (size_t c = 0; c < 3; ++c) {
      src[3 * i + c] = opsin2.PlaneRow(c, 0)[i];
    }
  }

  float* dst = xform.BufDst(0);
  ASSERT_TRUE(xform.Run(0, src, dst, kNumColors));

  JXL_ASSIGN_OR_DIE(Image3F out,
                    Image3F::Create(memory_manager, kNumColors, 1));
  for (size_t i = 0; i < kNumColors; ++i) {
    for (size_t c = 0; c < 3; ++c) {
      out.PlaneRow(c, 0)[i] = dst[3 * i + c];
    }
  }

  auto debug_print_color = [&](size_t i) {
    printf(
        "(%f, %f, %f) -> (%9.6f, %f, %f) -> (%f, %f, %f) -> "
        "(%9.6f, %9.6f, %9.6f)",
        in.PlaneRow(0, 0)[i], in.PlaneRow(1, 0)[i], in.PlaneRow(2, 0)[i],
        opsin.PlaneRow(0, 0)[i], opsin.PlaneRow(1, 0)[i],
        opsin.PlaneRow(2, 0)[i], opsin2.PlaneRow(0, 0)[i],
        opsin2.PlaneRow(1, 0)[i], opsin2.PlaneRow(2, 0)[i],
        out.PlaneRow(0, 0)[i], out.PlaneRow(1, 0)[i], out.PlaneRow(2, 0)[i]);
  };

  float max_err[3] = {};
  size_t max_err_i[3] = {};
  for (size_t i = 0; i < kNumColors; ++i) {
    for (size_t c = 0; c < 3; ++c) {
      // debug_print_color(i); printf("\n");
      float err = std::abs(in.PlaneRow(c, 0)[i] - out.PlaneRow(c, 0)[i]);
      if (err > max_err[c]) {
        max_err[c] = err;
        max_err_i[c] = i;
      }
    }
  }
  static float kMaxError[3] = {8.7e-4, 4.4e-4, 5.2e-4};
  printf("Maximum errors:\n");
  for (size_t c = 0; c < 3; ++c) {
    debug_print_color(max_err_i[c]);
    printf("    %f\n", max_err[c]);
    EXPECT_LT(max_err[c], kMaxError[c]);
  }
}

TEST_F(ColorManagementTest, GoldenXYBCube) {
  std::vector<int32_t> actual;
  const jxl::cms::ColorCube3D& cube = jxl::cms::UnscaledA2BCube();
  for (size_t ix = 0; ix < 2; ++ix) {
    for (size_t iy = 0; iy < 2; ++iy) {
      for (size_t ib = 0; ib < 2; ++ib) {
        const jxl::cms::ColorCube0D& out_f = cube[ix][iy][ib];
        for (int i = 0; i < 3; ++i) {
          int32_t val = static_cast<int32_t>(std::lroundf(65535 * out_f[i]));
          ASSERT_TRUE(val >= 0 && val <= 65535);
          actual.push_back(val);
        }
      }
    }
  }

  std::vector<int32_t> expected = {0,     3206,  0,     0,     3206,  28873,
                                   62329, 65535, 36662, 62329, 65535, 65535,
                                   3206,  0,     0,     3206,  0,     28873,
                                   65535, 62329, 36662, 65535, 62329, 65535};
  EXPECT_EQ(actual, expected);
}

}  // namespace
}  // namespace jxl
