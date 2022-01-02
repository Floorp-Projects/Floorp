// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "jxl/decode.h"

#include <stdint.h>
#include <stdlib.h>

#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "jxl/decode_cxx.h"
#include "jxl/resizable_parallel_runner_cxx.h"
#include "jxl/thread_parallel_runner_cxx.h"
#include "lib/extras/codec.h"
#include "lib/extras/codec_jpg.h"
#include "lib/jxl/base/byte_order.h"
#include "lib/jxl/base/file_io.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dec_external_image.h"
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
#include "lib/jxl/image_test_utils.h"
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
  kCSBF_Single_Other,
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
  // Have a compressed exif box before a regular codestream box
  kCSBF_Brob_Exif,
  // Not a value but used for counting amount of enum entries
  kCSBF_NUM_ENTRIES,
};

// Unknown boxes for testing
static const char* unk1_box_type = "unk1";
static const char* unk1_box_contents = "abcdefghijklmnopqrstuvwxyz";
static const size_t unk1_box_size = strlen(unk1_box_contents);
static const char* unk2_box_type = "unk2";
static const char* unk2_box_contents = "0123456789";
static const size_t unk2_box_size = strlen(unk2_box_contents);
static const char* unk3_box_type = "unk3";
static const char* unk3_box_contents = "ABCDEF123456";
static const size_t unk3_box_size = strlen(unk3_box_contents);
// Box with brob-compressed exif, including header
static const uint8_t* box_brob_exif = reinterpret_cast<const uint8_t*>(
    "\0\0\0@brobExif\241\350\2\300\177\244v\2525\304\360\27=?\267{"
    "\33\37\314\332\214QX17PT\"\256\0\0\202s\214\313t\333\310\320k\20\276\30"
    "\204\277l$\326c#\1\b");
size_t box_brob_exif_size = 64;
// The uncompressed Exif data from the brob box
static const uint8_t* exif_uncompressed = reinterpret_cast<const uint8_t*>(
    "\0\0\0\0MM\0*"
    "\0\0\0\b\0\5\1\22\0\3\0\0\0\1\0\5\0\0\1\32\0\5\0\0\0\1\0\0\0J\1\33\0\5\0\0"
    "\0\1\0\0\0R\1("
    "\0\3\0\0\0\1\0\1\0\0\2\23\0\3\0\0\0\1\0\1\0\0\0\0\0\0\0\0\0\1\0\0\0\1\0\0"
    "\0\1\0\0\0\1");
size_t exif_uncompressed_size = 94;

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

void AppendTestBox(const char* type, const char* contents, size_t contents_size,
                   bool unbounded, PaddedBytes* bytes) {
  AppendU32BE(contents_size + 8, bytes);
  bytes->push_back(type[0]);
  bytes->push_back(type[1]);
  bytes->push_back(type[2]);
  bytes->push_back(type[3]);
  const uint8_t* contents_u = reinterpret_cast<const uint8_t*>(contents);
  bytes->append(contents_u, contents_u + contents_size);
}

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
PaddedBytes CreateTestJXLCodestream(
    Span<const uint8_t> pixels, size_t xsize, size_t ysize, size_t num_channels,
    const CompressParams& cparams, CodeStreamBoxFormat add_container,
    JxlOrientation orientation, bool add_preview, bool add_icc_profile = false,
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
      /*flipped_y=*/false, &pool, &io.Main(), /*float_in=*/false));
  jxl::PaddedBytes jpeg_data;
  if (jpeg_codestream != nullptr) {
#if JPEGXL_ENABLE_JPEG
    jxl::PaddedBytes jpeg_bytes;
    EXPECT_TRUE(EncodeImageJPG(&io, jxl::extras::JpegEncoder::kLibJpeg,
                               /*quality=*/70, jxl::YCbCrChromaSubsampling(),
                               &pool, &jpeg_bytes));
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
  io.metadata.m.orientation = orientation;
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
      AppendTestBox(unk1_box_type, unk1_box_contents, unk1_box_size, false, &c);
      AppendTestBox(unk2_box_type, unk2_box_contents, unk2_box_size, false, &c);
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
      // Third (last) codestream part
      AppendU32BE(add_container == kCSBF_Multi_Zero_Terminated
                      ? 0
                      : (compressed2.size() + 12),
                  &c);
      c.push_back('j');
      c.push_back('x');
      c.push_back('l');
      c.push_back('p');
      AppendU32BE(jxlp_index++ | 0x80000000, &c);
      c.append(compressed2.data(), compressed2.data() + compressed2.size());
      if (add_container == kCSBF_Multi_Other_Terminated) {
        AppendTestBox(unk3_box_type, unk3_box_contents, unk3_box_size, false,
                      &c);
      }
      if (add_container == kCSBF_Multi_Other_Zero_Terminated) {
        AppendTestBox(unk3_box_type, unk3_box_contents, unk3_box_size, true,
                      &c);
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
      if (add_container == kCSBF_Brob_Exif) {
        c.append(box_brob_exif, box_brob_exif + box_brob_exif_size);
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
      if (add_container == kCSBF_Single_Other) {
        AppendTestBox(unk1_box_type, unk1_box_contents, unk1_box_size, false,
                      &c);
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
                                   bool use_callback, bool set_buffer_early,
                                   bool use_resizable_runner,
                                   PaddedBytes* icc = nullptr) {
  JxlThreadParallelRunnerPtr runner_fixed;
  JxlResizableParallelRunnerPtr runner_resizable;
  JxlParallelRunner runner_fn;
  void* runner;

  if (use_resizable_runner) {
    runner_resizable = JxlResizableParallelRunnerMake(nullptr);
    runner = runner_resizable.get();
    runner_fn = JxlResizableParallelRunner;
  } else {
    runner_fixed = JxlThreadParallelRunnerMake(
        nullptr, JxlThreadParallelRunnerDefaultNumWorkerThreads());
    runner = runner_fixed.get();
    runner_fn = JxlThreadParallelRunner;
  }
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetParallelRunner(dec, runner_fn, runner));

  EXPECT_EQ(
      JXL_DEC_SUCCESS,
      JxlDecoderSubscribeEvents(
          dec, JXL_DEC_BASIC_INFO | (set_buffer_early ? JXL_DEC_FRAME : 0) |
                   JXL_DEC_PREVIEW_IMAGE | JXL_DEC_FULL_IMAGE |
                   (icc != nullptr ? JXL_DEC_COLOR_ENCODING : 0)));

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetInput(dec, compressed.data(), compressed.size()));
  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
  size_t buffer_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
  if (use_resizable_runner) {
    JxlResizableParallelRunnerSetThreads(
        runner,
        JxlResizableParallelRunnerSuggestThreads(info.xsize, info.ysize));
  }

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

  if (status == JXL_DEC_COLOR_ENCODING) {
    size_t icc_size = 0;
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderGetICCProfileSize(
                  dec, &format, JXL_COLOR_PROFILE_TARGET_DATA, &icc_size));
    icc->resize(icc_size);
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetColorAsICCProfile(
                                   dec, &format, JXL_COLOR_PROFILE_TARGET_DATA,
                                   icc->data(), icc_size));

    status = JxlDecoderProcessInput(dec);
  }

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

  // After the full image was output, JxlDecoderProcessInput should return
  // success to indicate all is done.
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));

  return pixels;
}

