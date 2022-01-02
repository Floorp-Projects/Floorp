// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// Functions for clustering similar histograms together.

#ifndef LIB_JXL_ENC_CLUSTER_H_
#define LIB_JXL_ENC_CLUSTER_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <vector>

#include "lib/jxl/ans_params.h"
#include "lib/jxl/enc_ans.h"

namespace jxl {

struct Histogram {
  Histogram() { total_count_ = 0; }
  void Clear() {
    data_.clear();
    total_count_ = 0;
  }
  void Add(size_t symbol) {
    if (data_.size() <= symbol) {
      data_.resize(DivCeil(symbol + 1, kRounding) * kRounding);
    }
    ++data_[symbol];
    ++total_count_;
  }
  void AddHistogram(const Histogram& other) {
    if (other.data_.size() > data_.size()) {
      data_.resize(other.data_.size());
    }
    for (size_t i = 0; i < other.data_.size(); ++i) {
      data_[i] += other.data_[i];
    }
    total_count_ += other.total_count_;
  }
  float PopulationCost() const {
    return ANSPopulationCost(data_.data(), data_.size());
  }
  float ShannonEntropy() const;

  std::vector<ANSHistBin> data_;
  size_t total_count_;
  mutable float entropy_;  // WARNING: not kept up-to-date.
  static constexpr size_t kRounding = 8;
};

void ClusterHistograms(HistogramParams params, const std::vector<Histogram>& in,
                       size_t num_contexts, size_t max_histograms,
                       std::vector<Histogram>* out,
                       std::vector<uint32_t>* histogram_symbols);
}  // namespace jxl

#endif  // LIB_JXL_ENC_CLUSTER_H_
