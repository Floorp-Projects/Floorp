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
void EncodeSOF(j_compress_ptr cinfo, bool is_baseline);
void EncodeSOS(j_compress_ptr cinfo, int scan_index);
void EncodeDHT(j_compress_ptr cinfo, const JPEGHuffmanCode* huffman_codes,
               size_t num_huffman_codes, bool pre_shifted = false);
void EncodeDQT(j_compress_ptr cinfo, bool write_all_tables, bool* is_baseline);
bool EncodeDRI(j_compress_ptr cinfo);

bool EncodeScan(j_compress_ptr cinfo, int scan_index);

void EncodeSingleScan(j_compress_ptr cinfo);

void WriteiMCURow(j_compress_ptr cinfo);

}  // namespace jpegli

#endif  // LIB_JPEGLI_BITSTREAM_H_