// Decodes one-shot with the API for non-streaming decoding tests.
std::vector<uint8_t> DecodeWithAPI(Span<const uint8_t> compressed,
                                   const JxlPixelFormat& format,
                                   bool use_callback, bool set_buffer_early,
                                   bool use_resizable_runner) {
  JxlDecoder* dec = JxlDecoderCreate(NULL);
  std::vector<uint8_t> pixels =
      DecodeWithAPI(dec, compressed, format, use_callback, set_buffer_early,
                    use_resizable_runner);
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
// xsize, ysize: image dimensions to store in the SizeHeader, max 512.
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
  JXL_BOOL have_container[2] = {0, 1};
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
        EXPECT_EQ(1u, info.orientation);

        EXPECT_EQ(3u, info.num_color_channels);

        if (alpha_bits[i] != 0) {
          // Expect an extra channel
          EXPECT_EQ(1u, info.num_extra_channels);
          JxlExtraChannelInfo extra;
          EXPECT_EQ(0, JxlDecoderGetExtraChannelInfo(dec, 0, &extra));
          EXPECT_EQ(alpha_bits[i], extra.bits_per_sample);
          EXPECT_EQ(JXL_CHANNEL_ALPHA, extra.type);
          EXPECT_EQ(0, extra.alpha_premultiplied);
          // Verify the name "alpha_test" given to the alpha channel
          EXPECT_EQ(10u, extra.name_length);
          char name[11];
          EXPECT_EQ(0,
                    JxlDecoderGetExtraChannelName(dec, 0, name, sizeof(name)));
          EXPECT_EQ(std::string("alpha_test"), std::string(name));
        } else {
          EXPECT_EQ(0u, info.num_extra_channels);
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
  // The API returns LINEAR by default when the colorspace cannot be represented
  // by enum values.
  EXPECT_EQ(JXL_TRANSFER_FUNCTION_LINEAR, pixel_encoding.transfer_function);

  // Test the same but with integer format.
  EXPECT_EQ(
      JXL_DEC_SUCCESS,
      JxlDecoderGetColorAsEncodedProfile(
          dec, &format_int, JXL_COLOR_PROFILE_TARGET_DATA, &pixel_encoding));
  EXPECT_EQ(JXL_PRIMARIES_SRGB, pixel_encoding.primaries);
  EXPECT_EQ(JXL_TRANSFER_FUNCTION_LINEAR, pixel_encoding.transfer_function);

  // Test after setting the preferred color profile to non-linear sRGB:
  // for XYB images with ICC profile, this setting is expected to take effect.
  jxl::ColorEncoding temp_jxl_srgb = jxl::ColorEncoding::SRGB(false);
  JxlColorEncoding pixel_encoding_srgb;
  ConvertInternalToExternalColorEncoding(temp_jxl_srgb, &pixel_encoding_srgb);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetPreferredColorProfile(dec, &pixel_encoding_srgb));
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetColorAsEncodedProfile(
                dec, &format, JXL_COLOR_PROFILE_TARGET_DATA, &pixel_encoding));
  EXPECT_EQ(JXL_TRANSFER_FUNCTION_SRGB, pixel_encoding.transfer_function);

  // The decoder can also output this as a generated ICC profile anyway, and
  // we're certain that it will differ from the above defined profile since
  // the sRGB data should not have swapped R/G/B primaries.

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetICCProfileSize(
                                 dec, &format, JXL_COLOR_PROFILE_TARGET_DATA,
                                 &dec_profile_size));
  // We don't need to dictate exactly what size the generated ICC profile
  // must be (since there are many ways to represent the same color space),
  // but it should not be zero.
  EXPECT_NE(0u, dec_profile_size);
  jxl::PaddedBytes icc_profile2(dec_profile_size);
  if (0 != dec_profile_size) {
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetColorAsICCProfile(
                                   dec, &format, JXL_COLOR_PROFILE_TARGET_DATA,
                                   icc_profile2.data(), icc_profile2.size()));
    // expected not equal
    EXPECT_NE(icc_profile, icc_profile2);
  }

  // Test setting another different preferred profile, to verify that the
  // returned JXL_COLOR_PROFILE_TARGET_DATA ICC profile is correctly
  // updated.

  jxl::ColorEncoding temp_jxl_linear = jxl::ColorEncoding::LinearSRGB(false);
  JxlColorEncoding pixel_encoding_linear;
  ConvertInternalToExternalColorEncoding(temp_jxl_linear,
                                         &pixel_encoding_linear);

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetPreferredColorProfile(dec, &pixel_encoding_linear));
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetColorAsEncodedProfile(
                dec, &format, JXL_COLOR_PROFILE_TARGET_DATA, &pixel_encoding));
  EXPECT_EQ(JXL_TRANSFER_FUNCTION_LINEAR, pixel_encoding.transfer_function);
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetICCProfileSize(
                                 dec, &format, JXL_COLOR_PROFILE_TARGET_DATA,
                                 &dec_profile_size));
  EXPECT_NE(0u, dec_profile_size);
  jxl::PaddedBytes icc_profile3(dec_profile_size);
  if (0 != dec_profile_size) {
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetColorAsICCProfile(
                                   dec, &format, JXL_COLOR_PROFILE_TARGET_DATA,
                                   icc_profile3.data(), icc_profile3.size()));
    // expected not equal to the previously set preferred profile.
    EXPECT_NE(icc_profile2, icc_profile3);
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
  bool use_resizable_runner;
  // Exif orientation, 1-8
  JxlOrientation orientation;
  bool keep_orientation;
  size_t upsampling;
};

class DecodeTestParam : public ::testing::TestWithParam<PixelTestConfig> {};

TEST_P(DecodeTestParam, PixelTest) {
  PixelTestConfig config = GetParam();
  JxlDecoder* dec = JxlDecoderCreate(NULL);

  if (config.keep_orientation) {
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetKeepOrientation(dec, JXL_TRUE));
  }

  size_t num_pixels = config.xsize * config.ysize;
  uint32_t orig_channels =
      (config.grayscale ? 1 : 3) + (config.include_alpha ? 1 : 0);
  std::vector<uint8_t> pixels =
      jxl::test::GetSomeTestImage(config.xsize, config.ysize, orig_channels, 0);
  JxlPixelFormat format_orig = {orig_channels, JXL_TYPE_UINT16, JXL_BIG_ENDIAN,
                                0};
  jxl::CompressParams cparams;
  cparams.SetLossless();  // Lossless to verify pixels exactly after roundtrip.
  cparams.resampling = config.upsampling;
  cparams.ec_resampling = config.upsampling;
  jxl::PaddedBytes compressed = jxl::CreateTestJXLCodestream(
      jxl::Span<const uint8_t>(pixels.data(), pixels.size()), config.xsize,
      config.ysize, orig_channels, cparams, config.add_container,
      config.orientation, config.add_preview);

  JxlPixelFormat format = {config.output_channels, config.data_type,
                           config.endianness, 0};

  bool swap_xy = !config.keep_orientation && (config.orientation > 4);
  size_t xsize = swap_xy ? config.ysize : config.xsize;
  size_t ysize = swap_xy ? config.xsize : config.ysize;

  std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
      dec, jxl::Span<const uint8_t>(compressed.data(), compressed.size()),
      format, config.use_callback, config.set_buffer_early,
      config.use_resizable_runner);
  JxlDecoderReset(dec);
  EXPECT_EQ(num_pixels * config.output_channels *
                GetDataBits(config.data_type) / jxl::kBitsPerByte,
            pixels2.size());

  // If an orientation transformation is expected, to compare the pixels, also
  // apply this transformation to the original pixels. ConvertToExternal is
  // used to achieve this, with a temporary conversion to CodecInOut and back.
  if (config.orientation > 1 && !config.keep_orientation) {
    jxl::Span<const uint8_t> bytes(pixels.data(), pixels.size());
    jxl::ColorEncoding color_encoding =
        jxl::ColorEncoding::SRGB(config.grayscale);

    jxl::CodecInOut io;
    if (config.include_alpha) io.metadata.m.SetAlphaBits(16);
    io.SetSize(config.xsize, config.ysize);

    EXPECT_TRUE(ConvertFromExternal(
        bytes, config.xsize, config.ysize, color_encoding, config.include_alpha,
        /*alpha_is_premultiplied=*/false, 16, JXL_BIG_ENDIAN,
        /*flipped_y=*/false, nullptr, &io.Main(), /*float_in=*/false));

    for (size_t i = 0; i < pixels.size(); i++) pixels[i] = 0;
    EXPECT_TRUE(ConvertToExternal(
        io.Main(), 16,
        /*float_out=*/false, orig_channels, JXL_BIG_ENDIAN,
        xsize * 2 * orig_channels, nullptr, pixels.data(), pixels.size(),
        nullptr, nullptr, static_cast<jxl::Orientation>(config.orientation)));
  }
  if (config.upsampling == 1) {
    EXPECT_EQ(0u, ComparePixels(pixels.data(), pixels2.data(), xsize, ysize,
                                format_orig, format));
  } else {
    // resampling is of course not lossless, so as a rough check:
    // count pixels that are more than off-by-25 in the 8-bit value of one of
    // the channels
    EXPECT_LE(
        ComparePixels(
            pixels.data(), pixels2.data(), xsize, ysize, format_orig, format,
            50.0 * (config.data_type == JXL_TYPE_UINT8 ? 1.0 : 256.0)),
        300u);
  }

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
                       CodeStreamBoxFormat box, JxlOrientation orientation,
                       bool keep_orientation, OutputFormat format,
                       bool use_callback, bool set_buffer_early,
                       bool resizable_runner, size_t upsampling) {
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
    c.use_resizable_runner = resizable_runner;
    c.orientation = orientation;
    c.keep_orientation = keep_orientation;
    c.upsampling = upsampling;
    all_tests.push_back(c);
  };

  // Test output formats and methods.
  for (ChannelInfo ch : ch_info) {
    for (int use_callback = 0; use_callback <= 1; use_callback++) {
      for (size_t upsampling : {1, 2, 4, 8}) {
        for (OutputFormat fmt : out_formats) {
          make_test(ch, 301, 33, /*add_preview=*/false,
                    CodeStreamBoxFormat::kCSBF_None, JXL_ORIENT_IDENTITY,
                    /*keep_orientation=*/false, fmt, use_callback,
                    /*set_buffer_early=*/false, /*resizable_runner=*/false,
                    upsampling);
        }
      }
    }
  }
  // Test codestream formats.
  for (size_t box = 1; box < kCSBF_NUM_ENTRIES; ++box) {
    make_test(ch_info[0], 77, 33, /*add_preview=*/false,
              (CodeStreamBoxFormat)box, JXL_ORIENT_IDENTITY,
              /*keep_orientation=*/false, out_formats[0],
              /*use_callback=*/false,
              /*set_buffer_early=*/false, /*resizable_runner=*/false, 1);
  }
  // Test previews.
  for (int add_preview = 0; add_preview <= 1; add_preview++) {
    make_test(ch_info[0], 77, 33, add_preview, CodeStreamBoxFormat::kCSBF_None,
              JXL_ORIENT_IDENTITY, /*keep_orientation=*/false, out_formats[0],
              /*use_callback=*/false, /*set_buffer_early=*/false,
              /*resizable_runner=*/false, 1);
  }
  // Test setting buffers early.
  make_test(ch_info[0], 300, 33, /*add_preview=*/false,
            CodeStreamBoxFormat::kCSBF_None, JXL_ORIENT_IDENTITY,
            /*keep_orientation=*/false, out_formats[0],
            /*use_callback=*/false, /*set_buffer_early=*/true,
            /*resizable_runner=*/false, 1);

  // Test using the resizable runner
  for (size_t i = 0; i < 4; i++) {
    make_test(ch_info[0], 300 << i, 33 << i, /*add_preview=*/false,
              CodeStreamBoxFormat::kCSBF_None, JXL_ORIENT_IDENTITY,
              /*keep_orientation=*/false, out_formats[0],
              /*use_callback=*/false, /*set_buffer_early=*/false,
              /*resizable_runner=*/true, 1);
  }

  // Test orientations.
  for (int orientation = 1; orientation <= 8; ++orientation) {
    make_test(ch_info[0], 280, 12, /*add_preview=*/false,
              CodeStreamBoxFormat::kCSBF_None,
              static_cast<JxlOrientation>(orientation),
              /*keep_orientation=*/false, out_formats[0],
              /*use_callback=*/false, /*set_buffer_early=*/true,
              /*resizable_runner=*/false, 1);
    make_test(ch_info[0], 280, 12, /*add_preview=*/false,
              CodeStreamBoxFormat::kCSBF_None,
              static_cast<JxlOrientation>(orientation),
              /*keep_orientation=*/true, out_formats[0],
              /*use_callback=*/false, /*set_buffer_early=*/true,
              /*resizable_runner=*/false, 1);
  }

  return all_tests;
}

