// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <jxl/cms.h>
#include <jxl/encode.h>
#include <jxl/memory_manager.h>
#include <jxl/types.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "lib/extras/codec.h"
#include "lib/extras/dec/jxl.h"
#include "lib/extras/enc/jxl.h"
#include "lib/extras/metrics.h"
#include "lib/extras/packed_image.h"
#include "lib/jxl/base/common.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/random.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dec_bit_reader.h"
#include "lib/jxl/enc_aux_out.h"
#include "lib/jxl/enc_bit_writer.h"
#include "lib/jxl/enc_fields.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/enc_toc.h"
#include "lib/jxl/fields.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/headers.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/image_metadata.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/image_test_utils.h"
#include "lib/jxl/modular/encoding/enc_encoding.h"
#include "lib/jxl/modular/encoding/encoding.h"
#include "lib/jxl/modular/modular_image.h"
#include "lib/jxl/modular/options.h"
#include "lib/jxl/modular/transform/transform.h"
#include "lib/jxl/padded_bytes.h"
#include "lib/jxl/test_image.h"
#include "lib/jxl/test_memory_manager.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testing.h"

namespace jxl {
namespace {

using test::ButteraugliDistance;
using test::ReadTestData;
using test::Roundtrip;
using test::TestImage;

void TestLosslessGroups(size_t group_size_shift) {
  const std::vector<uint8_t> orig = ReadTestData("jxl/flower/flower.png");
  TestImage t;
  t.DecodeFromBytes(orig).ClearMetadata();
  t.SetDimensions(t.ppf().xsize() / 4, t.ppf().ysize() / 4);

  extras::JXLCompressParams cparams;
  cparams.distance = 0.0f;
  cparams.AddOption(JXL_ENC_FRAME_SETTING_MODULAR_GROUP_SIZE, group_size_shift);
  extras::JXLDecompressParams dparams;
  dparams.accepted_formats = {{3, JXL_TYPE_UINT16, JXL_LITTLE_ENDIAN, 0}};

  extras::PackedPixelFile ppf_out;
  size_t compressed_size =
      Roundtrip(t.ppf(), cparams, dparams, nullptr, &ppf_out);
  EXPECT_LE(compressed_size, 280000u);
  EXPECT_EQ(0.0f, test::ComputeDistance2(t.ppf(), ppf_out));
}

TEST(ModularTest, RoundtripLosslessGroups128) { TestLosslessGroups(0); }

JXL_TSAN_SLOW_TEST(ModularTest, RoundtripLosslessGroups512) {
  TestLosslessGroups(2);
}

JXL_TSAN_SLOW_TEST(ModularTest, RoundtripLosslessGroups1024) {
  TestLosslessGroups(3);
}

TEST(ModularTest, RoundtripLosslessCustomWpPermuteRCT) {
  const std::vector<uint8_t> orig =
      ReadTestData("external/wesaturate/500px/u76c0g_bliznaca_srgb8.png");
  TestImage t;
  t.DecodeFromBytes(orig).ClearMetadata();
  t.SetDimensions(100, 100);

  extras::JXLCompressParams cparams;
  cparams.distance = 0.0f;
  // 9 = permute to GBR, to test the special case of permutation-only
  cparams.AddOption(JXL_ENC_FRAME_SETTING_MODULAR_COLOR_SPACE, 9);
  cparams.AddOption(JXL_ENC_FRAME_SETTING_MODULAR_PREDICTOR,
                    static_cast<int64_t>(Predictor::Weighted));
  // slowest speed so different WP modes are tried
  cparams.AddOption(JXL_ENC_FRAME_SETTING_EFFORT, 9);
  extras::JXLDecompressParams dparams;
  dparams.accepted_formats = {{3, JXL_TYPE_UINT16, JXL_LITTLE_ENDIAN, 0}};

  extras::PackedPixelFile ppf_out;
  size_t compressed_size =
      Roundtrip(t.ppf(), cparams, dparams, nullptr, &ppf_out);
  EXPECT_LE(compressed_size, 10169u);
  EXPECT_EQ(0.0f, test::ComputeDistance2(t.ppf(), ppf_out));
}

TEST(ModularTest, RoundtripLossyDeltaPalette) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  const std::vector<uint8_t> orig =
      ReadTestData("external/wesaturate/500px/u76c0g_bliznaca_srgb8.png");
  CompressParams cparams;
  cparams.modular_mode = true;
  cparams.color_transform = jxl::ColorTransform::kNone;
  cparams.lossy_palette = true;
  cparams.palette_colors = 0;

