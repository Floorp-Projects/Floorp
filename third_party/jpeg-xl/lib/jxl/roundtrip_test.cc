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

#include "gtest/gtest.h"
#include "jxl/decode.h"
#include "jxl/decode_cxx.h"
#include "jxl/encode.h"
#include "jxl/encode_cxx.h"
#include "lib/extras/codec.h"
#include "lib/jxl/dec_external_image.h"
#include "lib/jxl/enc_butteraugli_comparator.h"
#include "lib/jxl/encode_internal.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testdata.h"

namespace {

// Converts a test image to a CodecInOut.
// icc_profile can be empty to automatically deduce profile from the pixel
// format, or filled in to force this ICC profile
jxl::CodecInOut ConvertTestImage(const std::vector<uint8_t>& buf,
                                 const size_t xsize, const size_t ysize,
                                 const JxlPixelFormat& pixel_format,
                                 const jxl::PaddedBytes& icc_profile) {
  jxl::CodecInOut io;
  io.SetSize(xsize, ysize);

  bool is_gray =
      pixel_format.num_channels == 1 || pixel_format.num_channels == 2;
  bool has_alpha =
      pixel_format.num_channels == 2 || pixel_format.num_channels == 4;

  io.metadata.m.color_encoding.SetColorSpace(is_gray ? jxl::ColorSpace::kGray
                                                     : jxl::ColorSpace::kRGB);
  if (has_alpha) {
    // Note: alpha > 16 not yet supported by the C++ codec
    switch (pixel_format.data_type) {
      case JXL_TYPE_UINT8:
        io.metadata.m.SetAlphaBits(8);
        break;
      case JXL_TYPE_UINT16:
      case JXL_TYPE_UINT32:
      case JXL_TYPE_FLOAT:
      case JXL_TYPE_FLOAT16:
        io.metadata.m.SetAlphaBits(16);
        break;
      default:
        EXPECT_TRUE(false) << "Roundtrip tests for data type "
                           << pixel_format.data_type << " not yet implemented.";
    }
  }
  size_t bitdepth = 0;
  switch (pixel_format.data_type) {
    case JXL_TYPE_FLOAT:
      bitdepth = 32;
      io.metadata.m.SetFloat32Samples();
      break;
    case JXL_TYPE_FLOAT16:
      bitdepth = 16;
      io.metadata.m.SetFloat16Samples();
      break;
    case JXL_TYPE_UINT8:
      bitdepth = 8;
      io.metadata.m.SetUintSamples(8);
      break;
    case JXL_TYPE_UINT16:
      bitdepth = 16;
      io.metadata.m.SetUintSamples(16);
      break;
    default:
      EXPECT_TRUE(false) << "Roundtrip tests for data type "
                         << pixel_format.data_type << " not yet implemented.";
  }
  jxl::ColorEncoding color_encoding;
  if (!icc_profile.empty()) {
    jxl::PaddedBytes icc_profile_copy(icc_profile);
    EXPECT_TRUE(color_encoding.SetICC(std::move(icc_profile_copy)));
  } else if (pixel_format.data_type == JXL_TYPE_FLOAT) {
    color_encoding = jxl::ColorEncoding::LinearSRGB(is_gray);
  } else {
    color_encoding = jxl::ColorEncoding::SRGB(is_gray);
  }
  EXPECT_TRUE(
      ConvertFromExternal(jxl::Span<const uint8_t>(buf.data(), buf.size()),
                          xsize, ysize, color_encoding, has_alpha,
                          /*alpha_is_premultiplied=*/false,
                          /*bits_per_sample=*/bitdepth, pixel_format.endianness,
                          /*flipped_y=*/false, /*pool=*/nullptr, &io.Main()));
  return io;
}

// Stores a float in big endian
void StoreBEFloat(float value, uint8_t* p) {
  uint32_t u;
  memcpy(&u, &value, 4);
  StoreBE32(u, p);
}

// Stores a float in little endian
void StoreLEFloat(float value, uint8_t* p) {
  uint32_t u;
  memcpy(&u, &value, 4);
  StoreLE32(u, p);
}

// Loads a float in big endian
float LoadBEFloat(const uint8_t* p) {
  float value;
  const uint32_t u = LoadBE32(p);
  memcpy(&value, &u, 4);
  return value;
}

// Loads a float in little endian
float LoadLEFloat(const uint8_t* p) {
  float value;
  const uint32_t u = LoadLE32(p);
  memcpy(&value, &u, 4);
  return value;
}

template <typename T>
T ConvertTestPixel(const float val);

template <>
float ConvertTestPixel<float>(const float val) {
  return val;
}

template <>
uint16_t ConvertTestPixel<uint16_t>(const float val) {
  return (uint16_t)(val * UINT16_MAX);
}

template <>
uint8_t ConvertTestPixel<uint8_t>(const float val) {
  return (uint8_t)(val * UINT8_MAX);
}

// Returns a test image.
template <typename T>
std::vector<uint8_t> GetTestImage(const size_t xsize, const size_t ysize,
                                  const JxlPixelFormat& pixel_format) {
  std::vector<T> pixels(xsize * ysize * pixel_format.num_channels);
  for (size_t y = 0; y < ysize; y++) {
    for (size_t x = 0; x < xsize; x++) {
      for (size_t chan = 0; chan < pixel_format.num_channels; chan++) {
        float val;
        switch (chan % 4) {
          case 0:
            val = static_cast<float>(y) / static_cast<float>(ysize);
            break;
          case 1:
            val = static_cast<float>(x) / static_cast<float>(xsize);
            break;
          case 2:
            val = static_cast<float>(x + y) / static_cast<float>(xsize + ysize);
            break;
          case 3:
            val = static_cast<float>(x * y) / static_cast<float>(xsize * ysize);
            break;
        }
        pixels[(y * xsize + x) * pixel_format.num_channels + chan] =
            ConvertTestPixel<T>(val);
      }
    }
  }
  std::vector<uint8_t> bytes(pixels.size() * sizeof(T));
  memcpy(bytes.data(), pixels.data(), sizeof(T) * pixels.size());
  return bytes;
}

void EncodeWithEncoder(JxlEncoder* enc, std::vector<uint8_t>* compressed) {
  compressed->resize(64);
  uint8_t* next_out = compressed->data();
  size_t avail_out = compressed->size() - (next_out - compressed->data());
  JxlEncoderStatus process_result = JXL_ENC_NEED_MORE_OUTPUT;
  while (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
    process_result = JxlEncoderProcessOutput(enc, &next_out, &avail_out);
    if (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
      size_t offset = next_out - compressed->data();
      compressed->resize(compressed->size() * 2);
      next_out = compressed->data() + offset;
      avail_out = compressed->size() - offset;
    }
  }
  compressed->resize(next_out - compressed->data());
  EXPECT_EQ(JXL_ENC_SUCCESS, process_result);
}

// Generates some pixels using using some dimensions and pixel_format,
// compresses them, and verifies that the decoded version is similar to the
// original pixels.
template <typename T>
void VerifyRoundtripCompression(const size_t xsize, const size_t ysize,
                                const JxlPixelFormat& input_pixel_format,
                                const JxlPixelFormat& output_pixel_format,
                                const bool lossless, const bool use_container) {
  const std::vector<uint8_t> original_bytes =
      GetTestImage<T>(xsize, ysize, input_pixel_format);
  jxl::CodecInOut original_io =
      ConvertTestImage(original_bytes, xsize, ysize, input_pixel_format, {});

  JxlEncoder* enc = JxlEncoderCreate(nullptr);
  EXPECT_NE(nullptr, enc);

  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderUseContainer(enc, use_container));
  JxlBasicInfo basic_info;
  jxl::test::JxlBasicInfoSetFromPixelFormat(&basic_info, &input_pixel_format);
  basic_info.xsize = xsize;
  basic_info.ysize = ysize;
  basic_info.uses_original_profile = lossless;
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetBasicInfo(enc, &basic_info));
  JxlColorEncoding color_encoding;
  if (input_pixel_format.data_type == JXL_TYPE_FLOAT) {
    JxlColorEncodingSetToLinearSRGB(
        &color_encoding,
        /*is_gray=*/input_pixel_format.num_channels < 3);
  } else {
    JxlColorEncodingSetToSRGB(&color_encoding,
                              /*is_gray=*/input_pixel_format.num_channels < 3);
  }
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetColorEncoding(enc, &color_encoding));
  JxlEncoderOptions* opts = JxlEncoderOptionsCreate(enc, nullptr);
  JxlEncoderOptionsSetLossless(opts, lossless);
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderAddImageFrame(opts, &input_pixel_format,
                                    (void*)original_bytes.data(),
                                    original_bytes.size()));
  JxlEncoderCloseInput(enc);

  std::vector<uint8_t> compressed;
  EncodeWithEncoder(enc, &compressed);
  JxlEncoderDestroy(enc);

  JxlDecoder* dec = JxlDecoderCreate(nullptr);
  EXPECT_NE(nullptr, dec);

  const uint8_t* next_in = compressed.data();
  size_t avail_in = compressed.size();

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO |
                                               JXL_DEC_COLOR_ENCODING |
                                               JXL_DEC_FULL_IMAGE));

  JxlDecoderSetInput(dec, next_in, avail_in);
  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
  size_t buffer_size;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderImageOutBufferSize(
                                 dec, &output_pixel_format, &buffer_size));
  if (&input_pixel_format == &output_pixel_format) {
    EXPECT_EQ(buffer_size, original_bytes.size());
  }

  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
  EXPECT_EQ(xsize, info.xsize);
  EXPECT_EQ(ysize, info.ysize);

  EXPECT_EQ(JXL_DEC_COLOR_ENCODING, JxlDecoderProcessInput(dec));

  size_t icc_profile_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetICCProfileSize(dec, &output_pixel_format,
                                        JXL_COLOR_PROFILE_TARGET_DATA,
                                        &icc_profile_size));
  jxl::PaddedBytes icc_profile(icc_profile_size);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetColorAsICCProfile(
                dec, &output_pixel_format, JXL_COLOR_PROFILE_TARGET_DATA,
                icc_profile.data(), icc_profile.size()));

  std::vector<uint8_t> decoded_bytes(buffer_size);

  EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                 dec, &output_pixel_format,
                                 decoded_bytes.data(), decoded_bytes.size()));

  EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));

  JxlDecoderDestroy(dec);

  jxl::CodecInOut decoded_io = ConvertTestImage(
      decoded_bytes, xsize, ysize, output_pixel_format, icc_profile);

  jxl::ButteraugliParams ba;
  float butteraugli_score = ButteraugliDistance(original_io, decoded_io, ba,
                                                /*distmap=*/nullptr, nullptr);
  if (lossless) {
    EXPECT_LE(butteraugli_score, 0.0f);
  } else {
    EXPECT_LE(butteraugli_score, 2.0f);
  }
}

}  // namespace