std::ostream& operator<<(std::ostream& os, const PixelTestConfig& c) {
  os << c.xsize << "x" << c.ysize;
  const char* colors[] = {"", "G", "GA", "RGB", "RGBA"};
  os << colors[(c.grayscale ? 1 : 3) + (c.include_alpha ? 1 : 0)];
  os << "to";
  os << colors[c.output_channels];
  switch (c.data_type) {
    case JXL_TYPE_UINT8:
      os << "u8";
      break;
    case JXL_TYPE_UINT16:
      os << "u16";
      break;
    case JXL_TYPE_FLOAT:
      os << "f32";
      break;
    case JXL_TYPE_FLOAT16:
      os << "f16";
      break;
    case JXL_TYPE_UINT32:
      os << "u32";
      break;
    case JXL_TYPE_BOOLEAN:
      os << "b";
      break;
  };
  if (GetDataBits(c.data_type) > jxl::kBitsPerByte) {
    if (c.endianness == JXL_NATIVE_ENDIAN) {
      // add nothing
    } else if (c.endianness == JXL_BIG_ENDIAN) {
      os << "BE";
    } else if (c.endianness == JXL_LITTLE_ENDIAN) {
      os << "LE";
    }
  }
  if (c.add_container != CodeStreamBoxFormat::kCSBF_None) {
    os << "Box";
    os << (size_t)c.add_container;
  }
  if (c.add_preview) os << "Preview";
  if (c.use_callback) os << "Callback";
  if (c.set_buffer_early) os << "EarlyBuffer";
  if (c.use_resizable_runner) os << "ResizableRunner";
  if (c.orientation != 1) os << "O" << c.orientation;
  if (c.keep_orientation) os << "Keep";
  if (c.upsampling > 1) os << "x" << c.upsampling;
  return os;
}

std::string PixelTestDescription(
    const testing::TestParamInfo<DecodeTestParam::ParamType>& info) {
  std::stringstream name;
  name << info.param;
  return name.str();
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
  cparams.SetLossless();  // Lossless to verify pixels exactly after roundtrip.
  // For variation: some have container and no preview, others have preview
  // and no container.
  jxl::PaddedBytes compressed = jxl::CreateTestJXLCodestream(
      jxl::Span<const uint8_t>(pixels.data(), pixels.size()), xsize, ysize, 4,
      cparams, kCSBF_None, JXL_ORIENT_IDENTITY, false, true);

  for (uint32_t channels = 3; channels <= 4; ++channels) {
    {
      JxlPixelFormat format = {channels, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};

      std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
          dec, jxl::Span<const uint8_t>(compressed.data(), compressed.size()),
          format, /*use_callback=*/false, /*set_buffer_early=*/false,
          /*use_resizable_runner=*/false);
      JxlDecoderReset(dec);
      EXPECT_EQ(num_pixels * channels, pixels2.size());
      EXPECT_EQ(0u, ComparePixels(pixels.data(), pixels2.data(), xsize, ysize,
                                  format_orig, format));
    }
    {
      JxlPixelFormat format = {channels, JXL_TYPE_UINT16, JXL_LITTLE_ENDIAN, 0};

      // Test with the container for one of the pixel formats.
      std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
          dec, jxl::Span<const uint8_t>(compressed.data(), compressed.size()),
          format, /*use_callback=*/true, /*set_buffer_early=*/true,
          /*use_resizable_runner=*/false);
      JxlDecoderReset(dec);
      EXPECT_EQ(num_pixels * channels * 2, pixels2.size());
      EXPECT_EQ(0u, ComparePixels(pixels.data(), pixels2.data(), xsize, ysize,
                                  format_orig, format));
    }

    {
      JxlPixelFormat format = {channels, JXL_TYPE_FLOAT, JXL_LITTLE_ENDIAN, 0};

      std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
          dec, jxl::Span<const uint8_t>(compressed.data(), compressed.size()),
          format, /*use_callback=*/false, /*set_buffer_early=*/false,
          /*use_resizable_runner=*/false);
      JxlDecoderReset(dec);
      EXPECT_EQ(num_pixels * channels * 4, pixels2.size());
      EXPECT_EQ(0u, ComparePixels(pixels.data(), pixels2.data(), xsize, ysize,
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
      cparams, kCSBF_None, JXL_ORIENT_IDENTITY, /*add_preview=*/false,
      /*add_icc_profile=*/true);
  uint32_t channels = 3;

  JxlPixelFormat format = {channels, JXL_TYPE_FLOAT, JXL_LITTLE_ENDIAN, 0};

  jxl::PaddedBytes icc;
  std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
      dec, jxl::Span<const uint8_t>(compressed.data(), compressed.size()),
      format, /*use_callback=*/false, /*set_buffer_early=*/true,
      /*use_resizable_runner=*/false, /*icc=*/&icc);
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
      /*flipped_y=*/false, /*pool=*/nullptr, &io0.Main(), /*float_in=*/false));

  jxl::ColorEncoding color_encoding1;
  EXPECT_TRUE(color_encoding1.SetICC(std::move(icc)));
  jxl::Span<const uint8_t> span1(pixels2.data(), pixels2.size());
  jxl::CodecInOut io1;
  io1.SetSize(xsize, ysize);
  EXPECT_TRUE(ConvertFromExternal(
      span1, xsize, ysize, color_encoding1,
      /*has_alpha=*/false, false, 32, format.endianness,
      /*flipped_y=*/false, /*pool=*/nullptr, &io1.Main(), /*float_in=*/true));

  jxl::ButteraugliParams ba;
  EXPECT_THAT(ButteraugliDistance(io0, io1, ba, /*distmap=*/nullptr, nullptr),
              IsSlightlyBelow(0.75f));

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
        cparams, kCSBF_None, JXL_ORIENT_IDENTITY, /*add_preview=*/false,
        /*add_icc_profile=*/false);

    JxlPixelFormat format = {channels, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};

    std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
        dec, jxl::Span<const uint8_t>(compressed.data(), compressed.size()),
        format, /*use_callback=*/true, /*set_buffer_early=*/false,
        /*use_resizable_runner=*/false);
    JxlDecoderReset(dec);
    EXPECT_EQ(num_pixels * channels, pixels2.size());

    // The input pixels use the profile matching GetIccTestProfile, since we set
    // add_icc_profile for CreateTestJXLCodestream to true.
    jxl::ColorEncoding color_encoding0 = jxl::ColorEncoding::SRGB(false);
    jxl::Span<const uint8_t> span0(pixels.data(), pixels.size());
    jxl::CodecInOut io0;
    io0.SetSize(xsize, ysize);
    EXPECT_TRUE(ConvertFromExternal(span0, xsize, ysize, color_encoding0,
                                    /*has_alpha=*/false, false, 16,
                                    format_orig.endianness,
                                    /*flipped_y=*/false, /*pool=*/nullptr,
                                    &io0.Main(), /*float_in=*/false));

    jxl::ColorEncoding color_encoding1 = jxl::ColorEncoding::SRGB(false);
    jxl::Span<const uint8_t> span1(pixels2.data(), pixels2.size());
    jxl::CodecInOut io1;
    if (channels == 4) {
      io1.metadata.m.SetAlphaBits(8);
      io1.SetSize(xsize, ysize);
      EXPECT_TRUE(ConvertFromExternal(span1, xsize, ysize, color_encoding1,
                                      /*has_alpha=*/true, false, 8,
                                      format.endianness,
                                      /*flipped_y=*/false, /*pool=*/nullptr,
                                      &io1.Main(), /*float_in=*/false));
      io1.metadata.m.SetAlphaBits(0);
      io1.Main().ClearExtraChannels();
    } else {
      EXPECT_TRUE(ConvertFromExternal(span1, xsize, ysize, color_encoding1,
                                      /*has_alpha=*/false, false, 8,
                                      format.endianness,
                                      /*flipped_y=*/false, /*pool=*/nullptr,
                                      &io1.Main(), /*float_in=*/false));
    }

    jxl::ButteraugliParams ba;
    EXPECT_THAT(ButteraugliDistance(io0, io1, ba, /*distmap=*/nullptr, nullptr),
                IsSlightlyBelow(0.8f));

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
        cparams, kCSBF_None, JXL_ORIENT_IDENTITY, /*add_preview=*/false,
        /*add_icc_profile=*/false);

    JxlPixelFormat format = {channels, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};

    std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
        dec, jxl::Span<const uint8_t>(compressed.data(), compressed.size()),
        format, /*use_callback=*/false, /*set_buffer_early=*/true,
        /*use_resizable_runner=*/false);
    JxlDecoderReset(dec);
    EXPECT_EQ(num_pixels * channels, pixels2.size());

    // The input pixels use the profile matching GetIccTestProfile, since we set
    // add_icc_profile for CreateTestJXLCodestream to true.
    jxl::ColorEncoding color_encoding0 = jxl::ColorEncoding::SRGB(false);
    jxl::Span<const uint8_t> span0(pixels.data(), pixels.size());
    jxl::CodecInOut io0;
    io0.SetSize(xsize, ysize);
    EXPECT_TRUE(ConvertFromExternal(span0, xsize, ysize, color_encoding0,
                                    /*has_alpha=*/false, false, 16,
                                    format_orig.endianness,
                                    /*flipped_y=*/false, /*pool=*/nullptr,
                                    &io0.Main(), /*float_in=*/false));

    jxl::ColorEncoding color_encoding1 = jxl::ColorEncoding::SRGB(false);
    jxl::Span<const uint8_t> span1(pixels2.data(), pixels2.size());
    jxl::CodecInOut io1;
    if (channels == 4) {
      io1.metadata.m.SetAlphaBits(8);
      io1.SetSize(xsize, ysize);
      EXPECT_TRUE(ConvertFromExternal(span1, xsize, ysize, color_encoding1,
                                      /*has_alpha=*/true, false, 8,
                                      format.endianness,
                                      /*flipped_y=*/false, /*pool=*/nullptr,
                                      &io1.Main(), /*float_in=*/false));
      io1.metadata.m.SetAlphaBits(0);
      io1.Main().ClearExtraChannels();
    } else {
      EXPECT_TRUE(ConvertFromExternal(span1, xsize, ysize, color_encoding1,
                                      /*has_alpha=*/false, false, 8,
                                      format.endianness,
                                      /*flipped_y=*/false, /*pool=*/nullptr,
                                      &io1.Main(), /*float_in=*/false));
    }

    jxl::ButteraugliParams ba;
    EXPECT_THAT(ButteraugliDistance(io0, io1, ba, /*distmap=*/nullptr, nullptr),
                IsSlightlyBelow(2.6f));

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
    // Lossless to verify pixels exactly after roundtrip.
    cparams.SetLossless();
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
        channels, cparams, add_container, JXL_ORIENT_IDENTITY,
        /*add_preview=*/true,
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
TEST(DecodeTest, JXL_TRANSCODE_JPEG_TEST(JPEGPartialTest)) {
  TestPartialStream(true);
}
#endif  // JPEGXL_ENABLE_JPEG