  CodecInOut io_out{memory_manager};

  CodecInOut io{memory_manager};
  ASSERT_TRUE(SetFromBytes(Bytes(orig), &io));
  io.ShrinkTo(300, 100);

  size_t compressed_size;
  JXL_EXPECT_OK(Roundtrip(&io, cparams, {}, &io_out, _, &compressed_size));
  EXPECT_LE(compressed_size, 6800u);
  EXPECT_SLIGHTLY_BELOW(
      ButteraugliDistance(io.frames, io_out.frames, ButteraugliParams(),
                          *JxlGetDefaultCms(),
                          /*distmap=*/nullptr),
      1.5);
}
TEST(ModularTest, RoundtripLossyDeltaPaletteWP) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  const std::vector<uint8_t> orig =
      ReadTestData("external/wesaturate/500px/u76c0g_bliznaca_srgb8.png");
  CompressParams cparams;
  cparams.SetLossless();
  cparams.lossy_palette = true;
  cparams.palette_colors = 0;
  // TODO(jon): this is currently ignored, and Avg4 is always used instead
  cparams.options.predictor = jxl::Predictor::Weighted;

  CodecInOut io_out{memory_manager};

  CodecInOut io{memory_manager};
  ASSERT_TRUE(SetFromBytes(Bytes(orig), &io));
  io.ShrinkTo(300, 100);

  size_t compressed_size;
  JXL_EXPECT_OK(Roundtrip(&io, cparams, {}, &io_out, _, &compressed_size));
  EXPECT_LE(compressed_size, 6500u);
  EXPECT_SLIGHTLY_BELOW(
      ButteraugliDistance(io.frames, io_out.frames, ButteraugliParams(),
                          *JxlGetDefaultCms(),
                          /*distmap=*/nullptr),
      1.5);
}

TEST(ModularTest, RoundtripLossy) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  const std::vector<uint8_t> orig =
      ReadTestData("external/wesaturate/500px/u76c0g_bliznaca_srgb8.png");
  CompressParams cparams;
  cparams.modular_mode = true;
  cparams.butteraugli_distance = 2.f;
  cparams.SetCms(*JxlGetDefaultCms());

  CodecInOut io_out{memory_manager};

  CodecInOut io{memory_manager};
  ASSERT_TRUE(SetFromBytes(Bytes(orig), &io));

  size_t compressed_size;
  JXL_EXPECT_OK(Roundtrip(&io, cparams, {}, &io_out, _, &compressed_size));
  EXPECT_LE(compressed_size, 30000u);
  EXPECT_SLIGHTLY_BELOW(
      ButteraugliDistance(io.frames, io_out.frames, ButteraugliParams(),
                          *JxlGetDefaultCms(),
                          /*distmap=*/nullptr),
      2.3);
}

TEST(ModularTest, RoundtripLossy16) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  const std::vector<uint8_t> orig =
      ReadTestData("external/raw.pixls/DJI-FC6310-16bit_709_v4_krita.png");
  CompressParams cparams;
  cparams.modular_mode = true;
  cparams.butteraugli_distance = 2.f;

  CodecInOut io_out{memory_manager};

  CodecInOut io{memory_manager};
  ASSERT_TRUE(SetFromBytes(Bytes(orig), &io));
  JXL_CHECK(!io.metadata.m.have_preview);
  JXL_CHECK(io.frames.size() == 1);
  JXL_CHECK(
      io.frames[0].TransformTo(ColorEncoding::SRGB(), *JxlGetDefaultCms()));
  io.metadata.m.color_encoding = ColorEncoding::SRGB();

  size_t compressed_size;
  JXL_EXPECT_OK(Roundtrip(&io, cparams, {}, &io_out, _, &compressed_size));
  EXPECT_LE(compressed_size, 300u);
  EXPECT_SLIGHTLY_BELOW(
      ButteraugliDistance(io.frames, io_out.frames, ButteraugliParams(),
                          *JxlGetDefaultCms(),
                          /*distmap=*/nullptr),
      1.6);
}

