// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <algorithm>
#include <cmath>
#include <vector>

#include "lib/jpegli/encode.h"
#include "lib/jpegli/test_utils.h"
#include "lib/jpegli/testing.h"
#include "lib/jxl/sanitizers.h"

namespace jpegli {
namespace {

struct TestConfig {
  TestImage input;
  CompressParams jparams;
  JpegIOMode input_mode = PIXELS;
  double max_bpp;
  double max_dist;
};

class EncodeAPITestParam : public ::testing::TestWithParam<TestConfig> {};

TEST_P(EncodeAPITestParam, TestAPI) {
  TestConfig config = GetParam();
  GeneratePixels(&config.input);
  if (config.input_mode == RAW_DATA) {
    GenerateRawData(config.jparams, &config.input);
  } else if (config.input_mode == COEFFICIENTS) {
    GenerateCoeffs(config.jparams, &config.input);
  }
  std::vector<uint8_t> compressed;
  ASSERT_TRUE(EncodeWithJpegli(config.input, config.jparams, &compressed));
  double bpp =
      compressed.size() * 8.0 / (config.input.xsize * config.input.ysize);
  printf("bpp: %f\n", bpp);
  EXPECT_LT(bpp, config.max_bpp);
  TestImage output;
  output.color_space = config.input.color_space;
  DecodeWithLibjpeg(config.jparams, compressed, config.input_mode, &output);
  VerifyOutputImage(config.input, output, config.max_dist);
}

std::vector<TestConfig> GenerateTests() {
  std::vector<TestConfig> all_tests;
  {
    TestConfig config;
    config.max_bpp = 1.45;
    config.max_dist = 2.2;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.jparams.quality = 100;
    config.max_bpp = 5.9;
    config.max_dist = 0.6;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.jparams.quality = 80;
    config.max_bpp = 0.95;
    config.max_dist = 2.9;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.jparams.h_sampling = {2, 1, 1};
    config.jparams.v_sampling = {2, 1, 1};
    config.max_bpp = 1.25;
    config.max_dist = 2.9;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.jparams.h_sampling = {1, 1, 1};
    config.jparams.v_sampling = {2, 1, 1};
    config.max_bpp = 1.35;
    config.max_dist = 2.5;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.jparams.h_sampling = {2, 1, 1};
    config.jparams.v_sampling = {1, 1, 1};
    config.max_bpp = 1.35;
    config.max_dist = 2.5;
    all_tests.push_back(config);
  }
  for (int h0_samp : {1, 2, 4}) {
    for (int v0_samp : {1, 2, 4}) {
      for (int h2_samp : {1, 2, 4}) {
        for (int v2_samp : {1, 2, 4}) {
          TestConfig config;
          config.input.xsize = 137;
          config.input.ysize = 75;
          config.jparams.h_sampling = {h0_samp, 1, h2_samp};
          config.jparams.v_sampling = {v0_samp, 1, v2_samp};
          config.max_bpp = 2.5;
          config.max_dist = 12.0;
          all_tests.push_back(config);
        }
      }
    }
  }
  for (int h0_samp : {1, 3}) {
    for (int v0_samp : {1, 3}) {
      for (int h2_samp : {1, 3}) {
        for (int v2_samp : {1, 3}) {
          TestConfig config;
          config.input.xsize = 205;
          config.input.ysize = 99;
          config.jparams.h_sampling = {h0_samp, 1, h2_samp};
          config.jparams.v_sampling = {v0_samp, 1, v2_samp};
          config.max_bpp = 2.5;
          config.max_dist = 10.0;
          all_tests.push_back(config);
        }
      }
    }
  }
  for (int h0_samp : {1, 2, 3, 4}) {
    for (int v0_samp : {1, 2, 3, 4}) {
      TestConfig config;
      config.input.xsize = 217;
      config.input.ysize = 129;
      config.jparams.h_sampling = {h0_samp, 1, 1};
      config.jparams.v_sampling = {v0_samp, 1, 1};
      config.max_bpp = 2.0;
      config.max_dist = 5.5;
      all_tests.push_back(config);
    }
  }
  for (int p = 0; p < kNumTestScripts; ++p) {
    TestConfig config;
    config.jparams.progressive_id = p + 1;
    config.max_bpp = 1.5;
    config.max_dist = 2.2;
    all_tests.push_back(config);
  }
  for (size_t l = 0; l <= 2; ++l) {
    TestConfig config;
    config.jparams.progressive_level = l;
    config.max_bpp = 1.5;
    config.max_dist = 2.2;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.jparams.xyb_mode = true;
    config.max_bpp = 1.5;
    config.max_dist = 3.5;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.jparams.libjpeg_mode = true;
    config.max_bpp = 2.1;
    config.max_dist = 1.7;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.input.color_space = JCS_YCbCr;
    config.max_bpp = 1.45;
    config.max_dist = 1.35;
    all_tests.push_back(config);
  }
  for (bool xyb : {false, true}) {
    TestConfig config;
    config.input.color_space = JCS_GRAYSCALE;
    config.jparams.xyb_mode = xyb;
    config.max_bpp = 1.25;
    config.max_dist = 1.4;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.input.color_space = JCS_RGB;
    config.jparams.set_jpeg_colorspace = true;
    config.jparams.jpeg_color_space = JCS_RGB;
    config.jparams.xyb_mode = false;
    config.max_bpp = 3.75;
    config.max_dist = 1.4;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.input.color_space = JCS_CMYK;
    config.max_bpp = 3.75;
    config.max_dist = 1.4;
    all_tests.push_back(config);
    config.jparams.set_jpeg_colorspace = true;
    config.jparams.jpeg_color_space = JCS_YCCK;
    config.max_bpp = 3.2;
    config.max_dist = 1.7;
    all_tests.push_back(config);
  }
  for (int channels = 1; channels <= 4; ++channels) {
    TestConfig config;
    config.input.color_space = JCS_UNKNOWN;
    config.input.components = channels;
    config.max_bpp = 1.25 * channels;
    config.max_dist = 1.4;
    all_tests.push_back(config);
  }
  for (size_t r : {1, 3, 17, 1024}) {
    TestConfig config;
    config.jparams.restart_interval = r;
    config.max_bpp = 1.5 + 5.5 / r;
    config.max_dist = 2.2;
    all_tests.push_back(config);
  }
  for (size_t rr : {1, 3, 8, 100}) {
    TestConfig config;
    config.jparams.restart_in_rows = rr;
    config.max_bpp = 1.5;
    config.max_dist = 2.2;
    all_tests.push_back(config);
  }
  for (int type : {0, 1, 10, 100, 10000}) {
    for (int scale : {1, 50, 100, 200, 500}) {
      for (bool add_raw : {false, true}) {
        for (bool baseline : {true, false}) {
          if (!baseline && (add_raw || type * scale < 25500)) continue;
          TestConfig config;
          config.input.xsize = 64;
          config.input.ysize = 64;
          CustomQuantTable table;
          table.table_type = type;
          table.scale_factor = scale;
          table.force_baseline = baseline;
          table.add_raw = add_raw;
          table.Generate();
          config.jparams.quant_tables.push_back(table);
          config.jparams.quant_indexes = {0, 0, 0};
          float q = (type == 0 ? 16 : type) * scale * 0.01f;
          if (baseline && !add_raw) q = std::max(1.0f, std::min(255.0f, q));
          config.max_bpp = 1.3f + 25.0f / q;
          config.max_dist = 0.6f + 0.25f * q;
          all_tests.push_back(config);
        }
      }
    }
  }
  for (int qidx = 0; qidx < 8; ++qidx) {
    if (qidx == 3) continue;
    TestConfig config;
    config.input.xsize = 256;
    config.input.ysize = 256;
    config.jparams.quant_indexes = {(qidx >> 2) & 1, (qidx >> 1) & 1,
                                    (qidx >> 0) & 1};
    config.max_bpp = 2.6;
    config.max_dist = 2.5;
    all_tests.push_back(config);
  }
  for (int qidx = 0; qidx < 8; ++qidx) {
    for (int slot_idx = 0; slot_idx < 2; ++slot_idx) {
      if (qidx == 0 && slot_idx == 0) continue;
      TestConfig config;
      config.input.xsize = 256;
      config.input.ysize = 256;
      config.jparams.quant_indexes = {(qidx >> 2) & 1, (qidx >> 1) & 1,
                                      (qidx >> 0) & 1};
      CustomQuantTable table;
      table.slot_idx = slot_idx;
      table.Generate();
      config.jparams.quant_tables.push_back(table);
      config.max_bpp = 2.6;
      config.max_dist = 2.75;
      all_tests.push_back(config);
    }
  }
  for (int qidx = 0; qidx < 8; ++qidx) {
    for (bool xyb : {false, true}) {
      TestConfig config;
      config.input.xsize = 256;
      config.input.ysize = 256;
      config.jparams.xyb_mode = xyb;
      config.jparams.quant_indexes = {(qidx >> 2) & 1, (qidx >> 1) & 1,
                                      (qidx >> 0) & 1};
      {
        CustomQuantTable table;
        table.slot_idx = 0;
        table.Generate();
        config.jparams.quant_tables.push_back(table);
      }
      {
        CustomQuantTable table;
        table.slot_idx = 1;
        table.table_type = 20;
        table.Generate();
        config.jparams.quant_tables.push_back(table);
      }
      config.max_bpp = 1.9;
      config.max_dist = 3.75;
      all_tests.push_back(config);
    }
  }
  for (bool xyb : {false, true}) {
    TestConfig config;
    config.input.xsize = 256;
    config.input.ysize = 256;
    config.jparams.xyb_mode = xyb;
    config.jparams.quant_indexes = {0, 1, 2};
    {
      CustomQuantTable table;
      table.slot_idx = 0;
      table.Generate();
      config.jparams.quant_tables.push_back(table);
    }
    {
      CustomQuantTable table;
      table.slot_idx = 1;
      table.table_type = 20;
      table.Generate();
      config.jparams.quant_tables.push_back(table);
    }
    {
      CustomQuantTable table;
      table.slot_idx = 2;
      table.table_type = 30;
      table.Generate();
      config.jparams.quant_tables.push_back(table);
    }
    config.max_bpp = 1.5;
    config.max_dist = 3.75;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.jparams.comp_ids = {7, 17, 177};
    config.max_bpp = 1.45;
    config.max_dist = 2.2;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.max_bpp = 1.45;
    config.max_dist = 2.2;
    config.jparams.override_JFIF = 1;
    all_tests.push_back(config);
    config.jparams.override_JFIF = 0;
    config.jparams.override_Adobe = 1;
    all_tests.push_back(config);
    config.jparams.override_JFIF = 1;
    config.jparams.override_Adobe = 1;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.input.xsize = config.input.ysize = 256;
    config.max_bpp = 1.7;
    config.max_dist = 2.3;
    config.jparams.add_marker = true;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.input.xsize = config.input.ysize = 256;
    config.jparams.progressive_level = 0;
    config.jparams.optimize_coding = false;
    config.max_bpp = 1.8;
    config.max_dist = 2.3;
    all_tests.push_back(config);
    config.jparams.use_flat_dc_luma_code = true;
    all_tests.push_back(config);
  }
  for (int xsize : {640, 641, 648, 649}) {
    for (int ysize : {640, 641, 648, 649}) {
      for (int h_sampling : {1, 2}) {
        for (int v_sampling : {1, 2}) {
          if (h_sampling == 1 && v_sampling == 1) continue;
          TestConfig config;
          config.input.xsize = xsize;
          config.input.ysize = ysize;
          config.input.color_space = JCS_YCbCr;
          config.jparams.h_sampling = {h_sampling, 1, 1};
          config.jparams.v_sampling = {v_sampling, 1, 1};
          config.input_mode = RAW_DATA;
          config.max_bpp = 1.7;
          config.max_dist = 2.0;
          all_tests.push_back(config);
          config.input_mode = COEFFICIENTS;
          if (xsize & 1) {
            config.jparams.add_marker = true;
          }
          config.max_bpp = 24.0;
          all_tests.push_back(config);
        }
      }
    }
  }
  return all_tests;
};

std::string ColorSpaceName(J_COLOR_SPACE colorspace) {
  switch (colorspace) {
    case JCS_UNKNOWN:
      return "UNKNOWN";
    case JCS_GRAYSCALE:
      return "GRAYSCALE";
    case JCS_RGB:
      return "RGB";
    case JCS_YCbCr:
      return "YCbCr";
    case JCS_CMYK:
      return "CMYK";
    default:
      return "";
  }
}

std::ostream& operator<<(std::ostream& os, const TestConfig& c) {
  os << c.input.xsize << "x" << c.input.ysize;
  os << ColorSpaceName(c.input.color_space);
  if (c.input.color_space == JCS_UNKNOWN) {
    os << c.input.components;
  }
  if (c.jparams.set_jpeg_colorspace) {
    os << "to" << ColorSpaceName(c.jparams.jpeg_color_space);
  }
  os << "Q" << c.jparams.quality;
  if (!c.jparams.h_sampling.empty()) {
    os << "SAMP";
    for (size_t i = 0; i < c.input.components; ++i) {
      os << "_";
      os << c.jparams.h_sampling[i] << "x" << c.jparams.v_sampling[i];
    }
  }
  if (!c.jparams.comp_ids.empty()) {
    os << "CID";
    for (size_t i = 0; i < c.input.components; ++i) {
      os << c.jparams.comp_ids[i];
    }
  }
  if (!c.jparams.quant_indexes.empty()) {
    os << "QIDX";
    for (size_t i = 0; i < c.input.components; ++i) {
      os << c.jparams.quant_indexes[i];
    }
    for (const auto& table : c.jparams.quant_tables) {
      os << "TABLE" << table.slot_idx << "T" << table.table_type << "F"
         << table.scale_factor
         << (table.add_raw          ? "R"
             : table.force_baseline ? "B"
                                    : "");
    }
  }
  if (c.jparams.progressive_id > 0) {
    os << "P" << c.jparams.progressive_id;
  }
  if (c.jparams.restart_interval > 0) {
    os << "R" << c.jparams.restart_interval;
  }
  if (c.jparams.restart_in_rows > 0) {
    os << "RR" << c.jparams.restart_in_rows;
  }
  if (c.jparams.progressive_level >= 0) {
    os << "PL" << c.jparams.progressive_level;
  }
  if (c.jparams.xyb_mode) {
    os << "XYB";
  } else if (c.jparams.libjpeg_mode) {
    os << "Libjpeg";
  }
  if (c.jparams.override_JFIF >= 0) {
    os << (c.jparams.override_JFIF ? "AddJFIF" : "NoJFIF");
  }
  if (c.jparams.override_Adobe >= 0) {
    os << (c.jparams.override_JFIF ? "AddAdobe" : "NoAdobe");
  }
  if (c.jparams.add_marker) {
    os << "AddMarker";
  }
  if (!c.jparams.optimize_coding) {
    os << "FixedTree";
    if (c.jparams.use_flat_dc_luma_code) {
      os << "FlatDCLuma";
    }
  }
  if (c.input_mode == RAW_DATA) {
    os << "RawDataIn";
  } else if (c.input_mode == COEFFICIENTS) {
    os << "WriteCoeffs";
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

TEST(EncodeAPITest, AbbreviatedStreams) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(FAIL());
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.table_stream, &data.table_stream_size);
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;
  jpegli_set_defaults(&cinfo);
  jpegli_write_tables(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.optimize_coding = false;
  jpegli_set_progressive_level(&cinfo, 0);
  jpegli_start_compress(&cinfo, FALSE);
  JSAMPLE image[3] = {0};
  JSAMPROW row[] = {image};
  jpegli_write_scanlines(&cinfo, row, 1);
  jpegli_finish_compress(&cinfo);
  EXPECT_LT(data.size, 50);
  jpegli_destroy_compress(&cinfo);

  jpeg_decompress_struct dinfo = {};
  dinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&dinfo);
  jpeg_mem_src(&dinfo, data.table_stream, data.table_stream_size);
  jpeg_read_header(&dinfo, FALSE);
  jpeg_mem_src(&dinfo, data.buffer, data.size);
  jpeg_read_header(&dinfo, TRUE);
  EXPECT_EQ(1, dinfo.image_width);
  EXPECT_EQ(1, dinfo.image_height);
  EXPECT_EQ(3, dinfo.num_components);
  jpeg_start_decompress(&dinfo);
  jpeg_read_scanlines(&dinfo, row, 1);
  jxl::msan::UnpoisonMemory(image, 3);
  EXPECT_EQ(0, image[0]);
  EXPECT_EQ(0, image[1]);
  EXPECT_EQ(0, image[2]);
  jpeg_finish_decompress(&dinfo);
  jpeg_destroy_decompress(&dinfo);
  free(data.buffer);
  free(data.table_stream);
}

#define EXPECT_FAILURE()              \
  if (data.buffer) free(data.buffer); \
  jpegli_destroy_compress(&cinfo);    \
  FAIL();

TEST(ErrorHandlingTest, MinimalSuccess) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(FAIL());
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 1;
  jpegli_set_defaults(&cinfo);
  jpegli_start_compress(&cinfo, TRUE);
  JSAMPLE image[1] = {0};
  JSAMPROW row[] = {image};
  jpegli_write_scanlines(&cinfo, row, 1);
  jpegli_finish_compress(&cinfo);
  jpegli_destroy_compress(&cinfo);