// The DC event still exists, but is no longer implemented, it is deprecated.
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
      cparams, kCSBF_Multi, JXL_ORIENT_IDENTITY, /*add_preview=*/true);

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
  EXPECT_THAT(ButteraugliDistance(io0, io1, ba,
                                  /*distmap=*/nullptr, nullptr),
              IsSlightlyBelow(0.6f));

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
      cparams, kCSBF_None, JXL_ORIENT_IDENTITY, false);

  size_t align = 17;
  JxlPixelFormat format = {3, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, align};
  // On purpose not using jxl::RoundUpTo to test it independently.
  size_t expected_line_bytes = (1 * 3 * xsize + align - 1) / align * align;

  for (int use_callback = 0; use_callback <= 1; ++use_callback) {
    std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
        jxl::Span<const uint8_t>(compressed.data(), compressed.size()), format,
        use_callback, /*set_buffer_early=*/false,
        /*use_resizable_runner=*/false);
    EXPECT_EQ(expected_line_bytes * ysize, pixels2.size());
    EXPECT_EQ(0u, ComparePixels(pixels.data(), pixels2.data(), xsize, ysize,
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
        JXL_BIG_ENDIAN, /*flipped_y=*/false, /*pool=*/nullptr, &bundle,
        /*float_in=*/false));
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
    EXPECT_EQ(0u, frame_header.name_length);
    // For now, test with empty name, there's currently no easy way to encode
    // a jxl file with a frame name because ImageBundle doesn't have a
    // jxl::FrameHeader to set the name in. We can test the null termination
    // character though.
    char name;
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetFrameName(dec, &name, 1));
    EXPECT_EQ(0, name);

    EXPECT_EQ(i + 1 == num_frames, frame_header.is_last);

    EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                   dec, &format, pixels.data(), pixels.size()));

    EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
    EXPECT_EQ(0u, ComparePixels(frames[i].data(), pixels.data(), xsize, ysize,
                                format, format));
  }

  // After all frames were decoded, JxlDecoderProcessInput should return
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
        JXL_BIG_ENDIAN, /*flipped_y=*/false, /*pool=*/nullptr, &bundle,
        /*float_in=*/false));
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

TEST(DecodeTest, ExtraChannelTest) {
  size_t xsize = 55, ysize = 257;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);
  JxlPixelFormat format_orig = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  jxl::CompressParams cparams;
  cparams.SetLossless();  // Lossless to verify pixels exactly after roundtrip.
  jxl::PaddedBytes compressed = jxl::CreateTestJXLCodestream(
      jxl::Span<const uint8_t>(pixels.data(), pixels.size()), xsize, ysize, 4,
      cparams, kCSBF_None, JXL_ORIENT_IDENTITY, false);

  size_t align = 17;
  JxlPixelFormat format = {3, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, align};

  JxlDecoder* dec = JxlDecoderCreate(NULL);

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSubscribeEvents(
                                 dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE));

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetInput(dec, compressed.data(), compressed.size()));
  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
  EXPECT_EQ(1u, info.num_extra_channels);
  EXPECT_EQ(JXL_FALSE, info.alpha_premultiplied);

  JxlExtraChannelInfo extra_info;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetExtraChannelInfo(dec, 0, &extra_info));
  EXPECT_EQ(0, extra_info.type);

  EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));
  size_t buffer_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
  size_t extra_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderExtraChannelBufferSize(dec, &format, &extra_size, 0));

  std::vector<uint8_t> image(buffer_size);
  std::vector<uint8_t> extra(extra_size);
  size_t bytes_per_pixel =
      format.num_channels * GetDataBits(format.data_type) / jxl::kBitsPerByte;
  size_t stride = bytes_per_pixel * info.xsize;
  if (format.align > 1) {
    stride = jxl::DivCeil(stride, format.align) * format.align;
  }

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                 dec, &format, image.data(), image.size()));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetExtraChannelBuffer(
                                 dec, &format, extra.data(), extra.size(), 0));

  EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));

  // After the full image was output, JxlDecoderProcessInput should return
  // success to indicate all is done.
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));
  JxlDecoderDestroy(dec);

  EXPECT_EQ(0u, ComparePixels(pixels.data(), image.data(), xsize, ysize,
                              format_orig, format));

  // Compare the extracted extra channel with the original alpha channel

  std::vector<uint8_t> alpha(pixels.size() / 4);
  for (size_t i = 0; i < pixels.size(); i += 8) {
    size_t index_alpha = i / 4;
    alpha[index_alpha + 0] = pixels[i + 6];
    alpha[index_alpha + 1] = pixels[i + 7];
  }
  JxlPixelFormat format_alpha = format;
  format_alpha.num_channels = 1;
  JxlPixelFormat format_orig_alpha = format_orig;
  format_orig_alpha.num_channels = 1;

  EXPECT_EQ(0u, ComparePixels(alpha.data(), extra.data(), xsize, ysize,
                              format_orig_alpha, format_alpha));
}

TEST(DecodeTest, SkipFrameTest) {
  size_t xsize = 90, ysize = 120;
  constexpr size_t num_frames = 16;
  std::vector<uint8_t> frames[num_frames];
  for (size_t i = 0; i < num_frames; i++) {
    frames[i] = jxl::test::GetSomeTestImage(xsize, ysize, 3, i);
  }
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
    if (i & 1) {
      // Mark some frames as referenceable, others not.
      bundle.use_for_next_frame = true;
    }

    EXPECT_TRUE(ConvertFromExternal(
        jxl::Span<const uint8_t>(frames[i].data(), frames[i].size()), xsize,
        ysize, jxl::ColorEncoding::SRGB(/*is_gray=*/false), /*has_alpha=*/false,
        /*alpha_is_premultiplied=*/false, /*bits_per_sample=*/16,
        JXL_BIG_ENDIAN, /*flipped_y=*/false, /*pool=*/nullptr, &bundle,
        /*float_in=*/false));
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
    if (i == 3) {
      JxlDecoderSkipFrames(dec, 5);
      i += 5;
    }
    std::vector<uint8_t> pixels(buffer_size);

    EXPECT_EQ(JXL_DEC_FRAME, JxlDecoderProcessInput(dec));

    JxlFrameHeader frame_header;
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetFrameHeader(dec, &frame_header));
    EXPECT_EQ(frame_durations[i], frame_header.duration);

    EXPECT_EQ(i + 1 == num_frames, frame_header.is_last);

    EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                   dec, &format, pixels.data(), pixels.size()));

    EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
    EXPECT_EQ(0u, ComparePixels(frames[i].data(), pixels.data(), xsize, ysize,
                                format, format));
  }

  // After all frames were decoded, JxlDecoderProcessInput should return
  // success to indicate all is done.
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));

  // Test rewinding the decoder and skipping different frames

  JxlDecoderRewind(dec);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(dec, JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));

  for (size_t i = 0; i < num_frames; ++i) {
    int test_skipping = (i == 9) ? 3 : 0;
    std::vector<uint8_t> pixels(buffer_size);

    EXPECT_EQ(JXL_DEC_FRAME, JxlDecoderProcessInput(dec));

    // Since this is after JXL_DEC_FRAME but before JXL_DEC_FULL_IMAGE, this
    // should only skip the next frame, not the currently processed one.
    if (test_skipping) JxlDecoderSkipFrames(dec, test_skipping);

    JxlFrameHeader frame_header;
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetFrameHeader(dec, &frame_header));
    EXPECT_EQ(frame_durations[i], frame_header.duration);

    EXPECT_EQ(i + 1 == num_frames, frame_header.is_last);

    EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                   dec, &format, pixels.data(), pixels.size()));

    EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
    EXPECT_EQ(0u, ComparePixels(frames[i].data(), pixels.data(), xsize, ysize,
                                format, format));

    if (test_skipping) i += test_skipping;
  }

  JxlThreadParallelRunnerDestroy(runner);
  JxlDecoderDestroy(dec);
}