TEST(ModularTest, RoundtripExtraProperties) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  constexpr size_t kSize = 250;
  JXL_ASSIGN_OR_DIE(Image image, Image::Create(memory_manager, kSize, kSize,
                                               /*bitdepth=*/8, 3));
  ModularOptions options;
  options.max_properties = 4;
  options.predictor = Predictor::Zero;
  Rng rng(0);
  for (size_t y = 0; y < kSize; y++) {
    for (size_t x = 0; x < kSize; x++) {
      image.channel[0].plane.Row(y)[x] = image.channel[2].plane.Row(y)[x] =
          rng.UniformU(0, 9);
    }
  }
  ZeroFillImage(&image.channel[1].plane);
  BitWriter writer{memory_manager};
  ASSERT_TRUE(ModularGenericCompress(image, options, &writer));
  writer.ZeroPadToByte();
  JXL_ASSIGN_OR_DIE(Image decoded,
                    Image::Create(memory_manager, kSize, kSize,
                                  /*bitdepth=*/8, image.channel.size()));
  for (size_t i = 0; i < image.channel.size(); i++) {
    const Channel& ch = image.channel[i];
    JXL_ASSIGN_OR_DIE(
        decoded.channel[i],
        Channel::Create(memory_manager, ch.w, ch.h, ch.hshift, ch.vshift));
  }
  Status status = true;
  {
    BitReader reader(writer.GetSpan());
    BitReaderScopedCloser closer(&reader, &status);
    ASSERT_TRUE(ModularGenericDecompress(&reader, decoded, /*header=*/nullptr,
                                         /*group_id=*/0, &options));
  }
  ASSERT_TRUE(status);
  ASSERT_EQ(image.channel.size(), decoded.channel.size());
  for (size_t c = 0; c < image.channel.size(); c++) {
    for (size_t y = 0; y < image.channel[c].plane.ysize(); y++) {
      for (size_t x = 0; x < image.channel[c].plane.xsize(); x++) {
        EXPECT_EQ(image.channel[c].plane.Row(y)[x],
                  decoded.channel[c].plane.Row(y)[x])
            << "c = " << c << ", x = " << x << ",  y = " << y;
      }
    }
  }
}

struct RoundtripLosslessConfig {
  int bitdepth;
  int responsive;
};
class ModularTestParam
    : public ::testing::TestWithParam<RoundtripLosslessConfig> {};

std::vector<RoundtripLosslessConfig> GenerateLosslessTests() {
  std::vector<RoundtripLosslessConfig> all;
  for (int responsive = 0; responsive <= 1; responsive++) {
    for (int bitdepth = 1; bitdepth < 32; bitdepth++) {
      if (responsive && bitdepth > 30) continue;
      all.push_back({bitdepth, responsive});
    }
  }
  return all;
}
std::string LosslessTestDescription(
    const testing::TestParamInfo<ModularTestParam::ParamType>& info) {
  std::stringstream name;
  name << info.param.bitdepth << "bit";
  if (info.param.responsive) name << "Squeeze";
  return name.str();
}

JXL_GTEST_INSTANTIATE_TEST_SUITE_P(RoundtripLossless, ModularTestParam,
                                   testing::ValuesIn(GenerateLosslessTests()),
                                   LosslessTestDescription);

