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

#ifndef LIB_JXL_ENC_HUFFMAN_H_
#define LIB_JXL_ENC_HUFFMAN_H_

#include "lib/jxl/enc_bit_writer.h"

namespace jxl {

// Builds a Huffman tree for the given histogram, and encodes it into writer
// in a format that can be read by HuffmanDecodingData::ReadFromBitstream.
// An allotment for `writer` must already have been created by the caller.
void BuildAndStoreHuffmanTree(const uint32_t* histogram, size_t length,
                              uint8_t* depth, uint16_t* bits,
                              BitWriter* writer);

}  // namespace jxl

#endif  // LIB_JXL_ENC_HUFFMAN_H_
