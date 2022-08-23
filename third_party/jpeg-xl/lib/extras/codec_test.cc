// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/codec.h"

#include <stddef.h>
#include <stdio.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "lib/extras/dec/pgx.h"
#include "lib/extras/dec/pnm.h"
#include "lib/extras/enc/encode.h"
#include "lib/extras/packed_image_convert.h"
#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/base/random.h"
#include "lib/jxl/base/thread_pool_internal.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/enc_color_management.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/image_test_utils.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testdata.h"

namespace jxl {
namespace extras {
namespace {

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::NotNull;
using ::testing::SizeIs;

std::string ExtensionFromCodec(Codec codec, const bool is_gray,
                               const bool has_alpha,
                               const size_t bits_per_sample) {
  switch (codec) {
    case Codec::kJPG:
      return ".jpg";
    case Codec::kPGX:
      return ".pgx";
    case Codec::kPNG:
      return ".png";
    case Codec::kPNM:
      if (has_alpha) return ".pam";
      if (is_gray) return ".pgm";
      return (bits_per_sample == 32) ? ".pfm" : ".ppm";
    case Codec::kGIF:
      return ".gif";
    case Codec::kEXR:
      return ".exr";
    case Codec::kUnknown:
      return std::string();
  }
  JXL_UNREACHABLE;
  return std::string();
}

void VerifySameImage(const PackedImage& im0, size_t bits_per_sample0,
                     const PackedImage& im1, size_t bits_per_sample1,
                     bool lossless = true) {
  ASSERT_EQ(im0.xsize, im1.xsize);
  ASSERT_EQ(im0.ysize, im1.ysize);
  ASSERT_EQ(im0.format.num_channels, im1.format.num_channels);
  auto get_factor = [](JxlPixelFormat f, size_t bits) -> double {
    return 1.0 / ((1u << std::min(test::GetPrecision(f.data_type), bits)) - 1);
  };
  double factor0 = get_factor(im0.format, bits_per_sample0);
  double factor1 = get_factor(im1.format, bits_per_sample1);
  auto pixels0 = static_cast<const uint8_t*>(im0.pixels());
  auto pixels1 = static_cast<const uint8_t*>(im1.pixels());
  auto rgba0 =
      test::ConvertToRGBA32(pixels0, im0.xsize, im0.ysize, im0.format, factor0);
  auto rgba1 =
      test::ConvertToRGBA32(pixels1, im1.xsize, im1.ysize, im1.format, factor1);
  double tolerance =
      lossless ? 0.5 * std::min(factor0, factor1) : 3.0f / 255.0f;
  if (bits_per_sample0 == 32 || bits_per_sample1 == 32) {
    tolerance = 0.5 * std::max(factor0, factor1);
  }
  for (size_t y = 0; y < im0.ysize; ++y) {
    for (size_t x = 0; x < im0.xsize; ++x) {
      for (size_t c = 0; c < im0.format.num_channels; ++c) {
        size_t ix = (y * im0.xsize + x) * 4 + c;
        double val0 = rgba0[ix];
        double val1 = rgba1[ix];
        ASSERT_NEAR(val1, val0, tolerance)
            << "y = " << y << " x = " << x << " c = " << c;
      }
    }
  }
}

JxlColorEncoding CreateTestColorEncoding(bool is_gray) {
  JxlColorEncoding c;
  c.color_space = is_gray ? JXL_COLOR_SPACE_GRAY : JXL_COLOR_SPACE_RGB;
  c.white_point = JXL_WHITE_POINT_D65;
  c.primaries = JXL_PRIMARIES_P3;
  c.rendering_intent = JXL_RENDERING_INTENT_RELATIVE;
  c.transfer_function = JXL_TRANSFER_FUNCTION_LINEAR;
  // Roundtrip through internal color encoding to fill in primaries and white
  // point CIE xy coordinates.
  ColorEncoding c_internal;
  JXL_CHECK(ConvertExternalToInternalColorEncoding(c, &c_internal));
  ConvertInternalToExternalColorEncoding(c_internal, &c);
  return c;
}

std::vector<uint8_t> GenerateICC(JxlColorEncoding color_encoding) {
  ColorEncoding c;
  JXL_CHECK(ConvertExternalToInternalColorEncoding(color_encoding, &c));
  JXL_CHECK(c.CreateICC());
  PaddedBytes icc = c.ICC();
  return std::vector<uint8_t>(icc.begin(), icc.end());
}

void StoreRandomValue(uint8_t* out, Rng* rng, JxlPixelFormat format,
                      size_t bits_per_sample) {
  uint64_t max_val = (1ull << bits_per_sample) - 1;
  if (format.data_type == JXL_TYPE_UINT8) {
    *out = rng->UniformU(0, max_val);
  } else if (format.data_type == JXL_TYPE_UINT16) {
    uint32_t val = rng->UniformU(0, max_val);
    if (format.endianness == JXL_BIG_ENDIAN) {
      StoreBE16(val, out);
    } else {
      StoreLE16(val, out);
    }
  } else {
    ASSERT_EQ(format.data_type, JXL_TYPE_FLOAT);
    float val = rng->UniformF(0.0, 1.0);
    uint32_t uval;
    memcpy(&uval, &val, 4);
    if (format.endianness == JXL_BIG_ENDIAN) {
      StoreBE32(uval, out);
    } else {
      StoreLE32(uval, out);
    }
  }
}

void FillPackedImage(size_t bits_per_sample, PackedImage* image) {
  JxlPixelFormat format = image->format;
  size_t bytes_per_channel = PackedImage::BitsPerChannel(format.data_type) / 8;
  uint8_t* out = static_cast<uint8_t*>(image->pixels());
  size_t stride = image->xsize * format.num_channels * bytes_per_channel;
  ASSERT_EQ(image->pixels_size, image->ysize * stride);
  Rng rng(129);
  for (size_t y = 0; y < image->ysize; ++y) {
    for (size_t x = 0; x < image->xsize; ++x) {
      for (size_t c = 0; c < format.num_channels; ++c) {
        StoreRandomValue(out, &rng, format, bits_per_sample);
        out += bytes_per_channel;
      }
    }
  }
}

struct TestImageParams {
  Codec codec;
  size_t xsize;
  size_t ysize;
  size_t bits_per_sample;
  bool is_gray;
  bool add_alpha;
  bool big_endian;

