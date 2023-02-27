// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <vector>

#include "lib/jpegli/encode.h"
#include "lib/jpegli/decode.h"
#include "lib/jpegli/test_utils.h"
#include "lib/jpegli/testing.h"

namespace jpegli {
namespace {

void TranscodeWithJpegli(const std::vector<uint8_t>& jpeg_input,
                         std::vector<uint8_t>* jpeg_output) {
  jpeg_decompress_struct dinfo = {};
  jpeg_compress_struct cinfo = {};
  uint8_t* transcoded_data = nullptr;
  unsigned long transcoded_size;
  const auto try_catch_block = [&]() {
    jpeg_error_mgr jerr;
    jmp_buf env;
    dinfo.err = jpegli_std_error(&jerr);
    if (setjmp(env)) {
      FAIL();
    }
    dinfo.client_data = reinterpret_cast<void*>(&env);
    dinfo.err->error_exit = [](j_common_ptr cinfo) {
      (*cinfo->err->output_message)(cinfo);
      jmp_buf* env = reinterpret_cast<jmp_buf*>(cinfo->client_data);
      longjmp(*env, 1);
    };
    jpegli_create_decompress(&dinfo);
    jpegli_mem_src(&dinfo, jpeg_input.data(), jpeg_input.size());
    EXPECT_EQ(JPEG_REACHED_SOS,
              jpegli_read_header(&dinfo, /*require_image=*/TRUE));
    jvirt_barray_ptr* coef_arrays = jpegli_read_coefficients(&dinfo);
    ASSERT_TRUE(coef_arrays != nullptr);
    cinfo.err = dinfo.err;
    cinfo.client_data = dinfo.client_data;
    jpegli_create_compress(&cinfo);
    jpegli_mem_dest(&cinfo, &transcoded_data, &transcoded_size);
    jpegli_copy_critical_parameters(&dinfo, &cinfo);
    jpegli_write_coefficients(&cinfo, coef_arrays);
    jpegli_finish_compress(&cinfo);
    jpegli_finish_decompress(&dinfo);
  };
  try_catch_block();
  jpegli_destroy_decompress(&dinfo);
  jpegli_destroy_compress(&cinfo);
  if (transcoded_data) {
    jpeg_output->assign(transcoded_data, transcoded_data + transcoded_size);
    free(transcoded_data);
  }
}

struct TestConfig {
  TestImage input;
  CompressParams jparams;
};

class TranscodeAPITestParam : public ::testing::TestWithParam<TestConfig> {};

TEST_P(TranscodeAPITestParam, TestAPI) {
  TestConfig config = GetParam();
  GeneratePixels(&config.input);

  // Start with sequential non-optimized jpeg.
  config.jparams.progressive_level = 0;
  config.jparams.optimize_coding = FALSE;
  std::vector<uint8_t> compressed;
  ASSERT_TRUE(EncodeWithJpegli(config.input, config.jparams, &compressed));

  // Transcode with default settings, this will create a progressive jpeg with
  // optimized Huffman codes.
  std::vector<uint8_t> transcoded;
  TranscodeWithJpegli(compressed, &transcoded);

  // We expect a size reduction of at least 5%.
  EXPECT_LT(transcoded.size(), compressed.size() * 0.95f);

  // Verify that transcoding is lossless.
  TestImage output0, output1;
  DecodeWithLibjpeg(CompressParams(), compressed, PIXELS, &output0);
  DecodeWithLibjpeg(CompressParams(), transcoded, PIXELS, &output1);
  VerifyOutputImage(output0, output1, 0.0);
}

std::vector<TestConfig> GenerateTests() {
  std::vector<TestConfig> all_tests;
  const size_t xsize0 = 1024;
  const size_t ysize0 = 768;
  for (int dxsize : {0, 1, 8, 9}) {
    for (int dysize : {0, 1, 8, 9}) {
      for (int h_sampling : {1, 2}) {
        for (int v_sampling : {1, 2}) {
          TestConfig config;
          config.input.xsize = xsize0 + dxsize;
          config.input.ysize = ysize0 + dysize;
          config.jparams.h_sampling = {h_sampling, 1, 1};
          config.jparams.v_sampling = {v_sampling, 1, 1};
          all_tests.push_back(config);
        }
      }
    }
  }
  return all_tests;
}

std::ostream& operator<<(std::ostream& os, const TestConfig& c) {
  os << c.input.xsize << "x" << c.input.ysize;
  if (!c.jparams.h_sampling.empty()) {
    os << "SAMP";
    for (size_t i = 0; i < c.input.components; ++i) {
      os << "_";
      os << c.jparams.h_sampling[i] << "x" << c.jparams.v_sampling[i];
    }
  }
  return os;
}

std::string TestDescription(
    const testing::TestParamInfo<TranscodeAPITestParam::ParamType>& info) {
  std::stringstream name;
  name << info.param;
  return name.str();
}

JPEGLI_INSTANTIATE_TEST_SUITE_P(TranscodeAPITest, TranscodeAPITestParam,
                                testing::ValuesIn(GenerateTests()),
                                TestDescription);

}  // namespace
}  // namespace jpegli
