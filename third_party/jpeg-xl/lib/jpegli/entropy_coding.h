// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JPEGLI_ENTROPY_CODING_H_
#define LIB_JPEGLI_ENTROPY_CODING_H_

/* clang-format off */
#include <stdio.h>
#include <jpeglib.h>
/* clang-format on */

#include <vector>

#include "lib/jpegli/encode_internal.h"
#include "lib/jxl/enc_cluster.h"

namespace jpegli {

void AddStandardHuffmanTables(j_compress_ptr cinfo, bool is_dc);

void CopyHuffmanCodes(j_compress_ptr cinfo,
                      std::vector<JPEGHuffmanCode>* huffman_codes);

size_t RestartIntervalForScan(j_compress_ptr cinfo, size_t scan_index);

struct Histogram {
  int count[kJpegHuffmanAlphabetSize];
  Histogram() { memset(count, 0, sizeof(count)); }
};

struct JpegClusteredHistograms {
  std::vector<jxl::Histogram> histograms;
  std::vector<uint32_t> histogram_indexes;
  std::vector<uint32_t> slot_ids;
};

void ClusterJpegHistograms(const Histogram* histo_data, size_t num,
                           JpegClusteredHistograms* clusters);

void AddJpegHuffmanCode(const jxl::Histogram& histogram, size_t slot_id,
                        std::vector<JPEGHuffmanCode>* huff_codes);

void OptimizeHuffmanCodes(
    j_compress_ptr cinfo,
    const std::vector<std::vector<jpegli::coeff_t> >& coeffs,
    std::vector<JPEGHuffmanCode>* huffman_codes);

}  // namespace jpegli

#endif  // LIB_JPEGLI_ENTROPY_CODING_H_