TEST_P(ModularTestParam, RoundtripLossless) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  RoundtripLosslessConfig config = GetParam();
  int bitdepth = config.bitdepth;
  int responsive = config.responsive;

  ThreadPool* pool = nullptr;
  Rng generator(123);
  const std::vector<uint8_t> orig =
      ReadTestData("external/wesaturate/500px/u76c0g_bliznaca_srgb8.png");
  CodecInOut io1{memory_manager};
  ASSERT_TRUE(SetFromBytes(Bytes(orig), &io1, pool));

  // vary the dimensions a bit, in case of bugs related to
  // even vs odd width or height.
  size_t xsize = 423 + bitdepth;
  size_t ysize = 467 + bitdepth;

  CodecInOut io{memory_manager};
  io.SetSize(xsize, ysize);
  io.metadata.m.color_encoding = jxl::ColorEncoding::SRGB(false);
  io.metadata.m.SetUintSamples(bitdepth);

  double factor = ((1lu << bitdepth) - 1lu);
  double ifactor = 1.0 / factor;
  JXL_ASSIGN_OR_DIE(Image3F noise_added,
                    Image3F::Create(memory_manager, xsize, ysize));

  for (size_t c = 0; c < 3; c++) {
    for (size_t y = 0; y < ysize; y++) {
      const float* in = io1.Main().color()->PlaneRow(c, y);
      float* out = noise_added.PlaneRow(c, y);
      for (size_t x = 0; x < xsize; x++) {
        // make the least significant bits random
        float f = in[x] + generator.UniformF(0.0f, 1.f / 255.f);
        if (f > 1.f) f = 1.f;
        // quantize to the bitdepth we're testing
        unsigned int u = static_cast<unsigned int>(std::lround(f * factor));
        out[x] = u * ifactor;
      }
    }
  }
  io.SetFromImage(std::move(noise_added), jxl::ColorEncoding::SRGB(false));

  CompressParams cparams;
  cparams.modular_mode = true;
  cparams.color_transform = jxl::ColorTransform::kNone;
  cparams.butteraugli_distance = 0.f;
  cparams.options.predictor = {Predictor::Zero};
  cparams.speed_tier = SpeedTier::kThunder;
  cparams.responsive = responsive;
  CodecInOut io2{memory_manager};
  size_t compressed_size;
  JXL_EXPECT_OK(Roundtrip(&io, cparams, {}, &io2, _, &compressed_size));
  EXPECT_LE(compressed_size, bitdepth * xsize * ysize / 3.0 * 1.1);
  EXPECT_LE(0, ComputeDistance2(io.Main(), io2.Main(), *JxlGetDefaultCms()));
  size_t different = 0;
  for (size_t c = 0; c < 3; c++) {
    for (size_t y = 0; y < ysize; y++) {
      const float* in = io.Main().color()->PlaneRow(c, y);
      const float* out = io2.Main().color()->PlaneRow(c, y);
      for (size_t x = 0; x < xsize; x++) {
        uint32_t uin = std::lroundf(in[x] * factor);
        uint32_t uout = std::lroundf(out[x] * factor);
        // check that the integer values are identical
        if (uin != uout) different++;
      }
    }
  }
  EXPECT_EQ(different, 0);
}

TEST(ModularTest, RoundtripLosslessCustomFloat) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  CodecInOut io{memory_manager};
  size_t xsize = 100;
  size_t ysize = 300;
  io.SetSize(xsize, ysize);
  io.metadata.m.bit_depth.bits_per_sample = 18;
  io.metadata.m.bit_depth.exponent_bits_per_sample = 6;
  io.metadata.m.bit_depth.floating_point_sample = true;
  io.metadata.m.modular_16_bit_buffer_sufficient = false;
  ColorEncoding color_encoding;
  color_encoding.Tf().SetTransferFunction(TransferFunction::kLinear);
  color_encoding.SetColorSpace(ColorSpace::kRGB);
  JXL_ASSIGN_OR_DIE(Image3F testimage,
                    Image3F::Create(memory_manager, xsize, ysize));
  float factor = 1.f / (1 << 14);
  for (size_t c = 0; c < 3; c++) {
    for (size_t y = 0; y < ysize; y++) {
      float* const JXL_RESTRICT row = testimage.PlaneRow(c, y);
      for (size_t x = 0; x < xsize; x++) {
        row[x] = factor * (x ^ y);
      }
    }
  }
  io.SetFromImage(std::move(testimage), color_encoding);
  io.metadata.m.color_encoding = color_encoding;
  io.metadata.m.SetIntensityTarget(255);

  CompressParams cparams;
  cparams.modular_mode = true;
  cparams.color_transform = jxl::ColorTransform::kNone;
  cparams.butteraugli_distance = 0.f;
  cparams.options.predictor = {Predictor::Zero};
  cparams.speed_tier = SpeedTier::kThunder;
  cparams.decoding_speed_tier = 2;

  CodecInOut io2{memory_manager};
  size_t compressed_size;
  JXL_EXPECT_OK(Roundtrip(&io, cparams, {}, &io2, _, &compressed_size));
  EXPECT_LE(compressed_size, 23000u);
  JXL_EXPECT_OK(SamePixels(*io.Main().color(), *io2.Main().color(), _));
}

