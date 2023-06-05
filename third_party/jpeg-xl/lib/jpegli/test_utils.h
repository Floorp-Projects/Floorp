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

#include "lib/jpegli/common.h"

namespace jpegli {

// We define this here as well to make sure that the *_api_test.cc tests only
// use the public API and therefore we don't include any *_internal.h headers.
template <typename T1, typename T2>
constexpr inline T1 DivCeil(T1 a, T2 b) {
  return (a + b - 1) / b;
}

#define ERROR_HANDLER_SETUP(flavor)                                \
  jpeg_error_mgr jerr;                                             \
  jmp_buf env;                                                     \
  cinfo.err = flavor##_std_error(&jerr);                           \
  if (setjmp(env)) {                                               \
    return false;                                                  \
  }                                                                \
  cinfo.client_data = reinterpret_cast<void*>(&env);               \
  cinfo.err->error_exit = [](j_common_ptr cinfo) {                 \
    (*cinfo->err->output_message)(cinfo);                          \
    jmp_buf* env = reinterpret_cast<jmp_buf*>(cinfo->client_data); \
    flavor##_destroy(cinfo);                                       \
    longjmp(*env, 1);                                              \
  };

#define ARRAY_SIZE(X) (sizeof(X) / sizeof((X)[0]))

static constexpr int kSpecialMarker0 = 0xe5;
static constexpr int kSpecialMarker1 = 0xe9;
static constexpr uint8_t kMarkerData[] = {0, 1, 255, 0, 17};
static constexpr uint8_t kMarkerSequence[] = {0xe6, 0xe8, 0xe7,
                                              0xe6, 0xe7, 0xe8};
static constexpr size_t kMarkerSequenceLen = ARRAY_SIZE(kMarkerSequence);

// Sequential non-interleaved.
static constexpr jpeg_scan_info kScript1[] = {
    {1, {0}, 0, 63, 0, 0},
    {1, {1}, 0, 63, 0, 0},
    {1, {2}, 0, 63, 0, 0},
};
// Sequential partially interleaved, chroma first.
static constexpr jpeg_scan_info kScript2[] = {
    {2, {1, 2}, 0, 63, 0, 0},
    {1, {0}, 0, 63, 0, 0},
};

// Rest of the scan scripts are progressive.

static constexpr jpeg_scan_info kScript3[] = {
    // Interleaved full DC.
    {3, {0, 1, 2}, 0, 0, 0, 0},
    // Full AC scans.
    {1, {0}, 1, 63, 0, 0},
    {1, {1}, 1, 63, 0, 0},
    {1, {2}, 1, 63, 0, 0},
};
static constexpr jpeg_scan_info kScript4[] = {
    // Non-interleaved full DC.
    {1, {0}, 0, 0, 0, 0},
    {1, {1}, 0, 0, 0, 0},
    {1, {2}, 0, 0, 0, 0},
    // Full AC scans.
    {1, {0}, 1, 63, 0, 0},
    {1, {1}, 1, 63, 0, 0},
    {1, {2}, 1, 63, 0, 0},
};
static constexpr jpeg_scan_info kScript5[] = {
    // Partially interleaved full DC, chroma first.
    {2, {1, 2}, 0, 0, 0, 0},
    {1, {0}, 0, 0, 0, 0},
    // AC shifted by 1 bit.
    {1, {0}, 1, 63, 0, 1},
    {1, {1}, 1, 63, 0, 1},
    {1, {2}, 1, 63, 0, 1},
    // AC refinement scan.
    {1, {0}, 1, 63, 1, 0},
    {1, {1}, 1, 63, 1, 0},
    {1, {2}, 1, 63, 1, 0},
};
static constexpr jpeg_scan_info kScript6[] = {
    // Interleaved DC shifted by 2 bits.
    {3, {0, 1, 2}, 0, 0, 0, 2},
    // Interleaved DC refinement scans.
    {3, {0, 1, 2}, 0, 0, 2, 1},
    {3, {0, 1, 2}, 0, 0, 1, 0},
    // Full AC scans.
    {1, {0}, 1, 63, 0, 0},
    {1, {1}, 1, 63, 0, 0},
    {1, {2}, 1, 63, 0, 0},
};

static constexpr jpeg_scan_info kScript7[] = {
    // Non-interleaved DC shifted by 2 bits.
    {1, {0}, 0, 0, 0, 2},
    {1, {1}, 0, 0, 0, 2},
    {1, {2}, 0, 0, 0, 2},
    // Non-interleaved DC first refinement scans.
    {1, {0}, 0, 0, 2, 1},
    {1, {1}, 0, 0, 2, 1},
    {1, {2}, 0, 0, 2, 1},
    // Non-interleaved DC second refinement scans.
    {1, {0}, 0, 0, 1, 0},
    {1, {1}, 0, 0, 1, 0},
    {1, {2}, 0, 0, 1, 0},
    // Full AC scans.
    {1, {0}, 1, 63, 0, 0},
    {1, {1}, 1, 63, 0, 0},
    {1, {2}, 1, 63, 0, 0},
};

static constexpr jpeg_scan_info kScript8[] = {
    // Partially interleaved DC shifted by 2 bits, chroma first
    {2, {1, 2}, 0, 0, 0, 2},
    {1, {0}, 0, 0, 0, 2},
    // Partially interleaved DC first refinement scans.
    {2, {0, 2}, 0, 0, 2, 1},
    {1, {1}, 0, 0, 2, 1},
    // Partially interleaved DC first refinement scans, chroma first.
    {2, {1, 2}, 0, 0, 1, 0},
    {1, {0}, 0, 0, 1, 0},
    // Full AC scans.
    {1, {0}, 1, 63, 0, 0},
    {1, {1}, 1, 63, 0, 0},
    {1, {2}, 1, 63, 0, 0},
};

static constexpr jpeg_scan_info kScript9[] = {
    // Interleaved full DC.
    {3, {0, 1, 2}, 0, 0, 0, 0},
    // AC scans for component 0
    // shifted by 1 bit, two spectral ranges
    {1, {0}, 1, 6, 0, 1},
    {1, {0}, 7, 63, 0, 1},
    // refinement scan, full
    {1, {0}, 1, 63, 1, 0},
    // AC scans for component 1
    // shifted by 1 bit, full
    {1, {1}, 1, 63, 0, 1},
    // refinement scan, two spectral ranges
    {1, {1}, 1, 6, 1, 0},
    {1, {1}, 7, 63, 1, 0},
    // AC scans for component 2
    // shifted by 1 bit, two spectral ranges
    {1, {2}, 1, 6, 0, 1},
    {1, {2}, 7, 63, 0, 1},
    // refinement scan, two spectral ranges (but different from above)
    {1, {2}, 1, 16, 1, 0},
    {1, {2}, 17, 63, 1, 0},
};

static constexpr jpeg_scan_info kScript10[] = {
    // Interleaved full DC.
    {3, {0, 1, 2}, 0, 0, 0, 0},
    // AC scans for spectral range 1..16
    // shifted by 1
    {1, {0}, 1, 16, 0, 1},
    {1, {1}, 1, 16, 0, 1},
    {1, {2}, 1, 16, 0, 1},
    // refinement scans, two sub-ranges
    {1, {0}, 1, 8, 1, 0},
    {1, {0}, 9, 16, 1, 0},
    {1, {1}, 1, 8, 1, 0},
    {1, {1}, 9, 16, 1, 0},
    {1, {2}, 1, 8, 1, 0},
    {1, {2}, 9, 16, 1, 0},
    // AC scans for spectral range 17..63
    {1, {0}, 17, 63, 0, 1},
    {1, {1}, 17, 63, 0, 1},
    {1, {2}, 17, 63, 0, 1},
    // refinement scans, two sub-ranges
    {1, {0}, 17, 28, 1, 0},
    {1, {0}, 29, 63, 1, 0},
    {1, {1}, 17, 28, 1, 0},
    {1, {1}, 29, 63, 1, 0},
    {1, {2}, 17, 28, 1, 0},
    {1, {2}, 29, 63, 1, 0},
};

struct ScanScript {
  int num_scans;
  const jpeg_scan_info* scans;
};

static constexpr ScanScript kTestScript[] = {
    {ARRAY_SIZE(kScript1), kScript1}, {ARRAY_SIZE(kScript2), kScript2},
    {ARRAY_SIZE(kScript3), kScript3}, {ARRAY_SIZE(kScript4), kScript4},
    {ARRAY_SIZE(kScript5), kScript5}, {ARRAY_SIZE(kScript6), kScript6},
    {ARRAY_SIZE(kScript7), kScript7}, {ARRAY_SIZE(kScript8), kScript8},
    {ARRAY_SIZE(kScript9), kScript9}, {ARRAY_SIZE(kScript10), kScript10},
};
static constexpr int kNumTestScripts = ARRAY_SIZE(kTestScript);

static constexpr int kLastScan = 0xffff;

static uint32_t kTestColorMap[] = {
    0x000000, 0xff0000, 0x00ff00, 0x0000ff, 0xffff00, 0x00ffff,
    0xff00ff, 0xffffff, 0x6251fc, 0x45d9c7, 0xa7f059, 0xd9a945,
    0xfa4e44, 0xceaffc, 0xbad7db, 0xc1f0b1, 0xdbca9a, 0xfacac5,
    0xf201ff, 0x0063db, 0x00f01c, 0xdbb204, 0xf12f0c, 0x7ba1dc};
static constexpr int kTestColorMapNumColors = ARRAY_SIZE(kTestColorMap);

std::string IOMethodName(JpegliDataType data_type, JpegliEndianness endianness);

std::string ColorSpaceName(J_COLOR_SPACE colorspace);

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
  size_t xsize = 2268;
  size_t ysize = 1512;
  J_COLOR_SPACE color_space = JCS_RGB;
  size_t components = 3;
  JpegliDataType data_type = JPEGLI_TYPE_UINT8;
  JpegliEndianness endianness = JPEGLI_NATIVE_ENDIAN;
  std::vector<uint8_t> pixels;
  std::vector<std::vector<uint8_t>> raw_data;
  std::vector<std::vector<JCOEF>> coeffs;
  void AllocatePixels() {
    pixels.resize(ysize * xsize * components *
                  jpegli_bytes_per_sample(data_type));
  }
  void Clear() {
    pixels.clear();
    raw_data.clear();
    coeffs.clear();
  }
};