TEST(RoundtripTest, FloatFrameRoundtripTest) {
  for (int use_container = 0; use_container < 2; use_container++) {
    for (int lossless = 0; lossless < 2; lossless++) {
      for (uint32_t num_channels = 1; num_channels < 5; num_channels++) {
        // There's no support (yet) for lossless extra float channels, so we
        // don't test it.
        if (num_channels % 2 != 0 || !lossless) {
          JxlPixelFormat pixel_format = JxlPixelFormat{
              num_channels, JXL_TYPE_FLOAT, JXL_NATIVE_ENDIAN, 0};
          VerifyRoundtripCompression<float>(63, 129, pixel_format, pixel_format,
                                            (bool)lossless,
                                            (bool)use_container);
        }
      }
    }
  }
}

TEST(RoundtripTest, Uint16FrameRoundtripTest) {
  for (int use_container = 0; use_container < 2; use_container++) {
    for (int lossless = 0; lossless < 2; lossless++) {
      for (uint32_t num_channels = 1; num_channels < 5; num_channels++) {
        JxlPixelFormat pixel_format =
            JxlPixelFormat{num_channels, JXL_TYPE_UINT16, JXL_NATIVE_ENDIAN, 0};
        VerifyRoundtripCompression<uint16_t>(63, 129, pixel_format,
                                             pixel_format, (bool)lossless,
                                             (bool)use_container);
      }
    }
  }
}

