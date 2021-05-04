// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LIB_JXL_JPEG_DEC_JPEG_SERIALIZATION_STATE_H_
#define LIB_JXL_JPEG_DEC_JPEG_SERIALIZATION_STATE_H_

#include <deque>
#include <vector>

#include "lib/jxl/jpeg/dec_jpeg_output_chunk.h"
#include "lib/jxl/jpeg/jpeg_data.h"

namespace jxl {
namespace jpeg {

struct HuffmanCodeTable {
  int depth[256];
  int code[256];
};

// Handles the packing of bits into output bytes.
struct JpegBitWriter {
  bool healthy;
  std::deque<OutputChunk>* output;
  OutputChunk chunk;
  uint8_t* data;
  size_t pos;
  uint64_t put_buffer;
  int put_bits;
};

// Holds data that is buffered between 8x8 blocks in progressive mode.
struct DCTCodingState {
  // The run length of end-of-band symbols in a progressive scan.
  int eob_run_;
  // The huffman table to be used when flushing the state.
  const HuffmanCodeTable* cur_ac_huff_;
  // The sequence of currently buffered refinement bits for a successive
  // approximation scan (one where Ah > 0).
  std::vector<int> refinement_bits_;
};

struct EncodeScanState {
  enum Stage { HEAD, BODY };

  Stage stage = HEAD;

  int mcu_y;
  JpegBitWriter bw;
  coeff_t last_dc_coeff[kMaxComponents] = {0};
  int restarts_to_go;
  int next_restart_marker;
  int block_scan_index;
  DCTCodingState coding_state;
  size_t extra_zero_runs_pos;
  int next_extra_zero_run_index;
  size_t next_reset_point_pos;
  int next_reset_point;
};

struct SerializationState {
  enum Stage {
    INIT,
    SERIALIZE_SECTION,
    DONE,
    ERROR,
  };

  Stage stage = INIT;

  std::deque<OutputChunk> output_queue;

  size_t section_index = 0;
  int dht_index = 0;
  int dqt_index = 0;
  int app_index = 0;
  int com_index = 0;
  int data_index = 0;
  int scan_index = 0;
  std::vector<HuffmanCodeTable> dc_huff_table;
  std::vector<HuffmanCodeTable> ac_huff_table;
  const uint8_t* pad_bits = nullptr;
  const uint8_t* pad_bits_end = nullptr;
  bool seen_dri_marker = false;
  bool is_progressive = false;

  EncodeScanState scan_state;
};

}  // namespace jpeg
}  // namespace jxl

#endif  // LIB_JXL_JPEG_DEC_JPEG_SERIALIZATION_STATE_H_
