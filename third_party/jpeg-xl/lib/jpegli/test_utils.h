// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <vector>

#include "gtest/gtest.h"
#include "lib/jxl/base/file_io.h"

// googletest before 1.10 didn't define INSTANTIATE_TEST_SUITE_P() but instead
// used INSTANTIATE_TEST_CASE_P which is now deprecated.
#ifdef INSTANTIATE_TEST_SUITE_P
#define JPEGLI_INSTANTIATE_TEST_SUITE_P INSTANTIATE_TEST_SUITE_P
#else
#define JPEGLI_INSTANTIATE_TEST_SUITE_P INSTANTIATE_TEST_CASE_P
#endif

namespace jpegli {

static inline std::vector<uint8_t> ReadTestData(const std::string& filename) {
  std::string full_path = std::string(TEST_DATA_PATH "/") + filename;
  std::vector<uint8_t> data;
  JXL_CHECK(jxl::ReadFile(full_path, &data));
  printf("Test data %s is %d bytes long.\n", filename.c_str(),
         static_cast<int>(data.size()));
  return data;
}

class PNMParser {
 public:
  explicit PNMParser(const uint8_t* data, const size_t len)
      : pos_(data), end_(data + len) {}

  // Sets "pos" to the first non-header byte/pixel on success.
  bool ParseHeader(const uint8_t** pos, volatile size_t* xsize,
                   volatile size_t* ysize, volatile size_t* num_channels,
                   volatile size_t* bitdepth) {
    if (pos_[0] != 'P' || (pos_[1] != '5' && pos_[1] != '6')) {
      fprintf(stderr, "Invalid PNM header.");
      return false;
    }
    *num_channels = (pos_[1] == '5' ? 1 : 3);
    pos_ += 2;

    size_t maxval;
    if (!SkipWhitespace() || !ParseUnsigned(xsize) || !SkipWhitespace() ||
        !ParseUnsigned(ysize) || !SkipWhitespace() || !ParseUnsigned(&maxval) ||
        !SkipWhitespace()) {
      return false;
    }
    if (maxval == 0 || maxval >= 65536) {
      fprintf(stderr, "Invalid maxval value.\n");
      return false;
    }
    bool found_bitdepth = false;
    for (int bits = 1; bits <= 16; ++bits) {
      if (maxval == (1u << bits) - 1) {
        *bitdepth = bits;
        found_bitdepth = true;
        break;
      }
    }
    if (!found_bitdepth) {
      fprintf(stderr, "Invalid maxval value.\n");
      return false;
    }

    *pos = pos_;
    return true;
  }

 private:
  static bool IsLineBreak(const uint8_t c) { return c == '\r' || c == '\n'; }
  static bool IsWhitespace(const uint8_t c) {
    return IsLineBreak(c) || c == '\t' || c == ' ';
  }

  bool ParseUnsigned(volatile size_t* number) {
    if (pos_ == end_ || *pos_ < '0' || *pos_ > '9') {
      fprintf(stderr, "Expected unsigned number.\n");
      return false;
    }
    *number = 0;
    while (pos_ < end_ && *pos_ >= '0' && *pos_ <= '9') {
      *number *= 10;
      *number += *pos_ - '0';
      ++pos_;
    }

    return true;
  }

  bool SkipWhitespace() {
    if (pos_ == end_ || !IsWhitespace(*pos_)) {
      fprintf(stderr, "Expected whitespace.\n");
      return false;
    }
    while (pos_ < end_ && IsWhitespace(*pos_)) {
      ++pos_;
    }
    return true;
  }

  const uint8_t* pos_;
  const uint8_t* const end_;
};

static inline bool ReadPNM(const std::vector<uint8_t>& data,
                           volatile size_t* xsize, volatile size_t* ysize,
                           volatile size_t* num_channels,
                           volatile size_t* bitdepth,
                           std::vector<uint8_t>* pixels) {
  if (data.size() < 2) {
    fprintf(stderr, "PNM file too small.\n");
    return false;
  }
  PNMParser parser(data.data(), data.size());
  const uint8_t* pos = nullptr;
  if (!parser.ParseHeader(&pos, xsize, ysize, num_channels, bitdepth)) {
    return false;
  }
  pixels->resize(data.data() + data.size() - pos);
  memcpy(&(*pixels)[0], pos, pixels->size());
  return true;
}

}  // namespace jpegli
