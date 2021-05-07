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

#include "jxl/decode.h"

#include <stdint.h>
#include <stdlib.h>

#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "jxl/decode_cxx.h"
#include "jxl/thread_parallel_runner.h"
#include "lib/extras/codec.h"
#include "lib/extras/codec_jpg.h"
#include "lib/jxl/base/byte_order.h"
#include "lib/jxl/base/file_io.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dec_file.h"
#include "lib/jxl/enc_butteraugli_comparator.h"
#include "lib/jxl/enc_external_image.h"
#include "lib/jxl/enc_file.h"
#include "lib/jxl/enc_gamma_correct.h"
#include "lib/jxl/enc_icc_codec.h"
#include "lib/jxl/encode_internal.h"
#include "lib/jxl/fields.h"
#include "lib/jxl/headers.h"
#include "lib/jxl/icc_codec.h"
#include "lib/jxl/jpeg/enc_jpeg_data.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testdata.h"
#include "tools/box/box.h"

////////////////////////////////////////////////////////////////////////////////

namespace {
void AppendU32BE(uint32_t u32, jxl::PaddedBytes* bytes) {
  bytes->push_back(u32 >> 24);
  bytes->push_back(u32 >> 16);
  bytes->push_back(u32 >> 8);
  bytes->push_back(u32 >> 0);
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

// What type of codestream format in the boxes to use for testing
enum CodeStreamBoxFormat {
  // Do not use box format at all, only pure codestream
  kCSBF_None,
  // Have a single codestream box, with its actual size given in the box
  kCSBF_Single,
  // Have a single codestream box, with box size 0 (final box running to end)
  kCSBF_Single_Zero_Terminated,
  // Single codestream box, with another unknown box behind it
  kCSBF_Single_other,
  // Have multiple partial codestream boxes
  kCSBF_Multi,
  // Have multiple partial codestream boxes, with final box size 0 (running
  // to end)
  kCSBF_Multi_Zero_Terminated,
  // Have multiple partial codestream boxes, terminated by non-codestream box
  kCSBF_Multi_Other_Terminated,
  // Have multiple partial codestream boxes, terminated by non-codestream box
  // that has its size set to 0 (running to end)
  kCSBF_Multi_Other_Zero_Terminated,
  // Have multiple partial codestream boxes, and the first one has a content
  // of zero length
  kCSBF_Multi_First_Empty,
  // Not a value but used for counting amount of enum entries
  kCSBF_NUM_ENTRIES,
};

// Returns an ICC profile output by the JPEG XL decoder for RGB_D65_SRG_Rel_Lin,
// but with, on purpose, rXYZ, bXYZ and gXYZ (the RGB primaries) switched to a
// different order to ensure the profile does not match any known profile, so
// the encoder cannot encode it in a compact struct instead.
jxl::PaddedBytes GetIccTestProfile() {
  const uint8_t* profile = reinterpret_cast<const uint8_t*>(
      "\0\0\3\200lcms\0040\0\0mntrRGB XYZ "
      "\a\344\0\a\0\27\0\21\0$"
      "\0\37acspAPPL\0\0\0\1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1\0\0\366"
      "\326\0\1\0\0\0\0\323-lcms\372c\207\36\227\200{"
      "\2\232s\255\327\340\0\n\26\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
      "\0\0\0\0\0\0\0\0\rdesc\0\0\1 "
      "\0\0\0Bcprt\0\0\1d\0\0\1\0wtpt\0\0\2d\0\0\0\24chad\0\0\2x\0\0\0,"
      "bXYZ\0\0\2\244\0\0\0\24gXYZ\0\0\2\270\0\0\0\24rXYZ\0\0\2\314\0\0\0\24rTR"
      "C\0\0\2\340\0\0\0 gTRC\0\0\2\340\0\0\0 bTRC\0\0\2\340\0\0\0 "
      "chrm\0\0\3\0\0\0\0$dmnd\0\0\3$\0\0\0("
      "dmdd\0\0\3L\0\0\0002mluc\0\0\0\0\0\0\0\1\0\0\0\fenUS\0\0\0&"
      "\0\0\0\34\0R\0G\0B\0_\0D\0006\0005\0_\0S\0R\0G\0_\0R\0e\0l\0_"
      "\0L\0i\0n\0\0mluc\0\0\0\0\0\0\0\1\0\0\0\fenUS\0\0\0\344\0\0\0\34\0C\0o\0"
      "p\0y\0r\0i\0g\0h\0t\0 \0002\0000\0001\08\0 \0G\0o\0o\0g\0l\0e\0 "
      "\0L\0L\0C\0,\0 \0C\0C\0-\0B\0Y\0-\0S\0A\0 \0003\0.\0000\0 "
      "\0U\0n\0p\0o\0r\0t\0e\0d\0 "
      "\0l\0i\0c\0e\0n\0s\0e\0(\0h\0t\0t\0p\0s\0:\0/\0/"
      "\0c\0r\0e\0a\0t\0i\0v\0e\0c\0o\0m\0m\0o\0n\0s\0.\0o\0r\0g\0/"
      "\0l\0i\0c\0e\0n\0s\0e\0s\0/\0b\0y\0-\0s\0a\0/\0003\0.\0000\0/"
      "\0l\0e\0g\0a\0l\0c\0o\0d\0e\0)XYZ "
      "\0\0\0\0\0\0\366\326\0\1\0\0\0\0\323-"
      "sf32\0\0\0\0\0\1\fB\0\0\5\336\377\377\363%"
      "\0\0\a\223\0\0\375\220\377\377\373\241\377\377\375\242\0\0\3\334\0\0\300"
      "nXYZ \0\0\0\0\0\0o\240\0\08\365\0\0\3\220XYZ "
      "\0\0\0\0\0\0$\237\0\0\17\204\0\0\266\304XYZ "
      "\0\0\0\0\0\0b\227\0\0\267\207\0\0\30\331para\0\0\0\0\0\3\0\0\0\1\0\0\0\1"
      "\0\0\0\0\0\0\0\1\0\0\0\0\0\0chrm\0\0\0\0\0\3\0\0\0\0\243\327\0\0T|"
      "\0\0L\315\0\0\231\232\0\0&"
      "g\0\0\17\\mluc\0\0\0\0\0\0\0\1\0\0\0\fenUS\0\0\0\f\0\0\0\34\0G\0o\0o\0g"
      "\0l\0emluc\0\0\0\0\0\0\0\1\0\0\0\fenUS\0\0\0\26\0\0\0\34\0I\0m\0a\0g\0e"
      "\0 \0c\0o\0d\0e\0c\0\0");
  size_t profile_size = 896;
  jxl::PaddedBytes icc_profile;
  icc_profile.assign(profile, profile + profile_size);
  return icc_profile;
}

}  // namespace

namespace jxl {
namespace {

// Input pixels always given as 16-bit RGBA, 8 bytes per pixel.
// include_alpha determines if the encoded image should contain the alpha
// channel.
// add_icc_profile: if false, encodes the image as sRGB using the JXL fields,
// for grayscale or RGB images. If true, encodes the image using the ICC profile
// returned by GetIccTestProfile, without the JXL fields, this requires the
// image is RGB, not grayscale.
// Providing jpeg_codestream will populate the jpeg_codestream with compressed
// JPEG bytes, and make it possible to reconstruct those exact JPEG bytes using
// the return value _if_ add_container indicates a box format.
PaddedBytes CreateTestJXLCodestream(Span<const uint8_t> pixels, size_t xsize,
                                    size_t ysize, size_t num_channels,
                                    const CompressParams& cparams,
                                    CodeStreamBoxFormat add_container,
                                    bool add_preview,
                                    bool add_icc_profile = false,
                                    PaddedBytes* jpeg_codestream = nullptr) {
  // Compress the pixels with JPEG XL.
  bool grayscale = (num_channels <= 2);
  bool include_alpha = !(num_channels & 1) && jpeg_codestream == nullptr;
  size_t bitdepth = jpeg_codestream == nullptr ? 16 : 8;
  CodecInOut io;
  io.SetSize(xsize, ysize);
  ColorEncoding color_encoding =
      jxl::ColorEncoding::SRGB(/*is_gray=*/grayscale);
  if (add_icc_profile) {
    // the hardcoded ICC profile we attach requires RGB.
    EXPECT_EQ(false, grayscale);
    EXPECT_TRUE(color_encoding.SetICC(GetIccTestProfile()));
  }
  ThreadPool pool(nullptr, nullptr);
  io.metadata.m.SetUintSamples(bitdepth);
  if (include_alpha) {
    io.metadata.m.SetAlphaBits(bitdepth);
  }
  // Make the grayscale-ness of the io metadata color_encoding and the packed
  // image match.
  io.metadata.m.color_encoding = color_encoding;
  EXPECT_TRUE(ConvertFromExternal(
      pixels, xsize, ysize, color_encoding, /*has_alpha=*/include_alpha,
      /*alpha_is_premultiplied=*/false, bitdepth, JXL_BIG_ENDIAN,
      /*flipped_y=*/false, &pool, &io.Main()));
  jxl::PaddedBytes jpeg_data;
  if (jpeg_codestream != nullptr) {
#if JPEGXL_ENABLE_JPEG
    jxl::PaddedBytes jpeg_bytes;
    EXPECT_TRUE(EncodeImageJPG(&io, jxl::JpegEncoder::kLibJpeg, /*quality=*/70,
                               jxl::YCbCrChromaSubsampling(), &pool,
                               &jpeg_bytes, jxl::DecodeTarget::kPixels));
    jpeg_codestream->append(jpeg_bytes.data(),
                            jpeg_bytes.data() + jpeg_bytes.size());
    EXPECT_TRUE(jxl::jpeg::DecodeImageJPG(
        jxl::Span<const uint8_t>(jpeg_bytes.data(), jpeg_bytes.size()), &io));
    EXPECT_TRUE(EncodeJPEGData(*io.Main().jpeg_data, &jpeg_data));
    io.metadata.m.xyb_encoded = false;
#else   // JPEGXL_ENABLE_JPEG
    JXL_ABORT(
        "unable to create reconstructible JPEG without JPEG support enabled");
#endif  // JPEGXL_ENABLE_JPEG
  }
  if (add_preview) {
    io.preview_frame = io.Main().Copy();
    io.preview_frame.ShrinkTo(xsize / 7, ysize / 7);
    io.metadata.m.have_preview = true;
    EXPECT_TRUE(io.metadata.m.preview_size.Set(io.preview_frame.xsize(),
                                               io.preview_frame.ysize()));
  }
  AuxOut aux_out;
  PaddedBytes compressed;
  PassesEncoderState enc_state;
  EXPECT_TRUE(
      EncodeFile(cparams, &io, &enc_state, &compressed, &aux_out, &pool));
  if (add_container != kCSBF_None) {
    // Header with signature box and ftyp box.
    const uint8_t header[] = {0,    0,    0,    0xc,  0x4a, 0x58, 0x4c, 0x20,
                              0xd,  0xa,  0x87, 0xa,  0,    0,    0,    0x14,
                              0x66, 0x74, 0x79, 0x70, 0x6a, 0x78, 0x6c, 0x20,
                              0,    0,    0,    0,    0x6a, 0x78, 0x6c, 0x20};
    // Unknown box, could be a box added by user, decoder must be able to skip
    // over it. Type is set to 'unkn', size to 24, contents to 16 0's.
    const uint8_t unknown[] = {0, 0, 0, 0x18, 0x75, 0x6e, 0x6b, 0x6e,
                               0, 0, 0, 0,    0,    0,    0,    0,
                               0, 0, 0, 0,    0,    0,    0,    0};
    // same as the unknown box, but with size set to 0, this can only be a final
    // box
    const uint8_t unknown_end[] = {0, 0, 0, 0, 0x75, 0x6e, 0x6b, 0x6e,
                                   0, 0, 0, 0, 0,    0,    0,    0,
                                   0, 0, 0, 0, 0,    0,    0,    0};

    bool is_multi = add_container == kCSBF_Multi ||
                    add_container == kCSBF_Multi_Zero_Terminated ||
                    add_container == kCSBF_Multi_Other_Terminated ||
                    add_container == kCSBF_Multi_Other_Zero_Terminated ||
                    add_container == kCSBF_Multi_First_Empty;

    if (is_multi) {
      size_t third = compressed.size() / 3;
      std::vector<uint8_t> compressed0(compressed.data(),
                                       compressed.data() + third);
      std::vector<uint8_t> compressed1(compressed.data() + third,
                                       compressed.data() + 2 * third);
      std::vector<uint8_t> compressed2(compressed.data() + 2 * third,
                                       compressed.data() + compressed.size());

      PaddedBytes c;
      c.append(header, header + sizeof(header));
      if (jpeg_codestream != nullptr) {
        jxl::AppendBoxHeader(jxl::MakeBoxType("jbrd"), jpeg_data.size(), false,
                             &c);
        c.append(jpeg_data.data(), jpeg_data.data() + jpeg_data.size());
      }
      uint32_t jxlp_index = 0;
      if (add_container == kCSBF_Multi_First_Empty) {
        // Dummy (empty) codestream part
        AppendU32BE(12, &c);
        c.push_back('j');
        c.push_back('x');
        c.push_back('l');
        c.push_back('p');
        AppendU32BE(jxlp_index++, &c);
      }
      // First codestream part
      AppendU32BE(compressed0.size() + 12, &c);
      c.push_back('j');
      c.push_back('x');
      c.push_back('l');
      c.push_back('p');
      AppendU32BE(jxlp_index++, &c);
      c.append(compressed0.data(), compressed0.data() + compressed0.size());
      // A few non-codestream boxes in between
      c.append(unknown, unknown + sizeof(unknown));
      c.append(unknown, unknown + sizeof(unknown));
      // Dummy (empty) codestream part
      AppendU32BE(12, &c);
      c.push_back('j');
      c.push_back('x');
      c.push_back('l');
      c.push_back('p');
      AppendU32BE(jxlp_index++, &c);
      // Second codestream part
      AppendU32BE(compressed1.size() + 12, &c);
      c.push_back('j');
      c.push_back('x');
      c.push_back('l');
      c.push_back('p');
      AppendU32BE(jxlp_index++, &c);
      c.append(compressed1.data(), compressed1.data() + compressed1.size());
      // Third codestream part
      AppendU32BE(add_container == kCSBF_Multi ? (compressed2.size() + 12) : 0,
                  &c);
      c.push_back('j');
      c.push_back('x');
      c.push_back('l');
      c.push_back('p');
      AppendU32BE(jxlp_index++ | 0x80000000, &c);
      c.append(compressed2.data(), compressed2.data() + compressed2.size());
      if (add_container == kCSBF_Multi_Other_Terminated) {
        c.append(unknown, unknown + sizeof(unknown));
      }
      if (add_container == kCSBF_Multi_Other_Zero_Terminated) {
        c.append(unknown_end, unknown_end + sizeof(unknown_end));
      }
      compressed.swap(c);
    } else {
      PaddedBytes c;
      c.append(header, header + sizeof(header));
      if (jpeg_codestream != nullptr) {
        jxl::AppendBoxHeader(jxl::MakeBoxType("jbrd"), jpeg_data.size(), false,
                             &c);
        c.append(jpeg_data.data(), jpeg_data.data() + jpeg_data.size());
      }
      AppendU32BE(add_container == kCSBF_Single_Zero_Terminated
                      ? 0
                      : (compressed.size() + 8),
                  &c);
      c.push_back('j');
      c.push_back('x');
      c.push_back('l');
      c.push_back('c');
      c.append(compressed.data(), compressed.data() + compressed.size());
      if (add_container == kCSBF_Single_other) {
        c.append(unknown, unknown + sizeof(unknown));
      }
      compressed.swap(c);
    }
  }

  return compressed;
}

// Decodes one-shot with the API for non-streaming decoding tests.
std::vector<uint8_t> DecodeWithAPI(JxlDecoder* dec,
                                   Span<const uint8_t> compressed,
                                   const JxlPixelFormat& format,
                                   bool use_callback, bool set_buffer_early) {
  void* runner = JxlThreadParallelRunnerCreate(
      NULL, JxlThreadParallelRunnerDefaultNumWorkerThreads());
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetParallelRunner(dec, JxlThreadParallelRunner, runner));