TEST(RoundtripTest, Uint8FrameRoundtripTest) {
  for (int use_container = 0; use_container < 2; use_container++) {
    for (int lossless = 0; lossless < 2; lossless++) {
      for (uint32_t num_channels = 1; num_channels < 5; num_channels++) {
        JxlPixelFormat pixel_format =
            JxlPixelFormat{num_channels, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
        VerifyRoundtripCompression<uint8_t>(63, 129, pixel_format, pixel_format,
                                            (bool)lossless,
                                            (bool)use_container);
      }
    }
  }
}

TEST(RoundtripTest, TestNonlinearSrgbAsXybEncoded) {
  for (int use_container = 0; use_container < 2; use_container++) {
    for (uint32_t num_channels = 1; num_channels < 5; num_channels++) {
      JxlPixelFormat pixel_format_in =
          JxlPixelFormat{num_channels, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
      JxlPixelFormat pixel_format_out =
          JxlPixelFormat{num_channels, JXL_TYPE_FLOAT, JXL_NATIVE_ENDIAN, 0};
      VerifyRoundtripCompression<uint8_t>(
          63, 129, pixel_format_in, pixel_format_out,
          /*lossless=*/false, (bool)use_container);
    }
  }
}

TEST(RoundtripTest, ExtraBoxesTest) {
  JxlPixelFormat pixel_format =
      JxlPixelFormat{4, JXL_TYPE_FLOAT, JXL_NATIVE_ENDIAN, 0};
  const size_t xsize = 61;
  const size_t ysize = 71;

  const std::vector<uint8_t> original_bytes =
      GetTestImage<float>(xsize, ysize, pixel_format);
  jxl::CodecInOut original_io =
      ConvertTestImage(original_bytes, xsize, ysize, pixel_format, {});

  JxlEncoder* enc = JxlEncoderCreate(nullptr);
  EXPECT_NE(nullptr, enc);

  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderUseContainer(enc, true));
  JxlBasicInfo basic_info;
  jxl::test::JxlBasicInfoSetFromPixelFormat(&basic_info, &pixel_format);
  basic_info.xsize = xsize;
  basic_info.ysize = ysize;
  basic_info.uses_original_profile = false;
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetBasicInfo(enc, &basic_info));
  JxlColorEncoding color_encoding;
  if (pixel_format.data_type == JXL_TYPE_FLOAT) {
    JxlColorEncodingSetToLinearSRGB(&color_encoding,
                                    /*is_gray=*/pixel_format.num_channels < 3);
  } else {
    JxlColorEncodingSetToSRGB(&color_encoding,
                              /*is_gray=*/pixel_format.num_channels < 3);
  }
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetColorEncoding(enc, &color_encoding));
  JxlEncoderOptions* opts = JxlEncoderOptionsCreate(enc, nullptr);
  JxlEncoderOptionsSetLossless(opts, false);
  EXPECT_EQ(
      JXL_ENC_SUCCESS,
      JxlEncoderAddImageFrame(opts, &pixel_format, (void*)original_bytes.data(),
                              original_bytes.size()));
  JxlEncoderCloseInput(enc);

  std::vector<uint8_t> compressed;
  EncodeWithEncoder(enc, &compressed);
  JxlEncoderDestroy(enc);

  std::vector<uint8_t> extra_data(1023);
  jxl::AppendBoxHeader(jxl::MakeBoxType("crud"), extra_data.size(), false,
                       &compressed);
  compressed.insert(compressed.end(), extra_data.begin(), extra_data.end());

  JxlDecoder* dec = JxlDecoderCreate(nullptr);
  EXPECT_NE(nullptr, dec);

  const uint8_t* next_in = compressed.data();
  size_t avail_in = compressed.size();

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO |
                                               JXL_DEC_COLOR_ENCODING |
                                               JXL_DEC_FULL_IMAGE));

  JxlDecoderSetInput(dec, next_in, avail_in);
  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
  size_t buffer_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderImageOutBufferSize(dec, &pixel_format, &buffer_size));
  EXPECT_EQ(buffer_size, original_bytes.size());

  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
  EXPECT_EQ(xsize, info.xsize);
  EXPECT_EQ(ysize, info.ysize);

  EXPECT_EQ(JXL_DEC_COLOR_ENCODING, JxlDecoderProcessInput(dec));

  size_t icc_profile_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetICCProfileSize(dec, &pixel_format,
                                        JXL_COLOR_PROFILE_TARGET_DATA,
                                        &icc_profile_size));
  jxl::PaddedBytes icc_profile(icc_profile_size);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetColorAsICCProfile(
                dec, &pixel_format, JXL_COLOR_PROFILE_TARGET_DATA,
                icc_profile.data(), icc_profile.size()));

  std::vector<uint8_t> decoded_bytes(buffer_size);

  EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(dec, &pixel_format,
                                                         decoded_bytes.data(),
                                                         decoded_bytes.size()));

  EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));

  JxlDecoderDestroy(dec);

  jxl::CodecInOut decoded_io =
      ConvertTestImage(decoded_bytes, xsize, ysize, pixel_format, icc_profile);

  jxl::ButteraugliParams ba;
  float butteraugli_score = ButteraugliDistance(original_io, decoded_io, ba,
                                                /*distmap=*/nullptr, nullptr);
  EXPECT_LE(butteraugli_score, 2.0f);
}

