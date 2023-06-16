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

namespace jpegli {

void CopyHuffmanCodes(j_compress_ptr cinfo, bool* is_baseline);

size_t RestartIntervalForScan(j_compress_ptr cinfo, size_t scan_index);

struct Histogram {
  int count[kJpegHuffmanAlphabetSize];
  Histogram() { memset(count, 0, sizeof(count)); }
};

struct JpegClusteredHistograms {
  std::vector<Histogram> histograms;
  std::vector<uint32_t> histogram_indexes;
  std::vector<uint32_t> slot_ids;
};

void ClusterJpegHistograms(const Histogram* histograms, size_t num,
                           JpegClusteredHistograms* clusters);

void AddJpegHuffmanCode(const Histogram& histogram, size_t slot_id,
                        JPEGHuffmanCode* huff_codes, size_t* num_huff_codes);

void OptimizeHuffmanCodes(j_compress_ptr cinfo, bool* is_baseline);

}  // namespace jpegli

#endif  // LIB_JPEGLI_ENTROPY_CODING_H_