  EXPECT_EQ(
      JXL_DEC_SUCCESS,
      JxlDecoderSubscribeEvents(
          dec, JXL_DEC_BASIC_INFO | (set_buffer_early ? JXL_DEC_FRAME : 0) |
                   JXL_DEC_PREVIEW_IMAGE | JXL_DEC_FULL_IMAGE));

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetInput(dec, compressed.data(), compressed.size()));
  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
  size_t buffer_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
  std::vector<uint8_t> pixels(buffer_size);

  size_t bytes_per_pixel =
      format.num_channels * GetDataBits(format.data_type) / jxl::kBitsPerByte;
  size_t stride = bytes_per_pixel * info.xsize;
  if (format.align > 1) {
    stride = jxl::DivCeil(stride, format.align) * format.align;
  }
  auto callback = [&](size_t x, size_t y, size_t num_pixels,
                      const void* pixels_row) {
    memcpy(pixels.data() + stride * y + bytes_per_pixel * x, pixels_row,
           num_pixels * bytes_per_pixel);
  };

  JxlDecoderStatus status = JxlDecoderProcessInput(dec);

  std::vector<uint8_t> preview;
  if (status == JXL_DEC_NEED_PREVIEW_OUT_BUFFER) {
    size_t buffer_size;
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderPreviewOutBufferSize(dec, &format, &buffer_size));
    preview.resize(buffer_size);
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSetPreviewOutBuffer(dec, &format, preview.data(),
                                            preview.size()));
    EXPECT_EQ(JXL_DEC_PREVIEW_IMAGE, JxlDecoderProcessInput(dec));

    status = JxlDecoderProcessInput(dec);
  }

  if (set_buffer_early) {
    EXPECT_EQ(JXL_DEC_FRAME, status);
  } else {
    EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, status);
  }

  if (use_callback) {
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSetImageOutCallback(
                  dec, &format,
                  [](void* opaque, size_t x, size_t y, size_t xsize,
                     const void* pixels_row) {
                    auto cb = static_cast<decltype(&callback)>(opaque);
                    (*cb)(x, y, xsize, pixels_row);
                  },
                  /*opaque=*/&callback));
  } else {
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                   dec, &format, pixels.data(), pixels.size()));
  }

  EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));

  // After the full image is gotten, JxlDecoderProcessInput should return
  // success to indicate all is done.
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));

  JxlThreadParallelRunnerDestroy(runner);

  return pixels;
}

// Decodes one-shot with the API for non-streaming decoding tests.
std::vector<uint8_t> DecodeWithAPI(Span<const uint8_t> compressed,
                                   const JxlPixelFormat& format,
                                   bool use_callback, bool set_buffer_early) {
  JxlDecoder* dec = JxlDecoderCreate(NULL);
  std::vector<uint8_t> pixels =
      DecodeWithAPI(dec, compressed, format, use_callback, set_buffer_early);
  JxlDecoderDestroy(dec);
  return pixels;
}

}  // namespace
}  // namespace jxl

namespace {

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
                     const JxlPixelFormat& format_b) {
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
  double precision = 0.5 / ((1ull << bits) - 1ull);
  if (format_a.data_type == JXL_TYPE_FLOAT16 ||
      format_b.data_type == JXL_TYPE_FLOAT16) {
    // Lower the precision for float16, because it currently looks like the
    // scalar and wasm implementations of hwy have 1 less bit of precision
    // than the x86 implementations.
    // TODO(lode): Set the required precision back to 11 bits when possible.
    precision = 0.5 / ((1ull << (bits - 1)) - 1ull);
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

}  // namespace

////////////////////////////////////////////////////////////////////////////////

TEST(DecodeTest, JxlSignatureCheckTest) {
  std::vector<std::pair<int, std::vector<uint8_t>>> tests = {
      // No JPEGXL header starts with 'a'.
      {JXL_SIG_INVALID, {'a'}},
      {JXL_SIG_INVALID, {'a', 'b', 'c', 'd', 'e', 'f'}},

      // Empty file is not enough bytes.
      {JXL_SIG_NOT_ENOUGH_BYTES, {}},

      // JPEGXL headers.
      {JXL_SIG_NOT_ENOUGH_BYTES, {0xff}},  // Part of a signature.
      {JXL_SIG_INVALID, {0xff, 0xD8}},     // JPEG-1
      {JXL_SIG_CODESTREAM, {0xff, 0x0a}},

      // JPEGXL container file.
      {JXL_SIG_CONTAINER,
       {0, 0, 0, 0xc, 'J', 'X', 'L', ' ', 0xD, 0xA, 0x87, 0xA}},
      // Ending with invalid byte.
      {JXL_SIG_INVALID, {0, 0, 0, 0xc, 'J', 'X', 'L', ' ', 0xD, 0xA, 0x87, 0}},
      // Part of signature.
      {JXL_SIG_NOT_ENOUGH_BYTES,
       {0, 0, 0, 0xc, 'J', 'X', 'L', ' ', 0xD, 0xA, 0x87}},
      {JXL_SIG_NOT_ENOUGH_BYTES, {0}},
  };
  for (const auto& test : tests) {
    EXPECT_EQ(test.first,
              JxlSignatureCheck(test.second.data(), test.second.size()))
        << "Where test data is " << ::testing::PrintToString(test.second);
  }
}

TEST(DecodeTest, DefaultAllocTest) {
  JxlDecoder* dec = JxlDecoderCreate(nullptr);
  EXPECT_NE(nullptr, dec);
  JxlDecoderDestroy(dec);
}

TEST(DecodeTest, CustomAllocTest) {
  struct CalledCounters {
    int allocs = 0;
    int frees = 0;
  } counters;

  JxlMemoryManager mm;
  mm.opaque = &counters;
  mm.alloc = [](void* opaque, size_t size) {
    reinterpret_cast<CalledCounters*>(opaque)->allocs++;
    return malloc(size);
  };
  mm.free = [](void* opaque, void* address) {
    reinterpret_cast<CalledCounters*>(opaque)->frees++;
    free(address);
  };

  JxlDecoder* dec = JxlDecoderCreate(&mm);
  EXPECT_NE(nullptr, dec);
  EXPECT_LE(1, counters.allocs);
  EXPECT_EQ(0, counters.frees);
  JxlDecoderDestroy(dec);
  EXPECT_LE(1, counters.frees);
}

// TODO(lode): add multi-threaded test when multithreaded pixel decoding from
// API is implemented.
TEST(DecodeTest, DefaultParallelRunnerTest) {
  JxlDecoder* dec = JxlDecoderCreate(nullptr);
  EXPECT_NE(nullptr, dec);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetParallelRunner(dec, nullptr, nullptr));
  JxlDecoderDestroy(dec);
}

