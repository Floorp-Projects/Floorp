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

#ifndef LIB_JXL_TEST_UTILS_H_
#define LIB_JXL_TEST_UTILS_H_

// Macros and functions useful for tests.

#include <random>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "jxl/codestream_header.h"
#include "lib/jxl/aux_out_fwd.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/dec_file.h"
#include "lib/jxl/dec_params.h"
#include "lib/jxl/enc_external_image.h"
#include "lib/jxl/enc_file.h"
#include "lib/jxl/enc_params.h"

#ifdef JXL_DISABLE_SLOW_TESTS
#define JXL_SLOW_TEST(X) DISABLED_##X
#else
#define JXL_SLOW_TEST(X) X
#endif  // JXL_DISABLE_SLOW_TESTS

#ifdef THREAD_SANITIZER
#define JXL_TSAN_SLOW_TEST(X) DISABLED_##X
#else
#define JXL_TSAN_SLOW_TEST(X) X
#endif  // THREAD_SANITIZER

// googletest before 1.10 didn't define INSTANTIATE_TEST_SUITE_P() but instead
// used INSTANTIATE_TEST_CASE_P which is now deprecated.
#ifdef INSTANTIATE_TEST_SUITE_P
#define JXL_GTEST_INSTANTIATE_TEST_SUITE_P INSTANTIATE_TEST_SUITE_P
#else
#define JXL_GTEST_INSTANTIATE_TEST_SUITE_P INSTANTIATE_TEST_CASE_P
#endif

