// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

/* clang-format off */
#include <stdio.h>
#include <jpeglib.h>
#include <setjmp.h>
/* clang-format on */

#include <cmath>
#include <vector>

#include "gtest/gtest.h"
#include "lib/jpegli/encode.h"
#include "lib/jpegli/test_utils.h"
#include "lib/jxl/base/file_io.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/sanitizers.h"

#define ARRAYSIZE(X) (sizeof(X) / sizeof((X)[0]))

namespace jpegli {
namespace {

static constexpr jpeg_scan_info kScript1[] = {
    {3, {0, 1, 2}, 0, 0, 0, 0},
    {1, {0}, 1, 63, 0, 0},
    {1, {1}, 1, 63, 0, 0},
    {1, {2}, 1, 63, 0, 0},
};
static constexpr jpeg_scan_info kScript2[] = {
    {1, {0}, 0, 0, 0, 0},  {1, {1}, 0, 0, 0, 0},  {1, {2}, 0, 0, 0, 0},
    {1, {0}, 1, 63, 0, 0}, {1, {1}, 1, 63, 0, 0}, {1, {2}, 1, 63, 0, 0},
};
static constexpr jpeg_scan_info kScript3[] = {
    {3, {0, 1, 2}, 0, 0, 0, 0}, {1, {0}, 1, 63, 0, 1}, {1, {1}, 1, 63, 0, 1},
    {1, {2}, 1, 63, 0, 1},      {1, {0}, 1, 63, 1, 0}, {1, {1}, 1, 63, 1, 0},
    {1, {2}, 1, 63, 1, 0},
};

struct ScanScript {
  size_t num_scans;
  const jpeg_scan_info* scans;
};

static constexpr ScanScript kTestScript[] = {
    {ARRAYSIZE(kScript1), kScript1},
    {ARRAYSIZE(kScript2), kScript2},
    {ARRAYSIZE(kScript3), kScript3},
};
static constexpr size_t kNumTestScripts = ARRAYSIZE(kTestScript);

// Verifies that an image encoded with libjpegli can be decoded with libjpeg.
void TestDecodedImage(const std::vector<uint8_t>& compressed,
                      const std::vector<uint8_t>& orig, size_t xsize,
                      size_t ysize, size_t num_channels, int progressive_id,
                      double max_dist) {
  jpeg_decompress_struct cinfo;
  // cinfo is initialized by libjpeg, which we are not instrumenting with
  // msan, therefore we need to initialize cinfo here.
  jxl::msan::UnpoisonMemory(&cinfo, sizeof(cinfo));
  jpeg_error_mgr jerr;
  cinfo.err = jpeg_std_error(&jerr);
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
  jpeg_create_decompress(&cinfo);
  jpeg_mem_src(&cinfo, compressed.data(), compressed.size());
  EXPECT_EQ(JPEG_REACHED_SOS, jpeg_read_header(&cinfo, /*require_image=*/TRUE));
  EXPECT_EQ(xsize, cinfo.image_width);
  EXPECT_EQ(ysize, cinfo.image_height);
  EXPECT_EQ(num_channels, cinfo.num_components);
  cinfo.buffered_image = TRUE;
  EXPECT_TRUE(jpeg_start_decompress(&cinfo));
  while (!jpeg_input_complete(&cinfo)) {
    EXPECT_GT(cinfo.input_scan_number, 0);
    EXPECT_TRUE(jpeg_start_output(&cinfo, cinfo.input_scan_number));
    if (progressive_id > 0) {
      ASSERT_LE(progressive_id, kNumTestScripts);
      const ScanScript& script = kTestScript[progressive_id - 1];
      ASSERT_LE(cinfo.input_scan_number, script.num_scans);
      const jpeg_scan_info& scan = script.scans[cinfo.input_scan_number - 1];
      ASSERT_EQ(cinfo.comps_in_scan, scan.comps_in_scan);
      for (int i = 0; i < cinfo.comps_in_scan; ++i) {
        EXPECT_EQ(cinfo.cur_comp_info[i]->component_index,
                  scan.component_index[i]);
      }
      EXPECT_EQ(cinfo.Ss, scan.Ss);
      EXPECT_EQ(cinfo.Se, scan.Se);
      EXPECT_EQ(cinfo.Ah, scan.Ah);
      EXPECT_EQ(cinfo.Al, scan.Al);
    }
    EXPECT_TRUE(jpeg_finish_output(&cinfo));
  }
  EXPECT_TRUE(jpeg_start_output(&cinfo, cinfo.input_scan_number));
  size_t stride = xsize * num_channels;
  std::vector<uint8_t> output(ysize * stride);
  for (size_t y = 0; y < cinfo.image_height; ++y) {
    JSAMPROW rows[] = {reinterpret_cast<JSAMPLE*>(&output[y * stride])};
    jxl::msan::UnpoisonMemory(
        rows[0], sizeof(JSAMPLE) * cinfo.output_components * cinfo.image_width);
    EXPECT_EQ(1, jpeg_read_scanlines(&cinfo, rows, 1));
  }
  EXPECT_TRUE(jpeg_finish_output(&cinfo));
  EXPECT_TRUE(jpeg_finish_decompress(&cinfo));
  jpeg_destroy_decompress(&cinfo);

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

enum ChromaSubsampling {
  SAMPLING_444,
  SAMPLING_420,
};

struct TestConfig {
  int quality = 90;
  ChromaSubsampling sampling = SAMPLING_444;
  int progressive_id = 0;
  double max_bpp;
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
  if (config.sampling == SAMPLING_420) {
    cinfo.comp_info[0].h_samp_factor = cinfo.comp_info[0].v_samp_factor = 2;
  }
  if (config.progressive_id > 0) {
    ASSERT_LE(config.progressive_id, kNumTestScripts);
    const ScanScript& script = kTestScript[config.progressive_id - 1];
    cinfo.scan_info = script.scans;
    cinfo.num_scans = script.num_scans;
  }
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
  double bpp = compressed.size() * 8.0 / (xsize * ysize);
  EXPECT_LT(bpp, config.max_bpp);
  TestDecodedImage(compressed, orig, xsize, ysize, num_channels,
                   config.progressive_id, config.max_dist);
}

std::vector<TestConfig> GenerateTests() {
  std::vector<TestConfig> all_tests;
  {
    TestConfig config;
    config.max_bpp = 1.7;
    config.max_dist = 2.0;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.quality = 100;
    config.max_bpp = 4.2;
    config.max_dist = 0.9;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.quality = 80;
    config.max_bpp = 1.0;
    config.max_dist = 2.75;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.sampling = SAMPLING_420;
    config.max_bpp = 1.5;
    config.max_dist = 2.4;
    all_tests.push_back(config);
  }
  {
    for (size_t p = 0; p < kNumTestScripts; ++p) {
      TestConfig config;
      config.progressive_id = p + 1;
      config.max_bpp = 1.75;
      config.max_dist = 2.0;
      all_tests.push_back(config);
    }
  }
  return all_tests;
};

std::ostream& operator<<(std::ostream& os, const TestConfig& c) {
  os << "Q" << c.quality;
  if (c.sampling == SAMPLING_444) {
    os << "YUV444";
  } else if (c.sampling == SAMPLING_420) {
    os << "YUV420";
  }
  if (c.progressive_id > 0) {
    os << "P" << c.progressive_id;
  }
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