// Creates the header of a JPEG XL file with various custom parameters for
// testing.
// xsize, ysize: image dimentions to store in the SizeHeader, max 512.
// bits_per_sample, orientation: a selection of header parameters to test with.
// orientation: image orientation to set in the metadata
// alpha_bits: if non-0, alpha extra channel bits to set in the metadata. Also
//   gives the alpha channel the name "alpha_test"
// have_container: add box container format around the codestream.
// metadata_default: if true, ImageMetadata is set to default and
//   bits_per_sample, orientation and alpha_bits are ignored.
// insert_box: insert an extra box before the codestream box, making the header
// farther away from the front than is ideal. Only used if have_container.
std::vector<uint8_t> GetTestHeader(size_t xsize, size_t ysize,
                                   size_t bits_per_sample, size_t orientation,
                                   size_t alpha_bits, bool xyb_encoded,
                                   bool have_container, bool metadata_default,
                                   bool insert_extra_box,
                                   const jxl::PaddedBytes& icc_profile) {
  jxl::BitWriter writer;
  jxl::BitWriter::Allotment allotment(&writer, 65536);  // Large enough

  if (have_container) {
    const std::vector<uint8_t> signature_box = {0,   0,   0,   0xc, 'J',  'X',
                                                'L', ' ', 0xd, 0xa, 0x87, 0xa};
    const std::vector<uint8_t> filetype_box = {
        0,   0,   0, 0x14, 'f', 't', 'y', 'p', 'j', 'x',
        'l', ' ', 0, 0,    0,   0,   'j', 'x', 'l', ' '};
    const std::vector<uint8_t> extra_box_header = {0,   0,   0,   0xff,
                                                   't', 'e', 's', 't'};
    // Beginning of codestream box, with an arbitrary size certainly large
    // enough to contain the header
    const std::vector<uint8_t> codestream_box_header = {0,   0,   0,   0xff,
                                                        'j', 'x', 'l', 'c'};

    for (size_t i = 0; i < signature_box.size(); i++) {
      writer.Write(8, signature_box[i]);
    }
    for (size_t i = 0; i < filetype_box.size(); i++) {
      writer.Write(8, filetype_box[i]);
    }
    if (insert_extra_box) {
      for (size_t i = 0; i < extra_box_header.size(); i++) {
        writer.Write(8, extra_box_header[i]);
      }
      for (size_t i = 0; i < 255 - 8; i++) {
        writer.Write(8, 0);
      }
    }
    for (size_t i = 0; i < codestream_box_header.size(); i++) {
      writer.Write(8, codestream_box_header[i]);
    }
  }

  // JXL signature
  writer.Write(8, 0xff);
  writer.Write(8, 0x0a);

  // SizeHeader
  jxl::CodecMetadata metadata;
  EXPECT_TRUE(metadata.size.Set(xsize, ysize));
  EXPECT_TRUE(WriteSizeHeader(metadata.size, &writer, 0, nullptr));

  if (!metadata_default) {
    metadata.m.SetUintSamples(bits_per_sample);
    metadata.m.orientation = orientation;
    metadata.m.SetAlphaBits(alpha_bits);
    metadata.m.xyb_encoded = xyb_encoded;
    if (alpha_bits != 0) {
      metadata.m.extra_channel_info[0].name = "alpha_test";
    }
  }

  if (!icc_profile.empty()) {
    jxl::PaddedBytes copy = icc_profile;
    EXPECT_TRUE(metadata.m.color_encoding.SetICC(std::move(copy)));
  }

  EXPECT_TRUE(jxl::Bundle::Write(metadata.m, &writer, 0, nullptr));
  metadata.transform_data.nonserialized_xyb_encoded = metadata.m.xyb_encoded;
  EXPECT_TRUE(jxl::Bundle::Write(metadata.transform_data, &writer, 0, nullptr));

  if (!icc_profile.empty()) {
    EXPECT_TRUE(metadata.m.color_encoding.WantICC());
    EXPECT_TRUE(jxl::WriteICC(icc_profile, &writer, 0, nullptr));
  }

  writer.ZeroPadToByte();
  ReclaimAndCharge(&writer, &allotment, 0, nullptr);
  return std::vector<uint8_t>(
      writer.GetSpan().data(),
      writer.GetSpan().data() + writer.GetSpan().size());
}

TEST(DecodeTest, BasicInfoTest) {
  size_t xsize[2] = {50, 33};
  size_t ysize[2] = {50, 77};
  size_t bits_per_sample[2] = {8, 23};
  size_t orientation[2] = {3, 5};
  size_t alpha_bits[2] = {0, 8};
  size_t have_container[2] = {0, 1};
  bool xyb_encoded = false;

  std::vector<std::vector<uint8_t>> test_samples;
  // Test with direct codestream
  test_samples.push_back(GetTestHeader(
      xsize[0], ysize[0], bits_per_sample[0], orientation[0], alpha_bits[0],
      xyb_encoded, have_container[0], /*metadata_default=*/false,
      /*insert_extra_box=*/false, {}));
  // Test with container and different parameters
  test_samples.push_back(GetTestHeader(
      xsize[1], ysize[1], bits_per_sample[1], orientation[1], alpha_bits[1],
      xyb_encoded, have_container[1], /*metadata_default=*/false,
      /*insert_extra_box=*/false, {}));

  for (size_t i = 0; i < test_samples.size(); ++i) {
    const std::vector<uint8_t>& data = test_samples[i];
    // Test decoding too small header first, until we reach the final byte.
    for (size_t size = 0; size <= data.size(); ++size) {
      // Test with a new decoder for each tested byte size.
      JxlDecoder* dec = JxlDecoderCreate(nullptr);
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO));
      const uint8_t* next_in = data.data();
      size_t avail_in = size;
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));
      JxlDecoderStatus status = JxlDecoderProcessInput(dec);

      JxlBasicInfo info;
      bool have_basic_info = !JxlDecoderGetBasicInfo(dec, &info);

      if (size == data.size()) {
        EXPECT_EQ(JXL_DEC_BASIC_INFO, status);

        // All header bytes given so the decoder must have the basic info.
        EXPECT_EQ(true, have_basic_info);
        EXPECT_EQ(have_container[i], info.have_container);
        EXPECT_EQ(alpha_bits[i], info.alpha_bits);
        // Orientations 5..8 swap the dimensions
        if (orientation[i] >= 5) {
          EXPECT_EQ(xsize[i], info.ysize);
          EXPECT_EQ(ysize[i], info.xsize);
        } else {
          EXPECT_EQ(xsize[i], info.xsize);
          EXPECT_EQ(ysize[i], info.ysize);
        }
        // The API should set the orientation to identity by default since it
        // already applies the transformation internally by default.
        EXPECT_EQ(1, info.orientation);

        EXPECT_EQ(3, info.num_color_channels);

        if (alpha_bits[i] != 0) {
          // Expect an extra channel
          EXPECT_EQ(1, info.num_extra_channels);
          JxlExtraChannelInfo extra;
          EXPECT_EQ(0, JxlDecoderGetExtraChannelInfo(dec, 0, &extra));
          EXPECT_EQ(alpha_bits[i], extra.bits_per_sample);
          EXPECT_EQ(JXL_CHANNEL_ALPHA, extra.type);
          EXPECT_EQ(0, extra.alpha_associated);
          // Verify the name "alpha_test" given to the alpha channel
          EXPECT_EQ(10, extra.name_length);
          char name[11];
          EXPECT_EQ(0,
                    JxlDecoderGetExtraChannelName(dec, 0, name, sizeof(name)));
          EXPECT_EQ(std::string("alpha_test"), std::string(name));
        } else {
          EXPECT_EQ(0, info.num_extra_channels);
        }

        EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));
      } else {
        // If we did not give the full header, the basic info should not be
        // available. Allow a few bytes of slack due to some bits for default
        // opsinmatrix/extension bits.
        if (size + 2 < data.size()) {
          EXPECT_EQ(false, have_basic_info);
          EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, status);
        }
      }

      // Test that decoder doesn't allow setting a setting required at beginning
      // unless it's reset
      EXPECT_EQ(JXL_DEC_ERROR,
                JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO));
      JxlDecoderReset(dec);
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO));

      JxlDecoderDestroy(dec);
    }
  }
}

TEST(DecodeTest, BufferSizeTest) {
  size_t xsize = 33;
  size_t ysize = 77;
  size_t bits_per_sample = 8;
  size_t orientation = 1;
  size_t alpha_bits = 8;
  bool have_container = false;
  bool xyb_encoded = false;

  std::vector<uint8_t> header =
      GetTestHeader(xsize, ysize, bits_per_sample, orientation, alpha_bits,
                    xyb_encoded, have_container, /*metadata_default=*/false,
                    /*insert_extra_box=*/false, {});

  JxlDecoder* dec = JxlDecoderCreate(nullptr);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO));
  const uint8_t* next_in = header.data();
  size_t avail_in = header.size();
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));
  JxlDecoderStatus status = JxlDecoderProcessInput(dec);
  EXPECT_EQ(JXL_DEC_BASIC_INFO, status);

  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
  EXPECT_EQ(xsize, info.xsize);
  EXPECT_EQ(ysize, info.ysize);

  JxlPixelFormat format = {4, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};
  size_t image_out_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderImageOutBufferSize(dec, &format, &image_out_size));
  EXPECT_EQ(xsize * ysize * 4, image_out_size);

  size_t dc_out_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderDCOutBufferSize(dec, &format, &dc_out_size));
  // expected dc size: ceil(33 / 8) * ceil(77 / 8) * 4 channels
  EXPECT_EQ(5 * 10 * 4, dc_out_size);

  JxlDecoderDestroy(dec);
}

TEST(DecodeTest, BasicInfoSizeHintTest) {
  // Test on a file where the size hint is too small initially due to inserting
  // a box before the codestream (something that is normally not recommended)
  size_t xsize = 50;
  size_t ysize = 50;
  size_t bits_per_sample = 16;
  size_t orientation = 1;
  size_t alpha_bits = 0;
  bool xyb_encoded = false;
  std::vector<uint8_t> data = GetTestHeader(
      xsize, ysize, bits_per_sample, orientation, alpha_bits, xyb_encoded,
      /*have_container=*/true, /*metadata_default=*/false,
      /*insert_extra_box=*/true, {});

  JxlDecoderStatus status;
  JxlDecoder* dec = JxlDecoderCreate(nullptr);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO));

  size_t hint0 = JxlDecoderSizeHintBasicInfo(dec);
  // Test that the test works as intended: we construct a file on purpose to
  // be larger than the first hint by having that extra box.
  EXPECT_LT(hint0, data.size());
  const uint8_t* next_in = data.data();
  // Do as if we have only as many bytes as indicated by the hint available
  size_t avail_in = std::min(hint0, data.size());
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));
  status = JxlDecoderProcessInput(dec);
  EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, status);
  // Basic info cannot be available yet due to the extra inserted box.
  EXPECT_EQ(false, !JxlDecoderGetBasicInfo(dec, nullptr));

  size_t num_read = avail_in - JxlDecoderReleaseInput(dec);
  EXPECT_LT(num_read, data.size());

  size_t hint1 = JxlDecoderSizeHintBasicInfo(dec);
  // The hint must be larger than the previous hint (taking already processed
  // bytes into account, the hint is a hint for the next avail_in) since the
  // decoder now knows there is a box in between.
  EXPECT_GT(hint1 + num_read, hint0);
  avail_in = std::min<size_t>(hint1, data.size() - num_read);
  next_in += num_read;

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));
  status = JxlDecoderProcessInput(dec);
  EXPECT_EQ(JXL_DEC_BASIC_INFO, status);
  JxlBasicInfo info;
  // We should have the basic info now, since we only added one box in-between,
  // and the decoder should have known its size, its implementation can return
  // a correct hint.
  EXPECT_EQ(true, !JxlDecoderGetBasicInfo(dec, &info));

  // Also test if the basic info is correct.
  EXPECT_EQ(1, info.have_container);
  EXPECT_EQ(xsize, info.xsize);
  EXPECT_EQ(ysize, info.ysize);
  EXPECT_EQ(orientation, info.orientation);
  EXPECT_EQ(bits_per_sample, info.bits_per_sample);

  JxlDecoderDestroy(dec);
}

std::vector<uint8_t> GetIccTestHeader(const jxl::PaddedBytes& icc_profile,
                                      bool xyb_encoded) {
  size_t xsize = 50;
  size_t ysize = 50;
  size_t bits_per_sample = 16;
  size_t orientation = 1;
  size_t alpha_bits = 0;
  return GetTestHeader(xsize, ysize, bits_per_sample, orientation, alpha_bits,
                       xyb_encoded,
                       /*have_container=*/false, /*metadata_default=*/false,
                       /*insert_extra_box=*/false, icc_profile);
}