  jpeg_decompress_struct dinfo = {};
  dinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&dinfo);
  jpeg_mem_src(&dinfo, data.buffer, data.size);
  jpeg_read_header(&dinfo, TRUE);
  EXPECT_EQ(1, dinfo.image_width);
  EXPECT_EQ(1, dinfo.image_height);
  jpeg_start_decompress(&dinfo);
  jpeg_read_scanlines(&dinfo, row, 1);
  jxl::msan::UnpoisonMemory(image, 1);
  EXPECT_EQ(0, image[0]);
  jpeg_finish_decompress(&dinfo);
  jpeg_destroy_decompress(&dinfo);
  free(data.buffer);
}

TEST(ErrorHandlingTest, NoDestination) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 1;
  jpegli_set_defaults(&cinfo);
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, NoImageDimensions) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.input_components = 1;
  jpegli_set_defaults(&cinfo);
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, ImageTooBig) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 100000;
  cinfo.image_height = 1;
  cinfo.input_components = 1;
  jpegli_set_defaults(&cinfo);
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, NoInputComponents) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  jpegli_set_defaults(&cinfo);
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, TooManyInputComponents) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 1000;
  jpegli_set_defaults(&cinfo);
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, NoSetDefaults) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 1;
  jpegli_start_compress(&cinfo, TRUE);
  JSAMPLE image[1] = {0};
  JSAMPROW row[] = {image};
  jpegli_write_scanlines(&cinfo, row, 1);
  jpegli_finish_compress(&cinfo);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, NoStartCompress) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 1;
  jpegli_set_defaults(&cinfo);
  JSAMPLE image[1] = {0};
  JSAMPROW row[] = {image};
  jpegli_write_scanlines(&cinfo, row, 1);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, NoWriteScanlines) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 1;
  jpegli_set_defaults(&cinfo);
  jpegli_start_compress(&cinfo, TRUE);
  jpegli_finish_compress(&cinfo);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, NoWriteAllScanlines) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 2;
  cinfo.input_components = 1;
  jpegli_set_defaults(&cinfo);
  jpegli_start_compress(&cinfo, TRUE);
  JSAMPLE image[1] = {0};
  JSAMPROW row[] = {image};
  jpegli_write_scanlines(&cinfo, row, 1);
  jpegli_finish_compress(&cinfo);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, InvalidQuantValue) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 1;
  jpegli_set_defaults(&cinfo);
  cinfo.quant_tbl_ptrs[0] = jpegli_alloc_quant_table((j_common_ptr)&cinfo);
  for (size_t k = 0; k < DCTSIZE2; ++k) {
    cinfo.quant_tbl_ptrs[0]->quantval[k] = 0;
  }
  jpegli_start_compress(&cinfo, TRUE);
  JSAMPLE image[1] = {0};
  JSAMPROW row[] = {image};
  jpegli_write_scanlines(&cinfo, row, 1);
  jpegli_finish_compress(&cinfo);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, InvalidQuantTableIndex) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 1;
  jpegli_set_defaults(&cinfo);
  cinfo.comp_info[0].quant_tbl_no = 3;
  jpegli_start_compress(&cinfo, TRUE);
  JSAMPLE image[1] = {0};
  JSAMPROW row[] = {image};
  jpegli_write_scanlines(&cinfo, row, 1);
  jpegli_finish_compress(&cinfo);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, NumberOfComponentsMismatch1) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 1;
  jpegli_set_defaults(&cinfo);
  cinfo.num_components = 100;
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, NumberOfComponentsMismatch2) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 1;
  jpegli_set_defaults(&cinfo);
  cinfo.num_components = 2;
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, NumberOfComponentsMismatch3) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 1;
  jpegli_set_defaults(&cinfo);
  cinfo.num_components = 2;
  cinfo.comp_info[1].h_samp_factor = cinfo.comp_info[1].v_samp_factor = 1;
  jpegli_start_compress(&cinfo, TRUE);
  JSAMPLE image[1] = {0};
  JSAMPROW row[] = {image};
  jpegli_write_scanlines(&cinfo, row, 1);
  jpegli_finish_compress(&cinfo);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, NumberOfComponentsMismatch4) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 1;
  cinfo.in_color_space = JCS_RGB;
  jpegli_set_defaults(&cinfo);
  jpegli_start_compress(&cinfo, TRUE);
  JSAMPLE image[1] = {0};
  JSAMPROW row[] = {image};
  jpegli_write_scanlines(&cinfo, row, 1);
  jpegli_finish_compress(&cinfo);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, NumberOfComponentsMismatch5) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_GRAYSCALE;
  jpegli_set_defaults(&cinfo);
  jpegli_start_compress(&cinfo, TRUE);
  JSAMPLE image[3] = {0};
  JSAMPROW row[] = {image};
  jpegli_write_scanlines(&cinfo, row, 1);
  jpegli_finish_compress(&cinfo);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, NumberOfComponentsMismatch6) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;
  jpegli_set_defaults(&cinfo);
  cinfo.num_components = 2;
  jpegli_start_compress(&cinfo, TRUE);
  JSAMPLE image[3] = {0};
  JSAMPROW row[] = {image};
  jpegli_write_scanlines(&cinfo, row, 1);
  jpegli_finish_compress(&cinfo);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, InvalidColorTransform) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_YCbCr;
  jpegli_set_defaults(&cinfo);
  cinfo.jpeg_color_space = JCS_RGB;
  jpegli_start_compress(&cinfo, TRUE);
  JSAMPLE image[3] = {0};
  JSAMPROW row[] = {image};
  jpegli_write_scanlines(&cinfo, row, 1);
  jpegli_finish_compress(&cinfo);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, DuplicateComponentIds) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 3;
  jpegli_set_defaults(&cinfo);
  cinfo.comp_info[0].component_id = 0;
  cinfo.comp_info[1].component_id = 0;
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, InvalidComponentIndex) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 3;
  jpegli_set_defaults(&cinfo);
  cinfo.comp_info[0].component_index = 17;
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, ArithmeticCoding) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 3;
  jpegli_set_defaults(&cinfo);
  cinfo.arith_code = TRUE;
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, CCIR601Sampling) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 3;
  jpegli_set_defaults(&cinfo);
  cinfo.CCIR601_sampling = TRUE;
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, InvalidScanScript1) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 1;
  jpegli_set_defaults(&cinfo);
  static constexpr jpeg_scan_info kScript[] = {{1, {0}, 0, 63, 0, 0}};  //
  cinfo.scan_info = kScript;
  cinfo.num_scans = 0;
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, InvalidScanScript2) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 1;
  jpegli_set_defaults(&cinfo);
  static constexpr jpeg_scan_info kScript[] = {{2, {0, 1}, 0, 63, 0, 0}};  //
  cinfo.scan_info = kScript;
  cinfo.num_scans = ARRAY_SIZE(kScript);
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, InvalidScanScript3) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 1;
  jpegli_set_defaults(&cinfo);
  static constexpr jpeg_scan_info kScript[] = {{5, {0}, 0, 63, 0, 0}};  //
  cinfo.scan_info = kScript;
  cinfo.num_scans = ARRAY_SIZE(kScript);
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, InvalidScanScript4) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 2;
  jpegli_set_defaults(&cinfo);
  static constexpr jpeg_scan_info kScript[] = {{2, {0, 0}, 0, 63, 0, 0}};  //
  cinfo.scan_info = kScript;
  cinfo.num_scans = ARRAY_SIZE(kScript);
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, InvalidScanScript5) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 2;
  jpegli_set_defaults(&cinfo);
  static constexpr jpeg_scan_info kScript[] = {{2, {1, 0}, 0, 63, 0, 0}};  //
  cinfo.scan_info = kScript;
  cinfo.num_scans = ARRAY_SIZE(kScript);
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, InvalidScanScript6) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 1;
  jpegli_set_defaults(&cinfo);
  static constexpr jpeg_scan_info kScript[] = {{1, {0}, 0, 64, 0, 0}};  //
  cinfo.scan_info = kScript;
  cinfo.num_scans = ARRAY_SIZE(kScript);
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, InvalidScanScript7) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 1;
  jpegli_set_defaults(&cinfo);
  static constexpr jpeg_scan_info kScript[] = {{1, {0}, 2, 1, 0, 0}};  //
  cinfo.scan_info = kScript;
  cinfo.num_scans = ARRAY_SIZE(kScript);
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, InvalidScanScript8) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 2;
  jpegli_set_defaults(&cinfo);
  static constexpr jpeg_scan_info kScript[] = {
      {1, {0}, 0, 63, 0, 0}, {1, {1}, 0, 0, 0, 0}, {1, {1}, 1, 63, 0, 0}  //
  };
  cinfo.scan_info = kScript;
  cinfo.num_scans = ARRAY_SIZE(kScript);
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, InvalidScanScript9) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 1;
  jpegli_set_defaults(&cinfo);
  static constexpr jpeg_scan_info kScript[] = {
      {1, {0}, 0, 1, 0, 0}, {1, {0}, 2, 63, 0, 0},  //
  };
  cinfo.scan_info = kScript;
  cinfo.num_scans = ARRAY_SIZE(kScript);
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, InvalidScanScript10) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 2;
  jpegli_set_defaults(&cinfo);
  static constexpr jpeg_scan_info kScript[] = {
      {2, {0, 1}, 0, 0, 0, 0}, {2, {0, 1}, 1, 63, 0, 0}  //
  };
  cinfo.scan_info = kScript;
  cinfo.num_scans = ARRAY_SIZE(kScript);
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, InvalidScanScript11) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 1;
  jpegli_set_defaults(&cinfo);
  static constexpr jpeg_scan_info kScript[] = {
      {1, {0}, 1, 63, 0, 0}, {1, {0}, 0, 0, 0, 0}  //
  };
  cinfo.scan_info = kScript;
  cinfo.num_scans = ARRAY_SIZE(kScript);
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, InvalidScanScript12) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 1;
  jpegli_set_defaults(&cinfo);
  static constexpr jpeg_scan_info kScript[] = {
      {1, {0}, 0, 0, 10, 1}, {1, {0}, 0, 0, 1, 0}, {1, {0}, 1, 63, 0, 0}  //
  };
  cinfo.scan_info = kScript;
  cinfo.num_scans = ARRAY_SIZE(kScript);
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, InvalidScanScript13) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 1;
  jpegli_set_defaults(&cinfo);
  static constexpr jpeg_scan_info kScript[] = {
      {1, {0}, 0, 0, 0, 2},
      {1, {0}, 0, 0, 1, 0},
      {1, {0}, 0, 0, 2, 1},  //
      {1, {0}, 1, 63, 0, 0}  //
  };
  cinfo.scan_info = kScript;
  cinfo.num_scans = ARRAY_SIZE(kScript);
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, RestartIntervalTooBig) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 1;
  jpegli_set_defaults(&cinfo);
  cinfo.restart_interval = 1000000;
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, SamplingFactorTooBig) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 3;
  jpegli_set_defaults(&cinfo);
  cinfo.comp_info[0].h_samp_factor = 5;
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

TEST(ErrorHandlingTest, NonIntegralSamplingRatio) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  ERROR_HANDLER_SETUP(return );
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = 1;
  cinfo.image_height = 1;
  cinfo.input_components = 3;
  jpegli_set_defaults(&cinfo);
  cinfo.comp_info[0].h_samp_factor = 3;
  cinfo.comp_info[1].h_samp_factor = 2;
  jpegli_start_compress(&cinfo, TRUE);
  EXPECT_FAILURE();
}

}  // namespace
}  // namespace jpegli