TEST(DecodeTest, SkipFrameWithBlendingTest) {
  size_t xsize = 90, ysize = 120;
  constexpr size_t num_frames = 16;
  std::vector<uint8_t> frames[num_frames];
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
    if (i < 5) {
      std::vector<uint8_t> frame_internal =
          jxl::test::GetSomeTestImage(xsize, ysize, 3, i * 2 + 1);
      // An internal frame with 0 duration, and use_for_next_frame, this is a
      // frame that is not rendered and not output by the API, but on which the
      // rendered frames depend
      jxl::ImageBundle bundle_internal(&io.metadata.m);
      EXPECT_TRUE(ConvertFromExternal(
          jxl::Span<const uint8_t>(frame_internal.data(),
                                   frame_internal.size()),
          xsize, ysize, jxl::ColorEncoding::SRGB(/*is_gray=*/false),
          /*has_alpha=*/false,
          /*alpha_is_premultiplied=*/false, /*bits_per_sample=*/16,
          JXL_BIG_ENDIAN, /*flipped_y=*/false, /*pool=*/nullptr,
          &bundle_internal, /*float_in=*/false));
      bundle_internal.duration = 0;
      bundle_internal.use_for_next_frame = true;
      io.frames.push_back(std::move(bundle_internal));
    }

    std::vector<uint8_t> frame =
        jxl::test::GetSomeTestImage(xsize, ysize, 3, i * 2);
    // Actual rendered frame
    frame_durations[i] = 5 + i;
    jxl::ImageBundle bundle(&io.metadata.m);
    EXPECT_TRUE(ConvertFromExternal(
        jxl::Span<const uint8_t>(frame.data(), frame.size()), xsize, ysize,
        jxl::ColorEncoding::SRGB(/*is_gray=*/false), /*has_alpha=*/false,
        /*alpha_is_premultiplied=*/false, /*bits_per_sample=*/16,
        JXL_BIG_ENDIAN, /*flipped_y=*/false, /*pool=*/nullptr, &bundle,
        /*float_in=*/false));
    bundle.duration = frame_durations[i];
    // Create some variation in which frames depend on which.
    if (i != 3 && i != 9 && i != 10) {
      bundle.use_for_next_frame = true;
    }
    if (i != 12) {
      bundle.blend = true;
      // Choose a blend mode that depends on the pixels of the saved frame and
      // doesn't use alpha
      bundle.blendmode = jxl::BlendMode::kMul;
    }
    io.frames.push_back(std::move(bundle));
  }

  jxl::CompressParams cparams;
  cparams.SetLossless();  // Lossless to verify pixels exactly after roundtrip.
  jxl::AuxOut aux_out;
  jxl::PaddedBytes compressed;
  jxl::PassesEncoderState enc_state;
  EXPECT_TRUE(jxl::EncodeFile(cparams, &io, &enc_state, &compressed, &aux_out,
                              nullptr));

  // Independently decode all frames without any skipping, to create the
  // expected blended frames, for the actual tests below to compare with.
  {
    JxlDecoder* dec = JxlDecoderCreate(NULL);
    const uint8_t* next_in = compressed.data();
    size_t avail_in = compressed.size();

    void* runner = JxlThreadParallelRunnerCreate(
        NULL, JxlThreadParallelRunnerDefaultNumWorkerThreads());
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetParallelRunner(
                                   dec, JxlThreadParallelRunner, runner));
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSubscribeEvents(dec, JXL_DEC_FULL_IMAGE));
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));
    for (size_t i = 0; i < num_frames; ++i) {
      EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));
      frames[i].resize(xsize * ysize * 6);
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderSetImageOutBuffer(dec, &format, frames[i].data(),
                                            frames[i].size()));
      EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
    }

    // After all frames were decoded, JxlDecoderProcessInput should return
    // success to indicate all is done.
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));
    JxlThreadParallelRunnerDestroy(runner);
    JxlDecoderDestroy(dec);
  }

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

    EXPECT_EQ(i + 1 == num_frames, frame_header.is_last);

    EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                   dec, &format, pixels.data(), pixels.size()));

    EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
    EXPECT_EQ(0u, ComparePixels(frames[i].data(), pixels.data(), xsize, ysize,
                                format, format));

    // Test rewinding mid-way, not decoding all frames.
    if (i == 8) {
      break;
    }
  }

  JxlDecoderRewind(dec);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(dec, JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));

  for (size_t i = 0; i < num_frames; ++i) {
    if (i == 3) {
      JxlDecoderSkipFrames(dec, 5);
      i += 5;
    }
    std::vector<uint8_t> pixels(buffer_size);

    EXPECT_EQ(JXL_DEC_FRAME, JxlDecoderProcessInput(dec));

    JxlFrameHeader frame_header;
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetFrameHeader(dec, &frame_header));
    EXPECT_EQ(frame_durations[i], frame_header.duration);

    EXPECT_EQ(i + 1 == num_frames, frame_header.is_last);

    EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                   dec, &format, pixels.data(), pixels.size()));

    EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
    EXPECT_EQ(0u, ComparePixels(frames[i].data(), pixels.data(), xsize, ysize,
                                format, format));
  }

  // After all frames were decoded, JxlDecoderProcessInput should return
  // success to indicate all is done.
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));

  // Test rewinding the decoder and skipping different frames

  JxlDecoderRewind(dec);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(dec, JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));

  for (size_t i = 0; i < num_frames; ++i) {
    int test_skipping = (i == 9) ? 3 : 0;
    std::vector<uint8_t> pixels(buffer_size);

    EXPECT_EQ(JXL_DEC_FRAME, JxlDecoderProcessInput(dec));

    // Since this is after JXL_DEC_FRAME but before JXL_DEC_FULL_IMAGE, this
    // should only skip the next frame, not the currently processed one.
    if (test_skipping) JxlDecoderSkipFrames(dec, test_skipping);

    JxlFrameHeader frame_header;
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetFrameHeader(dec, &frame_header));
    EXPECT_EQ(frame_durations[i], frame_header.duration);

    EXPECT_EQ(i + 1 == num_frames, frame_header.is_last);

    EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                   dec, &format, pixels.data(), pixels.size()));

    EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
    EXPECT_EQ(0u, ComparePixels(frames[i].data(), pixels.data(), xsize, ysize,
                                format, format));

    if (test_skipping) i += test_skipping;
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
      num_channels, cparams, kCSBF_None, JXL_ORIENT_IDENTITY, true);
  JxlPixelFormat format = {num_channels, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  std::vector<uint8_t> pixels2;
  pixels2.resize(pixels.size());

  JxlDecoder* dec = JxlDecoderCreate(nullptr);

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));

  // Ensure that the first part contains at least the full DC of the image,
  // otherwise flush does not work.
  size_t first_part = data.size() - 1;

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

  // Crude test of actual pixel data: pixel threshold of about 4% (2560/65535).
  // 29000 pixels can be above the threshold
  EXPECT_LE(ComparePixels(pixels2.data(), pixels.data(), xsize, ysize, format,
                          format, 2560.0),
            29000u);

  EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, JxlDecoderProcessInput(dec));

  size_t consumed = first_part - JxlDecoderReleaseInput(dec);

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, data.data() + consumed,
                                                data.size() - consumed));
  EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
  // Lower threshold for the final (still lossy) image
  EXPECT_LE(ComparePixels(pixels2.data(), pixels.data(), xsize, ysize, format,
                          format, 2560.0),
            11000u);

  JxlDecoderDestroy(dec);
}