// Tests the case where pixels and metadata ICC profile are the same
TEST(DecodeTest, IccProfileTestOriginal) {
  jxl::PaddedBytes icc_profile = GetIccTestProfile();
  bool xyb_encoded = false;
  std::vector<uint8_t> data = GetIccTestHeader(icc_profile, xyb_encoded);
  JxlPixelFormat format = {4, JXL_TYPE_FLOAT, JXL_LITTLE_ENDIAN, 0};

  JxlDecoder* dec = JxlDecoderCreate(nullptr);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec, JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, data.data(), data.size()));

  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));

  // Expect the opposite of xyb_encoded for uses_original_profile
  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
  EXPECT_EQ(JXL_TRUE, info.uses_original_profile);

  EXPECT_EQ(JXL_DEC_COLOR_ENCODING, JxlDecoderProcessInput(dec));

  // the encoded color profile expected to be not available, since the image
  // has an ICC profile instead
  EXPECT_EQ(JXL_DEC_ERROR,
            JxlDecoderGetColorAsEncodedProfile(
                dec, &format, JXL_COLOR_PROFILE_TARGET_ORIGINAL, nullptr));

  size_t dec_profile_size;
  EXPECT_EQ(
      JXL_DEC_SUCCESS,
      JxlDecoderGetICCProfileSize(
          dec, &format, JXL_COLOR_PROFILE_TARGET_ORIGINAL, &dec_profile_size));

  // Check that can get return status with NULL size
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetICCProfileSize(
                dec, &format, JXL_COLOR_PROFILE_TARGET_ORIGINAL, nullptr));

  // The profiles must be equal. This requires they have equal size, and if
  // they do, we can get the profile and compare the contents.
  EXPECT_EQ(icc_profile.size(), dec_profile_size);
  if (icc_profile.size() == dec_profile_size) {
    jxl::PaddedBytes icc_profile2(icc_profile.size());
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderGetColorAsICCProfile(
                  dec, &format, JXL_COLOR_PROFILE_TARGET_ORIGINAL,
                  icc_profile2.data(), icc_profile2.size()));
    EXPECT_EQ(icc_profile, icc_profile2);
  }

  // the data is not xyb_encoded, so same result expected for the pixel data
  // color profile
  EXPECT_EQ(JXL_DEC_ERROR,
            JxlDecoderGetColorAsEncodedProfile(
                dec, &format, JXL_COLOR_PROFILE_TARGET_DATA, nullptr));

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetICCProfileSize(
                                 dec, &format, JXL_COLOR_PROFILE_TARGET_DATA,
                                 &dec_profile_size));
  EXPECT_EQ(icc_profile.size(), dec_profile_size);

  JxlDecoderDestroy(dec);
}

// Tests the case where pixels and metadata ICC profile are different
TEST(DecodeTest, IccProfileTestXybEncoded) {
  jxl::PaddedBytes icc_profile = GetIccTestProfile();
  bool xyb_encoded = true;
  std::vector<uint8_t> data = GetIccTestHeader(icc_profile, xyb_encoded);
  JxlPixelFormat format = {4, JXL_TYPE_FLOAT, JXL_LITTLE_ENDIAN, 0};
  JxlPixelFormat format_int = {4, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};

  JxlDecoder* dec = JxlDecoderCreate(nullptr);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec, JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING));

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, data.data(), data.size()));
  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));

  // Expect the opposite of xyb_encoded for uses_original_profile
  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
  EXPECT_EQ(JXL_FALSE, info.uses_original_profile);

  EXPECT_EQ(JXL_DEC_COLOR_ENCODING, JxlDecoderProcessInput(dec));

  // the encoded color profile expected to be not available, since the image
  // has an ICC profile instead
  EXPECT_EQ(JXL_DEC_ERROR,
            JxlDecoderGetColorAsEncodedProfile(
                dec, &format, JXL_COLOR_PROFILE_TARGET_ORIGINAL, nullptr));

  // Check that can get return status with NULL size
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetICCProfileSize(
                dec, &format, JXL_COLOR_PROFILE_TARGET_ORIGINAL, nullptr));

  size_t dec_profile_size;
  EXPECT_EQ(
      JXL_DEC_SUCCESS,
      JxlDecoderGetICCProfileSize(
          dec, &format, JXL_COLOR_PROFILE_TARGET_ORIGINAL, &dec_profile_size));

  // The profiles must be equal. This requires they have equal size, and if
  // they do, we can get the profile and compare the contents.
  EXPECT_EQ(icc_profile.size(), dec_profile_size);
  if (icc_profile.size() == dec_profile_size) {
    jxl::PaddedBytes icc_profile2(icc_profile.size());
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderGetColorAsICCProfile(
                  dec, &format, JXL_COLOR_PROFILE_TARGET_ORIGINAL,
                  icc_profile2.data(), icc_profile2.size()));
    EXPECT_EQ(icc_profile, icc_profile2);
  }

  // Data is xyb_encoded, so the data profile is a different profile, encoded
  // as structured profile.
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetColorAsEncodedProfile(
                dec, &format, JXL_COLOR_PROFILE_TARGET_DATA, nullptr));
  JxlColorEncoding pixel_encoding;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetColorAsEncodedProfile(
                dec, &format, JXL_COLOR_PROFILE_TARGET_DATA, &pixel_encoding));
  EXPECT_EQ(JXL_PRIMARIES_SRGB, pixel_encoding.primaries);
  // The API returns LINEAR because the colorspace cannot be represented by enum
  // values.
  EXPECT_EQ(JXL_TRANSFER_FUNCTION_LINEAR, pixel_encoding.transfer_function);

  // Test the same but with integer format.
  EXPECT_EQ(
      JXL_DEC_SUCCESS,
      JxlDecoderGetColorAsEncodedProfile(
          dec, &format_int, JXL_COLOR_PROFILE_TARGET_DATA, &pixel_encoding));
  EXPECT_EQ(JXL_PRIMARIES_SRGB, pixel_encoding.primaries);
  EXPECT_EQ(JXL_TRANSFER_FUNCTION_LINEAR, pixel_encoding.transfer_function);

  // The decoder can also output this as a generated ICC profile anyway, and
  // we're certain that it will differ from the above defined profile since
  // the sRGB data should not have swapped R/G/B primaries.

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetICCProfileSize(
                                 dec, &format, JXL_COLOR_PROFILE_TARGET_DATA,
                                 &dec_profile_size));
  // We don't need to dictate exactly what size the generated ICC profile
  // must be (since there are many ways to represent the same color space),
  // but it should not be zero.
  EXPECT_NE(0, dec_profile_size);
  if (0 != dec_profile_size) {
    jxl::PaddedBytes icc_profile2(dec_profile_size);
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetColorAsICCProfile(
                                   dec, &format, JXL_COLOR_PROFILE_TARGET_DATA,
                                   icc_profile2.data(), icc_profile2.size()));
    // expected not equal
    EXPECT_NE(icc_profile, icc_profile2);
  }

  JxlDecoderDestroy(dec);
}

// Test decoding ICC from partial files byte for byte.
// This test must pass also if JXL_CRASH_ON_ERROR is enabled, that is, the
// decoding of the ANS histogram and stream of the encoded ICC profile must also
// handle the case of not enough input bytes with StatusCode::kNotEnoughBytes
// rather than fatal error status codes.
TEST(DecodeTest, ICCPartialTest) {
  jxl::PaddedBytes icc_profile = GetIccTestProfile();
  std::vector<uint8_t> data = GetIccTestHeader(icc_profile, false);
  JxlPixelFormat format = {4, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};

  const uint8_t* next_in = data.data();
  size_t avail_in = 0;

  JxlDecoder* dec = JxlDecoderCreate(nullptr);

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec, JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING));

  bool seen_basic_info = false;
  bool seen_color_encoding = false;
  size_t total_size = 0;

  for (;;) {
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));
    JxlDecoderStatus status = JxlDecoderProcessInput(dec);
    size_t remaining = JxlDecoderReleaseInput(dec);
    EXPECT_LE(remaining, avail_in);
    next_in += avail_in - remaining;
    avail_in = remaining;
    if (status == JXL_DEC_NEED_MORE_INPUT) {
      if (total_size >= data.size()) {
        // End of partial codestream with codestrema headers and ICC profile
        // reached, it should not require more input since full image is not
        // requested
        FAIL();
        break;
      }
      size_t increment = 1;
      if (total_size + increment > data.size()) {
        increment = data.size() - total_size;
      }
      total_size += increment;
      avail_in += increment;
    } else if (status == JXL_DEC_BASIC_INFO) {
      EXPECT_FALSE(seen_basic_info);
      seen_basic_info = true;
    } else if (status == JXL_DEC_COLOR_ENCODING) {
      EXPECT_TRUE(seen_basic_info);
      EXPECT_FALSE(seen_color_encoding);
      seen_color_encoding = true;

      // Sanity check that the ICC profile was decoded correctly
      size_t dec_profile_size;
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderGetICCProfileSize(dec, &format,
                                            JXL_COLOR_PROFILE_TARGET_ORIGINAL,
                                            &dec_profile_size));
      EXPECT_EQ(icc_profile.size(), dec_profile_size);

    } else if (status == JXL_DEC_SUCCESS) {
      EXPECT_TRUE(seen_color_encoding);
      break;
    } else {
      // We do not expect any other events or errors
      FAIL();
      break;
    }
  }

  EXPECT_TRUE(seen_basic_info);
  EXPECT_TRUE(seen_color_encoding);

  JxlDecoderDestroy(dec);
}

struct PixelTestConfig {
  // Input image definition.
  bool grayscale;
  bool include_alpha;
  size_t xsize;
  size_t ysize;
  bool add_preview;
  // Output format.
  JxlEndianness endianness;
  JxlDataType data_type;
  uint32_t output_channels;
  // Container options.
  CodeStreamBoxFormat add_container;
  // Decoding mode.
  bool use_callback;
  bool set_buffer_early;
};

class DecodeTestParam : public ::testing::TestWithParam<PixelTestConfig> {};

TEST_P(DecodeTestParam, PixelTest) {
  PixelTestConfig config = GetParam();
  JxlDecoder* dec = JxlDecoderCreate(NULL);

  size_t num_pixels = config.xsize * config.ysize;
  uint32_t orig_channels =
      (config.grayscale ? 1 : 3) + (config.include_alpha ? 1 : 0);
  std::vector<uint8_t> pixels =
      jxl::test::GetSomeTestImage(config.xsize, config.ysize, orig_channels, 0);
  JxlPixelFormat format_orig = {orig_channels, JXL_TYPE_UINT16, JXL_BIG_ENDIAN,
                                0};
  jxl::CompressParams cparams;
  // Lossless to verify pixels exactly after roundtrip.
  cparams.SetLossless();
  jxl::PaddedBytes compressed = jxl::CreateTestJXLCodestream(
      jxl::Span<const uint8_t>(pixels.data(), pixels.size()), config.xsize,
      config.ysize, orig_channels, cparams, config.add_container,
      config.add_preview);

  JxlPixelFormat format = {config.output_channels, config.data_type,
                           config.endianness, 0};

  std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
      dec, jxl::Span<const uint8_t>(compressed.data(), compressed.size()),
      format, config.use_callback, config.set_buffer_early);
  JxlDecoderReset(dec);
  EXPECT_EQ(num_pixels * config.output_channels *
                GetDataBits(config.data_type) / jxl::kBitsPerByte,
            pixels2.size());
  EXPECT_EQ(0, ComparePixels(pixels.data(), pixels2.data(), config.xsize,
                             config.ysize, format_orig, format));

  JxlDecoderDestroy(dec);
}

