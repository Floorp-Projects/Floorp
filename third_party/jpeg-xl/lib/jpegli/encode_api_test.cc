// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

/* clang-format off */
#include <stdio.h>
#include <jpeglib.h>
#include <setjmp.h>
/* clang-format on */

#include <algorithm>
#include <cmath>
#include <vector>

#include "gtest/gtest.h"
#include "lib/jpegli/common_internal.h"
#include "lib/jpegli/encode.h"
#include "lib/jpegli/test_utils.h"
#include "lib/jxl/base/file_io.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/sanitizers.h"

#define ARRAYSIZE(X) (sizeof(X) / sizeof((X)[0]))

namespace jpegli {
namespace {

static constexpr int kSpecialMarker = 0xe5;
static constexpr uint8_t kMarkerData[] = {0, 1, 255, 0, 17};

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

struct CustomQuantTable {
  int slot_idx = 0;
  uint16_t table_type = 0;
  int scale_factor = 100;
  bool add_raw = false;
  bool force_baseline = true;
  std::vector<unsigned int> basic_table;
  std::vector<unsigned int> quantval;
  void Generate() {
    basic_table.resize(DCTSIZE2);
    quantval.resize(DCTSIZE2);
    switch (table_type) {
      case 0: {
        for (int k = 0; k < DCTSIZE2; ++k) {
          basic_table[k] = k + 1;
        }
        break;
      }
      default:
        for (int k = 0; k < DCTSIZE2; ++k) {
          basic_table[k] = table_type;
        }
    }
    for (int k = 0; k < DCTSIZE2; ++k) {
      quantval[k] = (basic_table[k] * scale_factor + 50U) / 100U;
      quantval[k] = std::max(quantval[k], 1U);
      quantval[k] = std::min(quantval[k], 65535U);
      if (!add_raw) {
        quantval[k] = std::min(quantval[k], force_baseline ? 255U : 32767U);
      }
    }
  }
};

struct TestConfig {
  size_t xsize = 0;
  size_t ysize = 0;
  J_COLOR_SPACE in_color_space = JCS_RGB;
  size_t input_components = 3;
  bool set_jpeg_colorspace = false;
  J_COLOR_SPACE jpeg_color_space = JCS_UNKNOWN;
  std::vector<uint8_t> pixels;
  int quality = 90;
  bool custom_quant_tables = false;
  std::vector<CustomQuantTable> quant_tables;
  int quant_indexes[kMaxComponents] = {0, 1, 1};
  bool custom_sampling = false;
  int h_sampling[kMaxComponents] = {1, 1, 1};
  int v_sampling[kMaxComponents] = {1, 1, 1};
  bool custom_component_ids = false;
  int comp_id[kMaxComponents];
  int override_JFIF = -1;
  int override_Adobe = -1;
  bool add_marker = false;
  int progressive_id = 0;
  int progressive_level = -1;
  int restart_interval = 0;
  int restart_in_rows = 0;
  bool xyb_mode = false;
  bool libjpeg_mode = false;
  double max_bpp;
  double max_dist;
};

void SetNumChannels(J_COLOR_SPACE colorspace, size_t* channels) {
  if (colorspace == JCS_GRAYSCALE) {
    *channels = 1;
  } else if (colorspace == JCS_RGB || colorspace == JCS_YCbCr) {
    *channels = 3;
  } else if (colorspace == JCS_CMYK || colorspace == JCS_YCCK) {
    *channels = 4;
  } else if (colorspace == JCS_UNKNOWN) {
    ASSERT_LE(*channels, jpegli::kMaxComponents);
  } else {
    FAIL();
  }
}

void ConvertPixel(const uint8_t* input_rgb, uint8_t* out,
                  J_COLOR_SPACE colorspace, size_t num_channels) {
  const float kMul = 255.0f;
  const float r = input_rgb[0] / kMul;
  const float g = input_rgb[1] / kMul;
  const float b = input_rgb[2] / kMul;
  if (colorspace == JCS_GRAYSCALE) {
    const float Y = 0.299f * r + 0.587f * g + 0.114f * b;
    out[0] = static_cast<uint8_t>(std::round(Y * kMul));
  } else if (colorspace == JCS_RGB || colorspace == JCS_UNKNOWN) {
    for (size_t c = 0; c < num_channels; ++c) {
      size_t copy_channels = std::min<size_t>(3, num_channels - c);
      memcpy(out + c, input_rgb, copy_channels);
    }
  } else if (colorspace == JCS_YCbCr) {
    float Y = 0.299f * r + 0.587f * g + 0.114f * b;
    float Cb = -0.168736f * r - 0.331264f * g + 0.5f * b + 0.5f;
    float Cr = 0.5f * r - 0.418688f * g - 0.081312f * b + 0.5f;
    out[0] = static_cast<uint8_t>(std::round(Y * kMul));
    out[1] = static_cast<uint8_t>(std::round(Cb * kMul));
    out[2] = static_cast<uint8_t>(std::round(Cr * kMul));
  } else if (colorspace == JCS_CMYK) {
    float K = 1.0f - std::max(r, std::max(g, b));
    float scaleK = 1.0f / (1.0f - K);
    float C = (1.0f - K - r) * scaleK;
    float M = (1.0f - K - g) * scaleK;
    float Y = (1.0f - K - b) * scaleK;
    out[0] = static_cast<uint8_t>(std::round(C * kMul));
    out[1] = static_cast<uint8_t>(std::round(M * kMul));
    out[2] = static_cast<uint8_t>(std::round(Y * kMul));
    out[3] = static_cast<uint8_t>(std::round(K * kMul));
  } else {
    JXL_ABORT("Colorspace %d not supported", colorspace);
  }
}

void GeneratePixels(TestConfig* config) {
  const std::vector<uint8_t> imgdata = ReadTestData("jxl/flower/flower.pnm");
  size_t xsize, ysize, channels, bitdepth;
  std::vector<uint8_t> pixels;
  ASSERT_TRUE(ReadPNM(imgdata, &xsize, &ysize, &channels, &bitdepth, &pixels));
  if (config->xsize == 0) config->xsize = xsize;
  if (config->ysize == 0) config->ysize = ysize;
  ASSERT_LE(config->xsize, xsize);
  ASSERT_LE(config->ysize, ysize);
  ASSERT_EQ(3, channels);
  ASSERT_EQ(8, bitdepth);
  size_t in_bytes_per_pixel = channels;
  size_t in_stride = xsize * in_bytes_per_pixel;
  size_t x0 = (xsize - config->xsize) / 2;
  size_t y0 = (ysize - config->ysize) / 2;
  SetNumChannels(config->in_color_space, &config->input_components);
  size_t out_bytes_per_pixel = config->input_components;
  size_t out_stride = config->xsize * out_bytes_per_pixel;
  config->pixels.resize(config->ysize * out_stride);
  for (size_t iy = 0; iy < config->ysize; ++iy) {
    size_t y = y0 + iy;
    for (size_t ix = 0; ix < config->xsize; ++ix) {
      size_t x = x0 + ix;
      size_t idx_in = y * in_stride + x * in_bytes_per_pixel;
      size_t idx_out = iy * out_stride + ix * out_bytes_per_pixel;
      ConvertPixel(&pixels[idx_in], &config->pixels[idx_out],
                   config->in_color_space, config->input_components);
    }
  }
}

struct MyClientData {
  jmp_buf env;
  unsigned char* buffer = nullptr;
  unsigned long size = 0;
};

// Verifies that an image encoded with libjpegli can be decoded with libjpeg.
void TestDecodedImage(const TestConfig& config,
                      const std::vector<uint8_t>& compressed) {
  MyClientData data;
  jpeg_decompress_struct cinfo = {};
  jpeg_error_mgr jerr;
  cinfo.err = jpeg_std_error(&jerr);
  if (setjmp(data.env)) {
    FAIL();
  }
  cinfo.client_data = static_cast<void*>(&data);
  cinfo.err->error_exit = [](j_common_ptr cinfo) {
    (*cinfo->err->output_message)(cinfo);
    MyClientData* data = static_cast<MyClientData*>(cinfo->client_data);
    jpeg_destroy(cinfo);
    longjmp(data->env, 1);
  };
  jpeg_create_decompress(&cinfo);
  jpeg_mem_src(&cinfo, compressed.data(), compressed.size());
  if (config.add_marker) {
    jpeg_save_markers(&cinfo, kSpecialMarker, 0xffff);
  }
  EXPECT_EQ(JPEG_REACHED_SOS, jpeg_read_header(&cinfo, /*require_image=*/TRUE));
  EXPECT_EQ(config.xsize, cinfo.image_width);
  EXPECT_EQ(config.ysize, cinfo.image_height);
  EXPECT_EQ(config.input_components, cinfo.num_components);
  cinfo.buffered_image = TRUE;
  cinfo.out_color_space = config.in_color_space;
  if (config.override_JFIF >= 0) {
    EXPECT_EQ(cinfo.saw_JFIF_marker, config.override_JFIF);
  }
  if (config.override_Adobe >= 0) {
    EXPECT_EQ(cinfo.saw_Adobe_marker, config.override_Adobe);
  }
  if (config.in_color_space == JCS_UNKNOWN) {
    cinfo.jpeg_color_space = JCS_UNKNOWN;
  }
  if (config.add_marker) {
    bool marker_found = false;
    for (jpeg_saved_marker_ptr marker = cinfo.marker_list; marker != nullptr;
         marker = marker->next) {
      jxl::msan::UnpoisonMemory(marker, sizeof(*marker));
      jxl::msan::UnpoisonMemory(marker->data, marker->data_length);
      if (marker->marker == kSpecialMarker &&
          marker->data_length == sizeof(kMarkerData) &&
          memcmp(marker->data, kMarkerData, sizeof(kMarkerData)) == 0) {
        marker_found = true;
      }
    }
    EXPECT_TRUE(marker_found);
  }
  EXPECT_TRUE(jpeg_start_decompress(&cinfo));
#if !JXL_MEMORY_SANITIZER
  if (config.custom_component_ids) {
    for (int i = 0; i < cinfo.num_components; ++i) {
      EXPECT_EQ(cinfo.comp_info[i].component_id, config.comp_id[i]);
    }
  }
  if (config.custom_sampling) {
    for (int i = 0; i < cinfo.num_components; ++i) {
      EXPECT_EQ(cinfo.comp_info[i].h_samp_factor, config.h_sampling[i]);
      EXPECT_EQ(cinfo.comp_info[i].v_samp_factor, config.v_sampling[i]);
    }
  }
  if (config.custom_quant_tables) {
    for (int i = 0; i < cinfo.num_components; ++i) {
      EXPECT_EQ(cinfo.comp_info[i].quant_tbl_no, config.quant_indexes[i]);
    }
    for (const auto& table : config.quant_tables) {
      JQUANT_TBL* quant_table = cinfo.quant_tbl_ptrs[table.slot_idx];
      ASSERT_TRUE(quant_table != nullptr);
      for (int k = 0; k < DCTSIZE2; ++k) {
        EXPECT_EQ(quant_table->quantval[k], table.quantval[k]);
      }
    }
  }
#endif
  while (!jpeg_input_complete(&cinfo)) {
    EXPECT_GT(cinfo.input_scan_number, 0);
    EXPECT_TRUE(jpeg_start_output(&cinfo, cinfo.input_scan_number));
    if (config.progressive_id > 0) {
      ASSERT_LE(config.progressive_id, kNumTestScripts);
      const ScanScript& script = kTestScript[config.progressive_id - 1];
      ASSERT_LE(cinfo.input_scan_number, script.num_scans);
      const jpeg_scan_info& scan = script.scans[cinfo.input_scan_number - 1];
      ASSERT_EQ(cinfo.comps_in_scan, scan.comps_in_scan);
#if !JXL_MEMORY_SANITIZER
      for (int i = 0; i < cinfo.comps_in_scan; ++i) {
        EXPECT_EQ(cinfo.cur_comp_info[i]->component_index,
                  scan.component_index[i]);
      }
#endif
      EXPECT_EQ(cinfo.Ss, scan.Ss);
      EXPECT_EQ(cinfo.Se, scan.Se);
      EXPECT_EQ(cinfo.Ah, scan.Ah);
      EXPECT_EQ(cinfo.Al, scan.Al);
    }
    EXPECT_TRUE(jpeg_finish_output(&cinfo));
    if (config.restart_interval > 0) {
      EXPECT_EQ(cinfo.restart_interval, config.restart_interval);
    } else if (config.restart_in_rows > 0) {
      EXPECT_EQ(cinfo.restart_interval,
                config.restart_in_rows * cinfo.MCUs_per_row);
    }
  }
  EXPECT_TRUE(jpeg_start_output(&cinfo, cinfo.input_scan_number));
  size_t stride = cinfo.image_width * cinfo.out_color_components;
  std::vector<uint8_t> output(cinfo.image_height * stride);
  for (size_t y = 0; y < cinfo.image_height; ++y) {
    JSAMPROW rows[] = {reinterpret_cast<JSAMPLE*>(&output[y * stride])};
    jxl::msan::UnpoisonMemory(
        rows[0], sizeof(JSAMPLE) * cinfo.output_components * cinfo.image_width);
    EXPECT_EQ(1, jpeg_read_scanlines(&cinfo, rows, 1));
  }
  EXPECT_TRUE(jpeg_finish_output(&cinfo));
  EXPECT_TRUE(jpeg_finish_decompress(&cinfo));
  jpeg_destroy_decompress(&cinfo);

  double bpp = compressed.size() * 8.0 / (config.xsize * config.ysize);
  printf("bpp: %f\n", bpp);
  EXPECT_LT(bpp, config.max_bpp);

  ASSERT_EQ(output.size(), config.pixels.size());
  const double mul = 1.0 / 255.0;
  double diff2 = 0.0;
  for (size_t i = 0; i < config.pixels.size(); ++i) {
    double sample_orig = config.pixels[i] * mul;
    double sample_output = output[i] * mul;
    double diff = sample_orig - sample_output;
    diff2 += diff * diff;
  }
  double rms = std::sqrt(diff2 / config.pixels.size()) / mul;
  printf("rms: %f\n", rms);
  EXPECT_LE(rms, config.max_dist);
}

bool EncodeWithJpegli(const TestConfig& config,
                      std::vector<uint8_t>* compressed) {
  MyClientData data;
  jpeg_compress_struct cinfo;
  jpeg_error_mgr jerr;
  cinfo.err = jpegli_std_error(&jerr);
  if (setjmp(data.env)) {
    return false;
  }
  cinfo.client_data = static_cast<void*>(&data);
  cinfo.err->error_exit = [](j_common_ptr cinfo) {
    (*cinfo->err->output_message)(cinfo);
    MyClientData* data = reinterpret_cast<MyClientData*>(cinfo->client_data);
    if (data->buffer) free(data->buffer);
    jpegli_destroy(cinfo);
    longjmp(data->env, 1);
  };
  jpegli_create_compress(&cinfo);
  jpegli_mem_dest(&cinfo, &data.buffer, &data.size);
  cinfo.image_width = config.xsize;
  cinfo.image_height = config.ysize;
  cinfo.input_components = config.input_components;
  cinfo.in_color_space = config.in_color_space;
  if (config.xyb_mode) {
    jpegli_set_xyb_mode(&cinfo);
  }
  jpegli_set_defaults(&cinfo);
  if (config.override_JFIF >= 0) {
    cinfo.write_JFIF_header = config.override_JFIF;
  }
  if (config.override_Adobe >= 0) {
    cinfo.write_Adobe_marker = config.override_Adobe;
  }
  if (config.set_jpeg_colorspace) {
    jpegli_set_colorspace(&cinfo, config.jpeg_color_space);
  }
  if (config.custom_component_ids) {
    for (int c = 0; c < cinfo.num_components; ++c) {
      cinfo.comp_info[c].component_id = config.comp_id[c];
    }
  }
  if (config.custom_sampling) {
    for (int c = 0; c < cinfo.num_components; ++c) {
      cinfo.comp_info[c].h_samp_factor = config.h_sampling[c];
      cinfo.comp_info[c].v_samp_factor = config.v_sampling[c];
    }
  }
  if (config.custom_quant_tables) {
    for (int c = 0; c < cinfo.num_components; ++c) {
      cinfo.comp_info[c].quant_tbl_no = config.quant_indexes[c];
    }
    for (const auto& table : config.quant_tables) {
      if (table.add_raw) {
        cinfo.quant_tbl_ptrs[table.slot_idx] =
            jpegli_alloc_quant_table((j_common_ptr)&cinfo);
        for (int k = 0; k < DCTSIZE2; ++k) {
          cinfo.quant_tbl_ptrs[table.slot_idx]->quantval[k] = table.quantval[k];
        }
        cinfo.quant_tbl_ptrs[table.slot_idx]->sent_table = FALSE;
      } else {
        jpegli_add_quant_table(&cinfo, table.slot_idx, &table.basic_table[0],
                               table.scale_factor, table.force_baseline);
      }
    }
  }
  if (config.progressive_id > 0) {
    const ScanScript& script = kTestScript[config.progressive_id - 1];
    cinfo.scan_info = script.scans;
    cinfo.num_scans = script.num_scans;
  } else if (config.progressive_level >= 0) {
    jpegli_set_progressive_level(&cinfo, config.progressive_level);
  }
  cinfo.restart_interval = config.restart_interval;
  cinfo.restart_in_rows = config.restart_in_rows;
  cinfo.optimize_coding = TRUE;
  jpegli_set_quality(&cinfo, config.quality, TRUE);
  if (config.libjpeg_mode) {
    jpegli_enable_adaptive_quantization(&cinfo, FALSE);
    jpegli_use_standard_quant_tables(&cinfo);
    jpegli_set_progressive_level(&cinfo, 0);
  }
  jpegli_start_compress(&cinfo, TRUE);
  if (config.add_marker) {
    jpegli_write_marker(&cinfo, kSpecialMarker, kMarkerData,
                        sizeof(kMarkerData));
  }
  size_t stride = cinfo.image_width * cinfo.input_components;
  std::vector<uint8_t> row_bytes(stride);
  for (size_t y = 0; y < cinfo.image_height; ++y) {
    memcpy(&row_bytes[0], &config.pixels[y * stride], stride);
    JSAMPROW row[] = {row_bytes.data()};
    jpegli_write_scanlines(&cinfo, row, 1);
  }
  jpegli_finish_compress(&cinfo);
  jpegli_destroy_compress(&cinfo);
  compressed->resize(data.size);
  std::copy_n(data.buffer, data.size, compressed->data());
  std::free(data.buffer);
  return true;
}

class EncodeAPITestParam : public ::testing::TestWithParam<TestConfig> {};

TEST_P(EncodeAPITestParam, TestAPI) {
  TestConfig config = GetParam();
  GeneratePixels(&config);
  std::vector<uint8_t> compressed;
  ASSERT_TRUE(EncodeWithJpegli(config, &compressed));
  TestDecodedImage(config, compressed);
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
    config.quality = 100;
    config.max_bpp = 5.9;
    config.max_dist = 0.6;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.quality = 80;
    config.max_bpp = 0.95;
    config.max_dist = 2.9;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.custom_sampling = true;
    config.h_sampling[0] = 2;
    config.v_sampling[0] = 2;
    config.max_bpp = 1.25;
    config.max_dist = 2.9;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.custom_sampling = true;
    config.h_sampling[0] = 1;
    config.v_sampling[0] = 2;
    config.max_bpp = 1.35;
    config.max_dist = 2.5;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.custom_sampling = true;
    config.h_sampling[0] = 2;
    config.v_sampling[0] = 1;
    config.max_bpp = 1.35;
    config.max_dist = 2.5;
    all_tests.push_back(config);
  }
  for (size_t p = 0; p < kNumTestScripts; ++p) {
    TestConfig config;
    config.progressive_id = p + 1;
    config.max_bpp = 1.5;
    config.max_dist = 2.2;
    all_tests.push_back(config);
  }
  for (size_t l = 0; l <= 2; ++l) {
    TestConfig config;
    config.progressive_level = l;
    config.max_bpp = 1.5;
    config.max_dist = 2.2;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.xyb_mode = true;
    config.max_bpp = 1.5;
    config.max_dist = 3.5;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.libjpeg_mode = true;
    config.max_bpp = 2.1;
    config.max_dist = 1.7;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.in_color_space = JCS_YCbCr;
    config.max_bpp = 1.45;
    config.max_dist = 1.35;
    all_tests.push_back(config);
  }
  for (bool xyb : {false, true}) {
    TestConfig config;
    config.in_color_space = JCS_GRAYSCALE;
    config.xyb_mode = xyb;
    config.max_bpp = 1.25;
    config.max_dist = 1.4;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.in_color_space = JCS_RGB;
    config.set_jpeg_colorspace = true;
    config.jpeg_color_space = JCS_RGB;
    config.xyb_mode = false;
    config.max_bpp = 3.75;
    config.max_dist = 1.4;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.in_color_space = JCS_CMYK;
    config.max_bpp = 3.75;
    config.max_dist = 1.4;
    all_tests.push_back(config);
    config.set_jpeg_colorspace = true;
    config.jpeg_color_space = JCS_YCCK;
    config.max_bpp = 3.2;
    config.max_dist = 1.7;
    all_tests.push_back(config);
  }
  for (int channels = 1; channels <= jpegli::kMaxComponents; ++channels) {
    TestConfig config;
    config.in_color_space = JCS_UNKNOWN;
    config.input_components = channels;
    config.max_bpp = 1.25 * channels;
    config.max_dist = 1.4;
    all_tests.push_back(config);
  }
  for (size_t r : {1, 3, 17, 1024}) {
    TestConfig config;
    config.restart_interval = r;
    config.max_bpp = 1.5 + 5.5 / r;
    config.max_dist = 2.2;
    all_tests.push_back(config);
  }
  for (size_t rr : {1, 3, 8, 100}) {
    TestConfig config;
    config.restart_in_rows = rr;
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
          config.xsize = 64;
          config.ysize = 64;
          config.custom_quant_tables = true;
          config.quant_indexes[1] = 0;
          config.quant_indexes[2] = 0;
          CustomQuantTable table;
          table.table_type = type;
          table.scale_factor = scale;
          table.force_baseline = baseline;
          table.add_raw = add_raw;
          table.Generate();
          config.quant_tables.push_back(table);
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
    config.xsize = 256;
    config.ysize = 256;
    config.custom_quant_tables = true;
    config.quant_indexes[0] = (qidx >> 2) & 1;
    config.quant_indexes[1] = (qidx >> 1) & 1;
    config.quant_indexes[2] = (qidx >> 0) & 1;
    config.max_bpp = 2.6;
    config.max_dist = 2.5;
    all_tests.push_back(config);
  }
  for (int qidx = 0; qidx < 8; ++qidx) {
    for (int slot_idx = 0; slot_idx < 2; ++slot_idx) {
      if (qidx == 0 && slot_idx == 0) continue;
      TestConfig config;
      config.xsize = 256;
      config.ysize = 256;
      config.custom_quant_tables = true;
      config.quant_indexes[0] = (qidx >> 2) & 1;
      config.quant_indexes[1] = (qidx >> 1) & 1;
      config.quant_indexes[2] = (qidx >> 0) & 1;
      CustomQuantTable table;
      table.slot_idx = slot_idx;
      table.Generate();
      config.quant_tables.push_back(table);
      config.max_bpp = 2.6;
      config.max_dist = 2.75;
      all_tests.push_back(config);
    }
  }
  for (int qidx = 0; qidx < 8; ++qidx) {
    for (bool xyb : {false, true}) {
      TestConfig config;
      config.xsize = 256;
      config.ysize = 256;
      config.xyb_mode = xyb;
      config.custom_quant_tables = true;
      config.quant_indexes[0] = (qidx >> 2) & 1;
      config.quant_indexes[1] = (qidx >> 1) & 1;
      config.quant_indexes[2] = (qidx >> 0) & 1;
      {
        CustomQuantTable table;
        table.slot_idx = 0;
        table.Generate();
        config.quant_tables.push_back(table);
      }
      {
        CustomQuantTable table;
        table.slot_idx = 1;
        table.table_type = 20;
        table.Generate();
        config.quant_tables.push_back(table);
      }
      config.max_bpp = 1.9;
      config.max_dist = 3.75;
      all_tests.push_back(config);
    }
  }
  for (bool xyb : {false, true}) {
    TestConfig config;
    config.xsize = 256;
    config.ysize = 256;
    config.xyb_mode = xyb;
    config.custom_quant_tables = true;
    config.quant_indexes[0] = 0;
    config.quant_indexes[1] = 1;
    config.quant_indexes[2] = 2;
    {
      CustomQuantTable table;
      table.slot_idx = 0;
      table.Generate();
      config.quant_tables.push_back(table);
    }
    {
      CustomQuantTable table;
      table.slot_idx = 1;
      table.table_type = 20;
      table.Generate();
      config.quant_tables.push_back(table);
    }
    {
      CustomQuantTable table;
      table.slot_idx = 2;
      table.table_type = 30;
      table.Generate();
      config.quant_tables.push_back(table);
    }
    config.max_bpp = 1.5;
    config.max_dist = 3.75;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.custom_component_ids = true;
    config.comp_id[0] = 7;
    config.comp_id[1] = 17;
    config.comp_id[2] = 177;
    config.max_bpp = 1.45;
    config.max_dist = 2.2;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.max_bpp = 1.45;
    config.max_dist = 2.2;
    config.override_JFIF = 1;
    all_tests.push_back(config);
    config.override_JFIF = 0;
    config.override_Adobe = 1;
    all_tests.push_back(config);
    config.override_JFIF = 1;
    config.override_Adobe = 1;
    all_tests.push_back(config);
  }
  {
    TestConfig config;
    config.xsize = config.ysize = 256;
    config.max_bpp = 1.45;
    config.max_dist = 2.2;
    config.add_marker = true;
    all_tests.push_back(config);
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
  os << ColorSpaceName(c.in_color_space);
  if (c.in_color_space == JCS_UNKNOWN) {
    os << c.input_components;
  }
  if (c.set_jpeg_colorspace) {
    os << "to" << ColorSpaceName(c.jpeg_color_space);
  }
  os << "Q" << c.quality;
  if (c.custom_sampling) {
    os << "SAMP";
    for (size_t i = 0; i < c.input_components; ++i) {
      os << "_";
      os << c.h_sampling[i] << "x" << c.v_sampling[i];
    }
  }
  if (c.custom_component_ids) {
    os << "CID";
    for (size_t i = 0; i < c.input_components; ++i) {
      os << c.comp_id[i];
    }
  }
  if (c.custom_quant_tables) {
    os << "QIDX";
    for (size_t i = 0; i < c.input_components; ++i) {
      os << c.quant_indexes[i];
    }
    for (const auto& table : c.quant_tables) {
      os << "TABLE" << table.slot_idx << "T" << table.table_type << "F"
         << table.scale_factor
         << (table.add_raw          ? "R"
             : table.force_baseline ? "B"
                                    : "");
    }
  }
  if (c.progressive_id > 0) {
    os << "P" << c.progressive_id;
  }
  if (c.restart_interval > 0) {
    os << "R" << c.restart_interval;
  }
  if (c.restart_in_rows > 0) {
    os << "RR" << c.restart_in_rows;
  }
  if (c.progressive_level >= 0) {
    os << "PL" << c.progressive_level;
  }
  if (c.xyb_mode) {
    os << "XYB";
  } else if (c.libjpeg_mode) {
    os << "Libjpeg";
  }
  if (c.override_JFIF >= 0) {
    os << (c.override_JFIF ? "AddJFIF" : "NoJFIF");
  }
  if (c.override_Adobe >= 0) {
    os << (c.override_JFIF ? "AddAdobe" : "NoAdobe");
  }
  if (c.add_marker) {
    os << "AddMarker";
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

#define ERROR_HANDLER_SETUP(action)                                           \
  jpeg_error_mgr jerr;                                                        \
  cinfo.err = jpegli_std_error(&jerr);                                        \
  if (setjmp(data.env)) {                                                     \
    action;                                                                   \
  }                                                                           \
  cinfo.client_data = reinterpret_cast<void*>(&data);                         \
  cinfo.err->error_exit = [](j_common_ptr cinfo) {                            \
    (*cinfo->err->output_message)(cinfo);                                     \
    MyClientData* data = reinterpret_cast<MyClientData*>(cinfo->client_data); \
    if (data->buffer) free(data->buffer);                                     \
    jpegli_destroy(cinfo);                                                    \
    longjmp(data->env, 1);                                                    \
  };

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
  cinfo.num_scans = ARRAYSIZE(kScript);
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
  cinfo.num_scans = ARRAYSIZE(kScript);
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
  cinfo.num_scans = ARRAYSIZE(kScript);
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
  cinfo.num_scans = ARRAYSIZE(kScript);
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
  cinfo.num_scans = ARRAYSIZE(kScript);
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
  cinfo.num_scans = ARRAYSIZE(kScript);
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
  cinfo.num_scans = ARRAYSIZE(kScript);
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
  cinfo.num_scans = ARRAYSIZE(kScript);
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
  cinfo.num_scans = ARRAYSIZE(kScript);
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
  cinfo.num_scans = ARRAYSIZE(kScript);
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
  cinfo.num_scans = ARRAYSIZE(kScript);
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
  cinfo.num_scans = ARRAYSIZE(kScript);
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

}  // namespace
}  // namespace jpegli
