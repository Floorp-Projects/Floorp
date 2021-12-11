// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/enc_cluster.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <queue>
#include <tuple>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/enc_cluster.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/ac_context.h"
#include "lib/jxl/base/profiler.h"
#include "lib/jxl/fast_math-inl.h"
HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

template <class V>
V Entropy(V count, V inv_total, V total) {
  const HWY_CAPPED(float, Histogram::kRounding) d;
  const auto zero = Set(d, 0.0f);
  return IfThenZeroElse(count == total,
                        zero - count * FastLog2f(d, inv_total * count));
}

void HistogramEntropy(const Histogram& a) {
  a.entropy_ = 0.0f;
  if (a.total_count_ == 0) return;

  const HWY_CAPPED(float, Histogram::kRounding) df;
  const HWY_CAPPED(int32_t, Histogram::kRounding) di;

  const auto inv_tot = Set(df, 1.0f / a.total_count_);
  auto entropy_lanes = Zero(df);
  auto total = Set(df, a.total_count_);

  for (size_t i = 0; i < a.data_.size(); i += Lanes(di)) {
    const auto counts = LoadU(di, &a.data_[i]);
    entropy_lanes += Entropy(ConvertTo(df, counts), inv_tot, total);
  }
  a.entropy_ += GetLane(SumOfLanes(entropy_lanes));
}

float HistogramDistance(const Histogram& a, const Histogram& b) {
  if (a.total_count_ == 0 || b.total_count_ == 0) return 0;

  const HWY_CAPPED(float, Histogram::kRounding) df;
  const HWY_CAPPED(int32_t, Histogram::kRounding) di;

  const auto inv_tot = Set(df, 1.0f / (a.total_count_ + b.total_count_));
  auto distance_lanes = Zero(df);
  auto total = Set(df, a.total_count_ + b.total_count_);

  for (size_t i = 0; i < std::max(a.data_.size(), b.data_.size());
       i += Lanes(di)) {
    const auto a_counts =
        a.data_.size() > i ? LoadU(di, &a.data_[i]) : Zero(di);
    const auto b_counts =
        b.data_.size() > i ? LoadU(di, &b.data_[i]) : Zero(di);
    const auto counts = ConvertTo(df, a_counts + b_counts);
    distance_lanes += Entropy(counts, inv_tot, total);
  }
  const float total_distance = GetLane(SumOfLanes(distance_lanes));
  return total_distance - a.entropy_ - b.entropy_;
}