TEST(DecodeTest, FlushTestLossyProgressiveAlpha) {
  // Size large enough for multiple groups, required to have progressive
  // stages
  size_t xsize = 333, ysize = 300;
  uint32_t num_channels = 4;
  std::vector<uint8_t> pixels =
      jxl::test::GetSomeTestImage(xsize, ysize, num_channels, 0);
  jxl::CompressParams cparams;
  jxl::PaddedBytes data = jxl::CreateTestJXLCodestream(
      jxl::Span<const uint8_t>(pixels.data(), pixels.size()), xsize, ysize,
      num_channels, cparams, kCSBF_None, JXL_ORIENT_IDENTITY, true);
  JxlPixelFormat format = {num_channels, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  std::vector<uint8_t> pixels2;
  pixels2.resize(pixels.size());

  JxlDecoder* dec = JxlDecoderCreate(nullptr);

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));

  // Ensure that the first part contains at least the full DC of the image,
  // otherwise flush does not work.
  size_t first_part = data.size() - 1;

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

  EXPECT_LE(ComparePixels(pixels2.data(), pixels.data(), xsize, ysize, format,
                          format, 2560.0),
            30000u);

  EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, JxlDecoderProcessInput(dec));

  size_t consumed = first_part - JxlDecoderReleaseInput(dec);

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, data.data() + consumed,
                                                data.size() - consumed));

  EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
  EXPECT_LE(ComparePixels(pixels2.data(), pixels.data(), xsize, ysize, format,
                          format, 2560.0),
            11000u);

  JxlDecoderDestroy(dec);
}
TEST(DecodeTest, FlushTestLossyProgressiveAlphaUpsampling) {
  size_t xsize = 533, ysize = 401;
  uint32_t num_channels = 4;
  std::vector<uint8_t> pixels =
      jxl::test::GetSomeTestImage(xsize, ysize, num_channels, 0);
  jxl::CompressParams cparams;
  cparams.resampling = 2;
  cparams.ec_resampling = 4;
  jxl::PaddedBytes data = jxl::CreateTestJXLCodestream(
      jxl::Span<const uint8_t>(pixels.data(), pixels.size()), xsize, ysize,
      num_channels, cparams, kCSBF_None, JXL_ORIENT_IDENTITY, true);
  JxlPixelFormat format = {num_channels, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  std::vector<uint8_t> pixels2;
  pixels2.resize(pixels.size());

  JxlDecoder* dec = JxlDecoderCreate(nullptr);

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));

  // Ensure that the first part contains at least the full DC of the image,
  // otherwise flush does not work.
  size_t first_part = data.size() * 2 / 3;

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

  EXPECT_LE(ComparePixels(pixels2.data(), pixels.data(), xsize, ysize, format,
                          format, 2560.0),
            125000u);

  EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, JxlDecoderProcessInput(dec));

  size_t consumed = first_part - JxlDecoderReleaseInput(dec);

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, data.data() + consumed,
                                                data.size() - consumed));

  EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
  EXPECT_LE(ComparePixels(pixels2.data(), pixels.data(), xsize, ysize, format,
                          format, 2560.0),
            70000u);

  JxlDecoderDestroy(dec);
}
TEST(DecodeTest, FlushTestLosslessProgressiveAlpha) {
  // Size large enough for multiple groups, required to have progressive
  // stages
  size_t xsize = 333, ysize = 300;
  uint32_t num_channels = 4;
  std::vector<uint8_t> pixels =
      jxl::test::GetSomeTestImage(xsize, ysize, num_channels, 0);
  jxl::CompressParams cparams;
  cparams.SetLossless();
  cparams.responsive = 1;
  jxl::PaddedBytes data = jxl::CreateTestJXLCodestream(
      jxl::Span<const uint8_t>(pixels.data(), pixels.size()), xsize, ysize,
      num_channels, cparams, kCSBF_None, JXL_ORIENT_IDENTITY, true);
  JxlPixelFormat format = {num_channels, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  std::vector<uint8_t> pixels2;
  pixels2.resize(pixels.size());

  JxlDecoder* dec = JxlDecoderCreate(nullptr);

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));

  // Ensure that the first part contains at least the full DC of the image,
  // otherwise flush does not work.
  size_t first_part = data.size() / 2;

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

  EXPECT_LE(ComparePixels(pixels2.data(), pixels.data(), xsize, ysize, format,
                          format, 2560.0),
            1700u);

  EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, JxlDecoderProcessInput(dec));

  size_t consumed = first_part - JxlDecoderReleaseInput(dec);

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, data.data() + consumed,
                                                data.size() - consumed));

  EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
  EXPECT_LE(ComparePixels(pixels2.data(), pixels.data(), xsize, ysize, format,
                          format),
            0u);

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
TEST(DecodeTest, JXL_TRANSCODE_JPEG_TEST(JPEGReconstructTestCodestream)) {
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
      channels, cparams, kCSBF_Single, JXL_ORIENT_IDENTITY,
      /*add_preview=*/true,
      /*add_icc_profile=*/false, &jpeg_codestream);
  VerifyJPEGReconstruction(compressed, jpeg_codestream);
}
#endif  // JPEGXL_ENABLE_JPEG

TEST(DecodeTest, JXL_TRANSCODE_JPEG_TEST(JPEGReconstructionTest)) {
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

TEST(DecodeTest, JXL_TRANSCODE_JPEG_TEST(JPEGReconstructionMetadataTest)) {
  const std::string jpeg_path = "jxl/jpeg_reconstruction/1x1_exif_xmp.jpg";
  const std::string jxl_path = "jxl/jpeg_reconstruction/1x1_exif_xmp.jxl";
  const jxl::PaddedBytes jpeg = jxl::ReadTestData(jpeg_path);
  const jxl::PaddedBytes jxl = jxl::ReadTestData(jxl_path);
  VerifyJPEGReconstruction(jxl, jpeg);
}

TEST(DecodeTest, ContinueFinalNonEssentialBoxTest) {
  size_t xsize = 80, ysize = 90;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);
  jxl::CompressParams cparams;
  // Lossless to verify pixels exactly after roundtrip.
  jxl::PaddedBytes compressed = jxl::CreateTestJXLCodestream(
      jxl::Span<const uint8_t>(pixels.data(), pixels.size()), xsize, ysize, 4,
      cparams, kCSBF_Multi_Other_Terminated, JXL_ORIENT_IDENTITY, false, true);

  // The non-essential final box size including 8-byte header
  size_t final_box_size = unk3_box_size + 8;
  size_t last_box_begin = compressed.size() - final_box_size;
  // Verify that the test is indeed setup correctly to be at the beginning of
  // the 'unkn' box header.
  ASSERT_EQ(compressed[last_box_begin + 3], final_box_size);
  ASSERT_EQ(compressed[last_box_begin + 4], 'u');
  ASSERT_EQ(compressed[last_box_begin + 5], 'n');
  ASSERT_EQ(compressed[last_box_begin + 6], 'k');
  ASSERT_EQ(compressed[last_box_begin + 7], '3');

  JxlDecoder* dec = JxlDecoderCreate(nullptr);

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME));

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetInput(dec, compressed.data(), last_box_begin));

  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
  EXPECT_EQ(JXL_DEC_FRAME, JxlDecoderProcessInput(dec));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));

  size_t remaining = JxlDecoderReleaseInput(dec);
  // Since the test was set up to end exactly at the boundary of the final
  // codestream box, and the decoder returned success, all bytes are expected to
  // be consumed.
  EXPECT_EQ(0, remaining);

  // Now set the remaining non-codestream box as input.
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetInput(dec, compressed.data() + last_box_begin,
                               compressed.size() - last_box_begin));
  // Even though JxlDecoderProcessInput already returned JXL_DEC_SUCCESS before,
  // when calling it again now after setting more input, success is expected, no
  // event occurs but the box has been successfully skipped.
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));

  JxlDecoderDestroy(dec);
}

namespace {
bool BoxTypeEquals(const std::string& type_string, JxlBoxType type) {
  return type_string.size() == 4 && type_string[0] == type[0] &&
         type_string[1] == type[1] && type_string[2] == type[2] &&
         type_string[3] == type[3];
}
}  // namespace

TEST(DecodeTest, BoxTest) {
  size_t xsize = 1, ysize = 1;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);
  jxl::CompressParams cparams;
  jxl::PaddedBytes compressed = jxl::CreateTestJXLCodestream(
      jxl::Span<const uint8_t>(pixels.data(), pixels.size()), xsize, ysize, 4,
      cparams, kCSBF_Multi_Other_Terminated, JXL_ORIENT_IDENTITY, false, true);

  JxlDecoder* dec = JxlDecoderCreate(nullptr);

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSubscribeEvents(dec, JXL_DEC_BOX));
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetInput(dec, compressed.data(), compressed.size()));

  std::vector<std::string> expected_box_types = {
      "JXL ", "ftyp", "jxlp", "unk1", "unk2", "jxlp", "jxlp", "jxlp", "unk3"};

  // Value 0 means to not test the size: codestream is not required to be a
  // particular exact size.
  std::vector<size_t> expected_box_sizes = {12, 20, 0, 34, 18, 0, 0, 0, 20};

  JxlBoxType type;
  uint64_t box_size;
  std::vector<uint8_t> contents(50);
  size_t expected_release_size = 0;

  // Cannot get these when decoding didn't start yet
  EXPECT_EQ(JXL_DEC_ERROR, JxlDecoderGetBoxType(dec, type, JXL_FALSE));
  EXPECT_EQ(JXL_DEC_ERROR, JxlDecoderGetBoxSizeRaw(dec, &box_size));

  for (size_t i = 0; i < expected_box_types.size(); i++) {
    EXPECT_EQ(JXL_DEC_BOX, JxlDecoderProcessInput(dec));
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBoxType(dec, type, JXL_FALSE));
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBoxSizeRaw(dec, &box_size));
    EXPECT_TRUE(BoxTypeEquals(expected_box_types[i], type));
    if (expected_box_sizes[i]) {
      EXPECT_EQ(expected_box_sizes[i], box_size);
    }

    if (expected_release_size > 0) {
      EXPECT_EQ(expected_release_size, JxlDecoderReleaseBoxBuffer(dec));
      expected_release_size = 0;
    }

    if (type[0] == 'u' && type[1] == 'n' && type[2] == 'k') {
      JxlDecoderSetBoxBuffer(dec, contents.data(), contents.size());
      size_t expected_box_contents_size =
          type[3] == '1' ? unk1_box_size
                         : (type[3] == '2' ? unk2_box_size : unk3_box_size);
      expected_release_size = contents.size() - expected_box_contents_size;
    }
  }

  // Even though all input is given, the decoder cannot assume there aren't
  // more boxes if the input was not closed.
  EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, JxlDecoderProcessInput(dec));
  JxlDecoderCloseInput(dec);
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));

  JxlDecoderDestroy(dec);
}

