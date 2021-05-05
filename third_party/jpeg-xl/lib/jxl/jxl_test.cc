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

#include <stdint.h>
#include <stdio.h>

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "lib/extras/codec.h"
#include "lib/extras/codec_jpg.h"
#include "lib/jxl/aux_out.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/override.h"
#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/thread_pool_internal.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/dec_file.h"
#include "lib/jxl/dec_params.h"
#include "lib/jxl/enc_butteraugli_comparator.h"
#include "lib/jxl/enc_cache.h"
#include "lib/jxl/enc_file.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/image_test_utils.h"
#include "lib/jxl/jpeg/enc_jpeg_data.h"
#include "lib/jxl/modular/options.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testdata.h"
#include "tools/box/box.h"

namespace jxl {
namespace {
using test::Roundtrip;

#define JXL_TEST_NL 0  // Disabled in code

void CreateImage1x1(CodecInOut* io) {
  Image3F image(1, 1);
  ZeroFillImage(&image);
  io->metadata.m.SetUintSamples(8);
  io->metadata.m.color_encoding = ColorEncoding::SRGB();
  io->SetFromImage(std::move(image), io->metadata.m.color_encoding);
}

TEST(JxlTest, HeaderSize) {
  CodecInOut io;
  CreateImage1x1(&io);

  CompressParams cparams;
  cparams.butteraugli_distance = 1.5;
  DecompressParams dparams;
  ThreadPool* pool = nullptr;

  {
    CodecInOut io2;
    AuxOut aux_out;
    Roundtrip(&io, cparams, dparams, pool, &io2, &aux_out);
    EXPECT_LE(aux_out.layers[kLayerHeader].total_bits, 34);
  }

  {
    CodecInOut io2;
    io.metadata.m.SetAlphaBits(8);
    ImageF alpha(1, 1);
    alpha.Row(0)[0] = 1;
    io.Main().SetAlpha(std::move(alpha), /*alpha_is_premultiplied=*/false);
    AuxOut aux_out;
    Roundtrip(&io, cparams, dparams, pool, &io2, &aux_out);
    EXPECT_LE(aux_out.layers[kLayerHeader].total_bits, 57);
  }
}

TEST(JxlTest, RoundtripSinglePixel) {
  CodecInOut io;
  CreateImage1x1(&io);

  CompressParams cparams;
  cparams.butteraugli_distance = 1.0;
  DecompressParams dparams;
  ThreadPool* pool = nullptr;
  CodecInOut io2;
  Roundtrip(&io, cparams, dparams, pool, &io2);
}

// Changing serialized signature causes Decode to fail.
#ifndef JXL_CRASH_ON_ERROR
TEST(JxlTest, RoundtripMarker) {
  CodecInOut io;
  CreateImage1x1(&io);

  CompressParams cparams;
  cparams.butteraugli_distance = 1.0;
  DecompressParams dparams;
  AuxOut* aux_out = nullptr;
  ThreadPool* pool = nullptr;

  PassesEncoderState enc_state;
  for (size_t i = 0; i < 2; ++i) {
    PaddedBytes compressed;
    EXPECT_TRUE(
        EncodeFile(cparams, &io, &enc_state, &compressed, aux_out, pool));
    compressed[i] ^= 0xFF;
    CodecInOut io2;
    EXPECT_FALSE(DecodeFile(dparams, compressed, &io2, pool));
  }
}
#endif

TEST(JxlTest, RoundtripTinyFast) {
  ThreadPool* pool = nullptr;
  const PaddedBytes orig =
      ReadTestData("wesaturate/500px/u76c0g_bliznaca_srgb8.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, pool));
  io.ShrinkTo(32, 32);

  CompressParams cparams;
  cparams.speed_tier = SpeedTier::kSquirrel;
  cparams.butteraugli_distance = 4.0f;
  DecompressParams dparams;

  CodecInOut io2;
  const size_t enc_bytes = Roundtrip(&io, cparams, dparams, pool, &io2);
  printf("32x32 image size %zu bytes\n", enc_bytes);
}

TEST(JxlTest, RoundtripSmallD1) {
  ThreadPool* pool = nullptr;
  const PaddedBytes orig =
      ReadTestData("wesaturate/500px/u76c0g_bliznaca_srgb8.png");
  CompressParams cparams;
  cparams.butteraugli_distance = 1.0;
  DecompressParams dparams;

  CodecInOut io_out;
  size_t compressed_size;

  {
    CodecInOut io;
    ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, pool));
    io.ShrinkTo(io.xsize() / 8, io.ysize() / 8);

    compressed_size = Roundtrip(&io, cparams, dparams, pool, &io_out);
    EXPECT_LE(compressed_size, 1000);
    EXPECT_LE(ButteraugliDistance(io, io_out, cparams.ba_params,
                                  /*distmap=*/nullptr, pool),
              1.5);
  }

  {
    // And then, with a lower intensity target than the default, the bitrate
    // should be smaller.
    CodecInOut io_dim;
    io_dim.target_nits = 100;
    ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io_dim, pool));
    io_dim.ShrinkTo(io_dim.xsize() / 8, io_dim.ysize() / 8);
    EXPECT_LT(Roundtrip(&io_dim, cparams, dparams, pool, &io_out),
              compressed_size);
    EXPECT_LE(ButteraugliDistance(io_dim, io_out, cparams.ba_params,
                                  /*distmap=*/nullptr, pool),
              1.5);
    EXPECT_EQ(io_dim.metadata.m.IntensityTarget(),
              io_out.metadata.m.IntensityTarget());
  }
}

TEST(JxlTest, RoundtripOtherTransforms) {
  ThreadPool* pool = nullptr;
  const PaddedBytes orig =
      ReadTestData("wesaturate/64px/a2d1un_nkitzmiller_srgb8.png");
  std::unique_ptr<CodecInOut> io = jxl::make_unique<CodecInOut>();
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), io.get(), pool));

  CompressParams cparams;
  // Slow modes access linear image for adaptive quant search
  cparams.speed_tier = SpeedTier::kKitten;
  cparams.color_transform = ColorTransform::kNone;
  cparams.butteraugli_distance = 5.0f;
  DecompressParams dparams;

  std::unique_ptr<CodecInOut> io2 = jxl::make_unique<CodecInOut>();
  const size_t compressed_size =
      Roundtrip(io.get(), cparams, dparams, pool, io2.get());
  EXPECT_LE(compressed_size, 23000);
  EXPECT_LE(ButteraugliDistance(*io, *io2, cparams.ba_params,
                                /*distmap=*/nullptr, pool),
            6);

  // Check the consistency when performing another roundtrip.
  std::unique_ptr<CodecInOut> io3 = jxl::make_unique<CodecInOut>();
  const size_t compressed_size2 =
      Roundtrip(io.get(), cparams, dparams, pool, io3.get());
  EXPECT_LE(compressed_size2, 23000);
  EXPECT_LE(ButteraugliDistance(*io, *io3, cparams.ba_params,
                                /*distmap=*/nullptr, pool),
            6);
}