void WriteHeaders(BitWriter* writer, size_t xsize, size_t ysize) {
  BitWriter::Allotment allotment(writer, 16);
  writer->Write(8, 0xFF);
  writer->Write(8, kCodestreamMarker);
  allotment.ReclaimAndCharge(writer, 0, nullptr);
  CodecMetadata metadata;
  EXPECT_TRUE(metadata.size.Set(xsize, ysize));
  EXPECT_TRUE(WriteSizeHeader(metadata.size, writer, 0, nullptr));
  metadata.m.color_encoding = ColorEncoding::LinearSRGB(/*is_gray=*/true);
  metadata.m.xyb_encoded = false;
  metadata.m.SetUintSamples(31);
  EXPECT_TRUE(WriteImageMetadata(metadata.m, writer, 0, nullptr));
  metadata.transform_data.nonserialized_xyb_encoded = metadata.m.xyb_encoded;
  EXPECT_TRUE(Bundle::Write(metadata.transform_data, writer, 0, nullptr));
  writer->ZeroPadToByte();
  FrameHeader frame_header(&metadata);
  frame_header.encoding = FrameEncoding::kModular;
  frame_header.loop_filter.gab = false;
  frame_header.loop_filter.epf_iters = 0;
  EXPECT_TRUE(WriteFrameHeader(frame_header, writer, nullptr));
}

// Tree with single node, zero predictor, offset is 1 and multiplier is 1,
// entropy code is prefix tree with alphabet size 256 and all bits lengths 8.
void WriteHistograms(BitWriter* writer) {
  writer->Write(1, 1);  // default DC quant
  writer->Write(1, 1);  // has_tree
  // tree histograms
  writer->Write(1, 0);         // LZ77 disabled
  writer->Write(3, 1);         // simple context map
  writer->Write(1, 1);         // prefix code
  writer->Write(7, 0x63);      // UnintConfig(3, 2, 1)
  writer->Write(12, 0xfef);    // alphabet_size = 256
  writer->Write(32, 0x10003);  // all bit lengths 8
  // tree tokens
  writer->Write(8, 0);   // tree leaf
  writer->Write(8, 0);   // zero predictor
  writer->Write(8, 64);  // offset = UnpackSigned(ReverseBits(64)) = 1
  writer->Write(16, 0);  // multiplier = 1
  // histograms
  writer->Write(1, 0);         // LZ77 disabled
  writer->Write(1, 1);         // prefix code
  writer->Write(7, 0x63);      // UnintConfig(3, 2, 1)
  writer->Write(12, 0xfef);    // alphabet_size = 256
  writer->Write(32, 0x10003);  // all bit lengths 8
}

