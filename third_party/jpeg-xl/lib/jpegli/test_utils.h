// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JPEGLI_TEST_UTILS_H_
#define LIB_JPEGLI_TEST_UTILS_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>
#include <vector>

/* clang-format off */
#include <stdio.h>
#include <jpeglib.h>
#include <setjmp.h>
/* clang-format on */

namespace jpegli {

// TODO(eustas): use common.h?
template <typename T1, typename T2>
constexpr inline T1 DivCeil(T1 a, T2 b) {
  return (a + b - 1) / b;
}

struct MyClientData {
  jmp_buf env;
  unsigned char* buffer = nullptr;
  unsigned long size = 0;
  unsigned char* table_stream = nullptr;
  unsigned long table_stream_size = 0;
};

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

static constexpr int kSpecialMarker = 0xe5;
static constexpr uint8_t kMarkerData[] = {0, 1, 255, 0, 17};

#define ARRAY_SIZE(X) (sizeof(X) / sizeof((X)[0]))

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
  int num_scans;
  const jpeg_scan_info* scans;
};

static constexpr ScanScript kTestScript[] = {
    {ARRAY_SIZE(kScript1), kScript1},
    {ARRAY_SIZE(kScript2), kScript2},
    {ARRAY_SIZE(kScript3), kScript3},
};
static constexpr int kNumTestScripts = ARRAY_SIZE(kTestScript);

enum JpegIOMode {
  PIXELS,
  RAW_DATA,
  COEFFICIENTS,
};

struct CustomQuantTable {
  int slot_idx = 0;
  uint16_t table_type = 0;
  int scale_factor = 100;
  bool add_raw = false;
  bool force_baseline = true;
  std::vector<unsigned int> basic_table;
  std::vector<unsigned int> quantval;
  void Generate();
};

struct TestImage {
  size_t xsize = 0;
  size_t ysize = 0;
  J_COLOR_SPACE color_space = JCS_RGB;
  size_t components = 3;
  std::vector<uint8_t> pixels;
  std::vector<std::vector<uint8_t>> raw_data;
  std::vector<std::vector<JCOEF>> coeffs;
};

struct CompressParams {
  int quality = 90;
  bool set_jpeg_colorspace = false;
  J_COLOR_SPACE jpeg_color_space = JCS_UNKNOWN;
  std::vector<int> quant_indexes;
  std::vector<CustomQuantTable> quant_tables;
  std::vector<int> h_sampling;
  std::vector<int> v_sampling;
  std::vector<int> comp_ids;
  int override_JFIF = -1;
  int override_Adobe = -1;
  bool add_marker = false;
  int progressive_id = 0;
  int progressive_level = -1;
  unsigned int restart_interval = 0;
  int restart_in_rows = 0;
  bool optimize_coding = true;
  bool use_flat_dc_luma_code = false;
  bool xyb_mode = false;
  bool libjpeg_mode = false;

  int h_samp(int c) const { return h_sampling.empty() ? 1 : h_sampling[c]; }
  int v_samp(int c) const { return v_sampling.empty() ? 1 : v_sampling[c]; }
  int max_h_sample() const {
    auto it = std::max_element(h_sampling.begin(), h_sampling.end());
    return it == h_sampling.end() ? 1 : *it;
  }
  int max_v_sample() const {
    auto it = std::max_element(v_sampling.begin(), v_sampling.end());
    return it == v_sampling.end() ? 1 : *it;
  }
  int comp_width(const TestImage& input, int c) const {
    return DivCeil(input.xsize * h_samp(c), max_h_sample() * DCTSIZE) * DCTSIZE;
  }
  int comp_height(const TestImage& input, int c) const {
    return DivCeil(input.ysize * v_samp(c), max_v_sample() * DCTSIZE) * DCTSIZE;
  }
};

std::string GetTestDataPath(const std::string& filename);
std::vector<uint8_t> ReadTestData(const std::string& filename);

class PNMParser {
 public:
  explicit PNMParser(const uint8_t* data, const size_t len)
      : pos_(data), end_(data + len) {}

  // Sets "pos" to the first non-header byte/pixel on success.
  bool ParseHeader(const uint8_t** pos, size_t* xsize, size_t* ysize,
                   size_t* num_channels, size_t* bitdepth);

 private:
  static bool IsLineBreak(const uint8_t c) { return c == '\r' || c == '\n'; }
  static bool IsWhitespace(const uint8_t c) {
    return IsLineBreak(c) || c == '\t' || c == ' ';
  }

  bool ParseUnsigned(size_t* number);

  bool SkipWhitespace();

  const uint8_t* pos_;
  const uint8_t* const end_;
};

bool ReadPNM(const std::vector<uint8_t>& data, size_t* xsize, size_t* ysize,
             size_t* num_channels, size_t* bitdepth,
             std::vector<uint8_t>* pixels);

void SetNumChannels(J_COLOR_SPACE colorspace, size_t* channels);

void ConvertPixel(const uint8_t* input_rgb, uint8_t* out,
                  J_COLOR_SPACE colorspace, size_t num_channels);

void GeneratePixels(TestImage* img);

void GenerateRawData(const CompressParams& jparams, TestImage* img);

void GenerateCoeffs(const CompressParams& jparams, TestImage* img);

bool EncodeWithJpegli(const TestImage& input, const CompressParams& jparams,
                      std::vector<uint8_t>* compressed);

// Verifies that an image encoded with libjpegli can be decoded with libjpeg,
// and checks that the jpeg coding metadata matches jparams.
void DecodeWithLibjpeg(const CompressParams& jparams,
                       const std::vector<uint8_t>& compressed,
                       JpegIOMode output_mode, TestImage* output);

void VerifyOutputImage(const TestImage& input, const TestImage& output,
                       double max_rms);

}  // namespace jpegli

#endif  // LIB_JPEGLI_TEST_UTILS_H_