TEST(DecodeTest, ExifBrobBoxTest) {
  size_t xsize = 1, ysize = 1;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);
  jxl::CompressParams cparams;
  cparams.SetLossless();  // Lossless to verify pixels exactly after roundtrip.
  jxl::PaddedBytes compressed = jxl::CreateTestJXLCodestream(
      jxl::Span<const uint8_t>(pixels.data(), pixels.size()), xsize, ysize, 4,
      cparams, kCSBF_Brob_Exif, JXL_ORIENT_IDENTITY, false, true);

  // Test raw brob box, not brotli-decompressing
  for (int streaming = 0; streaming < 2; ++streaming) {
    JxlDecoder* dec = JxlDecoderCreate(nullptr);

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSubscribeEvents(dec, JXL_DEC_BOX));
    if (!streaming) {
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderSetInput(dec, compressed.data(), compressed.size()));
      JxlDecoderCloseInput(dec);
    }
    // for streaming input case
    const uint8_t* next_in = compressed.data();
    size_t avail_in = 0;
    size_t total_in = 0;
    size_t step_size = 64;

    std::vector<uint8_t> box_buffer;
    size_t box_num_output;
    bool seen_brob_begin = false;
    bool seen_brob_end = false;

    for (;;) {
      JxlDecoderStatus status = JxlDecoderProcessInput(dec);
      if (status == JXL_DEC_NEED_MORE_INPUT) {
        if (streaming) {
          size_t remaining = JxlDecoderReleaseInput(dec);
          EXPECT_LE(remaining, avail_in);
          next_in += avail_in - remaining;
          avail_in = remaining;
          size_t amount = step_size;
          if (total_in + amount > compressed.size()) {
            amount = compressed.size() - total_in;
          }
          avail_in += amount;
          total_in += amount;
          EXPECT_EQ(JXL_DEC_SUCCESS,
                    JxlDecoderSetInput(dec, next_in, avail_in));
          if (total_in == compressed.size()) JxlDecoderCloseInput(dec);
        } else {
          FAIL();
          break;
        }
      } else if (status == JXL_DEC_BOX || status == JXL_DEC_SUCCESS) {
        if (!box_buffer.empty()) {
          EXPECT_EQ(false, seen_brob_end);
          seen_brob_end = true;
          size_t remaining = JxlDecoderReleaseBoxBuffer(dec);
          box_num_output = box_buffer.size() - remaining;
          EXPECT_EQ(box_num_output, box_brob_exif_size - 8);
          EXPECT_EQ(
              0, memcmp(box_buffer.data(), box_brob_exif + 8, box_num_output));
          box_buffer.clear();
        }
        if (status == JXL_DEC_SUCCESS) break;
        JxlBoxType type;
        EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBoxType(dec, type, JXL_FALSE));
        if (BoxTypeEquals("brob", type)) {
          EXPECT_EQ(false, seen_brob_begin);
          seen_brob_begin = true;
          box_buffer.resize(8);
          box_num_output = 0;
          JxlDecoderSetBoxBuffer(dec, box_buffer.data(), box_buffer.size());
        }
      } else if (status == JXL_DEC_BOX_NEED_MORE_OUTPUT) {
        size_t remaining = JxlDecoderReleaseBoxBuffer(dec);
        box_num_output = box_buffer.size() - remaining;
        box_buffer.resize(box_buffer.size() * 2);
        JxlDecoderSetBoxBuffer(dec, box_buffer.data() + box_num_output,
                               box_buffer.size() - box_num_output);
      } else {
        // We do not expect any other events or errors
        FAIL();
        break;
      }
    }

    EXPECT_EQ(true, seen_brob_begin);
    EXPECT_EQ(true, seen_brob_end);

    JxlDecoderDestroy(dec);
  }

  // Test decompressed brob box
  for (int streaming = 0; streaming < 2; ++streaming) {
    JxlDecoder* dec = JxlDecoderCreate(nullptr);

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSubscribeEvents(dec, JXL_DEC_BOX));
    if (!streaming) {
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderSetInput(dec, compressed.data(), compressed.size()));
      JxlDecoderCloseInput(dec);
    }
    // for streaming input case
    const uint8_t* next_in = compressed.data();
    size_t avail_in = 0;
    size_t total_in = 0;
    size_t step_size = 64;

    std::vector<uint8_t> box_buffer;
    size_t box_num_output;
    bool seen_exif_begin = false;
    bool seen_exif_end = false;

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetDecompressBoxes(dec, JXL_TRUE));

    for (;;) {
      JxlDecoderStatus status = JxlDecoderProcessInput(dec);
      if (status == JXL_DEC_NEED_MORE_INPUT) {
        if (streaming) {
          size_t remaining = JxlDecoderReleaseInput(dec);
          EXPECT_LE(remaining, avail_in);
          next_in += avail_in - remaining;
          avail_in = remaining;
          size_t amount = step_size;
          if (total_in + amount > compressed.size()) {
            amount = compressed.size() - total_in;
          }
          avail_in += amount;
          total_in += amount;
          EXPECT_EQ(JXL_DEC_SUCCESS,
                    JxlDecoderSetInput(dec, next_in, avail_in));
          if (total_in == compressed.size()) JxlDecoderCloseInput(dec);
        } else {
          FAIL();
          break;
        }
      } else if (status == JXL_DEC_BOX || status == JXL_DEC_SUCCESS) {
        if (!box_buffer.empty()) {
          EXPECT_EQ(false, seen_exif_end);
          seen_exif_end = true;
          size_t remaining = JxlDecoderReleaseBoxBuffer(dec);
          box_num_output = box_buffer.size() - remaining;
          // Expect that the output has the same size and contents as the
          // uncompressed exif data. Only check contents if the sizes match to
          // avoid comparing uninitialized memory in the test.
          EXPECT_EQ(box_num_output, exif_uncompressed_size);
          if (box_num_output == exif_uncompressed_size) {
            EXPECT_EQ(0, memcmp(box_buffer.data(), exif_uncompressed,
                                exif_uncompressed_size));
          }
          box_buffer.clear();
        }
        if (status == JXL_DEC_SUCCESS) break;
        JxlBoxType type;
        EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBoxType(dec, type, JXL_TRUE));
        if (BoxTypeEquals("Exif", type)) {
          EXPECT_EQ(false, seen_exif_begin);
          seen_exif_begin = true;
          box_buffer.resize(8);
          box_num_output = 0;
          JxlDecoderSetBoxBuffer(dec, box_buffer.data(), box_buffer.size());
        }
      } else if (status == JXL_DEC_BOX_NEED_MORE_OUTPUT) {
        size_t remaining = JxlDecoderReleaseBoxBuffer(dec);
        box_num_output = box_buffer.size() - remaining;
        box_buffer.resize(box_buffer.size() * 2);
        JxlDecoderSetBoxBuffer(dec, box_buffer.data() + box_num_output,
                               box_buffer.size() - box_num_output);
      } else {
        // We do not expect any other events or errors
        FAIL();
        break;
      }
    }

    EXPECT_EQ(true, seen_exif_begin);
    EXPECT_EQ(true, seen_exif_end);

    JxlDecoderDestroy(dec);
  }
}

