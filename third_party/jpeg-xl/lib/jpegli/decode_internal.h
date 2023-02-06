// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JPEGLI_DECODE_INTERNAL_H_
#define LIB_JPEGLI_DECODE_INTERNAL_H_

#include <stdint.h>
#include <sys/types.h>

#include <array>
#include <set>
#include <vector>

#include "hwy/aligned_allocator.h"
#include "lib/jpegli/huffman.h"
#include "lib/jxl/base/compiler_specific.h"  // for ssize_t

namespace jpegli {

template <typename T1, typename T2>
constexpr inline T1 DivCeil(T1 a, T2 b) {
  return (a + b - 1) / b;
}

constexpr int kMaxComponents = 4;
constexpr int kJpegDCAlphabetSize = 12;

typedef int16_t coeff_t;

enum DecodeState {
  kNull,
  kStart,
  kInHeader,
  kHeaderDone,
  kProcessMarkers,
  kProcessScan,
};

// Represents one component of a jpeg file.
struct JPEGComponent {
  // The DCT coefficients of this component, laid out block-by-block, divided
  // through the quantization matrix values.
  hwy::AlignedFreeUniquePtr<coeff_t[]> coeffs;
};

// State of the decoder that has to be saved before decoding one MCU in case
// we run out of the bitstream.
struct MCUCodingState {
  coeff_t last_dc_coeff[kMaxComponents];
  int eobrun;
  std::vector<coeff_t> coeffs;
};

class RowBuffer {
 public:
  void Allocate(size_t num_rows, size_t stride) {
    ysize_ = num_rows;
    stride_ = stride;
    data_ = hwy::AllocateAligned<float>(ysize_ * stride_);
  }

  float* Row(ssize_t y) { return &data_[((ysize_ + y) % ysize_) * stride_]; }

  size_t stride() const { return stride_; }
  size_t memstride() const { return stride_ * sizeof(data_[0]); }

 private:
  size_t ysize_ = 0;
  size_t stride_ = 0;
  hwy::AlignedFreeUniquePtr<float[]> data_;
};

/* clang-format off */
constexpr uint32_t kJPEGNaturalOrder[80] = {
  0,   1,  8, 16,  9,  2,  3, 10,
  17, 24, 32, 25, 18, 11,  4,  5,
  12, 19, 26, 33, 40, 48, 41, 34,
  27, 20, 13,  6,  7, 14, 21, 28,
  35, 42, 49, 56, 57, 50, 43, 36,
  29, 22, 15, 23, 30, 37, 44, 51,
  58, 59, 52, 45, 38, 31, 39, 46,
  53, 60, 61, 54, 47, 55, 62, 63,
  // extra entries for safety in decoder
  63, 63, 63, 63, 63, 63, 63, 63,
  63, 63, 63, 63, 63, 63, 63, 63
};

/* clang-format on */

}  // namespace jpegli

// Use this forward-declared libjpeg struct to hold all our private variables.
// TODO(szabadka) Remove variables that have a corresponding version in cinfo.
struct jpeg_decomp_master {
  //
  // Input handling state.
  //
  // Number of bits after codestream_pos_ that were already processed.
  size_t codestream_bits_ahead_ = 0;

  //
  // Marker data processing state.
  //
  bool found_soi_ = false;
  bool found_dri_ = false;
  bool found_sof_ = false;
  bool found_eoi_ = false;
  size_t icc_index_ = 0;
  size_t icc_total_ = 0;
  std::vector<uint8_t> icc_profile_;
  std::vector<jpegli::JPEGComponent> components_;
  std::vector<jpegli::HuffmanTableEntry> dc_huff_lut_;
  std::vector<jpegli::HuffmanTableEntry> ac_huff_lut_;
  uint8_t huff_slot_defined_[256] = {};
  std::set<int> markers_to_save_;
  jpeg_marker_parser_method app_marker_parsers[16];
  jpeg_marker_parser_method com_marker_parser;
  // Whether this jpeg has multiple scans (progressive or non-interleaved
  // sequential).
  bool is_multiscan_;

  // Fields defined by SOF marker.
  size_t iMCU_cols_;

  // Initialized at strat of frame.
  uint16_t scan_progression_[jpegli::kMaxComponents][DCTSIZE2];

  //
  // Per scan state.
  //
  size_t scan_mcu_row_;
  size_t scan_mcu_col_;
  size_t mcu_rows_per_iMCU_row_;
  jpegli::coeff_t last_dc_coeff_[jpegli::kMaxComponents];
  int eobrun_;
  int restarts_to_go_;
  int next_restart_marker_;

  jpegli::MCUCodingState mcu_;

  //
  // Rendering state.
  //
  size_t output_bit_depth_ = 8;
  size_t xoffset_ = 0;

  JSAMPARRAY scanlines_;
  JDIMENSION max_lines_;
  size_t num_output_rows_;

  std::array<size_t, jpegli::kMaxComponents> raw_height_;
  std::array<jpegli::RowBuffer, jpegli::kMaxComponents> raw_output_;
  std::array<jpegli::RowBuffer, jpegli::kMaxComponents> render_output_;

  hwy::AlignedFreeUniquePtr<float[]> idct_scratch_;
  hwy::AlignedFreeUniquePtr<float[]> upsample_scratch_;
  hwy::AlignedFreeUniquePtr<uint8_t[]> output_scratch_;
  hwy::AlignedFreeUniquePtr<float[]> dequant_;

  // Per channel and per frequency statistics about the number of nonzeros and
  // the sum of coefficient absolute values, used in dequantization bias
  // computation.
  hwy::AlignedFreeUniquePtr<int[]> nonzeros_;
  hwy::AlignedFreeUniquePtr<int[]> sumabs_;
  std::vector<size_t> num_processed_blocks_;
  hwy::AlignedFreeUniquePtr<float[]> biases_;
};

#endif  // LIB_JPEGLI_DECODE_INTERNAL_H_