TEST(RoundtripTest, TestICCProfile) {
  // This ICC profile is not a valid ICC profile, however neither the encoder
  // nor the decoder parse this profile, and the bytes should be passed on
  // correctly through the roundtrip.
  jxl::PaddedBytes icc;
  for (size_t i = 0; i < 200; i++) {
    icc.push_back(i ^ 55);
  }

  JxlPixelFormat format =
      JxlPixelFormat{3, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};

  size_t xsize = 25;
  size_t ysize = 37;
  const std::vector<uint8_t> original_bytes =
      GetTestImage<uint8_t>(xsize, ysize, format);

  JxlEncoder* enc = JxlEncoderCreate(nullptr);
  EXPECT_NE(nullptr, enc);

  JxlBasicInfo basic_info;
  jxl::test::JxlBasicInfoSetFromPixelFormat(&basic_info, &format);
  basic_info.xsize = xsize;
  basic_info.ysize = ysize;
  basic_info.uses_original_profile = JXL_FALSE;
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetBasicInfo(enc, &basic_info));

  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderSetICCProfile(enc, icc.data(), icc.size()));
  JxlEncoderOptions* opts = JxlEncoderOptionsCreate(enc, nullptr);
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderAddImageFrame(opts, &format, (void*)original_bytes.data(),
                                    original_bytes.size()));
  JxlEncoderCloseInput(enc);

  std::vector<uint8_t> compressed;
  EncodeWithEncoder(enc, &compressed);
  JxlEncoderDestroy(enc);

  JxlDecoder* dec = JxlDecoderCreate(nullptr);
  EXPECT_NE(nullptr, dec);

  const uint8_t* next_in = compressed.data();
  size_t avail_in = compressed.size();

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO |
                                               JXL_DEC_COLOR_ENCODING |
                                               JXL_DEC_FULL_IMAGE));

  JxlDecoderSetInput(dec, next_in, avail_in);
  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
  size_t buffer_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
  EXPECT_EQ(buffer_size, original_bytes.size());

  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
  EXPECT_EQ(xsize, info.xsize);
  EXPECT_EQ(ysize, info.ysize);

  EXPECT_EQ(JXL_DEC_COLOR_ENCODING, JxlDecoderProcessInput(dec));

  size_t dec_icc_size;
  EXPECT_EQ(
      JXL_DEC_SUCCESS,
      JxlDecoderGetICCProfileSize(
          dec, &format, JXL_COLOR_PROFILE_TARGET_ORIGINAL, &dec_icc_size));
  EXPECT_EQ(icc.size(), dec_icc_size);
  jxl::PaddedBytes dec_icc(dec_icc_size);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetColorAsICCProfile(dec, &format,
                                           JXL_COLOR_PROFILE_TARGET_ORIGINAL,
                                           dec_icc.data(), dec_icc.size()));

  std::vector<uint8_t> decoded_bytes(buffer_size);

  EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetImageOutBuffer(dec, &format, decoded_bytes.data(),
                                        decoded_bytes.size()));

  EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));

  EXPECT_EQ(icc, dec_icc);

  JxlDecoderDestroy(dec);
}