TEST(DecodeTest, PartialCodestreamBoxTest) {
  size_t xsize = 23, ysize = 81;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);
  JxlPixelFormat format_orig = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  jxl::CompressParams cparams;
  cparams.SetLossless();  // Lossless to verify pixels exactly after roundtrip.
  jxl::PaddedBytes compressed = jxl::CreateTestJXLCodestream(
      jxl::Span<const uint8_t>(pixels.data(), pixels.size()), xsize, ysize, 4,
      cparams, kCSBF_Multi, JXL_ORIENT_IDENTITY, false, true);

  std::vector<uint8_t> extracted_codestream;

  {
    JxlDecoder* dec = JxlDecoderCreate(nullptr);

    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSubscribeEvents(
                  dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE | JXL_DEC_BOX));
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSetInput(dec, compressed.data(), compressed.size()));
    JxlDecoderCloseInput(dec);

    size_t num_jxlp = 0;

    std::vector<uint8_t> pixels2;
    pixels2.resize(pixels.size());

    std::vector<uint8_t> box_buffer;
    size_t box_num_output;

    for (;;) {
      JxlDecoderStatus status = JxlDecoderProcessInput(dec);
      if (status == JXL_DEC_NEED_MORE_INPUT) {
        FAIL();
        break;
      } else if (status == JXL_DEC_BASIC_INFO) {
        JxlBasicInfo info;
        EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
        EXPECT_EQ(info.xsize, xsize);
        EXPECT_EQ(info.ysize, ysize);
      } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
        EXPECT_EQ(JXL_DEC_SUCCESS,
                  JxlDecoderSetImageOutBuffer(dec, &format_orig, pixels2.data(),
                                              pixels2.size()));
      } else if (status == JXL_DEC_FULL_IMAGE) {
        continue;
      } else if (status == JXL_DEC_BOX || status == JXL_DEC_SUCCESS) {
        if (!box_buffer.empty()) {
          size_t remaining = JxlDecoderReleaseBoxBuffer(dec);
          box_num_output = box_buffer.size() - remaining;
          EXPECT_GE(box_num_output, 4);
          // Do not insert the first 4 bytes, which are not part of the
          // codestream, but the partial codestream box index
          extracted_codestream.insert(extracted_codestream.end(),
                                      box_buffer.begin() + 4,
                                      box_buffer.begin() + box_num_output);
          box_buffer.clear();
        }
        if (status == JXL_DEC_SUCCESS) break;
        JxlBoxType type;
        EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBoxType(dec, type, JXL_FALSE));
        if (BoxTypeEquals("jxlp", type)) {
          num_jxlp++;
          box_buffer.resize(8);
          box_num_output = 0;
          JxlDecoderSetBoxBuffer(dec, box_buffer.data(), box_buffer.size());
        }
      } else if (status == JXL_DEC_BOX_NEED_MORE_OUTPUT) {
        size_t remaining = JxlDecoderReleaseBoxBuffer(dec);
        box_num_output = box_buffer.size() - remaining;
        box_buffer.resize(box_buffer.size() * 2);
        JxlDecoderSetBoxBuffer(dec, box_buffer.data() + box_num_output,
                               box_buffer.size() - box_num_output);
      } else {
        // We do not expect any other events or errors
        FAIL();
        break;
      }
    }

    // The test file created with kCSBF_Multi is expected to have 4 jxlp boxes.
    EXPECT_EQ(4, num_jxlp);

    EXPECT_EQ(0u, ComparePixels(pixels.data(), pixels2.data(), xsize, ysize,
                                format_orig, format_orig));

    JxlDecoderDestroy(dec);
  }

  // Now test whether the codestream extracted from the jxlp boxes can itself
  // also be decoded and gives the same pixels
  {
    JxlDecoder* dec = JxlDecoderCreate(nullptr);

    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSubscribeEvents(
                  dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE | JXL_DEC_BOX));
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSetInput(dec, extracted_codestream.data(),
                                 extracted_codestream.size()));
    JxlDecoderCloseInput(dec);

    size_t num_boxes = 0;

    std::vector<uint8_t> pixels2;
    pixels2.resize(pixels.size());

    std::vector<uint8_t> box_buffer;
    size_t box_num_output;

    for (;;) {
      JxlDecoderStatus status = JxlDecoderProcessInput(dec);
      if (status == JXL_DEC_NEED_MORE_INPUT) {
        FAIL();
        break;
      } else if (status == JXL_DEC_BASIC_INFO) {
        JxlBasicInfo info;
        EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
        EXPECT_EQ(info.xsize, xsize);
        EXPECT_EQ(info.ysize, ysize);
      } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
        EXPECT_EQ(JXL_DEC_SUCCESS,
                  JxlDecoderSetImageOutBuffer(dec, &format_orig, pixels2.data(),
                                              pixels2.size()));
      } else if (status == JXL_DEC_FULL_IMAGE) {
        continue;
      } else if (status == JXL_DEC_BOX) {
        num_boxes++;
      } else if (status == JXL_DEC_BOX_NEED_MORE_OUTPUT) {
        size_t remaining = JxlDecoderReleaseBoxBuffer(dec);
        box_num_output = box_buffer.size() - remaining;
        box_buffer.resize(box_buffer.size() * 2);
        JxlDecoderSetBoxBuffer(dec, box_buffer.data() + box_num_output,
                               box_buffer.size() - box_num_output);
      } else if (status == JXL_DEC_SUCCESS) {
        break;
      } else {
        // We do not expect any other events or errors
        FAIL();
        break;
      }
    }

    EXPECT_EQ(0, num_boxes);  // The data does not use the container format.
    EXPECT_EQ(0u, ComparePixels(pixels.data(), pixels2.data(), xsize, ysize,
                                format_orig, format_orig));

    JxlDecoderDestroy(dec);
  }
}

TEST(DecodeTest, SpotColorTest) {
  jxl::ThreadPool* pool = nullptr;
  jxl::CodecInOut io;
  size_t xsize = 55, ysize = 257;
  io.metadata.m.color_encoding = jxl::ColorEncoding::LinearSRGB();
  jxl::Image3F main(xsize, ysize);
  jxl::ImageF spot(xsize, ysize);
  jxl::ZeroFillImage(&main);
  jxl::ZeroFillImage(&spot);

  for (size_t y = 0; y < ysize; y++) {
    float* JXL_RESTRICT rowm = main.PlaneRow(1, y);
    float* JXL_RESTRICT rows = spot.Row(y);
    for (size_t x = 0; x < xsize; x++) {
      rowm[x] = (x + y) * (1.f / 255.f);
      rows[x] = ((x ^ y) & 255) * (1.f / 255.f);
    }
  }
  io.SetFromImage(std::move(main), jxl::ColorEncoding::LinearSRGB());
  jxl::ExtraChannelInfo info;
  info.bit_depth.bits_per_sample = 8;
  info.dim_shift = 0;
  info.type = jxl::ExtraChannel::kSpotColor;
  info.spot_color[0] = 0.5f;
  info.spot_color[1] = 0.2f;
  info.spot_color[2] = 1.f;
  info.spot_color[3] = 0.5f;

  io.metadata.m.extra_channel_info.push_back(info);
  std::vector<jxl::ImageF> ec;
  ec.push_back(std::move(spot));
  io.frames[0].SetExtraChannels(std::move(ec));

  jxl::CompressParams cparams;
  cparams.speed_tier = jxl::SpeedTier::kLightning;
  cparams.modular_mode = true;
  cparams.color_transform = jxl::ColorTransform::kNone;
  cparams.quality_pair = {100, 100};

  jxl::PaddedBytes compressed;
  std::unique_ptr<jxl::PassesEncoderState> enc_state =
      jxl::make_unique<jxl::PassesEncoderState>();
  EXPECT_TRUE(jxl::EncodeFile(cparams, &io, enc_state.get(), &compressed,
                              nullptr, pool));

  for (size_t render_spot = 0; render_spot < 2; render_spot++) {
    JxlPixelFormat format = {3, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};

    JxlDecoder* dec = JxlDecoderCreate(NULL);

    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSubscribeEvents(
                  dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE));
    if (!render_spot) {
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetRenderSpotcolors(dec, JXL_FALSE));
    }

    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSetInput(dec, compressed.data(), compressed.size()));
    EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
    JxlBasicInfo binfo;
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &binfo));
    EXPECT_EQ(1u, binfo.num_extra_channels);
    EXPECT_EQ(xsize, binfo.xsize);
    EXPECT_EQ(ysize, binfo.ysize);

    JxlExtraChannelInfo extra_info;
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderGetExtraChannelInfo(dec, 0, &extra_info));
    EXPECT_EQ((unsigned int)jxl::ExtraChannel::kSpotColor, extra_info.type);

    EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));
    size_t buffer_size;
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
    size_t extra_size;
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderExtraChannelBufferSize(dec, &format, &extra_size, 0));

    std::vector<uint8_t> image(buffer_size);
    std::vector<uint8_t> extra(extra_size);
    size_t bytes_per_pixel =
        format.num_channels * GetDataBits(format.data_type) / jxl::kBitsPerByte;
    size_t stride = bytes_per_pixel * binfo.xsize;

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                   dec, &format, image.data(), image.size()));
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSetExtraChannelBuffer(dec, &format, extra.data(),
                                              extra.size(), 0));

    EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));

    // After the full image was output, JxlDecoderProcessInput should return
    // success to indicate all is done.
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));
    JxlDecoderDestroy(dec);

    for (size_t y = 0; y < ysize; y++) {
      uint8_t* JXL_RESTRICT rowm = image.data() + stride * y;
      uint8_t* JXL_RESTRICT rows = extra.data() + xsize * y;
      for (size_t x = 0; x < xsize; x++) {
        if (!render_spot) {
          // if spot color isn't rendered, main image should be as we made it
          // (red and blue are all zeroes)

          EXPECT_EQ(rowm[x * 3 + 0], 0);
          EXPECT_EQ(rowm[x * 3 + 1], (x + y > 255 ? 255 : x + y));
          EXPECT_EQ(rowm[x * 3 + 2], 0);
        }
        if (render_spot) {
          // if spot color is rendered, expect red and blue to look like the
          // spot color channel
          EXPECT_LT(abs(rowm[x * 3 + 0] - (rows[x] * 0.25f)), 1);
          EXPECT_LT(abs(rowm[x * 3 + 2] - (rows[x] * 0.5f)), 1);
        }
        EXPECT_EQ(rows[x], ((x ^ y) & 255));
      }
    }
  }
}

TEST(DecodeTest, CloseInput) {
  std::vector<uint8_t> partial_file = {0xff};

  JxlDecoderPtr dec = JxlDecoderMake(nullptr);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(dec.get(),
                                      JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec.get(), partial_file.data(),
                                                partial_file.size()));
  EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, JxlDecoderProcessInput(dec.get()));
  EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, JxlDecoderProcessInput(dec.get()));
  JxlDecoderCloseInput(dec.get());
  EXPECT_EQ(JXL_DEC_ERROR, JxlDecoderProcessInput(dec.get()));
}
