// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_TEST_UTILS_H_
#define LIB_JXL_TEST_UTILS_H_

// Macros and functions useful for tests.

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "jxl/codestream_header.h"
#include "jxl/encode.h"
#include "lib/jxl/aux_out_fwd.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/random.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/common.h"  // JPEGXL_ENABLE_TRANSCODE_JPEG
#include "lib/jxl/dec_file.h"
#include "lib/jxl/dec_params.h"
#include "lib/jxl/enc_color_management.h"
#include "lib/jxl/enc_external_image.h"
#include "lib/jxl/enc_file.h"
#include "lib/jxl/enc_params.h"

#ifdef JXL_DISABLE_SLOW_TESTS
#define JXL_SLOW_TEST(X) DISABLED_##X
#else
#define JXL_SLOW_TEST(X) X
#endif  // JXL_DISABLE_SLOW_TESTS

#if JPEGXL_ENABLE_TRANSCODE_JPEG
#define JXL_TRANSCODE_JPEG_TEST(X) X
#else
#define JXL_TRANSCODE_JPEG_TEST(X) DISABLED_##X
#endif  // JPEGXL_ENABLE_TRANSCODE_JPEG

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

// Ensures that we don't make our test bounds too lax, effectively disabling the
// tests.
MATCHER_P(IsSlightlyBelow, max, "") { return max * 0.75 <= arg && arg <= max; }

