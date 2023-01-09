// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <setjmp.h>
#include <stdio.h>

#include <cmath>
#include <vector>

#include "gtest/gtest.h"
#include "lib/jpegli/decode.h"
#include "lib/jpegli/encode.h"
#include "lib/jpegli/test_utils.h"
#include "lib/jxl/base/file_io.h"
#include "lib/jxl/base/status.h"

namespace jpegli {
namespace {

void TestDecodedImage(const std::vector<uint8_t>& compressed,
                      const std::vector<uint8_t>& orig, size_t xsize,
                      size_t ysize, size_t num_channels, double max_dist) {
  jpeg_decompress_struct cinfo;
  jpeg_error_mgr jerr;
  cinfo.err = jpegli_std_error(&jerr);
  jmp_buf env;
  if (setjmp(env)) {
    FAIL();
  }
  cinfo.client_data = static_cast<void*>(&env);
  cinfo.err->error_exit = [](j_common_ptr cinfo) {
    (*cinfo->err->output_message)(cinfo);
    jmp_buf* env = static_cast<jmp_buf*>(cinfo->client_data);
    longjmp(*env, 1);
  };
  jpegli_create_decompress(&cinfo);
  jpegli_mem_src(&cinfo, compressed.data(), compressed.size());
  EXPECT_EQ(JPEG_REACHED_SOS,
            jpegli_read_header(&cinfo, /*require_image=*/TRUE));
  EXPECT_EQ(xsize, cinfo.image_width);
  EXPECT_EQ(ysize, cinfo.image_height);
  EXPECT_EQ(num_channels, cinfo.num_components);
  EXPECT_TRUE(jpegli_start_decompress(&cinfo));
  size_t stride = xsize * num_channels;
  std::vector<uint8_t> output(ysize * stride);
  std::vector<JSAMPROW> scanlines(ysize);
  for (size_t i = 0; i < ysize; ++i) {
    scanlines[i] = &output[i * stride];
  }
  EXPECT_EQ(ysize, jpegli_read_scanlines(&cinfo, &scanlines[0], ysize));
  EXPECT_TRUE(jpegli_finish_decompress(&cinfo));
  jpegli_destroy_decompress(&cinfo);

  ASSERT_EQ(output.size(), orig.size());
  const double mul = 1.0 / 255.0;
  double diff2 = 0.0;
  for (size_t i = 0; i < orig.size(); ++i) {
    double sample_orig = orig[i] * mul;
    double sample_output = output[i] * mul;
    double diff = sample_orig - sample_output;
    diff2 += diff * diff;
  }
  double rms = std::sqrt(diff2 / orig.size()) / mul;
  EXPECT_LE(rms, max_dist);
}

struct TestConfig {
  int quality;
  double max_dist;
};

class EncodeAPITestParam : public ::testing::TestWithParam<TestConfig> {};

TEST_P(EncodeAPITestParam, TestAPI) {
  TestConfig config = GetParam();
  const std::vector<uint8_t> origdata = ReadTestData("jxl/flower/flower.pnm");
  // These has to be volatile to make setjmp/longjmp work.
  volatile size_t xsize, ysize, num_channels, bitdepth;
  std::vector<uint8_t> orig;
  ASSERT_TRUE(
      ReadPNM(origdata, &xsize, &ysize, &num_channels, &bitdepth, &orig));
  ASSERT_EQ(8, bitdepth);
  jpeg_compress_struct cinfo;
  jpeg_error_mgr jerr;
  cinfo.err = jpegli_std_error(&jerr);
  jmp_buf env;
  if (setjmp(env)) {
    FAIL();
  }
  cinfo.client_data = static_cast<void*>(&env);
  cinfo.err->error_exit = [](j_common_ptr cinfo) {
    (*cinfo->err->output_message)(cinfo);
    jmp_buf* env = static_cast<jmp_buf*>(cinfo->client_data);
    longjmp(*env, 1);
  };
  jpegli_create_compress(&cinfo);
  unsigned char* buffer = nullptr;
  unsigned long size = 0;
  jpegli_mem_dest(&cinfo, &buffer, &size);
  cinfo.image_width = xsize;
  cinfo.image_height = ysize;
  cinfo.input_components = num_channels;
  cinfo.in_color_space = num_channels == 1 ? JCS_GRAYSCALE : JCS_RGB;
  jpegli_set_defaults(&cinfo);
  cinfo.optimize_coding = TRUE;
  jpegli_set_quality(&cinfo, config.quality, TRUE);
  jpegli_start_compress(&cinfo, TRUE);
  size_t stride = xsize * cinfo.input_components;
  for (size_t y = 0; y < ysize; ++y) {
    JSAMPROW row[] = {orig.data() + y * stride};
    jpegli_write_scanlines(&cinfo, row, 1);
  }
  jpegli_finish_compress(&cinfo);
  jpegli_destroy_compress(&cinfo);
  std::vector<uint8_t> compressed;
  compressed.resize(size);
  std::copy_n(buffer, size, compressed.data());
  std::free(buffer);
  TestDecodedImage(compressed, orig, xsize, ysize, num_channels,
                   config.max_dist);
}

std::vector<TestConfig> GenerateTests() {
  std::vector<TestConfig> all_tests;
  for (int quality : {80, 90, 100}) {
    TestConfig config;
    config.quality = quality;
    config.max_dist = quality == 100 ? 0.5 : quality == 90 ? 1.95 : 2.75;
    all_tests.push_back(config);
  }
  return all_tests;
};

std::ostream& operator<<(std::ostream& os, const TestConfig& c) {
  os << "Q" << c.quality;
  return os;
}

std::string TestDescription(
    const testing::TestParamInfo<EncodeAPITestParam::ParamType>& info) {
  std::stringstream name;
  name << info.param;
  return name.str();
}

JPEGLI_INSTANTIATE_TEST_SUITE_P(EncodeAPITest, EncodeAPITestParam,
                                testing::ValuesIn(GenerateTests()),
                                TestDescription);

}  // namespace
}  // namespace jpegli
