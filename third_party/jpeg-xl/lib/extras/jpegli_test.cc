// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#if JPEGXL_ENABLE_JPEG && JPEGXL_ENABLE_JPEGLI

#include "lib/extras/dec/jpegli.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "jxl/color_encoding.h"
#include "lib/extras/dec/color_hints.h"
#include "lib/extras/dec/decode.h"
#include "lib/extras/dec/jpg.h"
#include "lib/extras/enc/encode.h"
#include "lib/extras/enc/jpegli.h"
#include "lib/extras/enc/jpg.h"
#include "lib/extras/packed_image.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/size_constraints.h"
#include "lib/jxl/test_image.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testdata.h"

namespace jxl {
namespace extras {
namespace {

using test::ButteraugliDistance;
using test::TestImage;

Status ReadTestImage(const std::string& pathname, PackedPixelFile* ppf) {
  const PaddedBytes encoded = ReadTestData(pathname);
  ColorHints color_hints;
  if (pathname.find(".ppm") != std::string::npos) {
    color_hints.Add("color_space", "RGB_D65_SRG_Rel_SRG");
  } else if (pathname.find(".pgm") != std::string::npos) {
    color_hints.Add("color_space", "Gra_D65_Rel_SRG");
  }
  return DecodeBytes(Span<const uint8_t>(encoded), color_hints,
                     SizeConstraints(), ppf);
}

Status DecodeWithLibjpeg(const std::vector<uint8_t>& compressed,
                         PackedPixelFile* ppf) {
  return DecodeImageJPG(Span<const uint8_t>(compressed), ColorHints(),
                        SizeConstraints(), ppf);
}

Status EncodeWithLibjpeg(const PackedPixelFile& ppf, int quality,
                         std::vector<uint8_t>* compressed) {
  std::unique_ptr<Encoder> encoder = GetJPEGEncoder();
  encoder->SetOption("q", std::to_string(quality));
  EncodedImage encoded;
  JXL_RETURN_IF_ERROR(encoder->Encode(ppf, &encoded));
  JXL_RETURN_IF_ERROR(!encoded.bitstreams.empty());
  *compressed = std::move(encoded.bitstreams[0]);
  return true;
}

std::string Description(const JxlColorEncoding& color_encoding) {
  ColorEncoding c_enc;
  JXL_CHECK(ConvertExternalToInternalColorEncoding(color_encoding, &c_enc));
  return Description(c_enc);
}

float BitsPerPixel(const PackedPixelFile& ppf,
                   const std::vector<uint8_t>& compressed) {
  const size_t num_pixels = ppf.info.xsize * ppf.info.ysize;
  return compressed.size() * 8.0 / num_pixels;
}

TEST(JpegliTest, JpegliSRGBDecodeTest) {
  std::string testimage = "jxl/flower/flower_small.rgb.depth8.ppm";
  PackedPixelFile ppf0;
  ASSERT_TRUE(ReadTestImage(testimage, &ppf0));
  EXPECT_EQ("RGB_D65_SRG_Rel_SRG", Description(ppf0.color_encoding));
  EXPECT_EQ(8, ppf0.info.bits_per_sample);

  std::vector<uint8_t> compressed;
  ASSERT_TRUE(EncodeWithLibjpeg(ppf0, 90, &compressed));

  PackedPixelFile ppf1;
  ASSERT_TRUE(DecodeWithLibjpeg(compressed, &ppf1));
  PackedPixelFile ppf2;
  ASSERT_TRUE(DecodeJpeg(compressed, JXL_TYPE_UINT8, nullptr, &ppf2));
  EXPECT_LT(ButteraugliDistance(ppf0, ppf2), ButteraugliDistance(ppf0, ppf1));
}

TEST(JpegliTest, JpegliGrayscaleDecodeTest) {
  std::string testimage = "jxl/flower/flower_small.g.depth8.pgm";
  PackedPixelFile ppf0;
  ASSERT_TRUE(ReadTestImage(testimage, &ppf0));
  EXPECT_EQ("Gra_D65_Rel_SRG", Description(ppf0.color_encoding));
  EXPECT_EQ(8, ppf0.info.bits_per_sample);

  std::vector<uint8_t> compressed;
  ASSERT_TRUE(EncodeWithLibjpeg(ppf0, 90, &compressed));

  PackedPixelFile ppf1;
  ASSERT_TRUE(DecodeWithLibjpeg(compressed, &ppf1));
  PackedPixelFile ppf2;
  ASSERT_TRUE(DecodeJpeg(compressed, JXL_TYPE_UINT8, nullptr, &ppf2));
  EXPECT_LT(ButteraugliDistance(ppf0, ppf2), ButteraugliDistance(ppf0, ppf1));
}

TEST(JpegliTest, JpegliXYBEncodeTest) {
  std::string testimage = "jxl/flower/flower_small.rgb.depth8.ppm";
  PackedPixelFile ppf_in;
  ASSERT_TRUE(ReadTestImage(testimage, &ppf_in));
  EXPECT_EQ("RGB_D65_SRG_Rel_SRG", Description(ppf_in.color_encoding));
  EXPECT_EQ(8, ppf_in.info.bits_per_sample);

  std::vector<uint8_t> compressed;
  JpegSettings settings;
  settings.xyb = true;
  ASSERT_TRUE(EncodeJpeg(ppf_in, settings, nullptr, &compressed));

  PackedPixelFile ppf_out;
  ASSERT_TRUE(DecodeWithLibjpeg(compressed, &ppf_out));
  EXPECT_THAT(BitsPerPixel(ppf_in, compressed), IsSlightlyBelow(1.45f));
  EXPECT_THAT(ButteraugliDistance(ppf_in, ppf_out), IsSlightlyBelow(1.32f));
}

TEST(JpegliTest, JpegliDecodeTestLargeSmoothArea) {
  TestImage t;
  const size_t xsize = 2070;
  const size_t ysize = 1063;
  t.SetDimensions(xsize, ysize).SetChannels(3);
  t.SetAllBitDepths(8).SetEndianness(JXL_NATIVE_ENDIAN);
  TestImage::Frame frame = t.AddFrame();
  frame.RandomFill();
  // Create a large smooth area in the top half of the image. This is to test
  // that the bias statistics calculation can handle many blocks with all-zero
  // AC coefficients.
  for (size_t y = 0; y < ysize / 2; ++y) {
    for (size_t x = 0; x < xsize; ++x) {
      for (size_t c = 0; c < 3; ++c) {
        frame.SetValue(y, x, c, 0.5f);
      }
    }
  }
  const PackedPixelFile& ppf0 = t.ppf();

  std::vector<uint8_t> compressed;
  ASSERT_TRUE(EncodeWithLibjpeg(ppf0, 90, &compressed));

  PackedPixelFile ppf1;
  ASSERT_TRUE(DecodeJpeg(compressed, JXL_TYPE_UINT8, nullptr, &ppf1));
  EXPECT_LT(ButteraugliDistance(ppf0, ppf1), 3.0f);
}

TEST(JpegliTest, JpegliYUVEncodeTest) {
  std::string testimage = "jxl/flower/flower_small.rgb.depth8.ppm";
  PackedPixelFile ppf_in;
  ASSERT_TRUE(ReadTestImage(testimage, &ppf_in));
  EXPECT_EQ("RGB_D65_SRG_Rel_SRG", Description(ppf_in.color_encoding));
  EXPECT_EQ(8, ppf_in.info.bits_per_sample);

  std::vector<uint8_t> compressed;
  JpegSettings settings;
  settings.xyb = false;
  ASSERT_TRUE(EncodeJpeg(ppf_in, settings, nullptr, &compressed));

  PackedPixelFile ppf_out;
  ASSERT_TRUE(DecodeWithLibjpeg(compressed, &ppf_out));
  EXPECT_THAT(BitsPerPixel(ppf_in, compressed), IsSlightlyBelow(1.68f));
  EXPECT_THAT(ButteraugliDistance(ppf_in, ppf_out), IsSlightlyBelow(1.28f));
}

TEST(JpegliTest, JpegliYUVChromaSubsamplingEncodeTest) {
  std::string testimage = "jxl/flower/flower_small.rgb.depth8.ppm";
  PackedPixelFile ppf_in;
  ASSERT_TRUE(ReadTestImage(testimage, &ppf_in));
  EXPECT_EQ("RGB_D65_SRG_Rel_SRG", Description(ppf_in.color_encoding));
  EXPECT_EQ(8, ppf_in.info.bits_per_sample);

  std::vector<uint8_t> compressed;
  JpegSettings settings;
  for (const std::string& sampling : {"440", "422", "420"}) {
    settings.xyb = false;
    settings.chroma_subsampling = sampling;
    ASSERT_TRUE(EncodeJpeg(ppf_in, settings, nullptr, &compressed));

    PackedPixelFile ppf_out;
    ASSERT_TRUE(DecodeWithLibjpeg(compressed, &ppf_out));
    EXPECT_LE(BitsPerPixel(ppf_in, compressed), 1.6f);
    EXPECT_LE(ButteraugliDistance(ppf_in, ppf_out), 1.8f);
  }
}

TEST(JpegliTest, JpegliYUVEncodeTestNoAq) {
  std::string testimage = "jxl/flower/flower_small.rgb.depth8.ppm";
  PackedPixelFile ppf_in;
  ASSERT_TRUE(ReadTestImage(testimage, &ppf_in));
  EXPECT_EQ("RGB_D65_SRG_Rel_SRG", Description(ppf_in.color_encoding));
  EXPECT_EQ(8, ppf_in.info.bits_per_sample);

  std::vector<uint8_t> compressed;
  JpegSettings settings;
  settings.xyb = false;
  settings.use_adaptive_quantization = false;
  ASSERT_TRUE(EncodeJpeg(ppf_in, settings, nullptr, &compressed));

  PackedPixelFile ppf_out;
  ASSERT_TRUE(DecodeWithLibjpeg(compressed, &ppf_out));
  EXPECT_THAT(BitsPerPixel(ppf_in, compressed), IsSlightlyBelow(1.72f));
  EXPECT_THAT(ButteraugliDistance(ppf_in, ppf_out), IsSlightlyBelow(1.45f));
}

TEST(JpegliTest, JpegliHDRRoundtripTest) {
  std::string testimage = "jxl/hdr_room.png";
  PackedPixelFile ppf_in;
  ASSERT_TRUE(ReadTestImage(testimage, &ppf_in));
  EXPECT_EQ("RGB_D65_202_Rel_HLG", Description(ppf_in.color_encoding));
  EXPECT_EQ(16, ppf_in.info.bits_per_sample);

  std::vector<uint8_t> compressed;
  JpegSettings settings;
  settings.xyb = false;
  ASSERT_TRUE(EncodeJpeg(ppf_in, settings, nullptr, &compressed));

  PackedPixelFile ppf_out;
  ASSERT_TRUE(DecodeJpeg(compressed, JXL_TYPE_UINT16, nullptr, &ppf_out));
  EXPECT_THAT(BitsPerPixel(ppf_in, compressed), IsSlightlyBelow(2.95f));
  EXPECT_THAT(ButteraugliDistance(ppf_in, ppf_out), IsSlightlyBelow(1.05f));
}

}  // namespace
}  // namespace extras
}  // namespace jxl
#endif  // JPEGXL_ENABLE_JPEG