std::ostream& operator<<(std::ostream& os, const TestImage& input);

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
  bool simple_progression = false;
  // -1 is library default
  // 0, 1, 2 is set through jpegli_set_progressive_level()
  // 2 + N is kScriptN
  int progressive_mode = -1;
  unsigned int restart_interval = 0;
  int restart_in_rows = 0;
  int smoothing_factor = 0;
  int optimize_coding = -1;
  bool use_flat_dc_luma_code = false;
  bool omit_standard_tables = false;
  bool xyb_mode = false;
  bool libjpeg_mode = false;
  bool use_adaptive_quantization = true;
  std::vector<uint8_t> icc;

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

std::ostream& operator<<(std::ostream& os, const CompressParams& jparams);

void VerifyHeader(const CompressParams& jparams, j_decompress_ptr cinfo);
void VerifyScanHeader(const CompressParams& jparams, j_decompress_ptr cinfo);

enum ColorQuantMode {
  CQUANT_1PASS,
  CQUANT_2PASS,
  CQUANT_EXTERNAL,
  CQUANT_REUSE,
};

struct ScanDecompressParams {
  int max_scan_number;
  J_DITHER_MODE dither_mode;
  ColorQuantMode color_quant_mode;
};

struct DecompressParams {
  float size_factor = 1.0f;
  size_t chunk_size = 65536;
  size_t max_output_lines = 16;
  JpegIOMode output_mode = PIXELS;
  JpegliDataType data_type = JPEGLI_TYPE_UINT8;
  JpegliEndianness endianness = JPEGLI_NATIVE_ENDIAN;
  bool set_out_color_space = false;
  J_COLOR_SPACE out_color_space = JCS_UNKNOWN;
  bool crop_output = false;
  bool do_block_smoothing = false;
  bool do_fancy_upsampling = true;
  bool skip_scans = false;
  int scale_num = 1;
  int scale_denom = 1;
  bool quantize_colors = false;
  int desired_number_of_colors = 256;
  std::vector<ScanDecompressParams> scan_params;
};

