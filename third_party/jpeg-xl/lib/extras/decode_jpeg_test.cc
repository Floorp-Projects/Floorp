// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/decode_jpeg.h"

#include <stddef.h>
#include <stdio.h>

#if JPEGXL_ENABLE_JPEG
#include "lib/extras/dec/jpg.h"
#endif
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testdata.h"

namespace jxl {
namespace extras {
namespace {

using test::DistanceRMS;

struct TestConfig {
  std::string fn;
  std::string fn_desc;
  size_t chunk_size;
  size_t max_output_lines;
};

class DecodeJpegTestParam : public ::testing::TestWithParam<TestConfig> {};

TEST_P(DecodeJpegTestParam, Streaming) {
  TestConfig config = GetParam();
  const PaddedBytes compressed = ReadTestData(config.fn.c_str());

#if JPEGXL_ENABLE_JPEG
  PackedPixelFile ppf_libjpeg;
  EXPECT_TRUE(
      DecodeImageJPG(Span<const uint8_t>(compressed.data(), compressed.size()),
                     ColorHints(), SizeConstraints(), &ppf_libjpeg));
  ASSERT_EQ(1, ppf_libjpeg.frames.size());
#endif

  JpegDecoder dec;

  size_t chunk_size = config.chunk_size;
  if (chunk_size == 0) chunk_size = compressed.size();
  size_t pos = std::min(chunk_size, compressed.size());
  ASSERT_EQ(JpegDecoder::Status::kSuccess,
            dec.SetInput(compressed.data(), pos));

  JpegDecoder::Status status;
  for (;;) {
    status = dec.ReadHeaders();
    if (status == JpegDecoder::Status::kNeedMoreInput) {
      ASSERT_LT(pos, compressed.size());
      size_t len = std::min(chunk_size, compressed.size() - pos);
      ASSERT_EQ(JpegDecoder::Status::kSuccess,
                dec.SetInput(compressed.data() + pos, len));
      pos += len;
      continue;
    }
    ASSERT_EQ(status, JpegDecoder::Status::kSuccess);
    break;
  }

#if JPEGXL_ENABLE_JPEG
  EXPECT_EQ(ppf_libjpeg.info.xsize, dec.xsize());
  EXPECT_EQ(ppf_libjpeg.info.ysize, dec.ysize());
  EXPECT_EQ(ppf_libjpeg.info.num_color_channels, dec.num_channels());
#endif

  JxlPixelFormat format = {static_cast<uint32_t>(dec.num_channels()),
                           JXL_TYPE_UINT8, JXL_BIG_ENDIAN, 0};
  PackedImage output(dec.xsize(), dec.ysize(), format);
  ASSERT_EQ(JpegDecoder::Status::kSuccess, dec.SetOutput(&output));

  size_t max_output_lines = config.max_output_lines;
  if (max_output_lines == 0) max_output_lines = dec.ysize();

  size_t total_output_lines = 0;
  while (total_output_lines < dec.ysize()) {
    size_t num_output_lines = 0;
    status = dec.ReadScanLines(&num_output_lines, max_output_lines);
    total_output_lines += num_output_lines;
    if (status == JpegDecoder::Status::kNeedMoreInput) {
      ASSERT_LT(pos, compressed.size());
      size_t len = std::min(chunk_size, compressed.size() - pos);
      ASSERT_EQ(JpegDecoder::Status::kSuccess,
                dec.SetInput(compressed.data() + pos, len));
      pos += len;
      continue;
    }
    ASSERT_EQ(status, JpegDecoder::Status::kSuccess);
    if (total_output_lines < dec.ysize()) {
      EXPECT_EQ(num_output_lines, max_output_lines);
    }
  }

#if JPEGXL_ENABLE_JPEG
  const PackedImage& output_libjpeg = ppf_libjpeg.frames[0].color;
  ASSERT_EQ(output.xsize, output_libjpeg.xsize);
  ASSERT_EQ(output.ysize, output_libjpeg.ysize);
  EXPECT_LE(
      DistanceRMS(reinterpret_cast<const uint8_t*>(output.pixels()),
                  reinterpret_cast<const uint8_t*>(output_libjpeg.pixels()),
                  output.xsize, output.ysize, output.format),
      0.0075);
#endif
}

std::vector<TestConfig> GenerateTests() {
  std::vector<TestConfig> all_tests;
  {
    std::vector<std::pair<std::string, std::string>> testfiles({
        {"jxl/flower/flower.png.im_q85_444.jpg", "Q85YUV444"},
        {"jxl/flower/flower.png.im_q85_420.jpg", "Q85YUV420"},
        {"jxl/flower/flower.png.im_q85_420_progr.jpg", "Q85YUV420PROGR"},
        {"jxl/flower/flower.png.im_q85_420_R13B.jpg", "Q85YUV420R13B"},
    });
    for (const auto& it : testfiles) {
      for (size_t chunk_size : {0, 1, 64, 65536}) {
        for (size_t max_output_lines : {0, 1, 8, 16}) {
          TestConfig config;
          config.fn = it.first;
          config.fn_desc = it.second;
          config.chunk_size = chunk_size;
          config.max_output_lines = max_output_lines;
          all_tests.push_back(config);
        }
      }
    }
  }
  {
    std::vector<std::pair<std::string, std::string>> testfiles({
        {"jxl/flower/flower.png.im_q85_422.jpg", "Q85YUV422"},
        {"jxl/flower/flower.png.im_q85_440.jpg", "Q85YUV440"},
        {"jxl/flower/flower.png.im_q85_444_1x2.jpg", "Q85YUV444_1x2"},
        {"jxl/flower/flower.png.im_q85_asymmetric.jpg", "Q85Asymmetric"},
        {"jxl/flower/flower.png.im_q85_gray.jpg", "Q85Gray"},
        {"jxl/flower/flower.png.im_q85_luma_subsample.jpg", "Q85LumaSubsample"},
        {"jxl/flower/flower.png.im_q85_rgb.jpg", "Q85RGB"},
        {"jxl/flower/flower.png.im_q85_rgb_subsample_blue.jpg",
         "Q85RGBSubsampleBlue"},
    });
    for (const auto& it : testfiles) {
      for (size_t chunk_size : {0, 64}) {
        for (size_t max_output_lines : {0, 16}) {
          TestConfig config;
          config.fn = it.first;
          config.fn_desc = it.second;
          config.chunk_size = chunk_size;
          config.max_output_lines = max_output_lines;
          all_tests.push_back(config);
        }
      }
    }
  }
  return all_tests;
}

std::ostream& operator<<(std::ostream& os, const TestConfig& c) {
  os << c.fn_desc;
  if (c.chunk_size == 0) {
    os << "CompleteInput";
  } else {
    os << "InputChunks" << c.chunk_size;
  }
  if (c.max_output_lines == 0) {
    os << "CompleteOutput";
  } else {
    os << "OutputLines" << c.max_output_lines;
  }
  return os;
}

std::string TestDescription(
    const testing::TestParamInfo<DecodeJpegTestParam::ParamType>& info) {
  std::stringstream name;
  name << info.param;
  return name.str();
}

JXL_GTEST_INSTANTIATE_TEST_SUITE_P(DecodeJpegTest, DecodeJpegTestParam,
                                   testing::ValuesIn(GenerateTests()),
                                   TestDescription);

}  // namespace
}  // namespace extras
}  // namespace jxl
