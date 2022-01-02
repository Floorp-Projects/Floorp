// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// Utility function for building a Huffman lookup table for the jpeg decoder.

#ifndef LIB_JXL_JPEG_ENC_JPEG_HUFFMAN_DECODE_H_
#define LIB_JXL_JPEG_ENC_JPEG_HUFFMAN_DECODE_H_

#include <stdint.h>

namespace jxl {
namespace jpeg {

constexpr int kJpegHuffmanRootTableBits = 8;
// Maximum huffman lookup table size.
// According to zlib/examples/enough.c, 758 entries are always enough for
// an alphabet of 257 symbols (256 + 1 special symbol for the all 1s code) and
// max bit length 16 if the root table has 8 bits.
constexpr int kJpegHuffmanLutSize = 758;

struct HuffmanTableEntry {
  // Initialize the value to an invalid symbol so that we can recognize it
  // when reading the bit stream using a Huffman code with space > 0.
  HuffmanTableEntry() : bits(0), value(0xffff) {}

  uint8_t bits;    // number of bits used for this symbol
  uint16_t value;  // symbol value or table offset
};

// Builds jpeg-style Huffman lookup table from the given symbols.
// The symbols are in order of increasing bit lengths. The number of symbols
// with bit length n is given in counts[n] for each n >= 1.
void BuildJpegHuffmanTable(const uint32_t* counts, const uint32_t* symbols,
                           HuffmanTableEntry* lut);

}  // namespace jpeg
}  // namespace jxl

#endif  // LIB_JXL_JPEG_ENC_JPEG_HUFFMAN_DECODE_H_