std::vector<PixelTestConfig> GeneratePixelTests() {
  std::vector<PixelTestConfig> all_tests;
  struct ChannelInfo {
    bool grayscale;
    bool include_alpha;
    size_t output_channels;
  };
  ChannelInfo ch_info[] = {
      {false, true, 4},   // RGBA -> RGBA
      {true, false, 1},   // G -> G
      {true, true, 1},    // GA -> G
      {true, true, 2},    // GA -> GA
      {false, false, 3},  // RGB -> RGB
      {false, true, 3},   // RGBA -> RGB
      {false, false, 4},  // RGB -> RGBA
  };

  struct OutputFormat {
    JxlEndianness endianness;
    JxlDataType data_type;
  };
  OutputFormat out_formats[] = {
      {JXL_NATIVE_ENDIAN, JXL_TYPE_UINT8},
      {JXL_LITTLE_ENDIAN, JXL_TYPE_UINT16},
      {JXL_BIG_ENDIAN, JXL_TYPE_UINT16},
      {JXL_NATIVE_ENDIAN, JXL_TYPE_FLOAT16},
      {JXL_LITTLE_ENDIAN, JXL_TYPE_FLOAT},
      {JXL_BIG_ENDIAN, JXL_TYPE_FLOAT},
  };

  auto make_test = [&](ChannelInfo ch, size_t xsize, size_t ysize, bool preview,
                       CodeStreamBoxFormat box, OutputFormat format,
                       bool use_callback, bool set_buffer_early) {
    PixelTestConfig c;
    c.grayscale = ch.grayscale;
    c.include_alpha = ch.include_alpha;
    c.add_preview = preview;
    c.xsize = xsize;
    c.ysize = ysize;
    c.add_container = (CodeStreamBoxFormat)box;
    c.output_channels = ch.output_channels;
    c.data_type = format.data_type;
    c.endianness = format.endianness;
    c.use_callback = use_callback;
    c.set_buffer_early = set_buffer_early;
    all_tests.push_back(c);
  };

  // Test output formats and methods.
  for (ChannelInfo ch : ch_info) {
    for (int use_callback = 0; use_callback <= 1; use_callback++) {
      for (OutputFormat fmt : out_formats) {
        make_test(ch, 301, 33, /*add_preview=*/false,
                  CodeStreamBoxFormat::kCSBF_None, fmt, use_callback,
                  /*set_buffer_early=*/false);
      }
    }
  }
  // Test codestream formats.
  for (size_t box = 1; box < kCSBF_NUM_ENTRIES; ++box) {
    make_test(ch_info[0], 77, 33, /*add_preview=*/false,
              (CodeStreamBoxFormat)box, out_formats[0], /*use_callback=*/false,
              /*set_buffer_early=*/false);
  }
  // Test previews.
  for (int add_preview = 0; add_preview <= 1; add_preview++) {
    make_test(ch_info[0], 77, 33, add_preview, CodeStreamBoxFormat::kCSBF_None,
              out_formats[0],
              /*use_callback=*/false, /*set_buffer_early=*/false);
  }
  // Test setting buffers early.
  make_test(ch_info[0], 300, 33, /*add_preview=*/false,
            CodeStreamBoxFormat::kCSBF_None, out_formats[0],
            /*use_callback=*/false, /*set_buffer_early=*/true);
  return all_tests;
}

std::string PixelTestDescription(
    const testing::TestParamInfo<DecodeTestParam::ParamType>& info) {
  PixelTestConfig c = info.param;
  std::string name;
  name += std::to_string(c.xsize) + "x" + std::to_string(c.ysize);
  const char* colors[] = {"", "G", "GA", "RGB", "RGBA"};
  name += colors[(c.grayscale ? 1 : 3) + (c.include_alpha ? 1 : 0)];
  name += "to";
  name += colors[c.output_channels];
  switch (c.data_type) {
    case JXL_TYPE_UINT8:
      name += "u8";
      break;
    case JXL_TYPE_UINT16:
      name += "u16";
      break;
    case JXL_TYPE_FLOAT:
      name += "f32";
      break;
    case JXL_TYPE_FLOAT16:
      name += "f16";
      break;
    case JXL_TYPE_UINT32:
      name += "u32";
      break;
    case JXL_TYPE_BOOLEAN:
      name += "b";
      break;
  };
  if (GetDataBits(c.data_type) > jxl::kBitsPerByte) {
    if (c.endianness == JXL_NATIVE_ENDIAN) {
      // add nothing
    } else if (c.endianness == JXL_BIG_ENDIAN) {
      name += "BE";
    } else if (c.endianness == JXL_LITTLE_ENDIAN) {
      name += "LE";
    }
  }
  if (c.add_container != CodeStreamBoxFormat::kCSBF_None) {
    name += "Box" + std::to_string((size_t)c.add_container);
  }
  if (c.add_preview) name += "Preview";
  if (c.use_callback) name += "Callback";
  if (c.set_buffer_early) name += "EarlyBuffer";
  return name;
}

JXL_GTEST_INSTANTIATE_TEST_SUITE_P(DecodeTest, DecodeTestParam,
                                   testing::ValuesIn(GeneratePixelTests()),
                                   PixelTestDescription);

TEST(DecodeTest, PixelTestWithICCProfileLossless) {
  JxlDecoder* dec = JxlDecoderCreate(NULL);

  size_t xsize = 123, ysize = 77;
  size_t num_pixels = xsize * ysize;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);
  JxlPixelFormat format_orig = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  jxl::CompressParams cparams;
  // Lossless to verify pixels exactly after roundtrip.
  cparams.SetLossless();
  // For variation: some have container and no preview, others have preview
  // and no container.
  jxl::PaddedBytes compressed = jxl::CreateTestJXLCodestream(
      jxl::Span<const uint8_t>(pixels.data(), pixels.size()), xsize, ysize, 4,
      cparams, kCSBF_None, false, true);

  for (uint32_t channels = 3; channels <= 4; ++channels) {
    {
      JxlPixelFormat format = {channels, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};

      std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
          dec, jxl::Span<const uint8_t>(compressed.data(), compressed.size()),
          format, /*use_callback=*/false, /*set_buffer_early=*/false);
      JxlDecoderReset(dec);
      EXPECT_EQ(num_pixels * channels, pixels2.size());
      EXPECT_EQ(0, ComparePixels(pixels.data(), pixels2.data(), xsize, ysize,
                                 format_orig, format));
    }
    {
      JxlPixelFormat format = {channels, JXL_TYPE_UINT16, JXL_LITTLE_ENDIAN, 0};

      // Test with the container for one of the pixel formats.
      std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
          dec, jxl::Span<const uint8_t>(compressed.data(), compressed.size()),
          format, /*use_callback=*/true, /*set_buffer_early=*/true);
      JxlDecoderReset(dec);
      EXPECT_EQ(num_pixels * channels * 2, pixels2.size());
      EXPECT_EQ(0, ComparePixels(pixels.data(), pixels2.data(), xsize, ysize,
                                 format_orig, format));
    }

    {
      JxlPixelFormat format = {channels, JXL_TYPE_FLOAT, JXL_LITTLE_ENDIAN, 0};

      std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
          dec, jxl::Span<const uint8_t>(compressed.data(), compressed.size()),
          format, /*use_callback=*/false, /*set_buffer_early=*/false);
      JxlDecoderReset(dec);
      EXPECT_EQ(num_pixels * channels * 4, pixels2.size());
      EXPECT_EQ(0, ComparePixels(pixels.data(), pixels2.data(), xsize, ysize,
                                 format_orig, format));
    }
  }

  JxlDecoderDestroy(dec);
}

TEST(DecodeTest, PixelTestWithICCProfileLossy) {
  JxlDecoder* dec = JxlDecoderCreate(NULL);

  size_t xsize = 123, ysize = 77;
  size_t num_pixels = xsize * ysize;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 3, 0);
  JxlPixelFormat format_orig = {3, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  jxl::CompressParams cparams;
  jxl::PaddedBytes compressed = jxl::CreateTestJXLCodestream(
      jxl::Span<const uint8_t>(pixels.data(), pixels.size()), xsize, ysize, 3,
      cparams, kCSBF_None, /*add_preview=*/false, /*add_icc_profile=*/true);
  uint32_t channels = 3;

  JxlPixelFormat format = {channels, JXL_TYPE_FLOAT, JXL_LITTLE_ENDIAN, 0};

  std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
      dec, jxl::Span<const uint8_t>(compressed.data(), compressed.size()),
      format, /*use_callback=*/false, /*set_buffer_early=*/true);
  JxlDecoderReset(dec);
  EXPECT_EQ(num_pixels * channels * 4, pixels2.size());

  // The input pixels use the profile matching GetIccTestProfile, since we set
  // add_icc_profile for CreateTestJXLCodestream to true.
  jxl::ColorEncoding color_encoding0;
  EXPECT_TRUE(color_encoding0.SetICC(GetIccTestProfile()));
  jxl::Span<const uint8_t> span0(pixels.data(), pixels.size());
  jxl::CodecInOut io0;
  io0.SetSize(xsize, ysize);
  EXPECT_TRUE(ConvertFromExternal(
      span0, xsize, ysize, color_encoding0,
      /*has_alpha=*/false, false, 16, format_orig.endianness,
      /*flipped_y=*/false, /*pool=*/nullptr, &io0.Main()));

  // The output pixels are expected to be in the same colorspace as the input
  // profile, as the profile can be represented by enum values.
  jxl::ColorEncoding color_encoding1 = color_encoding0;
  jxl::Span<const uint8_t> span1(pixels2.data(), pixels2.size());
  jxl::CodecInOut io1;
  io1.SetSize(xsize, ysize);
  EXPECT_TRUE(
      ConvertFromExternal(span1, xsize, ysize, color_encoding1,
                          /*has_alpha=*/false, false, 32, format.endianness,
                          /*flipped_y=*/false, /*pool=*/nullptr, &io1.Main()));

  jxl::ButteraugliParams ba;
  EXPECT_LE(ButteraugliDistance(io0, io1, ba, /*distmap=*/nullptr, nullptr),
            2.0f);

  JxlDecoderDestroy(dec);
}

// Tests the case of lossy sRGB image without alpha channel, decoded to RGB8
// and to RGBA8
TEST(DecodeTest, PixelTestOpaqueSrgbLossy) {
  for (unsigned channels = 3; channels <= 4; channels++) {
    JxlDecoder* dec = JxlDecoderCreate(NULL);

    size_t xsize = 123, ysize = 77;
    size_t num_pixels = xsize * ysize;
    std::vector<uint8_t> pixels =
        jxl::test::GetSomeTestImage(xsize, ysize, 3, 0);
    JxlPixelFormat format_orig = {3, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
    jxl::CompressParams cparams;
    jxl::PaddedBytes compressed = jxl::CreateTestJXLCodestream(
        jxl::Span<const uint8_t>(pixels.data(), pixels.size()), xsize, ysize, 3,
        cparams, kCSBF_None, /*add_preview=*/false, /*add_icc_profile=*/false);

    JxlPixelFormat format = {channels, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};

    std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
        dec, jxl::Span<const uint8_t>(compressed.data(), compressed.size()),
        format, /*use_callback=*/true, /*set_buffer_early=*/false);
    JxlDecoderReset(dec);
    EXPECT_EQ(num_pixels * channels, pixels2.size());

    // The input pixels use the profile matching GetIccTestProfile, since we set
    // add_icc_profile for CreateTestJXLCodestream to true.
    jxl::ColorEncoding color_encoding0 = jxl::ColorEncoding::SRGB(false);
    jxl::Span<const uint8_t> span0(pixels.data(), pixels.size());
    jxl::CodecInOut io0;
    io0.SetSize(xsize, ysize);
    EXPECT_TRUE(ConvertFromExternal(
        span0, xsize, ysize, color_encoding0,
        /*has_alpha=*/false, false, 16, format_orig.endianness,
        /*flipped_y=*/false, /*pool=*/nullptr, &io0.Main()));

    jxl::ColorEncoding color_encoding1 = jxl::ColorEncoding::SRGB(false);
    jxl::Span<const uint8_t> span1(pixels2.data(), pixels2.size());
    jxl::CodecInOut io1;
    if (channels == 4) {
      io1.metadata.m.SetAlphaBits(8);
      io1.SetSize(xsize, ysize);
      EXPECT_TRUE(ConvertFromExternal(
          span1, xsize, ysize, color_encoding1,
          /*has_alpha=*/true, false, 8, format.endianness,
          /*flipped_y=*/false, /*pool=*/nullptr, &io1.Main()));
      io1.metadata.m.SetAlphaBits(0);
      io1.Main().ClearExtraChannels();
    } else {
      EXPECT_TRUE(ConvertFromExternal(
          span1, xsize, ysize, color_encoding1,
          /*has_alpha=*/false, false, 8, format.endianness,
          /*flipped_y=*/false, /*pool=*/nullptr, &io1.Main()));
    }

    jxl::ButteraugliParams ba;
    EXPECT_LE(ButteraugliDistance(io0, io1, ba, /*distmap=*/nullptr, nullptr),
              2.0f);

    JxlDecoderDestroy(dec);
  }
}