void SetDecompressParams(const DecompressParams& dparams,
                         j_decompress_ptr cinfo, bool is_jpegli);

void SetScanDecompressParams(const DecompressParams& dparams,
                             j_decompress_ptr cinfo, int scan_number,
                             bool is_jpegli);

void CopyCoefficients(j_decompress_ptr cinfo, jvirt_barray_ptr* coef_arrays,
                      TestImage* output);

void UnmapColors(uint8_t* row, size_t xsize, int components,
                 JSAMPARRAY colormap, size_t num_colors);

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

void ConvertToGrayscale(TestImage* img);

void GeneratePixels(TestImage* img);

void GenerateRawData(const CompressParams& jparams, TestImage* img);

void GenerateCoeffs(const CompressParams& jparams, TestImage* img);

void EncodeWithJpegli(const TestImage& input, const CompressParams& jparams,
                      j_compress_ptr cinfo);

bool EncodeWithJpegli(const TestImage& input, const CompressParams& jparams,
                      std::vector<uint8_t>* compressed);

// Verifies that an image encoded with libjpegli can be decoded with libjpeg,
// and checks that the jpeg coding metadata matches jparams.
void DecodeAllScansWithLibjpeg(const CompressParams& jparams,
                               const DecompressParams& dparams,
                               const std::vector<uint8_t>& compressed,
                               std::vector<TestImage>* output_progression);
void DecodeWithLibjpeg(const CompressParams& jparams,
                       const DecompressParams& dparams, j_decompress_ptr cinfo,
                       TestImage* output);
void DecodeWithLibjpeg(const CompressParams& jparams,
                       const DecompressParams& dparams,
                       const std::vector<uint8_t>& compressed,
                       TestImage* output);

double DistanceRms(const TestImage& input, const TestImage& output,
                   size_t start_line, size_t num_lines,
                   double* max_diff = nullptr);

double DistanceRms(const TestImage& input, const TestImage& output,
                   double* max_diff = nullptr);

void VerifyOutputImage(const TestImage& input, const TestImage& output,
                       size_t start_line, size_t num_lines, double max_rms,
                       double max_diff = 255.0);

void VerifyOutputImage(const TestImage& input, const TestImage& output,
                       double max_rms, double max_diff = 255.0);

}  // namespace jpegli

#endif  // LIB_JPEGLI_TEST_UTILS_H_