TEST(JxlTest, RoundtripResample2) {
  ThreadPool* pool = nullptr;
  const PaddedBytes orig =
      ReadTestData("wesaturate/500px/u76c0g_bliznaca_srgb8.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, pool));
  io.ShrinkTo(io.xsize(), io.ysize());
  CompressParams cparams;
  cparams.resampling = 2;
  DecompressParams dparams;
  CodecInOut io2;
  EXPECT_LE(Roundtrip(&io, cparams, dparams, pool, &io2), 15777);
  EXPECT_LE(ButteraugliDistance(io, io2, cparams.ba_params,
                                /*distmap=*/nullptr, pool),
            13.5);
}

TEST(JxlTest, RoundtripResample4) {
  ThreadPool* pool = nullptr;
  const PaddedBytes orig =
      ReadTestData("wesaturate/500px/u76c0g_bliznaca_srgb8.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, pool));
  io.ShrinkTo(io.xsize(), io.ysize());
  CompressParams cparams;
  cparams.resampling = 4;
  DecompressParams dparams;
  CodecInOut io2;
  EXPECT_LE(Roundtrip(&io, cparams, dparams, pool, &io2), 6000);
  EXPECT_LE(ButteraugliDistance(io, io2, cparams.ba_params,
                                /*distmap=*/nullptr, pool),
            28);
}

TEST(JxlTest, RoundtripResample8) {
  ThreadPool* pool = nullptr;
  const PaddedBytes orig =
      ReadTestData("wesaturate/500px/u76c0g_bliznaca_srgb8.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, pool));
  io.ShrinkTo(io.xsize(), io.ysize());
  CompressParams cparams;
  cparams.resampling = 8;
  DecompressParams dparams;
  CodecInOut io2;
  EXPECT_LE(Roundtrip(&io, cparams, dparams, pool, &io2), 2100);
  EXPECT_LE(ButteraugliDistance(io, io2, cparams.ba_params,
                                /*distmap=*/nullptr, pool),
            80);
}

TEST(JxlTest, RoundtripUnalignedD2) {
  ThreadPool* pool = nullptr;
  const PaddedBytes orig =
      ReadTestData("wesaturate/500px/u76c0g_bliznaca_srgb8.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, pool));
  io.ShrinkTo(io.xsize() / 12, io.ysize() / 7);

  CompressParams cparams;
  cparams.butteraugli_distance = 2.0;
  DecompressParams dparams;

  CodecInOut io2;
  EXPECT_LE(Roundtrip(&io, cparams, dparams, pool, &io2), 700);
  EXPECT_LE(ButteraugliDistance(io, io2, cparams.ba_params,
                                /*distmap=*/nullptr, pool),
            3.2);
}

#if JXL_TEST_NL

TEST(JxlTest, RoundtripMultiGroupNL) {
  ThreadPoolInternal pool(4);
  const PaddedBytes orig =
      ReadTestData("imagecompression.info/flower_foveon.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, &pool));
  io.ShrinkTo(600, 1024);  // partial X, full Y group

  CompressParams cparams;
  DecompressParams dparams;

  cparams.fast_mode = true;
  cparams.butteraugli_distance = 1.0f;
  CodecInOut io2;
  Roundtrip(&io, cparams, dparams, &pool, &io2);
  EXPECT_LE(ButteraugliDistance(io, io2, cparams.ba_params,
                                /*distmap=*/nullptr, &pool),
            0.9f);

  cparams.butteraugli_distance = 2.0f;
  CodecInOut io3;
  EXPECT_LE(Roundtrip(&io, cparams, dparams, &pool, &io3), 80000);
  EXPECT_LE(ButteraugliDistance(io, io3, cparams.ba_params,
                                /*distmap=*/nullptr, &pool),
            1.5f);
}

#endif

TEST(JxlTest, RoundtripMultiGroup) {
  ThreadPoolInternal pool(4);
  const PaddedBytes orig =
      ReadTestData("imagecompression.info/flower_foveon.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, &pool));
  io.ShrinkTo(600, 1024);

  CompressParams cparams;
  DecompressParams dparams;

  cparams.butteraugli_distance = 1.0f;
  cparams.speed_tier = SpeedTier::kKitten;
  CodecInOut io2;
  EXPECT_LE(Roundtrip(&io, cparams, dparams, &pool, &io2), 40000);
  EXPECT_LE(ButteraugliDistance(io, io2, cparams.ba_params,
                                /*distmap=*/nullptr, &pool),
            1.99f);

  cparams.butteraugli_distance = 2.0f;
  CodecInOut io3;
  EXPECT_LE(Roundtrip(&io, cparams, dparams, &pool, &io3), 22100);
  EXPECT_LE(ButteraugliDistance(io, io3, cparams.ba_params,
                                /*distmap=*/nullptr, &pool),
            3.0f);
}

TEST(JxlTest, RoundtripLargeFast) {
  ThreadPoolInternal pool(8);
  const PaddedBytes orig =
      ReadTestData("imagecompression.info/flower_foveon.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, &pool));

  CompressParams cparams;
  cparams.speed_tier = SpeedTier::kSquirrel;
  DecompressParams dparams;

  CodecInOut io2;
  EXPECT_LE(Roundtrip(&io, cparams, dparams, &pool, &io2), 265000);
}

TEST(JxlTest, RoundtripDotsForceEpf) {
  ThreadPoolInternal pool(8);
  const PaddedBytes orig =
      ReadTestData("wesaturate/500px/cvo9xd_keong_macan_srgb8.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, &pool));

  CompressParams cparams;
  cparams.epf = 2;
  cparams.dots = Override::kOn;
  cparams.speed_tier = SpeedTier::kSquirrel;
  DecompressParams dparams;

  CodecInOut io2;
  EXPECT_LE(Roundtrip(&io, cparams, dparams, &pool, &io2), 265000);
}

// Checks for differing size/distance in two consecutive runs of distance 2,
// which involves additional processing including adaptive reconstruction.
// Failing this may be a sign of race conditions or invalid memory accesses.
TEST(JxlTest, RoundtripD2Consistent) {
  ThreadPoolInternal pool(8);
  const PaddedBytes orig =
      ReadTestData("imagecompression.info/flower_foveon.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, &pool));

  CompressParams cparams;
  cparams.speed_tier = SpeedTier::kSquirrel;
  cparams.butteraugli_distance = 2.0;
  DecompressParams dparams;

  // Try each xsize mod kBlockDim to verify right border handling.
  for (size_t xsize = 48; xsize > 40; --xsize) {
    io.ShrinkTo(xsize, 15);

    CodecInOut io2;
    const size_t size2 = Roundtrip(&io, cparams, dparams, &pool, &io2);

    CodecInOut io3;
    const size_t size3 = Roundtrip(&io, cparams, dparams, &pool, &io3);

    // Exact same compressed size.
    EXPECT_EQ(size2, size3);

    // Exact same distance.
    const float dist2 = ButteraugliDistance(io, io2, cparams.ba_params,
                                            /*distmap=*/nullptr, &pool);
    const float dist3 = ButteraugliDistance(io, io3, cparams.ba_params,
                                            /*distmap=*/nullptr, &pool);
    EXPECT_EQ(dist2, dist3);
  }
}

// Same as above, but for full image, testing multiple groups.
TEST(JxlTest, RoundtripLargeConsistent) {
  ThreadPoolInternal pool(8);
  const PaddedBytes orig =
      ReadTestData("imagecompression.info/flower_foveon.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, &pool));

  CompressParams cparams;
  cparams.speed_tier = SpeedTier::kSquirrel;
  cparams.butteraugli_distance = 2.0;
  DecompressParams dparams;

  // Try each xsize mod kBlockDim to verify right border handling.
  CodecInOut io2;
  const size_t size2 = Roundtrip(&io, cparams, dparams, &pool, &io2);

  CodecInOut io3;
  const size_t size3 = Roundtrip(&io, cparams, dparams, &pool, &io3);

  // Exact same compressed size.
  EXPECT_EQ(size2, size3);

  // Exact same distance.
  const float dist2 = ButteraugliDistance(io, io2, cparams.ba_params,
                                          /*distmap=*/nullptr, &pool);
  const float dist3 = ButteraugliDistance(io, io3, cparams.ba_params,
                                          /*distmap=*/nullptr, &pool);
  EXPECT_EQ(dist2, dist3);
}

#if JXL_TEST_NL

TEST(JxlTest, RoundtripSmallNL) {
  ThreadPool* pool = nullptr;
  const PaddedBytes orig =
      ReadTestData("wesaturate/500px/u76c0g_bliznaca_srgb8.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, pool));
  io.ShrinkTo(io.xsize() / 8, io.ysize() / 8);

  CompressParams cparams;
  cparams.butteraugli_distance = 1.0;
  DecompressParams dparams;

  CodecInOut io2;
  EXPECT_LE(Roundtrip(&io, cparams, dparams, pool, &io2), 1500);
  EXPECT_LE(ButteraugliDistance(io, io2, cparams.ba_params,
                                /*distmap=*/nullptr, pool),
            1.7);
}

#endif

TEST(JxlTest, RoundtripNoGaborishNoAR) {
  ThreadPool* pool = nullptr;
  const PaddedBytes orig =
      ReadTestData("wesaturate/500px/u76c0g_bliznaca_srgb8.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, pool));

  CompressParams cparams;
  cparams.gaborish = Override::kOff;
  cparams.epf = 0;
  cparams.butteraugli_distance = 1.0;
  DecompressParams dparams;

  CodecInOut io2;
  EXPECT_LE(Roundtrip(&io, cparams, dparams, pool, &io2), 40000);
  EXPECT_LE(ButteraugliDistance(io, io2, cparams.ba_params,
                                /*distmap=*/nullptr, pool),
            2.5);
}

TEST(JxlTest, RoundtripSmallNoGaborish) {
  ThreadPool* pool = nullptr;
  const PaddedBytes orig =
      ReadTestData("wesaturate/500px/u76c0g_bliznaca_srgb8.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, pool));
  io.ShrinkTo(io.xsize() / 8, io.ysize() / 8);

  CompressParams cparams;
  cparams.gaborish = Override::kOff;
  cparams.butteraugli_distance = 1.0;
  DecompressParams dparams;

  CodecInOut io2;
  EXPECT_LE(Roundtrip(&io, cparams, dparams, pool, &io2), 900);
  EXPECT_LE(ButteraugliDistance(io, io2, cparams.ba_params,
                                /*distmap=*/nullptr, pool),
            1.7);
}

TEST(JxlTest, RoundtripSmallPatchesAlpha) {
  ThreadPool* pool = nullptr;
  CodecInOut io;
  io.metadata.m.color_encoding = ColorEncoding::LinearSRGB();
  Image3F black_with_small_lines(256, 256);
  ImageF alpha(black_with_small_lines.xsize(), black_with_small_lines.ysize());
  ZeroFillImage(&black_with_small_lines);
  // This pattern should be picked up by the patch detection heuristics.
  for (size_t y = 0; y < black_with_small_lines.ysize(); y++) {
    float* JXL_RESTRICT row = black_with_small_lines.PlaneRow(1, y);
    for (size_t x = 0; x < black_with_small_lines.xsize(); x++) {
      if (x % 4 == 0 && (y / 32) % 4 == 0) row[x] = 127.0f;
    }
  }
  io.metadata.m.SetAlphaBits(8);
  io.SetFromImage(std::move(black_with_small_lines),
                  ColorEncoding::LinearSRGB());
  FillImage(1.0f, &alpha);
  io.Main().SetAlpha(std::move(alpha), /*alpha_is_premultiplied=*/false);

  CompressParams cparams;
  cparams.speed_tier = SpeedTier::kSquirrel;
  cparams.butteraugli_distance = 0.1f;
  DecompressParams dparams;

  CodecInOut io2;
  EXPECT_LE(Roundtrip(&io, cparams, dparams, pool, &io2), 2000);
  EXPECT_LE(ButteraugliDistance(io, io2, cparams.ba_params,
                                /*distmap=*/nullptr, pool),
            0.5f);
}

TEST(JxlTest, RoundtripSmallPatches) {
  ThreadPool* pool = nullptr;
  CodecInOut io;
  io.metadata.m.color_encoding = ColorEncoding::LinearSRGB();
  Image3F black_with_small_lines(256, 256);
  ZeroFillImage(&black_with_small_lines);
  // This pattern should be picked up by the patch detection heuristics.
  for (size_t y = 0; y < black_with_small_lines.ysize(); y++) {
    float* JXL_RESTRICT row = black_with_small_lines.PlaneRow(1, y);
    for (size_t x = 0; x < black_with_small_lines.xsize(); x++) {
      if (x % 4 == 0 && (y / 32) % 4 == 0) row[x] = 127.0f;
    }
  }
  io.SetFromImage(std::move(black_with_small_lines),
                  ColorEncoding::LinearSRGB());

  CompressParams cparams;
  cparams.speed_tier = SpeedTier::kSquirrel;
  cparams.butteraugli_distance = 0.1f;
  DecompressParams dparams;

  CodecInOut io2;
  EXPECT_LE(Roundtrip(&io, cparams, dparams, pool, &io2), 2000);
  EXPECT_LE(ButteraugliDistance(io, io2, cparams.ba_params,
                                /*distmap=*/nullptr, pool),
            0.5f);
}

// Test header encoding of original bits per sample
TEST(JxlTest, RoundtripImageBundleOriginalBits) {
  ThreadPool* pool = nullptr;

  // Image does not matter, only io.metadata.m and io2.metadata.m are tested.
  Image3F image(1, 1);
  ZeroFillImage(&image);
  CodecInOut io;
  io.metadata.m.color_encoding = ColorEncoding::LinearSRGB();
  io.SetFromImage(std::move(image), ColorEncoding::LinearSRGB());

  CompressParams cparams;
  DecompressParams dparams;

  // Test unsigned integers from 1 to 32 bits
  for (uint32_t bit_depth = 1; bit_depth <= 32; bit_depth++) {
    if (bit_depth == 32) {
      // TODO(lode): allow testing 32, however the code below ends up in
      // enc_modular which does not support 32. We only want to test the header
      // encoding though, so try without modular.
      break;
    }

    io.metadata.m.SetUintSamples(bit_depth);
    CodecInOut io2;
    Roundtrip(&io, cparams, dparams, pool, &io2);

    EXPECT_EQ(bit_depth, io2.metadata.m.bit_depth.bits_per_sample);
    EXPECT_FALSE(io2.metadata.m.bit_depth.floating_point_sample);
    EXPECT_EQ(0, io2.metadata.m.bit_depth.exponent_bits_per_sample);
    EXPECT_EQ(0, io2.metadata.m.GetAlphaBits());
  }

  // Test various existing and non-existing floating point formats
  for (uint32_t bit_depth = 8; bit_depth <= 32; bit_depth++) {
    if (bit_depth != 32) {
      // TODO: test other float types once they work
      break;
    }

    uint32_t exponent_bit_depth;
    if (bit_depth < 10) {
      exponent_bit_depth = 2;
    } else if (bit_depth < 12) {
      exponent_bit_depth = 3;
    } else if (bit_depth < 16) {
      exponent_bit_depth = 4;
    } else if (bit_depth < 20) {
      exponent_bit_depth = 5;
    } else if (bit_depth < 24) {
      exponent_bit_depth = 6;
    } else if (bit_depth < 28) {
      exponent_bit_depth = 7;
    } else {
      exponent_bit_depth = 8;
    }

    io.metadata.m.bit_depth.bits_per_sample = bit_depth;
    io.metadata.m.bit_depth.floating_point_sample = true;
    io.metadata.m.bit_depth.exponent_bits_per_sample = exponent_bit_depth;

    CodecInOut io2;
    Roundtrip(&io, cparams, dparams, pool, &io2);

    EXPECT_EQ(bit_depth, io2.metadata.m.bit_depth.bits_per_sample);
    EXPECT_TRUE(io2.metadata.m.bit_depth.floating_point_sample);
    EXPECT_EQ(exponent_bit_depth,
              io2.metadata.m.bit_depth.exponent_bits_per_sample);
    EXPECT_EQ(0, io2.metadata.m.GetAlphaBits());
  }
}

TEST(JxlTest, RoundtripGrayscale) {
  ThreadPool* pool = nullptr;
  const PaddedBytes orig =
      ReadTestData("wesaturate/500px/cvo9xd_keong_macan_grayscale.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, pool));
  ASSERT_NE(io.xsize(), 0);
  io.ShrinkTo(128, 128);
  EXPECT_TRUE(io.Main().IsGray());
  EXPECT_EQ(8, io.metadata.m.bit_depth.bits_per_sample);
  EXPECT_FALSE(io.metadata.m.bit_depth.floating_point_sample);
  EXPECT_EQ(0, io.metadata.m.bit_depth.exponent_bits_per_sample);
  EXPECT_TRUE(io.metadata.m.color_encoding.tf.IsSRGB());

  PassesEncoderState enc_state;
  AuxOut* aux_out = nullptr;

  {
    CompressParams cparams;
    cparams.butteraugli_distance = 1.0;
    DecompressParams dparams;

    PaddedBytes compressed;
    EXPECT_TRUE(
        EncodeFile(cparams, &io, &enc_state, &compressed, aux_out, pool));
    CodecInOut io2;
    EXPECT_TRUE(DecodeFile(dparams, compressed, &io2, pool));
    EXPECT_TRUE(io2.Main().IsGray());

    EXPECT_LE(compressed.size(), 7000);
    EXPECT_LE(ButteraugliDistance(io, io2, cparams.ba_params,
                                  /*distmap=*/nullptr, pool),
              1.7777777);
  }

  // Test with larger butteraugli distance and other settings enabled so
  // different jxl codepaths trigger.
  {
    CompressParams cparams;
    cparams.butteraugli_distance = 8.0;
    DecompressParams dparams;

    PaddedBytes compressed;
    EXPECT_TRUE(
        EncodeFile(cparams, &io, &enc_state, &compressed, aux_out, pool));
    CodecInOut io2;
    EXPECT_TRUE(DecodeFile(dparams, compressed, &io2, pool));
    EXPECT_TRUE(io2.Main().IsGray());

    EXPECT_LE(compressed.size(), 1300);
    EXPECT_LE(ButteraugliDistance(io, io2, cparams.ba_params,
                                  /*distmap=*/nullptr, pool),
              9.0);
  }
}

TEST(JxlTest, RoundtripAlpha) {
  ThreadPool* pool = nullptr;
  const PaddedBytes orig =
      ReadTestData("wesaturate/500px/tmshre_riaphotographs_alpha.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, pool));

  ASSERT_NE(io.xsize(), 0);
  ASSERT_TRUE(io.metadata.m.HasAlpha());
  ASSERT_TRUE(io.Main().HasAlpha());
  io.ShrinkTo(128, 128);

  CompressParams cparams;
  cparams.butteraugli_distance = 1.0;
  DecompressParams dparams;

  EXPECT_EQ(8, io.metadata.m.bit_depth.bits_per_sample);
  EXPECT_FALSE(io.metadata.m.bit_depth.floating_point_sample);
  EXPECT_EQ(0, io.metadata.m.bit_depth.exponent_bits_per_sample);
  EXPECT_TRUE(io.metadata.m.color_encoding.tf.IsSRGB());
  PassesEncoderState enc_state;
  AuxOut* aux_out = nullptr;
  PaddedBytes compressed;
  EXPECT_TRUE(EncodeFile(cparams, &io, &enc_state, &compressed, aux_out, pool));
  CodecInOut io2;
  EXPECT_TRUE(DecodeFile(dparams, compressed, &io2, pool));

  EXPECT_LE(compressed.size(), 5500);

  // TODO(robryk): Fix the following line in presence of different alpha_bits in
  // the two contexts.
  // EXPECT_TRUE(SamePixels(io.Main().alpha(), io2.Main().alpha()));
  // TODO(robryk): Fix the distance estimate used in the encoder.
  EXPECT_LE(ButteraugliDistance(io, io2, cparams.ba_params,
                                /*distmap=*/nullptr, pool),
            6.3);
}

TEST(JxlTest, RoundtripAlphaNonMultipleOf8) {
  ThreadPool* pool = nullptr;
  const PaddedBytes orig =
      ReadTestData("wesaturate/500px/tmshre_riaphotographs_alpha.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, pool));

  ASSERT_NE(io.xsize(), 0);
  ASSERT_TRUE(io.metadata.m.HasAlpha());
  ASSERT_TRUE(io.Main().HasAlpha());
  io.ShrinkTo(12, 12);

  CompressParams cparams;
  cparams.butteraugli_distance = 1.0;
  DecompressParams dparams;

  EXPECT_EQ(8, io.metadata.m.bit_depth.bits_per_sample);
  EXPECT_FALSE(io.metadata.m.bit_depth.floating_point_sample);
  EXPECT_EQ(0, io.metadata.m.bit_depth.exponent_bits_per_sample);
  EXPECT_TRUE(io.metadata.m.color_encoding.tf.IsSRGB());
  PassesEncoderState enc_state;
  AuxOut* aux_out = nullptr;
  PaddedBytes compressed;
  EXPECT_TRUE(EncodeFile(cparams, &io, &enc_state, &compressed, aux_out, pool));
  CodecInOut io2;
  EXPECT_TRUE(DecodeFile(dparams, compressed, &io2, pool));

  EXPECT_LE(compressed.size(), 200);

  // TODO(robryk): Fix the following line in presence of different alpha_bits in
  // the two contexts.
  // EXPECT_TRUE(SamePixels(io.Main().alpha(), io2.Main().alpha()));
  // TODO(robryk): Fix the distance estimate used in the encoder.
  EXPECT_LE(ButteraugliDistance(io, io2, cparams.ba_params,
                                /*distmap=*/nullptr, pool),
            6.3);
}

TEST(JxlTest, RoundtripAlpha16) {
  ThreadPoolInternal pool(4);

  size_t xsize = 1200, ysize = 160;
  Image3F color(xsize, ysize);
  ImageF alpha(xsize, ysize);
  // Generate 16-bit pattern that uses various colors and alpha values.
  for (size_t y = 0; y < ysize; y++) {
    for (size_t x = 0; x < xsize; x++) {
      color.PlaneRow(0, y)[x] = (y * 65535 / ysize) * (1.0f / 65535);
      color.PlaneRow(1, y)[x] = (x * 65535 / xsize) * (1.0f / 65535);
      color.PlaneRow(2, y)[x] =
          ((y + x) * 65535 / (xsize + ysize)) * (1.0f / 65535);
      alpha.Row(y)[x] = (x * 65535 / xsize) * (1.0f / 65535);
    }
  }
  const bool is_gray = false;
  CodecInOut io;
  io.metadata.m.SetUintSamples(16);
  io.metadata.m.SetAlphaBits(16);
  io.metadata.m.color_encoding = ColorEncoding::SRGB(is_gray);
  io.SetFromImage(std::move(color), io.metadata.m.color_encoding);
  io.Main().SetAlpha(std::move(alpha), /*alpha_is_premultiplied=*/false);

  // The image is wider than 512 pixels to ensure multiple groups are tested.

  ASSERT_NE(io.xsize(), 0);
  ASSERT_TRUE(io.metadata.m.HasAlpha());
  ASSERT_TRUE(io.Main().HasAlpha());

  CompressParams cparams;
  cparams.butteraugli_distance = 0.5;
  // Prevent the test to be too slow, does not affect alpha
  cparams.speed_tier = SpeedTier::kSquirrel;
  DecompressParams dparams;

  io.metadata.m.SetUintSamples(16);
  EXPECT_TRUE(io.metadata.m.color_encoding.tf.IsSRGB());
  PassesEncoderState enc_state;
  AuxOut* aux_out = nullptr;
  PaddedBytes compressed;
  EXPECT_TRUE(
      EncodeFile(cparams, &io, &enc_state, &compressed, aux_out, &pool));
  CodecInOut io2;
  EXPECT_TRUE(DecodeFile(dparams, compressed, &io2, &pool));

  EXPECT_TRUE(SamePixels(*io.Main().alpha(), *io2.Main().alpha()));
}

namespace {
CompressParams CParamsForLossless() {
  CompressParams cparams;
  cparams.modular_mode = true;
  cparams.color_transform = jxl::ColorTransform::kNone;
  cparams.quality_pair = {100, 100};
  cparams.options.predictor = {Predictor::Weighted};
  return cparams;
}
}  // namespace

TEST(JxlTest, JXL_SLOW_TEST(RoundtripLossless8)) {
  ThreadPoolInternal pool(8);
  const PaddedBytes orig =
      ReadTestData("wesaturate/500px/tmshre_riaphotographs_srgb8.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, &pool));

  CompressParams cparams = CParamsForLossless();
  DecompressParams dparams;

  CodecInOut io2;
  EXPECT_LE(Roundtrip(&io, cparams, dparams, &pool, &io2), 3500000);
  // If this test fails with a very close to 0.0 but not exactly 0.0 butteraugli
  // distance, then there is likely a floating point issue, that could be
  // happening either in io or io2. The values of io are generated by
  // external_image.cc, and those in io2 by the jxl decoder. If they use
  // slightly different floating point operations (say, one casts int to float
  // while other divides the int through 255.0f and later multiplies it by
  // 255 again) they will get slightly different values. To fix, ensure both
  // sides do the following formula for converting integer range 0-255 to
  // floating point range 0.0f-255.0f: static_cast<float>(i)
  // without any further intermediate operations.
  // Note that this precision issue is not a problem in practice if the values
  // are equal when rounded to 8-bit int, but currently full exact precision is
  // tested.
  EXPECT_EQ(0.0, ButteraugliDistance(io, io2, cparams.ba_params,
                                     /*distmap=*/nullptr, &pool));
}

TEST(JxlTest, JXL_SLOW_TEST(RoundtripLosslessNoEncoderFastPath)) {
  ThreadPoolInternal pool(8);
  const PaddedBytes orig =
      ReadTestData("wesaturate/500px/tmshre_riaphotographs_srgb8.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, &pool));

  CompressParams cparams = CParamsForLossless();
  cparams.options.skip_encoder_fast_path = true;
  DecompressParams dparams;

  CodecInOut io2;
  EXPECT_LE(Roundtrip(&io, cparams, dparams, &pool, &io2), 3500000);
  EXPECT_EQ(0.0, ButteraugliDistance(io, io2, cparams.ba_params,
                                     /*distmap=*/nullptr, &pool));
}

TEST(JxlTest, JXL_SLOW_TEST(RoundtripLossless8Falcon)) {
  ThreadPoolInternal pool(8);
  const PaddedBytes orig =
      ReadTestData("wesaturate/500px/tmshre_riaphotographs_srgb8.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, &pool));

  CompressParams cparams = CParamsForLossless();
  cparams.speed_tier = SpeedTier::kFalcon;
  DecompressParams dparams;

  CodecInOut io2;
  EXPECT_LE(Roundtrip(&io, cparams, dparams, &pool, &io2), 3500000);
  EXPECT_EQ(0.0, ButteraugliDistance(io, io2, cparams.ba_params,
                                     /*distmap=*/nullptr, &pool));
}

TEST(JxlTest, RoundtripLossless8Alpha) {
  ThreadPool* pool = nullptr;
  const PaddedBytes orig =
      ReadTestData("wesaturate/500px/tmshre_riaphotographs_alpha.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, pool));
  EXPECT_EQ(8, io.metadata.m.GetAlphaBits());
  EXPECT_EQ(8, io.metadata.m.bit_depth.bits_per_sample);
  EXPECT_FALSE(io.metadata.m.bit_depth.floating_point_sample);
  EXPECT_EQ(0, io.metadata.m.bit_depth.exponent_bits_per_sample);

  CompressParams cparams = CParamsForLossless();
  DecompressParams dparams;

  CodecInOut io2;
  EXPECT_LE(Roundtrip(&io, cparams, dparams, pool, &io2), 350000);
  // If fails, see note about floating point in RoundtripLossless8.
  EXPECT_EQ(0.0, ButteraugliDistance(io, io2, cparams.ba_params,
                                     /*distmap=*/nullptr, pool));
  EXPECT_TRUE(SamePixels(*io.Main().alpha(), *io2.Main().alpha()));
  EXPECT_EQ(8, io2.metadata.m.GetAlphaBits());
  EXPECT_EQ(8, io2.metadata.m.bit_depth.bits_per_sample);
  EXPECT_FALSE(io2.metadata.m.bit_depth.floating_point_sample);
  EXPECT_EQ(0, io2.metadata.m.bit_depth.exponent_bits_per_sample);
}

TEST(JxlTest, RoundtripLossless16Alpha) {
  ThreadPool* pool = nullptr;

  size_t xsize = 1200, ysize = 160;
  Image3F color(xsize, ysize);
  ImageF alpha(xsize, ysize);
  // Generate 16-bit pattern that uses various colors and alpha values.
  for (size_t y = 0; y < ysize; y++) {
    for (size_t x = 0; x < xsize; x++) {
      color.PlaneRow(0, y)[x] = (y * 65535 / ysize) * (1.0f / 65535);
      color.PlaneRow(1, y)[x] = (x * 65535 / xsize) * (1.0f / 65535);
      color.PlaneRow(2, y)[x] =
          ((y + x) * 65535 / (xsize + ysize)) * (1.0f / 65535);
      alpha.Row(y)[x] = (x * 65535 / xsize) * (1.0f / 65535);
    }
  }
  const bool is_gray = false;
  CodecInOut io;
  io.metadata.m.SetUintSamples(16);
  io.metadata.m.SetAlphaBits(16);
  io.metadata.m.color_encoding = ColorEncoding::SRGB(is_gray);
  io.SetFromImage(std::move(color), io.metadata.m.color_encoding);
  io.Main().SetAlpha(std::move(alpha), /*alpha_is_premultiplied=*/false);

  EXPECT_EQ(16, io.metadata.m.GetAlphaBits());
  EXPECT_EQ(16, io.metadata.m.bit_depth.bits_per_sample);
  EXPECT_FALSE(io.metadata.m.bit_depth.floating_point_sample);
  EXPECT_EQ(0, io.metadata.m.bit_depth.exponent_bits_per_sample);

  CompressParams cparams = CParamsForLossless();
  DecompressParams dparams;

  CodecInOut io2;
  EXPECT_LE(Roundtrip(&io, cparams, dparams, pool, &io2), 7100);
  // If this test fails with a very close to 0.0 but not exactly 0.0 butteraugli
  // distance, then there is likely a floating point issue, that could be
  // happening either in io or io2. The values of io are generated by
  // external_image.cc, and those in io2 by the jxl decoder. If they use
  // slightly different floating point operations (say, one does "i / 257.0f"
  // while the other does "i * (1.0f / 257)" they will get slightly different
  // values. To fix, ensure both sides do the following formula for converting
  // integer range 0-65535 to Image3F floating point range 0.0f-255.0f:
  // "i * (1.0f / 257)".
  // Note that this precision issue is not a problem in practice if the values
  // are equal when rounded to 16-bit int, but currently full exact precision is
  // tested.
  EXPECT_EQ(0.0, ButteraugliDistance(io, io2, cparams.ba_params,
                                     /*distmap=*/nullptr, pool));
  EXPECT_TRUE(SamePixels(*io.Main().alpha(), *io2.Main().alpha()));
  EXPECT_EQ(16, io2.metadata.m.GetAlphaBits());
  EXPECT_EQ(16, io2.metadata.m.bit_depth.bits_per_sample);
  EXPECT_FALSE(io2.metadata.m.bit_depth.floating_point_sample);
  EXPECT_EQ(0, io2.metadata.m.bit_depth.exponent_bits_per_sample);
}

TEST(JxlTest, RoundtripLossless16AlphaNotMisdetectedAs8Bit) {
  ThreadPool* pool = nullptr;

  size_t xsize = 128, ysize = 128;
  Image3F color(xsize, ysize);
  ImageF alpha(xsize, ysize);
  // All 16-bit values, both color and alpha, of this image are below 64.
  // This allows testing if a code path wrongly concludes it's an 8-bit instead
  // of 16-bit image (or even 6-bit).
  for (size_t y = 0; y < ysize; y++) {
    for (size_t x = 0; x < xsize; x++) {
      color.PlaneRow(0, y)[x] = (y * 64 / ysize) * (1.0f / 65535);
      color.PlaneRow(1, y)[x] = (x * 64 / xsize) * (1.0f / 65535);
      color.PlaneRow(2, y)[x] =
          ((y + x) * 64 / (xsize + ysize)) * (1.0f / 65535);
      alpha.Row(y)[x] = (64 * x / xsize) * (1.0f / 65535);
    }
  }
  const bool is_gray = false;
  CodecInOut io;
  io.metadata.m.SetUintSamples(16);
  io.metadata.m.SetAlphaBits(16);
  io.metadata.m.color_encoding = ColorEncoding::SRGB(is_gray);
  io.SetFromImage(std::move(color), io.metadata.m.color_encoding);
  io.Main().SetAlpha(std::move(alpha), /*alpha_is_premultiplied=*/false);

  EXPECT_EQ(16, io.metadata.m.GetAlphaBits());
  EXPECT_EQ(16, io.metadata.m.bit_depth.bits_per_sample);
  EXPECT_FALSE(io.metadata.m.bit_depth.floating_point_sample);
  EXPECT_EQ(0, io.metadata.m.bit_depth.exponent_bits_per_sample);

  CompressParams cparams = CParamsForLossless();
  DecompressParams dparams;

  CodecInOut io2;
  EXPECT_LE(Roundtrip(&io, cparams, dparams, pool, &io2), 3100);
  EXPECT_EQ(16, io2.metadata.m.GetAlphaBits());
  EXPECT_EQ(16, io2.metadata.m.bit_depth.bits_per_sample);
  EXPECT_FALSE(io2.metadata.m.bit_depth.floating_point_sample);
  EXPECT_EQ(0, io2.metadata.m.bit_depth.exponent_bits_per_sample);
  // If fails, see note about floating point in RoundtripLossless8.
  EXPECT_EQ(0.0, ButteraugliDistance(io, io2, cparams.ba_params,
                                     /*distmap=*/nullptr, pool));
  EXPECT_TRUE(SamePixels(*io.Main().alpha(), *io2.Main().alpha()));
}

TEST(JxlTest, RoundtripDots) {
  ThreadPool* pool = nullptr;
  const PaddedBytes orig =
      ReadTestData("wesaturate/500px/cvo9xd_keong_macan_srgb8.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, pool));

  ASSERT_NE(io.xsize(), 0);

  CompressParams cparams;
  cparams.dots = Override::kOn;
  cparams.butteraugli_distance = 0.04;
  cparams.speed_tier = SpeedTier::kSquirrel;
  DecompressParams dparams;

  EXPECT_EQ(8, io.metadata.m.bit_depth.bits_per_sample);
  EXPECT_EQ(0, io.metadata.m.bit_depth.exponent_bits_per_sample);
  EXPECT_FALSE(io.metadata.m.bit_depth.floating_point_sample);
  EXPECT_TRUE(io.metadata.m.color_encoding.tf.IsSRGB());
  PassesEncoderState enc_state;
  AuxOut* aux_out = nullptr;
  PaddedBytes compressed;
  EXPECT_TRUE(EncodeFile(cparams, &io, &enc_state, &compressed, aux_out, pool));
  CodecInOut io2;
  EXPECT_TRUE(DecodeFile(dparams, compressed, &io2, pool));

  EXPECT_LE(compressed.size(), 400000);
  EXPECT_LE(ButteraugliDistance(io, io2, cparams.ba_params,
                                /*distmap=*/nullptr, pool),
            2.2);
}

TEST(JxlTest, RoundtripNoise) {
  ThreadPool* pool = nullptr;
  const PaddedBytes orig =
      ReadTestData("wesaturate/500px/cvo9xd_keong_macan_srgb8.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, pool));

  ASSERT_NE(io.xsize(), 0);

  CompressParams cparams;
  cparams.noise = Override::kOn;
  cparams.speed_tier = SpeedTier::kSquirrel;
  DecompressParams dparams;

  EXPECT_EQ(8, io.metadata.m.bit_depth.bits_per_sample);
  EXPECT_EQ(0, io.metadata.m.bit_depth.exponent_bits_per_sample);
  EXPECT_FALSE(io.metadata.m.bit_depth.floating_point_sample);
  EXPECT_TRUE(io.metadata.m.color_encoding.tf.IsSRGB());
  PassesEncoderState enc_state;
  AuxOut* aux_out = nullptr;
  PaddedBytes compressed;
  EXPECT_TRUE(EncodeFile(cparams, &io, &enc_state, &compressed, aux_out, pool));
  CodecInOut io2;
  EXPECT_TRUE(DecodeFile(dparams, compressed, &io2, pool));

  EXPECT_LE(compressed.size(), 40000);
  EXPECT_LE(ButteraugliDistance(io, io2, cparams.ba_params,
                                /*distmap=*/nullptr, pool),
            2.2);
}

TEST(JxlTest, RoundtripLossless8Gray) {
  ThreadPool* pool = nullptr;
  const PaddedBytes orig =
      ReadTestData("wesaturate/500px/cvo9xd_keong_macan_grayscale.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, pool));

  CompressParams cparams = CParamsForLossless();
  DecompressParams dparams;

  EXPECT_TRUE(io.Main().IsGray());
  EXPECT_EQ(8, io.metadata.m.bit_depth.bits_per_sample);
  EXPECT_FALSE(io.metadata.m.bit_depth.floating_point_sample);
  EXPECT_EQ(0, io.metadata.m.bit_depth.exponent_bits_per_sample);
  CodecInOut io2;
  EXPECT_LE(Roundtrip(&io, cparams, dparams, pool, &io2), 130000);
  // If fails, see note about floating point in RoundtripLossless8.
  EXPECT_EQ(0.0, ButteraugliDistance(io, io2, cparams.ba_params,
                                     /*distmap=*/nullptr, pool));
  EXPECT_TRUE(io2.Main().IsGray());
  EXPECT_EQ(8, io2.metadata.m.bit_depth.bits_per_sample);
  EXPECT_FALSE(io2.metadata.m.bit_depth.floating_point_sample);
  EXPECT_EQ(0, io2.metadata.m.bit_depth.exponent_bits_per_sample);
}

#if JPEGXL_ENABLE_GIF

TEST(JxlTest, RoundtripAnimation) {
  ThreadPool* pool = nullptr;
  const PaddedBytes orig = ReadTestData("jxl/traffic_light.gif");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, pool));
  ASSERT_EQ(4, io.frames.size());

  CompressParams cparams;
  DecompressParams dparams;
  CodecInOut io2;
  EXPECT_LE(Roundtrip(&io, cparams, dparams, pool, &io2), 3000);

  EXPECT_EQ(io2.frames.size(), io.frames.size());
  test::CoalesceGIFAnimationWithAlpha(&io);
  EXPECT_LE(ButteraugliDistance(io, io2, cparams.ba_params,
                                /*distmap=*/nullptr, pool),
#if JXL_HIGH_PRECISION
            1.55);
#else
            1.75);
#endif
}

TEST(JxlTest, RoundtripLosslessAnimation) {
  ThreadPool* pool = nullptr;
  const PaddedBytes orig = ReadTestData("jxl/traffic_light.gif");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, pool));
  ASSERT_EQ(4, io.frames.size());

  CompressParams cparams = CParamsForLossless();
  DecompressParams dparams;
  CodecInOut io2;
  EXPECT_LE(Roundtrip(&io, cparams, dparams, pool, &io2), 1200);

  EXPECT_EQ(io2.frames.size(), io.frames.size());
  test::CoalesceGIFAnimationWithAlpha(&io);
  EXPECT_LE(ButteraugliDistance(io, io2, cparams.ba_params,
                                /*distmap=*/nullptr, pool),
            5e-4);
}

#endif  // JPEGXL_ENABLE_GIF

#if JPEGXL_ENABLE_JPEG

namespace {

jxl::Status DecompressJxlToJPEGForTest(
    const jpegxl::tools::JpegXlContainer& container, jxl::ThreadPool* pool,
    jxl::PaddedBytes* output) {
  output->clear();
  jxl::Span<const uint8_t> compressed(container.codestream,
                                      container.codestream_size);

  JXL_RETURN_IF_ERROR(compressed.size() >= 2);

  // JXL case
  // Decode to DCT when possible and generate a JPG file.
  jxl::CodecInOut io;
  jxl::DecompressParams params;
  params.keep_dct = true;
  if (!jpegxl::tools::DecodeJpegXlToJpeg(params, container, &io, pool)) {
    return JXL_FAILURE("Failed to decode JXL to JPEG");
  }
  io.jpeg_quality = 95;
  if (!EncodeImageJPG(&io, jxl::JpegEncoder::kLibJpeg, io.jpeg_quality,
                      jxl::YCbCrChromaSubsampling(), pool, output,
                      jxl::DecodeTarget::kQuantizedCoeffs)) {
    return JXL_FAILURE("Failed to generate JPEG");
  }
  return true;
}

}  // namespace

size_t RoundtripJpeg(const PaddedBytes& jpeg_in, ThreadPool* pool) {
  CodecInOut io;
  io.dec_target = jxl::DecodeTarget::kQuantizedCoeffs;
  EXPECT_TRUE(SetFromBytes(Span<const uint8_t>(jpeg_in), &io, pool));
  CompressParams cparams;
  cparams.color_transform = jxl::ColorTransform::kYCbCr;

  PassesEncoderState passes_enc_state;
  PaddedBytes compressed, codestream;

  EXPECT_TRUE(EncodeFile(cparams, &io, &passes_enc_state, &codestream,
                         /*aux_out=*/nullptr, pool));
  jpegxl::tools::JpegXlContainer enc_container;
  enc_container.codestream = codestream.data();
  enc_container.codestream_size = codestream.size();
  jpeg::JPEGData data_in = *io.Main().jpeg_data;
  jxl::PaddedBytes jpeg_data;
  EXPECT_TRUE(EncodeJPEGData(data_in, &jpeg_data));
  enc_container.jpeg_reconstruction = jpeg_data.data();
  enc_container.jpeg_reconstruction_size = jpeg_data.size();
  EXPECT_TRUE(EncodeJpegXlContainerOneShot(enc_container, &compressed));

  jpegxl::tools::JpegXlContainer container;
  EXPECT_TRUE(DecodeJpegXlContainerOneShot(compressed.data(), compressed.size(),
                                           &container));
  PaddedBytes out;
  EXPECT_TRUE(DecompressJxlToJPEGForTest(container, pool, &out));
  EXPECT_EQ(out.size(), jpeg_in.size());
  size_t failures = 0;
  for (size_t i = 0; i < std::min(out.size(), jpeg_in.size()); i++) {
    if (out[i] != jpeg_in[i]) {
      EXPECT_EQ(out[i], jpeg_in[i])
          << "byte mismatch " << i << " " << out[i] << " != " << jpeg_in[i];
      if (++failures > 4) {
        return compressed.size();
      }
    }
  }
  return compressed.size();
}

TEST(JxlTest, RoundtripJpegRecompression444) {
  ThreadPoolInternal pool(8);
  const PaddedBytes orig =
      ReadTestData("imagecompression.info/flower_foveon.png.im_q85_444.jpg");
  // JPEG size is 326'916 bytes.
  EXPECT_LE(RoundtripJpeg(orig, &pool), 256000);
}

TEST(JxlTest, RoundtripJpegRecompressionToPixels) {
  ThreadPoolInternal pool(8);
  const PaddedBytes orig =
      ReadTestData("imagecompression.info/flower_foveon.png.im_q85_444.jpg");
  CodecInOut io;
  io.dec_target = jxl::DecodeTarget::kQuantizedCoeffs;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, &pool));

  CodecInOut io2;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io2, &pool));

  CompressParams cparams;
  cparams.color_transform = jxl::ColorTransform::kYCbCr;

  DecompressParams dparams;

  CodecInOut io3;
  Roundtrip(&io, cparams, dparams, &pool, &io3);

  // TODO(eustas): investigate, why SJPEG and JpegRecompression pixels are
  // different.
  EXPECT_GE(1.8, ButteraugliDistance(io2, io3, cparams.ba_params,
                                     /*distmap=*/nullptr, &pool));
}

TEST(JxlTest, RoundtripJpegRecompressionToPixels420) {
  ThreadPoolInternal pool(8);
  const PaddedBytes orig =
      ReadTestData("imagecompression.info/flower_foveon.png.im_q85_420.jpg");
  CodecInOut io;
  io.dec_target = jxl::DecodeTarget::kQuantizedCoeffs;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, &pool));

  CodecInOut io2;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io2, &pool));

  CompressParams cparams;
  cparams.color_transform = jxl::ColorTransform::kYCbCr;

  DecompressParams dparams;

  CodecInOut io3;
  Roundtrip(&io, cparams, dparams, &pool, &io3);

  EXPECT_GE(1.5, ButteraugliDistance(io2, io3, cparams.ba_params,
                                     /*distmap=*/nullptr, &pool));
}

TEST(JxlTest, RoundtripJpegRecompressionToPixels_asymmetric) {
  ThreadPoolInternal pool(8);
  const PaddedBytes orig = ReadTestData(
      "imagecompression.info/flower_foveon.png.im_q85_asymmetric.jpg");
  CodecInOut io;
  io.dec_target = jxl::DecodeTarget::kQuantizedCoeffs;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, &pool));

  CodecInOut io2;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io2, &pool));

  CompressParams cparams;
  cparams.color_transform = jxl::ColorTransform::kYCbCr;

  DecompressParams dparams;

  CodecInOut io3;
  Roundtrip(&io, cparams, dparams, &pool, &io3);

  EXPECT_GE(1.5, ButteraugliDistance(io2, io3, cparams.ba_params,
                                     /*distmap=*/nullptr, &pool));
}

TEST(JxlTest, RoundtripJpegRecompressionGray) {
  ThreadPoolInternal pool(8);
  const PaddedBytes orig =
      ReadTestData("imagecompression.info/flower_foveon.png.im_q85_gray.jpg");
  // JPEG size is 167'025 bytes.
  EXPECT_LE(RoundtripJpeg(orig, &pool), 140000);
}

TEST(JxlTest, RoundtripJpegRecompression420) {
  ThreadPoolInternal pool(8);
  const PaddedBytes orig =
      ReadTestData("imagecompression.info/flower_foveon.png.im_q85_420.jpg");
  // JPEG size is 226'018 bytes.
  EXPECT_LE(RoundtripJpeg(orig, &pool), 181050);
}

TEST(JxlTest, RoundtripJpegRecompression_luma_subsample) {
  ThreadPoolInternal pool(8);
  const PaddedBytes orig = ReadTestData(
      "imagecompression.info/flower_foveon.png.im_q85_luma_subsample.jpg");
  // JPEG size is 216'069 bytes.
  EXPECT_LE(RoundtripJpeg(orig, &pool), 181000);
}

TEST(JxlTest, RoundtripJpegRecompression444_12) {
  // 444 JPEG that has an interesting sampling-factor (1x2, 1x2, 1x2).
  ThreadPoolInternal pool(8);
  const PaddedBytes orig = ReadTestData(
      "imagecompression.info/flower_foveon.png.im_q85_444_1x2.jpg");
  // JPEG size is 329'942 bytes.
  EXPECT_LE(RoundtripJpeg(orig, &pool), 256000);
}

TEST(JxlTest, RoundtripJpegRecompression422) {
  ThreadPoolInternal pool(8);
  const PaddedBytes orig =
      ReadTestData("imagecompression.info/flower_foveon.png.im_q85_422.jpg");
  // JPEG size is 265'590 bytes.
  EXPECT_LE(RoundtripJpeg(orig, &pool), 209000);
}

TEST(JxlTest, RoundtripJpegRecompression440) {
  ThreadPoolInternal pool(8);
  const PaddedBytes orig =
      ReadTestData("imagecompression.info/flower_foveon.png.im_q85_440.jpg");
  // JPEG size is 262'249 bytes.
  EXPECT_LE(RoundtripJpeg(orig, &pool), 209000);
}

TEST(JxlTest, RoundtripJpegRecompression_asymmetric) {
  // 2x vertical downsample of one chroma channel, 2x horizontal downsample of
  // the other.
  ThreadPoolInternal pool(8);
  const PaddedBytes orig = ReadTestData(
      "imagecompression.info/flower_foveon.png.im_q85_asymmetric.jpg");
  // JPEG size is 262'249 bytes.
  EXPECT_LE(RoundtripJpeg(orig, &pool), 209000);
}

TEST(JxlTest, RoundtripJpegRecompression420Progr) {
  ThreadPoolInternal pool(8);
  const PaddedBytes orig = ReadTestData(
      "imagecompression.info/flower_foveon.png.im_q85_420_progr.jpg");
  EXPECT_LE(RoundtripJpeg(orig, &pool), 181000);
}

#endif  // JPEGXL_ENABLE_JPEG

TEST(JxlTest, RoundtripProgressive) {
  ThreadPoolInternal pool(4);
  const PaddedBytes orig =
      ReadTestData("imagecompression.info/flower_foveon.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, &pool));
  io.ShrinkTo(600, 1024);

  CompressParams cparams;
  DecompressParams dparams;

  cparams.butteraugli_distance = 1.0f;
  cparams.progressive_dc = true;
  cparams.responsive = true;
  cparams.progressive_mode = true;
  CodecInOut io2;
  EXPECT_LE(Roundtrip(&io, cparams, dparams, &pool, &io2), 40000);
  EXPECT_LE(ButteraugliDistance(io, io2, cparams.ba_params,
                                /*distmap=*/nullptr, &pool),
            4.0f);
}

}  // namespace
}  // namespace jxl