namespace jxl {
namespace test {

void JxlBasicInfoSetFromPixelFormat(JxlBasicInfo* basic_info,
                                    const JxlPixelFormat* pixel_format) {
  switch (pixel_format->data_type) {
    case JXL_TYPE_FLOAT:
      basic_info->bits_per_sample = 32;
      basic_info->exponent_bits_per_sample = 8;
      break;
    case JXL_TYPE_FLOAT16:
      basic_info->bits_per_sample = 16;
      basic_info->exponent_bits_per_sample = 5;
      break;
    case JXL_TYPE_UINT8:
      basic_info->bits_per_sample = 8;
      basic_info->exponent_bits_per_sample = 0;
      break;
    case JXL_TYPE_UINT16:
      basic_info->bits_per_sample = 16;
      basic_info->exponent_bits_per_sample = 0;
      break;
    case JXL_TYPE_UINT32:
      basic_info->bits_per_sample = 32;
      basic_info->exponent_bits_per_sample = 0;
      break;
    case JXL_TYPE_BOOLEAN:
      basic_info->bits_per_sample = 1;
      basic_info->exponent_bits_per_sample = 0;
      break;
  }
  if (pixel_format->num_channels == 2 || pixel_format->num_channels == 4) {
    basic_info->alpha_exponent_bits = 0;
    if (basic_info->bits_per_sample == 32) {
      basic_info->alpha_bits = 16;
    } else {
      basic_info->alpha_bits = basic_info->bits_per_sample;
    }
  } else {
    basic_info->alpha_exponent_bits = 0;
    basic_info->alpha_bits = 0;
  }
}

MATCHER_P(MatchesPrimariesAndTransferFunction, color_encoding, "") {
  return arg.primaries == color_encoding.primaries &&
         arg.tf.IsSame(color_encoding.tf);
}

MATCHER(MatchesPrimariesAndTransferFunction, "") {
  return testing::ExplainMatchResult(
      MatchesPrimariesAndTransferFunction(std::get<1>(arg)), std::get<0>(arg),
      result_listener);
}

// Returns compressed size [bytes].
size_t Roundtrip(const CodecInOut* io, const CompressParams& cparams,
                 const DecompressParams& dparams, ThreadPool* pool,
                 CodecInOut* JXL_RESTRICT io2, AuxOut* aux_out = nullptr) {
  PaddedBytes compressed;

  std::vector<ColorEncoding> original_metadata_encodings;
  std::vector<ColorEncoding> original_current_encodings;
  for (const ImageBundle& ib : io->frames) {
    // Remember original encoding, will be returned by decoder.
    original_metadata_encodings.push_back(ib.metadata()->color_encoding);
    // c_current should not change during encoding.
    original_current_encodings.push_back(ib.c_current());
  }

  std::unique_ptr<PassesEncoderState> enc_state =
      jxl::make_unique<PassesEncoderState>();
  EXPECT_TRUE(
      EncodeFile(cparams, io, enc_state.get(), &compressed, aux_out, pool));

  std::vector<ColorEncoding> metadata_encodings_1;
  for (const ImageBundle& ib1 : io->frames) {
    metadata_encodings_1.push_back(ib1.metadata()->color_encoding);
  }

  // Should still be in the same color space after encoding.
  EXPECT_THAT(metadata_encodings_1,
              testing::Pointwise(MatchesPrimariesAndTransferFunction(),
                                 original_metadata_encodings));

  EXPECT_TRUE(DecodeFile(dparams, compressed, io2, pool));

  std::vector<ColorEncoding> metadata_encodings_2;
  std::vector<ColorEncoding> current_encodings_2;
  for (const ImageBundle& ib2 : io2->frames) {
    metadata_encodings_2.push_back(ib2.metadata()->color_encoding);
    current_encodings_2.push_back(ib2.c_current());
  }

  EXPECT_THAT(io2->frames, testing::SizeIs(io->frames.size()));
  // We always produce the original color encoding if a color transform hook is
  // set.
  EXPECT_THAT(current_encodings_2,
              testing::Pointwise(MatchesPrimariesAndTransferFunction(),
                                 original_current_encodings));

  // Decoder returns the originals passed to the encoder.
  EXPECT_THAT(metadata_encodings_2,
              testing::Pointwise(MatchesPrimariesAndTransferFunction(),
                                 original_metadata_encodings));

  return compressed.size();
}

void CoalesceGIFAnimationWithAlpha(CodecInOut* io) {
  ImageBundle canvas = io->frames[0].Copy();
  for (size_t i = 1; i < io->frames.size(); i++) {
    const ImageBundle& frame = io->frames[i];
    ImageBundle rendered = canvas.Copy();
    for (size_t y = 0; y < frame.ysize(); y++) {
      float* row0 =
          rendered.color()->PlaneRow(0, frame.origin.y0 + y) + frame.origin.x0;
      float* row1 =
          rendered.color()->PlaneRow(1, frame.origin.y0 + y) + frame.origin.x0;
      float* row2 =
          rendered.color()->PlaneRow(2, frame.origin.y0 + y) + frame.origin.x0;
      float* rowa =
          rendered.alpha()->Row(frame.origin.y0 + y) + frame.origin.x0;
      const float* row0f = frame.color().PlaneRow(0, y);
      const float* row1f = frame.color().PlaneRow(1, y);
      const float* row2f = frame.color().PlaneRow(2, y);
      const float* rowaf = frame.alpha().Row(y);
      for (size_t x = 0; x < frame.xsize(); x++) {
        if (rowaf[x] != 0) {
          row0[x] = row0f[x];
          row1[x] = row1f[x];
          row2[x] = row2f[x];
          rowa[x] = rowaf[x];
        }
      }
    }
    if (frame.use_for_next_frame) {
      canvas = rendered.Copy();
    }
    io->frames[i] = std::move(rendered);
  }
}

// A POD descriptor of a ColorEncoding. Only used in tests as the return value
// of AllEncodings().
struct ColorEncodingDescriptor {
  ColorSpace color_space;
  WhitePoint white_point;
  Primaries primaries;
  TransferFunction tf;
  RenderingIntent rendering_intent;
};

static inline ColorEncoding ColorEncodingFromDescriptor(
    const ColorEncodingDescriptor& desc) {
  ColorEncoding c;
  c.SetColorSpace(desc.color_space);
  c.white_point = desc.white_point;
  c.primaries = desc.primaries;
  c.tf.SetTransferFunction(desc.tf);
  c.rendering_intent = desc.rendering_intent;
  return c;
}

// Define the operator<< for tests.
static inline ::std::ostream& operator<<(::std::ostream& os,
                                         const ColorEncodingDescriptor& c) {
  return os << "ColorEncoding/" << Description(ColorEncodingFromDescriptor(c));
}

// Returns ColorEncodingDescriptors, which are only used in tests. To obtain a
// ColorEncoding object call ColorEncodingFromDescriptor and then call
// ColorEncoding::CreateProfile() on that object to generate a profile.
std::vector<ColorEncodingDescriptor> AllEncodings() {
  std::vector<ColorEncodingDescriptor> all_encodings;
  all_encodings.reserve(300);
  ColorEncoding c;

  for (ColorSpace cs : Values<ColorSpace>()) {
    if (cs == ColorSpace::kUnknown || cs == ColorSpace::kXYB) continue;
    c.SetColorSpace(cs);

    for (WhitePoint wp : Values<WhitePoint>()) {
      if (wp == WhitePoint::kCustom) continue;
      if (c.ImplicitWhitePoint() && c.white_point != wp) continue;
      c.white_point = wp;

      for (Primaries primaries : Values<Primaries>()) {
        if (primaries == Primaries::kCustom) continue;
        if (!c.HasPrimaries()) continue;
        c.primaries = primaries;

        for (TransferFunction tf : Values<TransferFunction>()) {
          if (tf == TransferFunction::kUnknown) continue;
          if (c.tf.SetImplicit() &&
              (c.tf.IsGamma() || c.tf.GetTransferFunction() != tf)) {
            continue;
          }
          c.tf.SetTransferFunction(tf);

          for (RenderingIntent ri : Values<RenderingIntent>()) {
            ColorEncodingDescriptor cdesc;
            cdesc.color_space = cs;
            cdesc.white_point = wp;
            cdesc.primaries = primaries;
            cdesc.tf = tf;
            cdesc.rendering_intent = ri;
            all_encodings.push_back(cdesc);
          }
        }
      }
    }
  }

  return all_encodings;
}

// Returns a test image with some autogenerated pixel content, using 16 bits per
// channel, big endian order, 1 to 4 channels
// The seed parameter allows to create images with different pixel content.
std::vector<uint8_t> GetSomeTestImage(size_t xsize, size_t ysize,
                                      size_t num_channels, uint16_t seed) {
  // Cause more significant image difference for successive seeds.
  std::mt19937 rng(seed);
  std::uniform_int_distribution<uint16_t> dark(0, 32767);
  size_t num_pixels = xsize * ysize;
  // 16 bits per channel, big endian, 4 channels
  std::vector<uint8_t> pixels(num_pixels * num_channels * 2);
  // Create pixel content to test, actual content does not matter as long as it
  // can be compared after roundtrip.
  for (size_t y = 0; y < ysize; y++) {
    for (size_t x = 0; x < xsize; x++) {
      uint16_t r = dark(rng);
      uint16_t g = dark(rng);
      uint16_t b = dark(rng);
      uint16_t a = dark(rng);
      // put some shape in there for visual debugging
      if (x * x + y * y < 1000) {
        r = (65535 - x * y) ^ seed;
        g = (x << 8) + y + seed;
        b = (y << 8) + x * seed;
        a = 32768 + x * 256 - y;
      }
      size_t i = (y * xsize + x) * 2 * num_channels;
      pixels[i + 0] = (r >> 8);
      pixels[i + 1] = (r & 255);
      if (num_channels >= 2) {
        // This may store what is called 'g' in the alpha channel of a 2-channel
        // image, but that's ok since the content is arbitrary
        pixels[i + 2] = (g >> 8);
        pixels[i + 3] = (g & 255);
      }
      if (num_channels >= 3) {
        pixels[i + 4] = (b >> 8);
        pixels[i + 5] = (b & 255);
      }
      if (num_channels >= 4) {
        pixels[i + 6] = (a >> 8);
        pixels[i + 7] = (a & 255);
      }
    }
  }
  return pixels;
}

// Returns a CodecInOut based on the buf, xsize, ysize, and the assumption
// that the buffer was created using `GetSomeTestImage`.
jxl::CodecInOut SomeTestImageToCodecInOut(const std::vector<uint8_t>& buf,
                                          size_t num_channels, size_t xsize,
                                          size_t ysize) {
  jxl::CodecInOut io;
  io.SetSize(xsize, ysize);
  io.metadata.m.SetAlphaBits(16);
  io.metadata.m.color_encoding = jxl::ColorEncoding::SRGB(
      /*is_gray=*/num_channels == 1 || num_channels == 2);
  EXPECT_TRUE(ConvertFromExternal(
      jxl::Span<const uint8_t>(buf.data(), buf.size()), xsize, ysize,
      jxl::ColorEncoding::SRGB(/*is_gray=*/num_channels == 1 ||
                               num_channels == 2),
      /*has_alpha=*/num_channels == 2 || num_channels == 4,
      /*alpha_is_premultiplied=*/false, /*bits_per_sample=*/16, JXL_BIG_ENDIAN,
      /*flipped_y=*/false, /*pool=*/nullptr,
      /*ib=*/&io.Main()));
  return io;
}

}  // namespace test

bool operator==(const jxl::PaddedBytes& a, const jxl::PaddedBytes& b) {
  if (a.size() != b.size()) return false;
  if (memcmp(a.data(), b.data(), a.size()) != 0) return false;
  return true;
}

// Allow using EXPECT_EQ on jxl::PaddedBytes
bool operator!=(const jxl::PaddedBytes& a, const jxl::PaddedBytes& b) {
  return !(a == b);
}
}  // namespace jxl

#endif  // LIB_JXL_TEST_UTILS_H_
