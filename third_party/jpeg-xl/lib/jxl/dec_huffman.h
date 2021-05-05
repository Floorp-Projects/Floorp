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

#ifndef LIB_JXL_DEC_HUFFMAN_H_
#define LIB_JXL_DEC_HUFFMAN_H_

#include <memory>
#include <vector>

#include "lib/jxl/dec_bit_reader.h"
#include "lib/jxl/huffman_table.h"

namespace jxl {

static constexpr size_t kHuffmanTableBits = 8u;

struct HuffmanDecodingData {
  // Decodes the Huffman code lengths from the bit-stream and fills in the
  // pre-allocated table with the corresponding 2-level Huffman decoding table.
  // Returns false if the Huffman code lengths can not de decoded.
  bool ReadFromBitStream(size_t alphabet_size, BitReader* br);

  uint16_t ReadSymbol(BitReader* br) const;

  std::vector<HuffmanCode> table_;
};

}  // namespace jxl

#endif  // LIB_JXL_DEC_HUFFMAN_H_