TEST(ModularTest, PredictorIntegerOverflow) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  const size_t xsize = 1;
  const size_t ysize = 1;
  BitWriter writer{memory_manager};
  WriteHeaders(&writer, xsize, ysize);
  std::vector<std::unique_ptr<BitWriter>> group_codes;
  group_codes.emplace_back(jxl::make_unique<BitWriter>(memory_manager));
  {
    std::unique_ptr<BitWriter>& bw = group_codes[0];
    BitWriter::Allotment allotment(bw.get(), 1 << 20);
    WriteHistograms(bw.get());
    GroupHeader header;
    header.use_global_tree = true;
    EXPECT_TRUE(Bundle::Write(header, bw.get(), 0, nullptr));
    // After UnpackSigned this becomes (1 << 31) - 1, the largest pixel_type,
    // and after adding the offset we get -(1 << 31).
    bw->Write(8, 119);
    bw->Write(28, 0xfffffff);
    bw->ZeroPadToByte();
    allotment.ReclaimAndCharge(bw.get(), 0, nullptr);
  }
  EXPECT_TRUE(WriteGroupOffsets(group_codes, {}, &writer, nullptr));
  writer.AppendByteAligned(group_codes);

  PaddedBytes compressed = std::move(writer).TakeBytes();
  extras::PackedPixelFile ppf;
  extras::JXLDecompressParams params;
  params.accepted_formats.push_back({1, JXL_TYPE_FLOAT, JXL_NATIVE_ENDIAN, 0});
  EXPECT_TRUE(DecodeImageJXL(compressed.data(), compressed.size(), params,
                             nullptr, &ppf));
  ASSERT_EQ(1, ppf.frames.size());
  const auto& img = ppf.frames[0].color;
  const auto* pixels = reinterpret_cast<const float*>(img.pixels());
  EXPECT_EQ(-1.0f, pixels[0]);
}

TEST(ModularTest, UnsqueezeIntegerOverflow) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  // Image width is 9 so we can test both the SIMD and non-vector code paths.
  const size_t xsize = 9;
  const size_t ysize = 2;
  BitWriter writer{memory_manager};
  WriteHeaders(&writer, xsize, ysize);
  std::vector<std::unique_ptr<BitWriter>> group_codes;
  group_codes.emplace_back(jxl::make_unique<BitWriter>(memory_manager));
  {
    std::unique_ptr<BitWriter>& bw = group_codes[0];
    BitWriter::Allotment allotment(bw.get(), 1 << 20);
    WriteHistograms(bw.get());
    GroupHeader header;
    header.use_global_tree = true;
    header.transforms.emplace_back();
    header.transforms[0].id = TransformId::kSqueeze;
    SqueezeParams params;
    params.horizontal = false;
    params.in_place = true;
    params.begin_c = 0;
    params.num_c = 1;
    header.transforms[0].squeezes.emplace_back(params);
    EXPECT_TRUE(Bundle::Write(header, bw.get(), 0, nullptr));
    for (size_t i = 0; i < xsize * ysize; ++i) {
      // After UnpackSigned and adding offset, this becomes (1 << 31) - 1, both
      // in the image and in the residual channels, and unsqueeze makes them
      // ~(3 << 30) and (1 << 30) (in pixel_type_w) and the first wraps around
      // to about -(1 << 30).
      bw->Write(8, 119);
      bw->Write(28, 0xffffffe);
    }
    bw->ZeroPadToByte();
    allotment.ReclaimAndCharge(bw.get(), 0, nullptr);
  }
  EXPECT_TRUE(WriteGroupOffsets(group_codes, {}, &writer, nullptr));
  writer.AppendByteAligned(group_codes);

  PaddedBytes compressed = std::move(writer).TakeBytes();
  extras::PackedPixelFile ppf;
  extras::JXLDecompressParams params;
  params.accepted_formats.push_back({1, JXL_TYPE_FLOAT, JXL_NATIVE_ENDIAN, 0});
  EXPECT_TRUE(DecodeImageJXL(compressed.data(), compressed.size(), params,
                             nullptr, &ppf));
  ASSERT_EQ(1, ppf.frames.size());
  const auto& img = ppf.frames[0].color;
  const float* pixels = reinterpret_cast<const float*>(img.pixels());
  for (size_t x = 0; x < xsize; ++x) {
    EXPECT_NEAR(-0.5f, pixels[x], 1e-10);
    EXPECT_NEAR(0.5f, pixels[xsize + x], 1e-10);
  }
}

}  // namespace
}  // namespace jxl