// Opaque image with noise enabled, decoded to RGB8 and RGBA8.
TEST(DecodeTest, PixelTestOpaqueSrgbLossyNoise) {
  for (unsigned channels = 3; channels <= 4; channels++) {
    JxlDecoder* dec = JxlDecoderCreate(NULL);

    size_t xsize = 512, ysize = 300;
    size_t num_pixels = xsize * ysize;
    std::vector<uint8_t> pixels =
        jxl::test::GetSomeTestImage(xsize, ysize, 3, 0);
    JxlPixelFormat format_orig = {3, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
    jxl::CompressParams cparams;
    cparams.noise = jxl::Override::kOn;
    jxl::PaddedBytes compressed = jxl::CreateTestJXLCodestream(
        jxl::Span<const uint8_t>(pixels.data(), pixels.size()), xsize, ysize, 3,
        cparams, kCSBF_None, /*add_preview=*/false, /*add_icc_profile=*/false);

    JxlPixelFormat format = {channels, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};

    std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
        dec, jxl::Span<const uint8_t>(compressed.data(), compressed.size()),
        format, /*use_callback=*/false, /*set_buffer_early=*/true);
    JxlDecoderReset(dec);
    EXPECT_EQ(num_pixels * channels, pixels2.size());

    // The input pixels use the profile matching GetIccTestProfile, since we set
    // add_icc_profile for CreateTestJXLCodestream to true.
    jxl::ColorEncoding color_encoding0 = jxl::ColorEncoding::SRGB(false);
    jxl::Span<const uint8_t> span0(pixels.data(), pixels.size());
    jxl::CodecInOut io0;
    io0.SetSize(xsize, ysize);
    EXPECT_TRUE(ConvertFromExternal(
        span0, xsize, ysize, color_encoding0,
        /*has_alpha=*/false, false, 16, format_orig.endianness,
        /*flipped_y=*/false, /*pool=*/nullptr, &io0.Main()));

    jxl::ColorEncoding color_encoding1 = jxl::ColorEncoding::SRGB(false);
    jxl::Span<const uint8_t> span1(pixels2.data(), pixels2.size());
    jxl::CodecInOut io1;
    if (channels == 4) {
      io1.metadata.m.SetAlphaBits(8);
      io1.SetSize(xsize, ysize);
      EXPECT_TRUE(ConvertFromExternal(
          span1, xsize, ysize, color_encoding1,
          /*has_alpha=*/true, false, 8, format.endianness,
          /*flipped_y=*/false, /*pool=*/nullptr, &io1.Main()));
      io1.metadata.m.SetAlphaBits(0);
      io1.Main().ClearExtraChannels();
    } else {
      EXPECT_TRUE(ConvertFromExternal(
          span1, xsize, ysize, color_encoding1,
          /*has_alpha=*/false, false, 8, format.endianness,
          /*flipped_y=*/false, /*pool=*/nullptr, &io1.Main()));
    }

    jxl::ButteraugliParams ba;
    EXPECT_LE(ButteraugliDistance(io0, io1, ba, /*distmap=*/nullptr, nullptr),
              2.0f);

    JxlDecoderDestroy(dec);
  }
}

void TestPartialStream(bool reconstructible_jpeg) {
  size_t xsize = 123, ysize = 77;
  uint32_t channels = 4;
  if (reconstructible_jpeg) {
    channels = 3;
  }
  std::vector<uint8_t> pixels =
      jxl::test::GetSomeTestImage(xsize, ysize, channels, 0);
  JxlPixelFormat format_orig = {channels, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  jxl::CompressParams cparams;
  if (reconstructible_jpeg) {
    cparams.color_transform = jxl::ColorTransform::kNone;
  } else {
    cparams
        .SetLossless();  // Lossless to verify pixels exactly after roundtrip.
  }

  std::vector<uint8_t> pixels2;
  pixels2.resize(pixels.size());

  jxl::PaddedBytes jpeg_output(64);
  size_t used_jpeg_output = 0;

  std::vector<jxl::PaddedBytes> codestreams(kCSBF_NUM_ENTRIES);
  std::vector<jxl::PaddedBytes> jpeg_codestreams(kCSBF_NUM_ENTRIES);
  for (size_t i = 0; i < kCSBF_NUM_ENTRIES; ++i) {
    CodeStreamBoxFormat add_container = (CodeStreamBoxFormat)i;

    codestreams[i] = jxl::CreateTestJXLCodestream(
        jxl::Span<const uint8_t>(pixels.data(), pixels.size()), xsize, ysize,
        channels, cparams, add_container, /*add_preview=*/true,
        /*add_icc_profile=*/false,
        reconstructible_jpeg ? &jpeg_codestreams[i] : nullptr);
  }

  // Test multiple step sizes, to test different combinations of the streaming
  // box parsing.
  std::vector<size_t> increments = {1, 3, 17, 23, 120, 700, 1050};

  for (size_t index = 0; index < increments.size(); index++) {
    for (size_t i = 0; i < kCSBF_NUM_ENTRIES; ++i) {
      if (reconstructible_jpeg &&
          (CodeStreamBoxFormat)i == CodeStreamBoxFormat::kCSBF_None) {
        continue;
      }
      const jxl::PaddedBytes& data = codestreams[i];
      const uint8_t* next_in = data.data();
      size_t avail_in = 0;

      JxlDecoder* dec = JxlDecoderCreate(nullptr);

      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderSubscribeEvents(
                    dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE |
                             JXL_DEC_JPEG_RECONSTRUCTION));

      bool seen_basic_info = false;
      bool seen_full_image = false;
      bool seen_jpeg_recon = false;

      size_t total_size = 0;

      for (;;) {
        EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));
        JxlDecoderStatus status = JxlDecoderProcessInput(dec);
        size_t remaining = JxlDecoderReleaseInput(dec);
        EXPECT_LE(remaining, avail_in);
        next_in += avail_in - remaining;
        avail_in = remaining;
        if (status == JXL_DEC_NEED_MORE_INPUT) {
          if (total_size >= data.size()) {
            // End of test data reached, it should have successfully decoded the
            // image now.
            FAIL();
            break;
          }

          size_t increment = increments[index];
          // End of the file reached, should be the final test.
          if (total_size + increment > data.size()) {
            increment = data.size() - total_size;
          }
          total_size += increment;
          avail_in += increment;
        } else if (status == JXL_DEC_BASIC_INFO) {
          // This event should happen exactly once
          EXPECT_FALSE(seen_basic_info);
          if (seen_basic_info) break;
          seen_basic_info = true;
          JxlBasicInfo info;
          EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
          EXPECT_EQ(info.xsize, xsize);
          EXPECT_EQ(info.ysize, ysize);
        } else if (status == JXL_DEC_JPEG_RECONSTRUCTION) {
          EXPECT_FALSE(seen_basic_info);
          EXPECT_FALSE(seen_full_image);
          EXPECT_EQ(JXL_DEC_SUCCESS,
                    JxlDecoderSetJPEGBuffer(dec, jpeg_output.data(),
                                            jpeg_output.size()));
          seen_jpeg_recon = true;
        } else if (status == JXL_DEC_JPEG_NEED_MORE_OUTPUT) {
          EXPECT_TRUE(seen_jpeg_recon);
          used_jpeg_output =
              jpeg_output.size() - JxlDecoderReleaseJPEGBuffer(dec);
          jpeg_output.resize(jpeg_output.size() * 2);
          EXPECT_EQ(JXL_DEC_SUCCESS,
                    JxlDecoderSetJPEGBuffer(
                        dec, jpeg_output.data() + used_jpeg_output,
                        jpeg_output.size() - used_jpeg_output));
        } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
          EXPECT_EQ(JXL_DEC_SUCCESS,
                    JxlDecoderSetImageOutBuffer(
                        dec, &format_orig, pixels2.data(), pixels2.size()));
        } else if (status == JXL_DEC_FULL_IMAGE) {
          // This event should happen exactly once
          EXPECT_FALSE(seen_full_image);
          if (seen_full_image) break;
          // This event should happen after basic info
          EXPECT_TRUE(seen_basic_info);
          seen_full_image = true;
          if (reconstructible_jpeg) {
            used_jpeg_output =
                jpeg_output.size() - JxlDecoderReleaseJPEGBuffer(dec);
            EXPECT_EQ(used_jpeg_output, jpeg_codestreams[i].size());
            EXPECT_EQ(0, memcmp(jpeg_output.data(), jpeg_codestreams[i].data(),
                                used_jpeg_output));
          } else {
            EXPECT_EQ(pixels, pixels2);
          }
        } else if (status == JXL_DEC_SUCCESS) {
          EXPECT_TRUE(seen_full_image);
          break;
        } else {
          // We do not expect any other events or errors
          FAIL();
          break;
        }
      }

      // Ensure the decoder emitted the basic info and full image events
      EXPECT_TRUE(seen_basic_info);
      EXPECT_TRUE(seen_full_image);

      JxlDecoderDestroy(dec);
    }
  }
}

// Tests the return status when trying to decode pixels on incomplete file: it
// should return JXL_DEC_NEED_MORE_INPUT, not error.
TEST(DecodeTest, PixelPartialTest) { TestPartialStream(false); }

#if JPEGXL_ENABLE_JPEG
// Tests the return status when trying to decode JPEG bytes on incomplete file.
TEST(DecodeTest, JPEGPartialTest) { TestPartialStream(true); }
#endif  // JPEGXL_ENABLE_JPEG

TEST(DecodeTest, DCTest) {
  using jxl::kBlockDim;

  // TODO(lode): test with a completely black image, with alpha channel
  // 65536, since that gave an error during debuging for getting DC
  // image (namely: "Failed to decode AC metadata")

  // Ensure a dimension is larger than 256 so that there are multiple groups,
  // otherwise getting DC does not work due to how TOC is then laid out.
  size_t xsize = 260, ysize = 77;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 3, 0);

  // Set the params to lossy, since getting DC with API is only supported for
  // lossy at this time.
  jxl::CompressParams cparams;
  jxl::PaddedBytes compressed = jxl::CreateTestJXLCodestream(
      jxl::Span<const uint8_t>(pixels.data(), pixels.size()), xsize, ysize, 3,
      cparams, kCSBF_Multi, true);

  JxlPixelFormat format = {3, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};

  // Binary search for the DC size, the first byte where trying to get the DC
  // returns JXL_DEC_NEED_DC_OUT_BUFFER rather than JXL_DEC_NEED_MORE_INPUT.
  // This search is a test on its own, verifying the decoder succeeds after
  // this point and needs more input before it, without errors. It also allows
  // the main test below to work on a partial file with only DC.
  size_t start = 0;
  size_t end = compressed.size();
  size_t dc_size;
  for (;;) {
    dc_size = (start + end) / 2;
    JxlDecoderStatus status;
    JxlDecoder* dec = JxlDecoderCreate(NULL);
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSubscribeEvents(
                                   dec, JXL_DEC_BASIC_INFO | JXL_DEC_DC_IMAGE));
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSetInput(dec, compressed.data(), dc_size));
    status = JxlDecoderProcessInput(dec);
    EXPECT_TRUE(status == JXL_DEC_BASIC_INFO ||
                status == JXL_DEC_NEED_MORE_INPUT);
    if (status != JXL_DEC_NEED_MORE_INPUT) {
      status = JxlDecoderProcessInput(dec);
      EXPECT_TRUE(status == JXL_DEC_NEED_DC_OUT_BUFFER ||
                  status == JXL_DEC_NEED_MORE_INPUT);
    }
    JxlDecoderDestroy(dec);
    if (status == JXL_DEC_NEED_MORE_INPUT) {
      start = dc_size;
      if (start == end || start + 1 == end) {
        dc_size++;
        break;
      }
    } else {
      end = dc_size;
      if (start == end || start + 1 == end) {
        break;
      }
    }
  }

  // Test that the dc_size is within expected limits: it should be larger than
  // 0, and smaller than the entire file, taking 90% here, 50% is too
  // optimistic.
  EXPECT_LE(dc_size, compressed.size() * 9 / 10);
  EXPECT_GT(dc_size, 0);

  JxlDecoder* dec = JxlDecoderCreate(NULL);

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSubscribeEvents(
                                 dec, JXL_DEC_BASIC_INFO | JXL_DEC_DC_IMAGE));
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetInput(dec, compressed.data(), dc_size));

  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
  size_t buffer_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderDCOutBufferSize(dec, &format, &buffer_size));

  size_t xsize_dc = (xsize + kBlockDim - 1) / kBlockDim;
  size_t ysize_dc = (ysize + kBlockDim - 1) / kBlockDim;
  EXPECT_EQ(xsize_dc * ysize_dc * 3, buffer_size);

  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));

  EXPECT_EQ(JXL_DEC_NEED_DC_OUT_BUFFER, JxlDecoderProcessInput(dec));

  std::vector<uint8_t> dc(buffer_size);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetDCOutBuffer(dec, &format, dc.data(), dc.size()));

  EXPECT_EQ(JXL_DEC_DC_IMAGE, JxlDecoderProcessInput(dec));

  jxl::Image3F dc0(xsize_dc, ysize_dc);
  jxl::Image3F dc1(xsize_dc, ysize_dc);

  // Downscale the original image 8x8 to allow comparing with the DC.
  for (size_t y = 0; y < ysize_dc; y++) {
    for (size_t x = 0; x < xsize_dc; x++) {
      double r = 0, g = 0, b = 0;
      size_t num = 0;
      for (size_t by = 0; by < kBlockDim; by++) {
        size_t y2 = y * kBlockDim + by;
        if (y2 >= ysize) break;
        for (size_t bx = 0; bx < kBlockDim; bx++) {
          size_t x2 = x * kBlockDim + bx;
          if (x2 >= xsize) break;
          // Use linear RGB for correct downscaling.
          r += jxl::Srgb8ToLinearDirect((1.f / 255) *
                                        pixels[(y2 * xsize + x2) * 6 + 0]);
          g += jxl::Srgb8ToLinearDirect((1.f / 255) *
                                        pixels[(y2 * xsize + x2) * 6 + 2]);
          b += jxl::Srgb8ToLinearDirect((1.f / 255) *
                                        pixels[(y2 * xsize + x2) * 6 + 4]);
          num++;
        }
      }
      // Take average per block.
      double mul = 1.0 / num;
      r *= mul;
      g *= mul;
      b *= mul;
      dc0.PlaneRow(0, y)[x] = r;
      dc0.PlaneRow(1, y)[x] = g;
      dc0.PlaneRow(2, y)[x] = b;
      dc1.PlaneRow(0, y)[x] = (1.f / 255) * (dc[(y * xsize_dc + x) * 3 + 0]);
      dc1.PlaneRow(1, y)[x] = (1.f / 255) * (dc[(y * xsize_dc + x) * 3 + 1]);
      dc1.PlaneRow(2, y)[x] = (1.f / 255) * (dc[(y * xsize_dc + x) * 3 + 2]);
    }
  }

  // dc0 is in linear sRGB because we converted it to linear in the downscaling
  // above.
  jxl::CodecInOut dc0_io;
  dc0_io.SetFromImage(std::move(dc0), jxl::ColorEncoding::LinearSRGB(false));
  // dc1 is in non-linear sRGB because the C decoding API outputs non-linear
  // sRGB for VarDCT to integer output types
  jxl::CodecInOut dc1_io;
  dc1_io.SetFromImage(std::move(dc1), jxl::ColorEncoding::SRGB(false));

  // Check with butteraugli that the DC is close to the 8x8 downscaled original
  // image. We don't expect a score of 0, since the downscaling done may not
  // 100% match what is stored for the DC, and the lossy codec is used.
  // A reasonable butteraugli distance shows that the DC works, the color
  // encoding (transfer function) is correct and geometry (shifts, ...) is
  // correct.
  jxl::ButteraugliParams ba;
  EXPECT_LE(ButteraugliDistance(dc0_io, dc1_io, ba,
                                /*distmap=*/nullptr, nullptr),
            3.0f);

  JxlDecoderDestroy(dec);
}