  bool ShouldTestRoundtrip() const {
    if (codec == Codec::kPNG) {
      return true;
    } else if (codec == Codec::kPNM) {
      // TODO(szabadka) Make PNM encoder endianness-aware.
      return ((bits_per_sample <= 16 && big_endian) ||
              (bits_per_sample == 32 && !add_alpha && !big_endian));
    } else if (codec == Codec::kPGX) {
      return ((bits_per_sample == 8 || bits_per_sample == 16) && is_gray &&
              !add_alpha);
    } else if (codec == Codec::kEXR) {
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(THREAD_SANITIZER)
      // OpenEXR 2.3 has a memory leak in IlmThread_2_3::ThreadPool
      return false;
#else
      return bits_per_sample == 32 && !is_gray;
#endif
    } else if (codec == Codec::kJPG) {
      return bits_per_sample == 8 && !add_alpha;
    } else {
      return false;
    }
  }

  JxlPixelFormat PixelFormat() const {
    JxlPixelFormat format;
    format.num_channels = (is_gray ? 1 : 3) + (add_alpha ? 1 : 0);
    format.data_type = (bits_per_sample == 32 ? JXL_TYPE_FLOAT
                        : bits_per_sample > 8 ? JXL_TYPE_UINT16
                                              : JXL_TYPE_UINT8);
    format.endianness = big_endian ? JXL_BIG_ENDIAN : JXL_LITTLE_ENDIAN;
    format.align = 0;
    return format;
  }

