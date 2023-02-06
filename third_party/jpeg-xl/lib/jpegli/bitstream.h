// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JPEGLI_BITSTREAM_H_
#define LIB_JPEGLI_BITSTREAM_H_

#include <initializer_list>
#include <vector>

#include "lib/jpegli/encode_internal.h"

namespace jpegli {

void WriteOutput(j_compress_ptr cinfo, const uint8_t* buf, size_t bufsize);
void WriteOutput(j_compress_ptr cinfo, const std::vector<uint8_t>& bytes);
void WriteOutput(j_compress_ptr cinfo, std::initializer_list<uint8_t> bytes);

void EncodeAPP0(j_compress_ptr cinfo);
void EncodeAPP14(j_compress_ptr cinfo);
void EncodeSOF(j_compress_ptr cinfo);
void EncodeSOS(j_compress_ptr cinfo, int scan_index);
void EncodeDHT(j_compress_ptr cinfo,
               const std::vector<JPEGHuffmanCode>& huffman_code,
               size_t* next_huffman_code, size_t num_huffman_codes);
void EncodeDQT(j_compress_ptr cinfo);
bool EncodeDRI(j_compress_ptr cinfo);

bool EncodeScan(j_compress_ptr cinfo,
                const std::vector<std::vector<jpegli::coeff_t>>& coeffs,
                int scan_index);

}  // namespace jpegli

#endif  // LIB_JPEGLI_BITSTREAM_H_