TEST(DecodeTest, DCNotGettableTest) {
  // 1x1 pixel JXL image
  std::string compressed(
      "\377\n\0\20\260\23\0H\200("
      "\0\334\0U\17\0\0\250P\31e\334\340\345\\\317\227\37:,"
      "\246m\\gh\253m\vK\22E\306\261I\252C&pH\22\353 "
      "\363\6\22\bp\0\200\237\34\231W2d\255$\1",
      68);

  JxlDecoder* dec = JxlDecoderCreate(NULL);

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSubscribeEvents(
                                 dec, JXL_DEC_BASIC_INFO | JXL_DEC_DC_IMAGE));
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetInput(
                dec, reinterpret_cast<const uint8_t*>(compressed.data()),
                compressed.size()));

  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));

  // Since the image is only 1x1 pixel, there is only 1 group, the decoder is
  // unable to get DC size from this, and will not return the DC at all. Since
  // no full image is requested either, it is expected to return success.
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));

  JxlDecoderDestroy(dec);
}

TEST(DecodeTest, PreviewTest) {
  size_t xsize = 77, ysize = 120;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 3, 0);

  jxl::CompressParams cparams;
  jxl::PaddedBytes compressed = jxl::CreateTestJXLCodestream(
      jxl::Span<const uint8_t>(pixels.data(), pixels.size()), xsize, ysize, 3,
      cparams, kCSBF_Multi, /*add_preview=*/true);

  JxlPixelFormat format = {3, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};

  JxlDecoder* dec = JxlDecoderCreate(NULL);
  const uint8_t* next_in = compressed.data();
  size_t avail_in = compressed.size();

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec, JXL_DEC_BASIC_INFO | JXL_DEC_PREVIEW_IMAGE));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));

  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
  size_t buffer_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderPreviewOutBufferSize(dec, &format, &buffer_size));

  // GetSomeTestImage is hardcoded to use a top-left cropped preview with
  // floor of 1/7th of the size
  size_t xsize_preview = (xsize / 7);
  size_t ysize_preview = (ysize / 7);
  EXPECT_EQ(xsize_preview, info.preview.xsize);
  EXPECT_EQ(ysize_preview, info.preview.ysize);
  EXPECT_EQ(xsize_preview * ysize_preview * 3, buffer_size);

  EXPECT_EQ(JXL_DEC_NEED_PREVIEW_OUT_BUFFER, JxlDecoderProcessInput(dec));

  std::vector<uint8_t> preview(xsize_preview * ysize_preview * 3);
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetPreviewOutBuffer(
                                 dec, &format, preview.data(), preview.size()));

  EXPECT_EQ(JXL_DEC_PREVIEW_IMAGE, JxlDecoderProcessInput(dec));

  jxl::Image3F preview0(xsize_preview, ysize_preview);
  jxl::Image3F preview1(xsize_preview, ysize_preview);

  // For preview0, the original: top-left crop the preview image the way
  // GetSomeTestImage does.
  for (size_t y = 0; y < ysize_preview; y++) {
    for (size_t x = 0; x < xsize_preview; x++) {
      preview0.PlaneRow(0, y)[x] =
          (1.f / 255) * (pixels[(y * xsize + x) * 6 + 0]);
      preview0.PlaneRow(1, y)[x] =
          (1.f / 255) * (pixels[(y * xsize + x) * 6 + 2]);
      preview0.PlaneRow(2, y)[x] =
          (1.f / 255) * (pixels[(y * xsize + x) * 6 + 4]);
      preview1.PlaneRow(0, y)[x] =
          (1.f / 255) * (preview[(y * xsize_preview + x) * 3 + 0]);
      preview1.PlaneRow(1, y)[x] =
          (1.f / 255) * (preview[(y * xsize_preview + x) * 3 + 1]);
      preview1.PlaneRow(2, y)[x] =
          (1.f / 255) * (preview[(y * xsize_preview + x) * 3 + 2]);
    }
  }

  jxl::CodecInOut io0;
  io0.SetFromImage(std::move(preview0), jxl::ColorEncoding::SRGB(false));
  jxl::CodecInOut io1;
  io1.SetFromImage(std::move(preview1), jxl::ColorEncoding::SRGB(false));

  jxl::ButteraugliParams ba;
  // TODO(lode): this ButteraugliDistance silently returns 0 (dangerous for
  // tests) if xsize or ysize is < 8, no matter how different the images, a tiny
  // size that could happen for a preview. ButteraugliDiffmap does support
  // smaller than 8x8, but jxl's ButteraugliDistance does not. Perhaps move
  // butteraugli's <8x8 handling from ButteraugliDiffmap to
  // ButteraugliComparator::Diffmap in butteraugli.cc.
  EXPECT_LE(ButteraugliDistance(io0, io1, ba,
                                /*distmap=*/nullptr, nullptr),
            1.4f);

  JxlDecoderDestroy(dec);
}

TEST(DecodeTest, AlignTest) {
  size_t xsize = 123, ysize = 77;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);
  JxlPixelFormat format_orig = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  jxl::CompressParams cparams;
  cparams.SetLossless();  // Lossless to verify pixels exactly after roundtrip.
  jxl::PaddedBytes compressed = jxl::CreateTestJXLCodestream(
      jxl::Span<const uint8_t>(pixels.data(), pixels.size()), xsize, ysize, 4,
      cparams, kCSBF_None, false);

  size_t align = 17;
  JxlPixelFormat format = {3, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, align};
  // On purpose not using jxl::RoundUpTo to test it independently.
  size_t expected_line_bytes = (1 * 3 * xsize + align - 1) / align * align;

  for (int use_callback = 0; use_callback <= 1; ++use_callback) {
    std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
        jxl::Span<const uint8_t>(compressed.data(), compressed.size()), format,
        use_callback, /*set_buffer_early=*/false);
    EXPECT_EQ(expected_line_bytes * ysize, pixels2.size());
    EXPECT_EQ(0, ComparePixels(pixels.data(), pixels2.data(), xsize, ysize,
                               format_orig, format));
  }
}

TEST(DecodeTest, AnimationTest) {
  size_t xsize = 123, ysize = 77;
  static const size_t num_frames = 2;
  std::vector<uint8_t> frames[2];
  frames[0] = jxl::test::GetSomeTestImage(xsize, ysize, 3, 0);
  frames[1] = jxl::test::GetSomeTestImage(xsize, ysize, 3, 1);
  JxlPixelFormat format = {3, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  jxl::CodecInOut io;
  io.SetSize(xsize, ysize);
  io.metadata.m.SetUintSamples(16);
  io.metadata.m.color_encoding = jxl::ColorEncoding::SRGB(false);
  io.metadata.m.have_animation = true;
  io.frames.clear();
  io.frames.reserve(num_frames);
  io.SetSize(xsize, ysize);

  std::vector<uint32_t> frame_durations(num_frames);
  for (size_t i = 0; i < num_frames; ++i) {
    frame_durations[i] = 5 + i;
  }

  for (size_t i = 0; i < num_frames; ++i) {
    jxl::ImageBundle bundle(&io.metadata.m);

    EXPECT_TRUE(ConvertFromExternal(
        jxl::Span<const uint8_t>(frames[i].data(), frames[i].size()), xsize,
        ysize, jxl::ColorEncoding::SRGB(/*is_gray=*/false), /*has_alpha=*/false,
        /*alpha_is_premultiplied=*/false, /*bits_per_sample=*/16,
        JXL_BIG_ENDIAN, /*flipped_y=*/false, /*pool=*/nullptr, &bundle));
    bundle.duration = frame_durations[i];
    io.frames.push_back(std::move(bundle));
  }

  jxl::CompressParams cparams;
  cparams.SetLossless();  // Lossless to verify pixels exactly after roundtrip.
  jxl::AuxOut aux_out;
  jxl::PaddedBytes compressed;
  jxl::PassesEncoderState enc_state;
  EXPECT_TRUE(jxl::EncodeFile(cparams, &io, &enc_state, &compressed, &aux_out,
                              nullptr));

  // Decode and test the animation frames

  JxlDecoder* dec = JxlDecoderCreate(NULL);
  const uint8_t* next_in = compressed.data();
  size_t avail_in = compressed.size();

  void* runner = JxlThreadParallelRunnerCreate(
      NULL, JxlThreadParallelRunnerDefaultNumWorkerThreads());
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetParallelRunner(dec, JxlThreadParallelRunner, runner));

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));

  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
  size_t buffer_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));

  for (size_t i = 0; i < num_frames; ++i) {
    std::vector<uint8_t> pixels(buffer_size);

    EXPECT_EQ(JXL_DEC_FRAME, JxlDecoderProcessInput(dec));

    JxlFrameHeader frame_header;
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetFrameHeader(dec, &frame_header));
    EXPECT_EQ(frame_durations[i], frame_header.duration);
    EXPECT_EQ(0, frame_header.name_length);
    // For now, test with empty name, there's currently no easy way to encode
    // a jxl file with a frame name because ImageBundle doesn't have a
    // jxl::FrameHeader to set the name in. We can test the null termination
    // character though.
    char name;
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetFrameName(dec, &name, 1));
    EXPECT_EQ(0, name);

    EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                   dec, &format, pixels.data(), pixels.size()));

    EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
    EXPECT_EQ(0, ComparePixels(frames[i].data(), pixels.data(), xsize, ysize,
                               format, format));
  }

  // After all frames gotten, JxlDecoderProcessInput should return
  // success to indicate all is done.
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));

  JxlThreadParallelRunnerDestroy(runner);
  JxlDecoderDestroy(dec);
}