  std::string DebugString() const {
    std::ostringstream os;
    os << "bps:" << bits_per_sample << " gr:" << is_gray << " al:" << add_alpha
       << " be: " << big_endian;
    return os.str();
  }
};

void CreateTestImage(const TestImageParams& params, PackedPixelFile* ppf) {
  ppf->info.xsize = params.xsize;
  ppf->info.ysize = params.ysize;
  ppf->info.bits_per_sample = params.bits_per_sample;
  ppf->info.exponent_bits_per_sample = params.bits_per_sample == 32 ? 8 : 0;
  ppf->info.num_color_channels = params.is_gray ? 1 : 3;
  ppf->info.alpha_bits = params.add_alpha ? params.bits_per_sample : 0;
  ppf->info.alpha_premultiplied = (params.codec == Codec::kEXR);

  JxlColorEncoding color_encoding = CreateTestColorEncoding(params.is_gray);
  ppf->icc = GenerateICC(color_encoding);
  ppf->color_encoding = color_encoding;

  PackedFrame frame(params.xsize, params.ysize, params.PixelFormat());
  FillPackedImage(params.bits_per_sample, &frame.color);
  ppf->frames.emplace_back(std::move(frame));
}

// Ensures reading a newly written file leads to the same image pixels.
void TestRoundTrip(const TestImageParams& params, ThreadPool* pool) {
  if (!params.ShouldTestRoundtrip()) return;

  std::string extension = ExtensionFromCodec(
      params.codec, params.is_gray, params.add_alpha, params.bits_per_sample);
  printf("Codec %s %s\n", extension.c_str(), params.DebugString().c_str());

  PackedPixelFile ppf_in;
  CreateTestImage(params, &ppf_in);

  EncodedImage encoded;
  auto encoder = Encoder::FromExtension(extension);
  ASSERT_TRUE(encoder.get());
  ASSERT_TRUE(encoder->Encode(ppf_in, &encoded, pool));
  ASSERT_EQ(encoded.bitstreams.size(), 1);

  PackedPixelFile ppf_out;
  ASSERT_TRUE(DecodeBytes(Span<const uint8_t>(encoded.bitstreams[0]),
                          ColorHints(), SizeConstraints(), &ppf_out));

  if (params.codec != Codec::kPNM && params.codec != Codec::kPGX &&
      params.codec != Codec::kEXR) {
    EXPECT_EQ(ppf_in.icc, ppf_out.icc);
  }

  ASSERT_EQ(ppf_out.frames.size(), 1);
  VerifySameImage(ppf_in.frames[0].color, ppf_in.info.bits_per_sample,
                  ppf_out.frames[0].color, ppf_out.info.bits_per_sample,
                  /*lossless=*/params.codec != Codec::kJPG);
}

TEST(CodecTest, TestRoundTrip) {
  ThreadPoolInternal pool(12);

  TestImageParams params;
  params.xsize = 7;
  params.ysize = 4;

  for (Codec codec : AvailableCodecs()) {
    for (int bits_per_sample : {4, 8, 10, 12, 16, 32}) {
      for (bool is_gray : {false, true}) {
        for (bool add_alpha : {false, true}) {
          for (bool big_endian : {false, true}) {
            params.codec = codec;
            params.bits_per_sample = static_cast<size_t>(bits_per_sample);
            params.is_gray = is_gray;
            params.add_alpha = add_alpha;
            params.big_endian = big_endian;
            TestRoundTrip(params, &pool);
          }
        }
      }
    }
  }
}

CodecInOut DecodeRoundtrip(const std::string& pathname, ThreadPool* pool,
                           const ColorHints& color_hints = ColorHints()) {
  CodecInOut io;
  const PaddedBytes orig = ReadTestData(pathname);
  JXL_CHECK(
      SetFromBytes(Span<const uint8_t>(orig), color_hints, &io, pool, nullptr));
  const ImageBundle& ib1 = io.Main();

  // Encode/Decode again to make sure Encode carries through all metadata.
  std::vector<uint8_t> encoded;
  JXL_CHECK(Encode(io, Codec::kPNG, io.metadata.m.color_encoding,
                   io.metadata.m.bit_depth.bits_per_sample, &encoded, pool));

  CodecInOut io2;
  JXL_CHECK(SetFromBytes(Span<const uint8_t>(encoded), color_hints, &io2, pool,
                         nullptr));
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

  const char* paths[] = {"external/raw.pixls/DJI-FC6310-16bit_srgb8_v4_krita.png",
                         "external/raw.pixls/Google-Pixel2XL-16bit_srgb8_v4_krita.png",
                         "external/raw.pixls/HUAWEI-EVA-L09-16bit_srgb8_dt.png",
                         "external/raw.pixls/Nikon-D300-12bit_srgb8_dt.png",
                         "external/raw.pixls/Sony-DSC-RX1RM2-14bit_srgb8_v4_krita.png"};
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
      "external/raw.pixls/Google-Pixel2XL-16bit_acescg_g1_v4_krita.png",
      "external/raw.pixls/HUAWEI-EVA-L09-16bit_709_g1_dt.png",
      "external/raw.pixls/Nikon-D300-12bit_2020_g1_dt.png",
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
      "external/raw.pixls/DJI-FC6310-16bit_709_v4_krita.png",
      "external/raw.pixls/Sony-DSC-RX1RM2-14bit_709_v4_krita.png",
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

TEST(CodecTest, Testexternal/pngsuite) {
  ThreadPoolInternal pool(12);

  // Ensure we can load PNG with text, japanese UTF-8, compressed text.
  (void)DecodeRoundtrip("external/pngsuite/ct1n0g04.png", Codec::kPNG, &pool);
  (void)DecodeRoundtrip("external/pngsuite/ctjn0g04.png", Codec::kPNG, &pool);
  (void)DecodeRoundtrip("external/pngsuite/ctzn0g04.png", Codec::kPNG, &pool);

  // Extract gAMA
  const CodecInOut b1 =
      DecodeRoundtrip("external/pngsuite/g10n3p04.png", Codec::kPNG, &pool);
  EXPECT_TRUE(b1.metadata.color_encoding.tf.IsLinear());

  // Extract cHRM
  const CodecInOut b_p =
      DecodeRoundtrip("external/pngsuite/ccwn2c08.png", Codec::kPNG, &pool);
  EXPECT_EQ(Primaries::kSRGB, b_p.metadata.color_encoding.primaries);
  EXPECT_EQ(WhitePoint::kD65, b_p.metadata.color_encoding.white_point);

  // Extract EXIF from (new-style) dedicated chunk
  const CodecInOut b_exif =
      DecodeRoundtrip("external/pngsuite/exif2c08.png", Codec::kPNG, &pool);
  EXPECT_EQ(978, b_exif.blobs.exif.size());
}
#endif

void VerifyWideGamutMetadata(const std::string& relative_pathname,
                             const Primaries primaries, ThreadPool* pool) {
  const CodecInOut io = DecodeRoundtrip(relative_pathname, pool);

  EXPECT_EQ(8u, io.metadata.m.bit_depth.bits_per_sample);
  EXPECT_FALSE(io.metadata.m.bit_depth.floating_point_sample);
  EXPECT_EQ(0u, io.metadata.m.bit_depth.exponent_bits_per_sample);

  const ColorEncoding& c_original = io.metadata.m.color_encoding;
  EXPECT_FALSE(c_original.ICC().empty());
  EXPECT_EQ(RenderingIntent::kAbsolute, c_original.rendering_intent);
  EXPECT_EQ(ColorSpace::kRGB, c_original.GetColorSpace());
  EXPECT_EQ(WhitePoint::kD65, c_original.white_point);
  EXPECT_EQ(primaries, c_original.primaries);
}

TEST(CodecTest, TestWideGamut) {
  ThreadPoolInternal pool(12);
  // VerifyWideGamutMetadata("external/wide-gamut-tests/P3-sRGB-color-bars.png",
  //                        Primaries::kP3, &pool);
  VerifyWideGamutMetadata("external/wide-gamut-tests/P3-sRGB-color-ring.png",
                          Primaries::kP3, &pool);
  // VerifyWideGamutMetadata("external/wide-gamut-tests/R2020-sRGB-color-bars.png",
  //                        Primaries::k2100, &pool);
  // VerifyWideGamutMetadata("external/wide-gamut-tests/R2020-sRGB-color-ring.png",
  //                        Primaries::k2100, &pool);
}

TEST(CodecTest, TestPNM) { TestCodecPNM(); }

TEST(CodecTest, FormatNegotiation) {
  const std::vector<JxlPixelFormat> accepted_formats = {
      {/*num_channels=*/4,
       /*data_type=*/JXL_TYPE_UINT16,
       /*endianness=*/JXL_NATIVE_ENDIAN,
       /*align=*/0},
      {/*num_channels=*/3,
       /*data_type=*/JXL_TYPE_UINT8,
       /*endianness=*/JXL_NATIVE_ENDIAN,
       /*align=*/0},
      {/*num_channels=*/3,
       /*data_type=*/JXL_TYPE_UINT16,
       /*endianness=*/JXL_NATIVE_ENDIAN,
       /*align=*/0},
      {/*num_channels=*/1,
       /*data_type=*/JXL_TYPE_UINT8,
       /*endianness=*/JXL_NATIVE_ENDIAN,
       /*align=*/0},
  };

  JxlBasicInfo info;
  JxlEncoderInitBasicInfo(&info);
  info.bits_per_sample = 12;
  info.num_color_channels = 2;

  JxlPixelFormat format;
  EXPECT_FALSE(SelectFormat(accepted_formats, info, &format));

  info.num_color_channels = 3;
  ASSERT_TRUE(SelectFormat(accepted_formats, info, &format));
  EXPECT_EQ(format.num_channels, info.num_color_channels);
  // 16 is the smallest accepted format that can accommodate the 12-bit data.
  EXPECT_EQ(format.data_type, JXL_TYPE_UINT16);
}

TEST(CodecTest, EncodeToPNG) {
  ThreadPool* const pool = nullptr;

  std::unique_ptr<Encoder> png_encoder = Encoder::FromExtension(".png");
  ASSERT_THAT(png_encoder, NotNull());

  const PaddedBytes original_png =
      ReadTestData("external/wesaturate/500px/tmshre_riaphotographs_srgb8.png");
  PackedPixelFile ppf;
  ASSERT_TRUE(extras::DecodeBytes(Span<const uint8_t>(original_png),
                                  ColorHints(), SizeConstraints(), &ppf));

  const JxlPixelFormat& format = ppf.frames.front().color.format;
  ASSERT_THAT(
      png_encoder->AcceptedFormats(),
      Contains(AllOf(Field(&JxlPixelFormat::num_channels, format.num_channels),
                     Field(&JxlPixelFormat::data_type, format.data_type),
                     Field(&JxlPixelFormat::endianness, format.endianness))));
  EncodedImage encoded_png;
  ASSERT_TRUE(png_encoder->Encode(ppf, &encoded_png, pool));
  EXPECT_THAT(encoded_png.icc, IsEmpty());
  ASSERT_THAT(encoded_png.bitstreams, SizeIs(1));

  PackedPixelFile decoded_ppf;
  ASSERT_TRUE(
      extras::DecodeBytes(Span<const uint8_t>(encoded_png.bitstreams.front()),
                          ColorHints(), SizeConstraints(), &decoded_ppf));

  ASSERT_EQ(decoded_ppf.info.bits_per_sample, ppf.info.bits_per_sample);
  ASSERT_EQ(decoded_ppf.frames.size(), 1);
  VerifySameImage(ppf.frames[0].color, ppf.info.bits_per_sample,
                  decoded_ppf.frames[0].color,
                  decoded_ppf.info.bits_per_sample);
}

}  // namespace
}  // namespace extras
}  // namespace jxl