// First step of a k-means clustering with a fancy distance metric.
void FastClusterHistograms(const std::vector<Histogram>& in,
                           const size_t num_contexts_in, size_t max_histograms,
                           float min_distance, std::vector<Histogram>* out,
                           std::vector<uint32_t>* histogram_symbols) {
  PROFILER_FUNC;
  size_t largest_idx = 0;
  std::vector<uint32_t> nonempty_histograms;
  nonempty_histograms.reserve(in.size());
  for (size_t i = 0; i < num_contexts_in; i++) {
    if (in[i].total_count_ == 0) continue;
    HistogramEntropy(in[i]);
    if (in[i].total_count_ > in[largest_idx].total_count_) {
      largest_idx = i;
    }
    nonempty_histograms.push_back(i);
  }
  // No symbols.
  if (nonempty_histograms.empty()) {
    out->resize(1);
    histogram_symbols->clear();
    histogram_symbols->resize(in.size(), 0);
    return;
  }
  largest_idx = std::find(nonempty_histograms.begin(),
                          nonempty_histograms.end(), largest_idx) -
                nonempty_histograms.begin();
  size_t num_contexts = nonempty_histograms.size();
  out->clear();
  out->reserve(max_histograms);
  std::vector<float> dists(num_contexts, std::numeric_limits<float>::max());
  histogram_symbols->clear();
  histogram_symbols->resize(in.size(), max_histograms);

  while (out->size() < max_histograms && out->size() < num_contexts) {
    (*histogram_symbols)[nonempty_histograms[largest_idx]] = out->size();
    out->push_back(in[nonempty_histograms[largest_idx]]);
    largest_idx = 0;
    for (size_t i = 0; i < num_contexts; i++) {
      dists[i] = std::min(
          HistogramDistance(in[nonempty_histograms[i]], out->back()), dists[i]);
      // Avoid repeating histograms
      if ((*histogram_symbols)[nonempty_histograms[i]] != max_histograms) {
        continue;
      }
      if (dists[i] > dists[largest_idx]) largest_idx = i;
    }
    if (dists[largest_idx] < min_distance) break;
  }

  for (size_t i = 0; i < num_contexts_in; i++) {
    if ((*histogram_symbols)[i] != max_histograms) continue;
    if (in[i].total_count_ == 0) {
      (*histogram_symbols)[i] = 0;
      continue;
    }
    size_t best = 0;
    float best_dist = HistogramDistance(in[i], (*out)[best]);
    for (size_t j = 1; j < out->size(); j++) {
      float dist = HistogramDistance(in[i], (*out)[j]);
      if (dist < best_dist) {
        best = j;
        best_dist = dist;
      }
    }
    (*out)[best].AddHistogram(in[i]);
    HistogramEntropy((*out)[best]);
    (*histogram_symbols)[i] = best;
  }
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {
HWY_EXPORT(FastClusterHistograms);  // Local function
HWY_EXPORT(HistogramEntropy);       // Local function

float Histogram::ShannonEntropy() const {
  HWY_DYNAMIC_DISPATCH(HistogramEntropy)(*this);
  return entropy_;
}

namespace {
// -----------------------------------------------------------------------------
// Histogram refinement

// Reorder histograms in *out so that the new symbols in *symbols come in
// increasing order.
void HistogramReindex(std::vector<Histogram>* out,
                      std::vector<uint32_t>* symbols) {
  std::vector<Histogram> tmp(*out);
  std::map<int, int> new_index;
  int next_index = 0;
  for (uint32_t symbol : *symbols) {
    if (new_index.find(symbol) == new_index.end()) {
      new_index[symbol] = next_index;
      (*out)[next_index] = tmp[symbol];
      ++next_index;
    }
  }
  out->resize(next_index);
  for (uint32_t& symbol : *symbols) {
    symbol = new_index[symbol];
  }
}

}  // namespace

// Clusters similar histograms in 'in' together, the selected histograms are
// placed in 'out', and for each index in 'in', *histogram_symbols will
// indicate which of the 'out' histograms is the best approximation.
void ClusterHistograms(const HistogramParams params,
                       const std::vector<Histogram>& in,
                       const size_t num_contexts, size_t max_histograms,
                       std::vector<Histogram>* out,
                       std::vector<uint32_t>* histogram_symbols) {
  constexpr float kMinDistanceForDistinctFast = 64.0f;
  constexpr float kMinDistanceForDistinctBest = 16.0f;
  max_histograms = std::min(max_histograms, params.max_histograms);
  if (params.clustering == HistogramParams::ClusteringType::kFastest) {
    HWY_DYNAMIC_DISPATCH(FastClusterHistograms)
    (in, num_contexts, 4, kMinDistanceForDistinctFast, out, histogram_symbols);
  } else if (params.clustering == HistogramParams::ClusteringType::kFast) {
    HWY_DYNAMIC_DISPATCH(FastClusterHistograms)
    (in, num_contexts, max_histograms, kMinDistanceForDistinctFast, out,
     histogram_symbols);
  } else {
    PROFILER_FUNC;
    HWY_DYNAMIC_DISPATCH(FastClusterHistograms)
    (in, num_contexts, max_histograms, kMinDistanceForDistinctBest, out,
     histogram_symbols);
    for (size_t i = 0; i < out->size(); i++) {
      (*out)[i].entropy_ =
          ANSPopulationCost((*out)[i].data_.data(), (*out)[i].data_.size());
    }
    uint32_t next_version = 2;
    std::vector<uint32_t> version(out->size(), 1);
    std::vector<uint32_t> renumbering(out->size());
    std::iota(renumbering.begin(), renumbering.end(), 0);

    // Try to pair up clusters if doing so reduces the total cost.

    struct HistogramPair {
      // validity of a pair: p.version == max(version[i], version[j])
      float cost;
      uint32_t first;
      uint32_t second;
      uint32_t version;
      // We use > because priority queues sort in *decreasing* order, but we
      // want lower cost elements to appear first.
      bool operator<(const HistogramPair& other) const {
        return std::make_tuple(cost, first, second, version) >
               std::make_tuple(other.cost, other.first, other.second,
                               other.version);
      }
    };

    // Create list of all pairs by increasing merging cost.
    std::priority_queue<HistogramPair> pairs_to_merge;
    for (uint32_t i = 0; i < out->size(); i++) {
      for (uint32_t j = i + 1; j < out->size(); j++) {
        Histogram histo;
        histo.AddHistogram((*out)[i]);
        histo.AddHistogram((*out)[j]);
        float cost = ANSPopulationCost(histo.data_.data(), histo.data_.size()) -
                     (*out)[i].entropy_ - (*out)[j].entropy_;
        // Avoid enqueueing pairs that are not advantageous to merge.
        if (cost >= 0) continue;
        pairs_to_merge.push(
            HistogramPair{cost, i, j, std::max(version[i], version[j])});
      }
    }

    // Merge the best pair to merge, add new pairs that get formed as a
    // consequence.
    while (!pairs_to_merge.empty()) {
      uint32_t first = pairs_to_merge.top().first;
      uint32_t second = pairs_to_merge.top().second;
      uint32_t ver = pairs_to_merge.top().version;
      pairs_to_merge.pop();
      if (ver != std::max(version[first], version[second]) ||
          version[first] == 0 || version[second] == 0) {
        continue;
      }
      (*out)[first].AddHistogram((*out)[second]);
      (*out)[first].entropy_ = ANSPopulationCost((*out)[first].data_.data(),
                                                 (*out)[first].data_.size());
      for (size_t i = 0; i < renumbering.size(); i++) {
        if (renumbering[i] == second) {
          renumbering[i] = first;
        }
      }
      version[second] = 0;
      version[first] = next_version++;
      for (uint32_t j = 0; j < out->size(); j++) {
        if (j == first) continue;
        if (version[j] == 0) continue;
        Histogram histo;
        histo.AddHistogram((*out)[first]);
        histo.AddHistogram((*out)[j]);
        float cost = ANSPopulationCost(histo.data_.data(), histo.data_.size()) -
                     (*out)[first].entropy_ - (*out)[j].entropy_;
        // Avoid enqueueing pairs that are not advantageous to merge.
        if (cost >= 0) continue;
        pairs_to_merge.push(
            HistogramPair{cost, std::min(first, j), std::max(first, j),
                          std::max(version[first], version[j])});
      }
    }
    std::vector<uint32_t> reverse_renumbering(out->size(), -1);
    size_t num_alive = 0;
    for (size_t i = 0; i < out->size(); i++) {
      if (version[i] == 0) continue;
      (*out)[num_alive++] = (*out)[i];
      reverse_renumbering[i] = num_alive - 1;
    }
    out->resize(num_alive);
    for (size_t i = 0; i < histogram_symbols->size(); i++) {
      (*histogram_symbols)[i] =
          reverse_renumbering[renumbering[(*histogram_symbols)[i]]];
    }
  }

  // Convert the context map to a canonical form.
  HistogramReindex(out, histogram_symbols);
}

}  // namespace jxl
#endif  // HWY_ONCE