TEST(DecodeTest, AnimationTestStreaming) {
  size_t xsize = 123, ysize = 77;
  static const size_t num_frames = 2;
  std::vector<uint8_t> frames[2];
  frames[0] = jxl::test::GetSomeTestImage(xsize, ysize, 3, 0);
  frames[1] = jxl::test::GetSomeTestImage(xsize, ysize, 3, 1);
  JxlPixelFormat format = {3, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  jxl::CodecInOut io;
  io.SetSize(xsize, ysize);
  io.metadata.m.SetUintSamples(16);
  io.metadata.m.color_encoding = jxl::ColorEncoding::SRGB(false);
  io.metadata.m.have_animation = true;
  io.frames.clear();
  io.frames.reserve(num_frames);
  io.SetSize(xsize, ysize);

  std::vector<uint32_t> frame_durations(num_frames);
  for (size_t i = 0; i < num_frames; ++i) {
    frame_durations[i] = 5 + i;
  }

  for (size_t i = 0; i < num_frames; ++i) {
    jxl::ImageBundle bundle(&io.metadata.m);

    EXPECT_TRUE(ConvertFromExternal(
        jxl::Span<const uint8_t>(frames[i].data(), frames[i].size()), xsize,
        ysize, jxl::ColorEncoding::SRGB(/*is_gray=*/false), /*has_alpha=*/false,
        /*alpha_is_premultiplied=*/false, /*bits_per_sample=*/16,
        JXL_BIG_ENDIAN, /*flipped_y=*/false, /*pool=*/nullptr, &bundle));
    bundle.duration = frame_durations[i];
    io.frames.push_back(std::move(bundle));
  }

  jxl::CompressParams cparams;
  cparams.SetLossless();  // Lossless to verify pixels exactly after roundtrip.
  jxl::AuxOut aux_out;
  jxl::PaddedBytes compressed;
  jxl::PassesEncoderState enc_state;
  EXPECT_TRUE(jxl::EncodeFile(cparams, &io, &enc_state, &compressed, &aux_out,
                              nullptr));

  // Decode and test the animation frames

  const size_t step_size = 16;

  JxlDecoder* dec = JxlDecoderCreate(NULL);
  const uint8_t* next_in = compressed.data();
  size_t avail_in = 0;
  size_t frame_headers_seen = 0;
  size_t frames_seen = 0;
  bool seen_basic_info = false;

  void* runner = JxlThreadParallelRunnerCreate(
      NULL, JxlThreadParallelRunnerDefaultNumWorkerThreads());
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetParallelRunner(dec, JxlThreadParallelRunner, runner));

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));

  std::vector<uint8_t> frames2[2];
  for (size_t i = 0; i < num_frames; ++i) {
    frames2[i].resize(frames[i].size());
  }

  size_t total_in = 0;
  size_t loop_count = 0;

  for (;;) {
    if (loop_count++ > compressed.size()) {
      fprintf(stderr, "Too many loops\n");
      FAIL();
      break;
    }

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));
    auto status = JxlDecoderProcessInput(dec);
    size_t remaining = JxlDecoderReleaseInput(dec);
    EXPECT_LE(remaining, avail_in);
    next_in += avail_in - remaining;
    avail_in = remaining;

    if (status == JXL_DEC_SUCCESS) {
      break;
    } else if (status == JXL_DEC_ERROR) {
      FAIL();
    } else if (status == JXL_DEC_NEED_MORE_INPUT) {
      if (total_in >= compressed.size()) {
        fprintf(stderr, "Already gave all input data\n");
        FAIL();
        break;
      }
      size_t amount = step_size;
      if (total_in + amount > compressed.size()) {
        amount = compressed.size() - total_in;
      }
      avail_in += amount;
      total_in += amount;
    } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                     dec, &format, frames2[frames_seen].data(),
                                     frames2[frames_seen].size()));
    } else if (status == JXL_DEC_BASIC_INFO) {
      EXPECT_EQ(false, seen_basic_info);
      seen_basic_info = true;
      JxlBasicInfo info;
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
      EXPECT_EQ(xsize, info.xsize);
      EXPECT_EQ(ysize, info.ysize);
    } else if (status == JXL_DEC_FRAME) {
      EXPECT_EQ(true, seen_basic_info);
      frame_headers_seen++;
    } else if (status == JXL_DEC_FULL_IMAGE) {
      frames_seen++;
      EXPECT_EQ(frame_headers_seen, frames_seen);
    } else {
      fprintf(stderr, "Unexpected status: %d\n", (int)status);
      FAIL();
    }
  }

  EXPECT_EQ(true, seen_basic_info);
  EXPECT_EQ(num_frames, frames_seen);
  EXPECT_EQ(num_frames, frame_headers_seen);
  for (size_t i = 0; i < num_frames; ++i) {
    EXPECT_EQ(frames[i], frames2[i]);
  }

  JxlThreadParallelRunnerDestroy(runner);
  JxlDecoderDestroy(dec);
}

TEST(DecodeTest, FlushTest) {
  // Size large enough for multiple groups, required to have progressive
  // stages
  size_t xsize = 333, ysize = 300;
  uint32_t num_channels = 3;
  std::vector<uint8_t> pixels =
      jxl::test::GetSomeTestImage(xsize, ysize, num_channels, 0);
  jxl::CompressParams cparams;
  jxl::PaddedBytes data = jxl::CreateTestJXLCodestream(
      jxl::Span<const uint8_t>(pixels.data(), pixels.size()), xsize, ysize,
      num_channels, cparams, kCSBF_None, true);
  JxlPixelFormat format = {num_channels, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  std::vector<uint8_t> pixels2;
  pixels2.resize(pixels.size());

  JxlDecoder* dec = JxlDecoderCreate(nullptr);

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));

  // Ensure that the first part contains at least the full DC of the image,
  // otherwise flush does not work. The DC takes up more than 50% of the
  // image generated here.
  size_t first_part = data.size() * 3 / 4;

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, data.data(), first_part));

  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
  EXPECT_EQ(info.xsize, xsize);
  EXPECT_EQ(info.ysize, ysize);

  EXPECT_EQ(JXL_DEC_FRAME, JxlDecoderProcessInput(dec));

  // Output buffer not yet set
  EXPECT_EQ(JXL_DEC_ERROR, JxlDecoderFlushImage(dec));

  size_t buffer_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
  EXPECT_EQ(pixels2.size(), buffer_size);
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                 dec, &format, pixels2.data(), pixels2.size()));

  // Must process input further until we get JXL_DEC_NEED_MORE_INPUT, even if
  // data was already input before, since the processing of the frame only
  // happens at the JxlDecoderProcessInput call after JXL_DEC_FRAME.
  EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, JxlDecoderProcessInput(dec));

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderFlushImage(dec));

  // Note: actual pixel data not tested here, it should look similar to the
  // input image, but with less fine detail. Instead the expected events are
  // tested here.

  EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, JxlDecoderProcessInput(dec));

  size_t consumed = first_part - JxlDecoderReleaseInput(dec);

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, data.data() + consumed,
                                                data.size() - consumed));
  EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));

  JxlDecoderDestroy(dec);
}

void VerifyJPEGReconstruction(const jxl::PaddedBytes& container,
                              const jxl::PaddedBytes& jpeg_bytes) {
  JxlDecoderPtr dec = JxlDecoderMake(nullptr);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec.get(), JXL_DEC_JPEG_RECONSTRUCTION | JXL_DEC_FULL_IMAGE));
  JxlDecoderSetInput(dec.get(), container.data(), container.size());
  EXPECT_EQ(JXL_DEC_JPEG_RECONSTRUCTION, JxlDecoderProcessInput(dec.get()));
  std::vector<uint8_t> reconstructed_buffer(128);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetJPEGBuffer(dec.get(), reconstructed_buffer.data(),
                                    reconstructed_buffer.size()));
  size_t used = 0;
  JxlDecoderStatus process_result = JXL_DEC_JPEG_NEED_MORE_OUTPUT;
  while (process_result == JXL_DEC_JPEG_NEED_MORE_OUTPUT) {
    used = reconstructed_buffer.size() - JxlDecoderReleaseJPEGBuffer(dec.get());
    reconstructed_buffer.resize(reconstructed_buffer.size() * 2);
    EXPECT_EQ(
        JXL_DEC_SUCCESS,
        JxlDecoderSetJPEGBuffer(dec.get(), reconstructed_buffer.data() + used,
                                reconstructed_buffer.size() - used));
    process_result = JxlDecoderProcessInput(dec.get());
  }
  ASSERT_EQ(JXL_DEC_FULL_IMAGE, process_result);
  used = reconstructed_buffer.size() - JxlDecoderReleaseJPEGBuffer(dec.get());
  ASSERT_EQ(used, jpeg_bytes.size());
  EXPECT_EQ(0, memcmp(reconstructed_buffer.data(), jpeg_bytes.data(), used));
}

#if JPEGXL_ENABLE_JPEG
TEST(DecodeTest, JPEGReconstructTestCodestream) {
  size_t xsize = 123;
  size_t ysize = 77;
  size_t channels = 3;
  std::vector<uint8_t> pixels =
      jxl::test::GetSomeTestImage(xsize, ysize, channels, /*seed=*/0);
  jxl::CompressParams cparams;
  cparams.color_transform = jxl::ColorTransform::kNone;
  jxl::PaddedBytes jpeg_codestream;
  jxl::PaddedBytes compressed = jxl::CreateTestJXLCodestream(
      jxl::Span<const uint8_t>(pixels.data(), pixels.size()), xsize, ysize,
      channels, cparams, kCSBF_Single, /*add_preview=*/true,
      /*add_icc_profile=*/false, &jpeg_codestream);
  VerifyJPEGReconstruction(compressed, jpeg_codestream);
}
#endif  // JPEGXL_ENABLE_JPEG

TEST(DecodeTest, JPEGReconstructionTest) {
  const std::string jpeg_path =
      "imagecompression.info/flower_foveon.png.im_q85_420.jpg";
  const jxl::PaddedBytes orig = jxl::ReadTestData(jpeg_path);
  jxl::CodecInOut orig_io;
  ASSERT_TRUE(
      jxl::jpeg::DecodeImageJPG(jxl::Span<const uint8_t>(orig), &orig_io));
  orig_io.metadata.m.xyb_encoded = false;
  jxl::BitWriter writer;
  ASSERT_TRUE(WriteHeaders(&orig_io.metadata, &writer, nullptr));
  writer.ZeroPadToByte();
  jxl::PassesEncoderState enc_state;
  jxl::CompressParams cparams;
  cparams.color_transform = jxl::ColorTransform::kNone;
  ASSERT_TRUE(jxl::EncodeFrame(cparams, jxl::FrameInfo{}, &orig_io.metadata,
                               orig_io.Main(), &enc_state,
                               /*pool=*/nullptr, &writer,
                               /*aux_out=*/nullptr));

  jxl::PaddedBytes jpeg_data;
  ASSERT_TRUE(EncodeJPEGData(*orig_io.Main().jpeg_data.get(), &jpeg_data));
  jxl::PaddedBytes container;
  container.append(jxl::kContainerHeader,
                   jxl::kContainerHeader + sizeof(jxl::kContainerHeader));
  jxl::AppendBoxHeader(jxl::MakeBoxType("jbrd"), jpeg_data.size(), false,
                       &container);
  container.append(jpeg_data.data(), jpeg_data.data() + jpeg_data.size());
  jxl::AppendBoxHeader(jxl::MakeBoxType("jxlc"), 0, true, &container);
  jxl::PaddedBytes codestream = std::move(writer).TakeBytes();
  container.append(codestream.data(), codestream.data() + codestream.size());
  VerifyJPEGReconstruction(container, orig);
}