TEST(RoundtripTest, TestJPEGReconstruction) {
  const std::string jpeg_path =
      "imagecompression.info/flower_foveon.png.im_q85_420.jpg";
  const jxl::PaddedBytes orig = jxl::ReadTestData(jpeg_path);
  jxl::CodecInOut orig_io;
  ASSERT_TRUE(
      SetFromBytes(jxl::Span<const uint8_t>(orig), &orig_io, /*pool=*/nullptr));

  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);

  JxlBasicInfo basic_info;
  basic_info.exponent_bits_per_sample = 0;
  basic_info.bits_per_sample = 8;
  basic_info.alpha_bits = 0;
  basic_info.alpha_exponent_bits = 0;
  basic_info.xsize = orig_io.xsize();
  basic_info.ysize = orig_io.ysize();
  basic_info.uses_original_profile = true;
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetBasicInfo(enc.get(), &basic_info));
  JxlColorEncoding color_encoding;
  JxlColorEncodingSetToSRGB(&color_encoding, /*is_gray=*/false);
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderSetColorEncoding(enc.get(), &color_encoding));
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderUseContainer(enc.get(), JXL_TRUE));
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderStoreJPEGMetadata(enc.get(), JXL_TRUE));
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderAddJPEGFrame(options, orig.data(), orig.size()));
  JxlEncoderCloseInput(enc.get());

  std::vector<uint8_t> compressed;
  EncodeWithEncoder(enc.get(), &compressed);

  JxlDecoderPtr dec = JxlDecoderMake(nullptr);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec.get(), JXL_DEC_JPEG_RECONSTRUCTION | JXL_DEC_FULL_IMAGE));
  JxlDecoderSetInput(dec.get(), compressed.data(), compressed.size());
  EXPECT_EQ(JXL_DEC_JPEG_RECONSTRUCTION, JxlDecoderProcessInput(dec.get()));
  std::vector<uint8_t> reconstructed_buffer(128);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetJPEGBuffer(dec.get(), reconstructed_buffer.data(),
                                    reconstructed_buffer.size()));
  size_t used = 0;
  JxlDecoderStatus dec_process_result = JXL_DEC_JPEG_NEED_MORE_OUTPUT;
  while (dec_process_result == JXL_DEC_JPEG_NEED_MORE_OUTPUT) {
    used = reconstructed_buffer.size() - JxlDecoderReleaseJPEGBuffer(dec.get());
    reconstructed_buffer.resize(reconstructed_buffer.size() * 2);
    EXPECT_EQ(
        JXL_DEC_SUCCESS,
        JxlDecoderSetJPEGBuffer(dec.get(), reconstructed_buffer.data() + used,
                                reconstructed_buffer.size() - used));
    dec_process_result = JxlDecoderProcessInput(dec.get());
  }
  ASSERT_EQ(JXL_DEC_FULL_IMAGE, dec_process_result);
  used = reconstructed_buffer.size() - JxlDecoderReleaseJPEGBuffer(dec.get());
  ASSERT_EQ(used, orig.size());
  EXPECT_EQ(0, memcmp(reconstructed_buffer.data(), orig.data(), used));
}