namespace jxl {
namespace test {

void JxlBasicInfoSetFromPixelFormat(JxlBasicInfo* basic_info,
                                    const JxlPixelFormat* pixel_format) {
  JxlEncoderInitBasicInfo(basic_info);
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
      basic_info->num_extra_channels = 1;
    } else {
      basic_info->alpha_bits = basic_info->bits_per_sample;
      basic_info->num_extra_channels = 1;
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
  EXPECT_TRUE(EncodeFile(cparams, io, enc_state.get(), &compressed, GetJxlCms(),
                         aux_out, pool));

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
  Rng generator(seed);

  // Returns random integer in interval [0, max_value)
  auto rng = [&generator](size_t max_value) -> size_t {
    return generator.UniformU(0, max_value);
  };

  // Dark background gradient color
  uint16_t r0 = rng(32768);
  uint16_t g0 = rng(32768);
  uint16_t b0 = rng(32768);
  uint16_t a0 = rng(32768);
  uint16_t r1 = rng(32768);
  uint16_t g1 = rng(32768);
  uint16_t b1 = rng(32768);
  uint16_t a1 = rng(32768);

  // Circle with different color
  size_t circle_x = rng(xsize);
  size_t circle_y = rng(ysize);
  size_t circle_r = rng(std::min(xsize, ysize));

  // Rectangle with random noise
  size_t rect_x0 = rng(xsize);
  size_t rect_y0 = rng(ysize);
  size_t rect_x1 = rng(xsize);
  size_t rect_y1 = rng(ysize);
  if (rect_x1 < rect_x0) std::swap(rect_x0, rect_y1);
  if (rect_y1 < rect_y0) std::swap(rect_y0, rect_y1);

  size_t num_pixels = xsize * ysize;
  // 16 bits per channel, big endian, 4 channels
  std::vector<uint8_t> pixels(num_pixels * num_channels * 2);
  // Create pixel content to test, actual content does not matter as long as it
  // can be compared after roundtrip.
  for (size_t y = 0; y < ysize; y++) {
    for (size_t x = 0; x < xsize; x++) {
      uint16_t r = r0 * (ysize - y - 1) / ysize + r1 * y / ysize;
      uint16_t g = g0 * (ysize - y - 1) / ysize + g1 * y / ysize;
      uint16_t b = b0 * (ysize - y - 1) / ysize + b1 * y / ysize;
      uint16_t a = a0 * (ysize - y - 1) / ysize + a1 * y / ysize;
      // put some shape in there for visual debugging
      if ((x - circle_x) * (x - circle_x) + (y - circle_y) * (y - circle_y) <
          circle_r * circle_r) {
        r = (65535 - x * y) ^ seed;
        g = (x << 8) + y + seed;
        b = (y << 8) + x * seed;
        a = 32768 + x * 256 - y;
      } else if (x > rect_x0 && x < rect_x1 && y > rect_y0 && y < rect_y1) {
        r = rng(65536);
        g = rng(65536);
        b = rng(65536);
        a = rng(65536);
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
      /*ib=*/&io.Main(), /*float_in=*/false));
  return io;
}

bool Near(double expected, double value, double max_dist) {
  double dist = expected > value ? expected - value : value - expected;
  return dist <= max_dist;
}

// Loads a Big-Endian float
float LoadBEFloat(const uint8_t* p) {
  uint32_t u = LoadBE32(p);
  float result;
  memcpy(&result, &u, 4);
  return result;
}

// Loads a Little-Endian float
float LoadLEFloat(const uint8_t* p) {
  uint32_t u = LoadLE32(p);
  float result;
  memcpy(&result, &u, 4);
  return result;
}

// Based on highway scalar implementation, for testing
float LoadFloat16(uint16_t bits16) {
  const uint32_t sign = bits16 >> 15;
  const uint32_t biased_exp = (bits16 >> 10) & 0x1F;
  const uint32_t mantissa = bits16 & 0x3FF;

  // Subnormal or zero
  if (biased_exp == 0) {
    const float subnormal = (1.0f / 16384) * (mantissa * (1.0f / 1024));
    return sign ? -subnormal : subnormal;
  }

  // Normalized: convert the representation directly (faster than ldexp/tables).
  const uint32_t biased_exp32 = biased_exp + (127 - 15);
  const uint32_t mantissa32 = mantissa << (23 - 10);
  const uint32_t bits32 = (sign << 31) | (biased_exp32 << 23) | mantissa32;

  float result;
  memcpy(&result, &bits32, 4);
  return result;
}

float LoadLEFloat16(const uint8_t* p) {
  uint16_t bits16 = LoadLE16(p);
  return LoadFloat16(bits16);
}

float LoadBEFloat16(const uint8_t* p) {
  uint16_t bits16 = LoadBE16(p);
  return LoadFloat16(bits16);
}

size_t GetPrecision(JxlDataType data_type) {
  switch (data_type) {
    case JXL_TYPE_BOOLEAN:
      return 1;
    case JXL_TYPE_UINT8:
      return 8;
    case JXL_TYPE_UINT16:
      return 16;
    case JXL_TYPE_UINT32:
      return 32;
    case JXL_TYPE_FLOAT:
      // Floating point mantissa precision
      return 24;
    case JXL_TYPE_FLOAT16:
      return 11;
  }
  JXL_ASSERT(false);  // unknown type
}

size_t GetDataBits(JxlDataType data_type) {
  switch (data_type) {
    case JXL_TYPE_BOOLEAN:
      return 1;
    case JXL_TYPE_UINT8:
      return 8;
    case JXL_TYPE_UINT16:
      return 16;
    case JXL_TYPE_UINT32:
      return 32;
    case JXL_TYPE_FLOAT:
      return 32;
    case JXL_TYPE_FLOAT16:
      return 16;
  }
  JXL_ASSERT(false);  // unknown type
}

// Procedure to convert pixels to double precision, not efficient, but
// well-controlled for testing. It uses double, to be able to represent all
// precisions needed for the maximum data types the API supports: uint32_t
// integers, and, single precision float. The values are in range 0-1 for SDR.
std::vector<double> ConvertToRGBA32(const uint8_t* pixels, size_t xsize,
                                    size_t ysize,
                                    const JxlPixelFormat& format) {
  std::vector<double> result(xsize * ysize * 4);
  size_t num_channels = format.num_channels;
  bool gray = num_channels == 1 || num_channels == 2;
  bool alpha = num_channels == 2 || num_channels == 4;

  size_t stride =
      xsize * jxl::DivCeil(GetDataBits(format.data_type) * num_channels,
                           jxl::kBitsPerByte);
  if (format.align > 1) stride = jxl::RoundUpTo(stride, format.align);

  if (format.data_type == JXL_TYPE_BOOLEAN) {
    for (size_t y = 0; y < ysize; ++y) {
      jxl::BitReader br(jxl::Span<const uint8_t>(pixels + stride * y, stride));
      for (size_t x = 0; x < xsize; ++x) {
        size_t j = (y * xsize + x) * 4;
        double r = br.ReadBits(1);
        double g = gray ? r : br.ReadBits(1);
        double b = gray ? r : br.ReadBits(1);
        double a = alpha ? br.ReadBits(1) : 1;
        result[j + 0] = r;
        result[j + 1] = g;
        result[j + 2] = b;
        result[j + 3] = a;
      }
      JXL_CHECK(br.Close());
    }
  } else if (format.data_type == JXL_TYPE_UINT8) {
    double mul = 1.0 / 255.0;  // Multiplier to bring to 0-1.0 range
    for (size_t y = 0; y < ysize; ++y) {
      for (size_t x = 0; x < xsize; ++x) {
        size_t j = (y * xsize + x) * 4;
        size_t i = y * stride + x * num_channels;
        double r = pixels[i];
        double g = gray ? r : pixels[i + 1];
        double b = gray ? r : pixels[i + 2];
        double a = alpha ? pixels[i + num_channels - 1] : 255;
        result[j + 0] = r * mul;
        result[j + 1] = g * mul;
        result[j + 2] = b * mul;
        result[j + 3] = a * mul;
      }
    }
  } else if (format.data_type == JXL_TYPE_UINT16) {
    double mul = 1.0 / 65535.0;  // Multiplier to bring to 0-1.0 range
    for (size_t y = 0; y < ysize; ++y) {
      for (size_t x = 0; x < xsize; ++x) {
        size_t j = (y * xsize + x) * 4;
        size_t i = y * stride + x * num_channels * 2;
        double r, g, b, a;
        if (format.endianness == JXL_BIG_ENDIAN) {
          r = (pixels[i + 0] << 8) + pixels[i + 1];
          g = gray ? r : (pixels[i + 2] << 8) + pixels[i + 3];
          b = gray ? r : (pixels[i + 4] << 8) + pixels[i + 5];
          a = alpha ? (pixels[i + num_channels * 2 - 2] << 8) +
                          pixels[i + num_channels * 2 - 1]
                    : 65535;
        } else {
          r = (pixels[i + 1] << 8) + pixels[i + 0];
          g = gray ? r : (pixels[i + 3] << 8) + pixels[i + 2];
          b = gray ? r : (pixels[i + 5] << 8) + pixels[i + 4];
          a = alpha ? (pixels[i + num_channels * 2 - 1] << 8) +
                          pixels[i + num_channels * 2 - 2]
                    : 65535;
        }
        result[j + 0] = r * mul;
        result[j + 1] = g * mul;
        result[j + 2] = b * mul;
        result[j + 3] = a * mul;
      }
    }
  } else if (format.data_type == JXL_TYPE_UINT32) {
    double mul = 1.0 / 4294967295.0;  // Multiplier to bring to 0-1.0 range
    for (size_t y = 0; y < ysize; ++y) {
      for (size_t x = 0; x < xsize; ++x) {
        size_t j = (y * xsize + x) * 4;
        size_t i = y * stride + x * num_channels * 4;
        double r, g, b, a;
        if (format.endianness == JXL_BIG_ENDIAN) {
          r = LoadBE32(pixels + i);
          g = gray ? r : LoadBE32(pixels + i + 4);
          b = gray ? r : LoadBE32(pixels + i + 8);
          a = alpha ? LoadBE32(pixels + i + num_channels * 2 - 4) : 4294967295;

        } else {
          r = LoadLE32(pixels + i);
          g = gray ? r : LoadLE32(pixels + i + 4);
          b = gray ? r : LoadLE32(pixels + i + 8);
          a = alpha ? LoadLE32(pixels + i + num_channels * 2 - 4) : 4294967295;
        }
        result[j + 0] = r * mul;
        result[j + 1] = g * mul;
        result[j + 2] = b * mul;
        result[j + 3] = a * mul;
      }
    }
  } else if (format.data_type == JXL_TYPE_FLOAT) {
    for (size_t y = 0; y < ysize; ++y) {
      for (size_t x = 0; x < xsize; ++x) {
        size_t j = (y * xsize + x) * 4;
        size_t i = y * stride + x * num_channels * 4;
        double r, g, b, a;
        if (format.endianness == JXL_BIG_ENDIAN) {
          r = LoadBEFloat(pixels + i);
          g = gray ? r : LoadBEFloat(pixels + i + 4);
          b = gray ? r : LoadBEFloat(pixels + i + 8);
          a = alpha ? LoadBEFloat(pixels + i + num_channels * 4 - 4) : 1.0;
        } else {
          r = LoadLEFloat(pixels + i);
          g = gray ? r : LoadLEFloat(pixels + i + 4);
          b = gray ? r : LoadLEFloat(pixels + i + 8);
          a = alpha ? LoadLEFloat(pixels + i + num_channels * 4 - 4) : 1.0;
        }
        result[j + 0] = r;
        result[j + 1] = g;
        result[j + 2] = b;
        result[j + 3] = a;
      }
    }
  } else if (format.data_type == JXL_TYPE_FLOAT16) {
    for (size_t y = 0; y < ysize; ++y) {
      for (size_t x = 0; x < xsize; ++x) {
        size_t j = (y * xsize + x) * 4;
        size_t i = y * stride + x * num_channels * 2;
        double r, g, b, a;
        if (format.endianness == JXL_BIG_ENDIAN) {
          r = LoadBEFloat16(pixels + i);
          g = gray ? r : LoadBEFloat16(pixels + i + 2);
          b = gray ? r : LoadBEFloat16(pixels + i + 4);
          a = alpha ? LoadBEFloat16(pixels + i + num_channels * 2 - 2) : 1.0;
        } else {
          r = LoadLEFloat16(pixels + i);
          g = gray ? r : LoadLEFloat16(pixels + i + 2);
          b = gray ? r : LoadLEFloat16(pixels + i + 4);
          a = alpha ? LoadLEFloat16(pixels + i + num_channels * 2 - 2) : 1.0;
        }
        result[j + 0] = r;
        result[j + 1] = g;
        result[j + 2] = b;
        result[j + 3] = a;
      }
    }
  } else {
    JXL_ASSERT(false);  // Unsupported type
  }
  return result;
}
// Returns amount of pixels which differ between the two pictures. Image b is
// the image after roundtrip after roundtrip, image a before roundtrip. There
// are more strict requirements for the alpha channel and grayscale values of
// the output image.
size_t ComparePixels(const uint8_t* a, const uint8_t* b, size_t xsize,
                     size_t ysize, const JxlPixelFormat& format_a,
                     const JxlPixelFormat& format_b,
                     double threshold_multiplier = 1.0) {
  // Convert both images to equal full precision for comparison.
  std::vector<double> a_full = ConvertToRGBA32(a, xsize, ysize, format_a);
  std::vector<double> b_full = ConvertToRGBA32(b, xsize, ysize, format_b);
  bool gray_a = format_a.num_channels < 3;
  bool gray_b = format_b.num_channels < 3;
  bool alpha_a = !(format_a.num_channels & 1);
  bool alpha_b = !(format_b.num_channels & 1);
  size_t bits_a = GetPrecision(format_a.data_type);
  size_t bits_b = GetPrecision(format_b.data_type);
  size_t bits = std::min(bits_a, bits_b);
  // How much distance is allowed in case of pixels with lower bit depths, given
  // that the double precision float images use range 0-1.0.
  // E.g. in case of 1-bit this is 0.5 since 0.499 must map to 0 and 0.501 must
  // map to 1.
  double precision = 0.5 * threshold_multiplier / ((1ull << bits) - 1ull);
  if (format_a.data_type == JXL_TYPE_FLOAT16 ||
      format_b.data_type == JXL_TYPE_FLOAT16) {
    // Lower the precision for float16, because it currently looks like the
    // scalar and wasm implementations of hwy have 1 less bit of precision
    // than the x86 implementations.
    // TODO(lode): Set the required precision back to 11 bits when possible.
    precision = 0.5 * threshold_multiplier / ((1ull << (bits - 1)) - 1ull);
  }
  size_t numdiff = 0;
  for (size_t y = 0; y < ysize; y++) {
    for (size_t x = 0; x < xsize; x++) {
      size_t i = (y * xsize + x) * 4;
      bool ok = true;
      if (gray_a || gray_b) {
        if (!Near(a_full[i + 0], b_full[i + 0], precision)) ok = false;
        // If the input was grayscale and the output not, then the output must
        // have all channels equal.
        if (gray_a && b_full[i + 0] != b_full[i + 1] &&
            b_full[i + 2] != b_full[i + 2]) {
          ok = false;
        }
      } else {
        if (!Near(a_full[i + 0], b_full[i + 0], precision) ||
            !Near(a_full[i + 1], b_full[i + 1], precision) ||
            !Near(a_full[i + 2], b_full[i + 2], precision)) {
          ok = false;
        }
      }
      if (alpha_a && alpha_b) {
        if (!Near(a_full[i + 3], b_full[i + 3], precision)) ok = false;
      } else {
        // If the input had no alpha channel, the output should be opaque
        // after roundtrip.
        if (alpha_b && !Near(1.0, b_full[i + 3], precision)) ok = false;
      }
      if (!ok) numdiff++;
    }
  }
  return numdiff;
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
