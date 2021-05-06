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

#ifndef LIB_JXL_HUFFMAN_TABLE_H_
#define LIB_JXL_HUFFMAN_TABLE_H_

#include <stdint.h>
#include <stdlib.h>

namespace jxl {

struct HuffmanCode {
  uint8_t bits;   /* number of bits used for this symbol */
  uint16_t value; /* symbol value or table offset */
};

/* Builds Huffman lookup table assuming code lengths are in symbol order. */
/* Returns 0 in case of error (invalid tree or memory error), otherwise
   populated size of table. */
uint32_t BuildHuffmanTable(HuffmanCode* root_table, int root_bits,
                           const uint8_t* code_lengths,
                           size_t code_lengths_size, uint16_t* count);

}  // namespace jxl

#endif  // LIB_JXL_HUFFMAN_TABLE_H_
