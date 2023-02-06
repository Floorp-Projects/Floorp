// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jpegli/entropy_coding.h"

#include "lib/jxl/enc_cluster.h"
#include "lib/jxl/huffman_tree.h"
#include "lib/jxl/jpeg/dec_jpeg_data_writer.h"

namespace jpegli {
namespace {

float HistogramCost(const jxl::Histogram& histo) {
  std::vector<uint32_t> counts(jxl::jpeg::kJpegHuffmanAlphabetSize + 1);
  std::vector<uint8_t> depths(jxl::jpeg::kJpegHuffmanAlphabetSize + 1);
  for (size_t i = 0; i < jxl::jpeg::kJpegHuffmanAlphabetSize; ++i) {
    counts[i] = histo.data_[i];
  }
  counts[jxl::jpeg::kJpegHuffmanAlphabetSize] = 1;
  jxl::CreateHuffmanTree(counts.data(), counts.size(),
                         jxl::jpeg::kJpegHuffmanMaxBitLength, &depths[0]);
  size_t header_bits = (1 + jxl::jpeg::kJpegHuffmanMaxBitLength) * 8;
  size_t data_bits = 0;
  for (size_t i = 0; i < jxl::jpeg::kJpegHuffmanAlphabetSize; ++i) {
    if (depths[i] > 0) {
      header_bits += 8;
      data_bits += counts[i] * depths[i];
    }
  }
  return header_bits + data_bits;
}

struct JpegClusteredHistograms {
  std::vector<jxl::Histogram> histograms;
  std::vector<uint32_t> histogram_indexes;
  std::vector<uint32_t> slot_ids;
};

void ClusterJpegHistograms(
    const std::vector<jxl::jpeg::HuffmanCodeTable>& jpeg_in,
    JpegClusteredHistograms* clusters) {
  std::vector<jxl::Histogram> histograms;
  for (const auto& t : jpeg_in) {
    jxl::Histogram histo;
    histo.data_.resize(jxl::jpeg::kJpegHuffmanAlphabetSize);
    for (size_t i = 0; i < histo.data_.size(); ++i) {
      histo.data_[i] = t.depth[i];
      histo.total_count_ += histo.data_[i];
    }
    histograms.push_back(histo);
  }
  clusters->histogram_indexes.resize(histograms.size());
  std::vector<uint32_t> slot_histograms;
  std::vector<float> slot_costs;
  for (size_t i = 0; i < histograms.size(); ++i) {
    const jxl::Histogram& cur = histograms[i];
    if (cur.total_count_ == 0) {
      continue;
    }
    float best_cost = HistogramCost(cur);
    size_t best_slot = slot_histograms.size();
    for (size_t j = 0; j < slot_histograms.size(); ++j) {
      size_t prev_idx = slot_histograms[j];
      const jxl::Histogram& prev = clusters->histograms[prev_idx];
      jxl::Histogram combined;
      combined.AddHistogram(prev);
      combined.AddHistogram(cur);
      float combined_cost = HistogramCost(combined);
      float cost = combined_cost - slot_costs[j];
      if (cost < best_cost) {
        best_cost = cost;
        best_slot = j;
      }
    }
    if (best_slot == slot_histograms.size()) {
      // Create new histogram.
      size_t histogram_index = clusters->histograms.size();
      clusters->histograms.push_back(cur);
      clusters->histogram_indexes[i] = histogram_index;
      if (best_slot < 4) {
        // We have a free slot, so we put the new histogram there.
        slot_histograms.push_back(histogram_index);
        slot_costs.push_back(best_cost);
      } else {
        // TODO(szabadka) Find the best histogram to replce.
        best_slot = (clusters->slot_ids.back() + 1) % 4;
      }
      slot_histograms[best_slot] = histogram_index;
      slot_costs[best_slot] = best_cost;
      clusters->slot_ids.push_back(best_slot);
    } else {
      // Merge this histogram with a previous one.
      size_t histogram_index = slot_histograms[best_slot];
      clusters->histograms[histogram_index].AddHistogram(cur);
      clusters->histogram_indexes[i] = histogram_index;
      JXL_ASSERT(clusters->slot_ids[histogram_index] == best_slot);
      slot_costs[best_slot] += best_cost;
    }
  }
}

void BuildJpegHuffmanCode(const jxl::Histogram& histo,
                          jxl::jpeg::JPEGHuffmanCode* huff) {
  std::vector<uint32_t> counts(jxl::jpeg::kJpegHuffmanAlphabetSize + 1);
  std::vector<uint8_t> depths(jxl::jpeg::kJpegHuffmanAlphabetSize + 1);
  for (size_t j = 0; j < jxl::jpeg::kJpegHuffmanAlphabetSize; ++j) {
    counts[j] = histo.data_[j];
  }
  counts[jxl::jpeg::kJpegHuffmanAlphabetSize] = 1;
  jxl::CreateHuffmanTree(counts.data(), counts.size(),
                         jxl::jpeg::kJpegHuffmanMaxBitLength, &depths[0]);
  std::fill(std::begin(huff->counts), std::end(huff->counts), 0);
  std::fill(std::begin(huff->values), std::end(huff->values), 0);
  for (size_t i = 0; i <= jxl::jpeg::kJpegHuffmanAlphabetSize; ++i) {
    if (depths[i] > 0) {
      ++huff->counts[depths[i]];
    }
  }
  int offset[jxl::jpeg::kJpegHuffmanMaxBitLength + 1] = {0};
  for (size_t i = 1; i <= jxl::jpeg::kJpegHuffmanMaxBitLength; ++i) {
    offset[i] = offset[i - 1] + huff->counts[i - 1];
  }
  for (size_t i = 0; i <= jxl::jpeg::kJpegHuffmanAlphabetSize; ++i) {
    if (depths[i] > 0) {
      huff->values[offset[depths[i]]++] = i;
    }
  }
  huff->is_last = false;
}

void AddJpegHuffmanCode(const jxl::Histogram& histogram, size_t slot_id,
                        std::vector<jxl::jpeg::JPEGHuffmanCode>* huff_codes) {
  jxl::jpeg::JPEGHuffmanCode huff_code;
  huff_code.slot_id = slot_id;
  BuildJpegHuffmanCode(histogram, &huff_code);
  huff_codes->emplace_back(std::move(huff_code));
}

void SetJpegHuffmanCode(const JpegClusteredHistograms& clusters,
                        size_t histogram_id, size_t slot_id_offset,
                        std::vector<uint32_t>& slot_histograms,
                        uint32_t* slot_id,
                        std::vector<jxl::jpeg::JPEGHuffmanCode>* huff_codes) {
  JXL_ASSERT(histogram_id < clusters.histogram_indexes.size());
  uint32_t histogram_index = clusters.histogram_indexes[histogram_id];
  *slot_id = clusters.slot_ids[histogram_index];
  if (slot_histograms[*slot_id] != histogram_index) {
    AddJpegHuffmanCode(clusters.histograms[histogram_index],
                       slot_id_offset + *slot_id, huff_codes);
    slot_histograms[*slot_id] = histogram_index;
  }
}

}  // namespace

void OptimizeHuffmanCodes(jxl::jpeg::JPEGData* out) {
  // Gather histograms.
  jxl::jpeg::SerializationState ss;
  JXL_CHECK(jxl::jpeg::ProcessJpeg(*out, &ss));

  // Cluster DC histograms.
  JpegClusteredHistograms dc_clusters;
  ClusterJpegHistograms(ss.dc_huff_table, &dc_clusters);

  // Cluster AC histograms.
  JpegClusteredHistograms ac_clusters;
  ClusterJpegHistograms(ss.ac_huff_table, &ac_clusters);

  // Rebuild marker_order because we may want to emit Huffman codes in between
  // scans.
  out->marker_order.resize(out->marker_order.size() - 2 -
                           out->scan_info.size());

  // Add the first 4 DC and AC histograms in the first DHT segment.
  out->marker_order.emplace_back(0xc4);  // DHT
  std::vector<uint32_t> dc_slot_histograms;
  std::vector<uint32_t> ac_slot_histograms;
  for (size_t i = 0; i < dc_clusters.histograms.size(); ++i) {
    if (i >= 4) break;
    JXL_ASSERT(dc_clusters.slot_ids[i] == i);
    AddJpegHuffmanCode(dc_clusters.histograms[i], i, &out->huffman_code);
    dc_slot_histograms.push_back(i);
  }
  for (size_t i = 0; i < ac_clusters.histograms.size(); ++i) {
    if (i >= 4) break;
    JXL_ASSERT(ac_clusters.slot_ids[i] == i);
    AddJpegHuffmanCode(ac_clusters.histograms[i], 0x10 + i, &out->huffman_code);
    ac_slot_histograms.push_back(i);
  }
  out->huffman_code.back().is_last = true;

  // Set the Huffman table indexes in the scan_infos and emit additional DHT
  // segments if necessary.
  size_t histogram_id = 0;
  for (auto& si : out->scan_info) {
    size_t num_huff_codes = out->huffman_code.size();
    for (size_t c = 0; c < si.num_components; ++c) {
      SetJpegHuffmanCode(dc_clusters, histogram_id, 0, dc_slot_histograms,
                         &si.components[c].dc_tbl_idx, &out->huffman_code);
      SetJpegHuffmanCode(ac_clusters, histogram_id, 0x10, ac_slot_histograms,
                         &si.components[c].ac_tbl_idx, &out->huffman_code);
      ++histogram_id;
    }
    // If we updated a slot histogram in this scan, create an additional DHT
    // segment.
    if (out->huffman_code.size() > num_huff_codes) {
      out->marker_order.emplace_back(0xc4);  // DHT
      out->huffman_code.back().is_last = true;
    }
    out->marker_order.emplace_back(0xda);  // SOS
  }
  out->marker_order.emplace_back(0xd9);  // EOI
}

}  // namespace jpegli
