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

#include "lib/extras/codec.h"

#include <stddef.h>
#include <stdio.h>

#include <algorithm>
#include <random>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "lib/extras/codec_pgx.h"
#include "lib/extras/codec_pnm.h"
#include "lib/jxl/base/thread_pool_internal.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/image_test_utils.h"
#include "lib/jxl/luminance.h"
#include "lib/jxl/testdata.h"

namespace jxl {
namespace {

CodecInOut CreateTestImage(const size_t xsize, const size_t ysize,
                           const bool is_gray, const bool add_alpha,
                           const size_t bits_per_sample,
                           const ColorEncoding& c_native) {
  Image3F image(xsize, ysize);
  std::mt19937_64 rng(129);
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  if (is_gray) {
    for (size_t y = 0; y < ysize; ++y) {
      float* JXL_RESTRICT row0 = image.PlaneRow(0, y);
      float* JXL_RESTRICT row1 = image.PlaneRow(1, y);
      float* JXL_RESTRICT row2 = image.PlaneRow(2, y);
      for (size_t x = 0; x < xsize; ++x) {
        row0[x] = row1[x] = row2[x] = dist(rng);
      }
    }
  } else {
    RandomFillImage(&image, 1.0f);
  }
  CodecInOut io;

  if (bits_per_sample == 32) {
    io.metadata.m.SetFloat32Samples();
  } else {
    io.metadata.m.SetUintSamples(bits_per_sample);
  }
  io.metadata.m.color_encoding = c_native;
  io.SetFromImage(std::move(image), c_native);
  if (add_alpha) {
    ImageF alpha(xsize, ysize);
    RandomFillImage(&alpha, 1.f);
    io.metadata.m.SetAlphaBits(bits_per_sample <= 8 ? 8 : 16);
    io.Main().SetAlpha(std::move(alpha), /*alpha_is_premultiplied=*/false);
  }
  return io;
}

// Ensures reading a newly written file leads to the same image pixels.
void TestRoundTrip(Codec codec, const size_t xsize, const size_t ysize,
                   const bool is_gray, const bool add_alpha,
                   const size_t bits_per_sample, ThreadPool* pool) {
  // JPEG encoding is not lossless.
  if (codec == Codec::kJPG) return;
  if (codec == Codec::kPNM && add_alpha) return;
  // Our EXR codec always uses 16-bit premultiplied alpha, does not support
  // grayscale, and somehow does not have sufficient precision for this test.
  if (codec == Codec::kEXR) return;
  printf("Codec %s bps:%zu gr:%d al:%d\n",
         ExtensionFromCodec(codec, is_gray, bits_per_sample).c_str(),
         bits_per_sample, is_gray, add_alpha);

  ColorEncoding c_native;
  c_native.SetColorSpace(is_gray ? ColorSpace::kGray : ColorSpace::kRGB);
  // Note: this must not be wider than c_external, otherwise gamut clipping
  // will cause large round-trip errors.
  c_native.primaries = Primaries::kP3;
  c_native.tf.SetTransferFunction(TransferFunction::kLinear);
  JXL_CHECK(c_native.CreateICC());

  // Generally store same color space to reduce round trip errors..
  ColorEncoding c_external = c_native;
  // .. unless we have enough precision for some transforms.
  if (bits_per_sample >= 16) {
    c_external.white_point = WhitePoint::kE;
    c_external.primaries = Primaries::k2100;
    c_external.tf.SetTransferFunction(TransferFunction::kSRGB);
  }
  JXL_CHECK(c_external.CreateICC());

  const CodecInOut io = CreateTestImage(xsize, ysize, is_gray, add_alpha,
                                        bits_per_sample, c_native);
  const ImageBundle& ib1 = io.Main();

  PaddedBytes encoded;
  JXL_CHECK(Encode(io, codec, c_external, bits_per_sample, &encoded, pool));

  CodecInOut io2;
  io2.target_nits = io.metadata.m.IntensityTarget();
  // Only for PNM because PNG will warn about ignoring them.
  if (codec == Codec::kPNM) {
    io2.dec_hints.Add("color_space", Description(c_external));
  }
  JXL_CHECK(SetFromBytes(Span<const uint8_t>(encoded), &io2, pool));
  ImageBundle& ib2 = io2.Main();

  EXPECT_EQ(Description(c_external),
            Description(io2.metadata.m.color_encoding));

  // See c_external above - for low bits_per_sample the encoded space is
  // already the same.
  if (bits_per_sample < 16) {
    EXPECT_EQ(Description(ib1.c_current()), Description(ib2.c_current()));
  }

  if (add_alpha) {
    EXPECT_TRUE(SamePixels(ib1.alpha(), *ib2.alpha()));
  }

  JXL_CHECK(ib2.TransformTo(ib1.c_current(), pool));

  double max_l1, max_rel;
  // Round-trip tolerances must be higher than in external_image_test because
  // codecs do not support unbounded ranges.
#if JPEGXL_ENABLE_SKCMS
  if (bits_per_sample <= 12) {
    max_l1 = 0.5;
    max_rel = 6E-3;
  } else {
    max_l1 = 1E-3;
    max_rel = 5E-4;
  }
#else  // JPEGXL_ENABLE_SKCMS
  if (bits_per_sample <= 12) {
    max_l1 = 0.5;
    max_rel = 6E-3;
  } else if (bits_per_sample == 16) {
    max_l1 = 3E-3;
    max_rel = 1E-4;
  } else {
#ifdef __ARM_ARCH
    // pow() implementation in arm is a bit less precise than in x86 and
    // therefore we need a bigger error margin in this case.
    max_l1 = 1E-7;
    max_rel = 1E-4;
#else
    max_l1 = 1E-7;
    max_rel = 1E-5;
#endif
  }
#endif  // JPEGXL_ENABLE_SKCMS

  VerifyRelativeError(ib1.color(), *ib2.color(), max_l1, max_rel);
}

#if 0
TEST(CodecTest, TestRoundTrip) {
  ThreadPoolInternal pool(12);

  const size_t xsize = 7;
  const size_t ysize = 4;

  for (Codec codec : Values<Codec>()) {
    for (int bits_per_sample : {8, 10, 12, 16, 32}) {
      for (bool is_gray : {false, true}) {
        for (bool add_alpha : {false, true}) {
          TestRoundTrip(codec, xsize, ysize, is_gray, add_alpha,
                        static_cast<size_t>(bits_per_sample), &pool);
        }
      }
    }
  }
}
#endif

CodecInOut DecodeRoundtrip(const std::string& pathname, Codec expected_codec,
                           ThreadPool* pool,
                           const DecoderHints& dec_hints = DecoderHints()) {
  CodecInOut io;
  io.dec_hints = dec_hints;
  const PaddedBytes orig = ReadTestData(pathname);
  JXL_CHECK(SetFromBytes(Span<const uint8_t>(orig), &io, pool));
  const ImageBundle& ib1 = io.Main();

  // Encode/Decode again to make sure Encode carries through all metadata.
  PaddedBytes encoded;
  JXL_CHECK(Encode(io, expected_codec, io.metadata.m.color_encoding,
                   io.metadata.m.bit_depth.bits_per_sample, &encoded, pool));

  CodecInOut io2;
  io2.dec_hints = dec_hints;
  JXL_CHECK(SetFromBytes(Span<const uint8_t>(encoded), &io2, pool));
  const ImageBundle& ib2 = io2.Main();
  EXPECT_EQ(Description(ib1.metadata()->color_encoding),
            Description(ib2.metadata()->color_encoding));
  EXPECT_EQ(Description(ib1.c_current()), Description(ib2.c_current()));

  size_t bits_per_sample = io2.metadata.m.bit_depth.bits_per_sample;

  // "Same" pixels?
  double max_l1 = bits_per_sample <= 12 ? 1.3 : 2E-3;
  double max_rel = bits_per_sample <= 12 ? 6E-3 : 1E-4;
  if (ib1.metadata()->color_encoding.IsGray()) {
    max_rel *= 2.0;
  } else if (ib1.metadata()->color_encoding.primaries != Primaries::kSRGB) {
    // Need more tolerance for large gamuts (anything but sRGB)
    max_l1 *= 1.5;
    max_rel *= 3.0;
  }
  VerifyRelativeError(ib1.color(), ib2.color(), max_l1, max_rel);

  // Simulate the encoder removing profile and decoder restoring it.
  if (!ib2.metadata()->color_encoding.WantICC()) {
    io2.metadata.m.color_encoding.InternalRemoveICC();
    EXPECT_TRUE(io2.metadata.m.color_encoding.CreateICC());
  }

  return io2;
}

#if 0
TEST(CodecTest, TestMetadataSRGB) {
  ThreadPoolInternal pool(12);

  const char* paths[] = {"raw.pixls/DJI-FC6310-16bit_srgb8_v4_krita.png",
                         "raw.pixls/Google-Pixel2XL-16bit_srgb8_v4_krita.png",
                         "raw.pixls/HUAWEI-EVA-L09-16bit_srgb8_dt.png",
                         "raw.pixls/Nikon-D300-12bit_srgb8_dt.png",
                         "raw.pixls/Sony-DSC-RX1RM2-14bit_srgb8_v4_krita.png"};
  for (const char* relative_pathname : paths) {
    const CodecInOut io =
        DecodeRoundtrip(relative_pathname, Codec::kPNG, &pool);
    EXPECT_EQ(8, io.metadata.m.bit_depth.bits_per_sample);
    EXPECT_FALSE(io.metadata.m.bit_depth.floating_point_sample);
    EXPECT_EQ(0, io.metadata.m.bit_depth.exponent_bits_per_sample);

    EXPECT_EQ(64, io.xsize());
    EXPECT_EQ(64, io.ysize());
    EXPECT_FALSE(io.metadata.m.HasAlpha());

    const ColorEncoding& c_original = io.metadata.m.color_encoding;
    EXPECT_FALSE(c_original.ICC().empty());
    EXPECT_EQ(ColorSpace::kRGB, c_original.GetColorSpace());
    EXPECT_EQ(WhitePoint::kD65, c_original.white_point);
    EXPECT_EQ(Primaries::kSRGB, c_original.primaries);
    EXPECT_TRUE(c_original.tf.IsSRGB());
  }
}

TEST(CodecTest, TestMetadataLinear) {
  ThreadPoolInternal pool(12);

  const char* paths[3] = {
      "raw.pixls/Google-Pixel2XL-16bit_acescg_g1_v4_krita.png",
      "raw.pixls/HUAWEI-EVA-L09-16bit_709_g1_dt.png",
      "raw.pixls/Nikon-D300-12bit_2020_g1_dt.png",
  };
  const WhitePoint white_points[3] = {WhitePoint::kCustom, WhitePoint::kD65,
                                      WhitePoint::kD65};
  const Primaries primaries[3] = {Primaries::kCustom, Primaries::kSRGB,
                                  Primaries::k2100};

  for (size_t i = 0; i < 3; ++i) {
    const CodecInOut io = DecodeRoundtrip(paths[i], Codec::kPNG, &pool);
    EXPECT_EQ(16, io.metadata.m.bit_depth.bits_per_sample);
    EXPECT_FALSE(io.metadata.m.bit_depth.floating_point_sample);
    EXPECT_EQ(0, io.metadata.m.bit_depth.exponent_bits_per_sample);

    EXPECT_EQ(64, io.xsize());
    EXPECT_EQ(64, io.ysize());
    EXPECT_FALSE(io.metadata.m.HasAlpha());

    const ColorEncoding& c_original = io.metadata.m.color_encoding;
    EXPECT_FALSE(c_original.ICC().empty());
    EXPECT_EQ(ColorSpace::kRGB, c_original.GetColorSpace());
    EXPECT_EQ(white_points[i], c_original.white_point);
    EXPECT_EQ(primaries[i], c_original.primaries);
    EXPECT_TRUE(c_original.tf.IsLinear());
  }
}

TEST(CodecTest, TestMetadataICC) {
  ThreadPoolInternal pool(12);

  const char* paths[] = {
      "raw.pixls/DJI-FC6310-16bit_709_v4_krita.png",
      "raw.pixls/Sony-DSC-RX1RM2-14bit_709_v4_krita.png",
  };
  for (const char* relative_pathname : paths) {
    const CodecInOut io =
        DecodeRoundtrip(relative_pathname, Codec::kPNG, &pool);
    EXPECT_GE(16, io.metadata.m.bit_depth.bits_per_sample);
    EXPECT_LE(14, io.metadata.m.bit_depth.bits_per_sample);

    EXPECT_EQ(64, io.xsize());
    EXPECT_EQ(64, io.ysize());
    EXPECT_FALSE(io.metadata.m.HasAlpha());

    const ColorEncoding& c_original = io.metadata.m.color_encoding;
    EXPECT_FALSE(c_original.ICC().empty());
    EXPECT_EQ(RenderingIntent::kPerceptual, c_original.rendering_intent);
    EXPECT_EQ(ColorSpace::kRGB, c_original.GetColorSpace());
    EXPECT_EQ(WhitePoint::kD65, c_original.white_point);
    EXPECT_EQ(Primaries::kSRGB, c_original.primaries);
    EXPECT_EQ(TransferFunction::k709, c_original.tf.GetTransferFunction());
  }
}

TEST(CodecTest, TestPNGSuite) {
  ThreadPoolInternal pool(12);

  // Ensure we can load PNG with text, japanese UTF-8, compressed text.
  (void)DecodeRoundtrip("pngsuite/ct1n0g04.png", Codec::kPNG, &pool);
  (void)DecodeRoundtrip("pngsuite/ctjn0g04.png", Codec::kPNG, &pool);
  (void)DecodeRoundtrip("pngsuite/ctzn0g04.png", Codec::kPNG, &pool);

  // Extract gAMA
  const CodecInOut b1 =
      DecodeRoundtrip("pngsuite/g10n3p04.png", Codec::kPNG, &pool);
  EXPECT_TRUE(b1.metadata.color_encoding.tf.IsLinear());

  // Extract cHRM
  const CodecInOut b_p =
      DecodeRoundtrip("pngsuite/ccwn2c08.png", Codec::kPNG, &pool);
  EXPECT_EQ(Primaries::kSRGB, b_p.metadata.color_encoding.primaries);
  EXPECT_EQ(WhitePoint::kD65, b_p.metadata.color_encoding.white_point);

  // Extract EXIF from (new-style) dedicated chunk
  const CodecInOut b_exif =
      DecodeRoundtrip("pngsuite/exif2c08.png", Codec::kPNG, &pool);
  EXPECT_EQ(978, b_exif.blobs.exif.size());
}
#endif

void VerifyWideGamutMetadata(const std::string& relative_pathname,
                             const Primaries primaries, ThreadPool* pool) {
  const CodecInOut io = DecodeRoundtrip(relative_pathname, Codec::kPNG, pool);

  EXPECT_EQ(8, io.metadata.m.bit_depth.bits_per_sample);
  EXPECT_FALSE(io.metadata.m.bit_depth.floating_point_sample);
  EXPECT_EQ(0, io.metadata.m.bit_depth.exponent_bits_per_sample);

  const ColorEncoding& c_original = io.metadata.m.color_encoding;
  EXPECT_FALSE(c_original.ICC().empty());
  EXPECT_EQ(RenderingIntent::kAbsolute, c_original.rendering_intent);
  EXPECT_EQ(ColorSpace::kRGB, c_original.GetColorSpace());
  EXPECT_EQ(WhitePoint::kD65, c_original.white_point);
  EXPECT_EQ(primaries, c_original.primaries);
}

TEST(CodecTest, TestWideGamut) {
  ThreadPoolInternal pool(12);
  // VerifyWideGamutMetadata("wide-gamut-tests/P3-sRGB-color-bars.png",
  //                        Primaries::kP3, &pool);
  VerifyWideGamutMetadata("wide-gamut-tests/P3-sRGB-color-ring.png",
                          Primaries::kP3, &pool);
  // VerifyWideGamutMetadata("wide-gamut-tests/R2020-sRGB-color-bars.png",
  //                        Primaries::k2100, &pool);
  // VerifyWideGamutMetadata("wide-gamut-tests/R2020-sRGB-color-ring.png",
  //                        Primaries::k2100, &pool);
}

TEST(CodecTest, TestPNM) { TestCodecPNM(); }
TEST(CodecTest, TestPGX) { TestCodecPGX(); }

}  // namespace
}  // namespace jxl
