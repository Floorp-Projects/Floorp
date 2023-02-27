// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jpegli/entropy_coding.h"

#include "lib/jpegli/encode_internal.h"
#include "lib/jpegli/error.h"
#include "lib/jxl/enc_cluster.h"
#include "lib/jxl/enc_huffman_tree.h"

namespace jpegli {
namespace {

float HistogramCost(const jxl::Histogram& histo) {
  std::vector<uint32_t> counts(kJpegHuffmanAlphabetSize + 1);
  std::vector<uint8_t> depths(kJpegHuffmanAlphabetSize + 1);
  for (size_t i = 0; i < kJpegHuffmanAlphabetSize; ++i) {
    counts[i] = histo.data_[i];
  }
  counts[kJpegHuffmanAlphabetSize] = 1;
  jxl::CreateHuffmanTree(counts.data(), counts.size(), kJpegHuffmanMaxBitLength,
                         &depths[0]);
  size_t header_bits = (1 + kJpegHuffmanMaxBitLength) * 8;
  size_t data_bits = 0;
  for (size_t i = 0; i < kJpegHuffmanAlphabetSize; ++i) {
    if (depths[i] > 0) {
      header_bits += 8;
      data_bits += counts[i] * depths[i];
    }
  }
  return header_bits + data_bits;
}
}  // namespace

void ClusterJpegHistograms(const Histogram* histo_data, size_t num,
                           JpegClusteredHistograms* clusters) {
  std::vector<jxl::Histogram> histograms;
  for (size_t idx = 0; idx < num; ++idx) {
    jxl::Histogram histo;
    histo.data_.resize(kJpegHuffmanAlphabetSize);
    for (size_t i = 0; i < histo.data_.size(); ++i) {
      histo.data_[i] = histo_data[idx].count[i];
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

void BuildJpegHuffmanCode(const jxl::Histogram& histo, JPEGHuffmanCode* huff) {
  std::vector<uint32_t> counts(kJpegHuffmanAlphabetSize + 1);
  std::vector<uint8_t> depths(kJpegHuffmanAlphabetSize + 1);
  for (size_t j = 0; j < kJpegHuffmanAlphabetSize; ++j) {
    counts[j] = histo.data_[j];
  }
  counts[kJpegHuffmanAlphabetSize] = 1;
  jxl::CreateHuffmanTree(counts.data(), counts.size(), kJpegHuffmanMaxBitLength,
                         &depths[0]);
  std::fill(std::begin(huff->counts), std::end(huff->counts), 0);
  std::fill(std::begin(huff->values), std::end(huff->values), 0);
  for (size_t i = 0; i <= kJpegHuffmanAlphabetSize; ++i) {
    if (depths[i] > 0) {
      ++huff->counts[depths[i]];
    }
  }
  int offset[kJpegHuffmanMaxBitLength + 1] = {0};
  for (size_t i = 1; i <= kJpegHuffmanMaxBitLength; ++i) {
    offset[i] = offset[i - 1] + huff->counts[i - 1];
  }
  for (size_t i = 0; i <= kJpegHuffmanAlphabetSize; ++i) {
    if (depths[i] > 0) {
      huff->values[offset[depths[i]]++] = i;
    }
  }
}

void AddJpegHuffmanCode(const jxl::Histogram& histogram, size_t slot_id,
                        std::vector<JPEGHuffmanCode>* huff_codes) {
  JPEGHuffmanCode huff_code;
  huff_code.slot_id = slot_id;
  BuildJpegHuffmanCode(histogram, &huff_code);
  huff_codes->emplace_back(std::move(huff_code));
}

namespace {
void SetJpegHuffmanCode(const JpegClusteredHistograms& clusters,
                        size_t histogram_id, size_t slot_id_offset,
                        std::vector<uint32_t>& slot_histograms,
                        uint32_t* slot_id,
                        std::vector<JPEGHuffmanCode>* huff_codes) {
  JXL_ASSERT(histogram_id < clusters.histogram_indexes.size());
  uint32_t histogram_index = clusters.histogram_indexes[histogram_id];
  uint32_t id = clusters.slot_ids[histogram_index];
  *slot_id = id + (slot_id_offset / 4);
  if (slot_histograms[id] != histogram_index) {
    AddJpegHuffmanCode(clusters.histograms[histogram_index],
                       slot_id_offset + id, huff_codes);
    slot_histograms[id] = histogram_index;
  }
}

struct DCTState {
  int eob_run = 0;
  size_t num_refinement_bits = 0;
  Histogram* ac_histo = nullptr;
};

static JXL_INLINE void ProcessFlush(DCTState* s) {
  if (s->eob_run > 0) {
    int nbits = jxl::FloorLog2Nonzero<uint32_t>(s->eob_run);
    int symbol = nbits << 4u;
    ++s->ac_histo->count[symbol];
    s->eob_run = 0;
  }
  s->num_refinement_bits = 0;
}

static JXL_INLINE void ProcessEndOfBand(DCTState* s, size_t new_refinement_bits,
                                        Histogram* new_ac_histo) {
  if (s->eob_run == 0) {
    s->ac_histo = new_ac_histo;
  }
  ++s->eob_run;
  s->num_refinement_bits += new_refinement_bits;
  if (s->eob_run == 0x7FFF ||
      s->num_refinement_bits > kJPEGMaxCorrectionBits - kDCTBlockSize + 1) {
    ProcessFlush(s);
  }
}

bool ProcessDCTBlockSequential(const coeff_t* coeffs, Histogram* dc_histo,
                               Histogram* ac_histo, int num_zero_runs,
                               coeff_t* last_dc_coeff) {
  coeff_t temp2;
  coeff_t temp;
  temp2 = coeffs[0];
  temp = temp2 - *last_dc_coeff;
  *last_dc_coeff = temp2;
  temp2 = temp;
  if (temp < 0) {
    temp = -temp;
    if (temp < 0) return false;
    temp2--;
  }
  int dc_nbits = (temp == 0) ? 0 : (jxl::FloorLog2Nonzero<uint32_t>(temp) + 1);
  ++dc_histo->count[dc_nbits];
  if (dc_nbits >= 12) return false;
  int r = 0;
  for (int k = 1; k < 64; ++k) {
    if ((temp = coeffs[kJPEGNaturalOrder[k]]) == 0) {
      r++;
      continue;
    }
    if (temp < 0) {
      temp = -temp;
      if (temp < 0) return false;
      temp2 = ~temp;
    } else {
      temp2 = temp;
    }
    while (r > 15) {
      ++ac_histo->count[0xf0];
      r -= 16;
    }
    int ac_nbits = jxl::FloorLog2Nonzero<uint32_t>(temp) + 1;
    if (ac_nbits >= 16) return false;
    int symbol = (r << 4u) + ac_nbits;
    ++ac_histo->count[symbol];
    r = 0;
  }
  for (int i = 0; i < num_zero_runs; ++i) {
    ++ac_histo->count[0xf0];
    r -= 16;
  }
  if (r > 0) {
    ++ac_histo->count[0];
  }
  return true;
}

bool ProcessDCTBlockProgressive(const coeff_t* coeffs, Histogram* dc_histo,
                                Histogram* ac_histo, int Ss, int Se, int Al,
                                int num_zero_runs, DCTState* s,
                                coeff_t* last_dc_coeff) {
  bool eob_run_allowed = Ss > 0;
  coeff_t temp2;
  coeff_t temp;
  if (Ss == 0) {
    temp2 = coeffs[0] >> Al;
    temp = temp2 - *last_dc_coeff;
    *last_dc_coeff = temp2;
    temp2 = temp;
    if (temp < 0) {
      temp = -temp;
      if (temp < 0) return false;
      temp2--;
    }
    int nbits = (temp == 0) ? 0 : (jxl::FloorLog2Nonzero<uint32_t>(temp) + 1);
    ++dc_histo->count[nbits];
    ++Ss;
  }
  if (Ss > Se) {
    return true;
  }
  int r = 0;
  for (int k = Ss; k <= Se; ++k) {
    if ((temp = coeffs[kJPEGNaturalOrder[k]]) == 0) {
      r++;
      continue;
    }
    if (temp < 0) {
      temp = -temp;
      if (temp < 0) return false;
      temp >>= Al;
      temp2 = ~temp;
    } else {
      temp >>= Al;
      temp2 = temp;
    }
    if (temp == 0) {
      r++;
      continue;
    }
    ProcessFlush(s);
    while (r > 15) {
      ++ac_histo->count[0xf0];
      r -= 16;
    }
    int nbits = jxl::FloorLog2Nonzero<uint32_t>(temp) + 1;
    int symbol = (r << 4u) + nbits;
    ++ac_histo->count[symbol];
    r = 0;
  }
  if (num_zero_runs > 0) {
    ProcessFlush(s);
    for (int i = 0; i < num_zero_runs; ++i) {
      ++ac_histo->count[0xf0];
      r -= 16;
    }
  }
  if (r > 0) {
    ProcessEndOfBand(s, 0, ac_histo);
    if (!eob_run_allowed) {
      ProcessFlush(s);
    }
  }
  return true;
}

bool ProcessRefinementBits(const coeff_t* coeffs, Histogram* ac_histo, int Ss,
                           int Se, int Al, DCTState* s) {
  bool eob_run_allowed = Ss > 0;
  if (Ss == 0) {
    ++Ss;
  }
  if (Ss > Se) {
    return true;
  }
  int abs_values[kDCTBlockSize];
  int eob = 0;
  for (int k = Ss; k <= Se; k++) {
    const coeff_t abs_val = std::abs(coeffs[kJPEGNaturalOrder[k]]);
    abs_values[k] = abs_val >> Al;
    if (abs_values[k] == 1) {
      eob = k;
    }
  }
  int r = 0;
  size_t num_refinement_bits = 0;
  for (int k = Ss; k <= Se; k++) {
    if (abs_values[k] == 0) {
      r++;
      continue;
    }
    while (r > 15 && k <= eob) {
      ProcessFlush(s);
      ++ac_histo->count[0xf0];
      r -= 16;
      num_refinement_bits = 0;
    }
    if (abs_values[k] > 1) {
      ++num_refinement_bits;
      continue;
    }
    ProcessFlush(s);
    int symbol = (r << 4u) + 1;
    ++ac_histo->count[symbol];
    num_refinement_bits = 0;
    r = 0;
  }
  if (r > 0 || num_refinement_bits > 0) {
    ProcessEndOfBand(s, num_refinement_bits, ac_histo);
    if (!eob_run_allowed) {
      ProcessFlush(s);
    }
  }
  return true;
}

bool ProcessScan(j_compress_ptr cinfo,
                 const std::vector<std::vector<jpegli::coeff_t> >& coeffs,
                 size_t scan_index, int* histo_index, Histogram* dc_histograms,
                 Histogram* ac_histograms) {
  size_t restart_interval = RestartIntervalForScan(cinfo, scan_index);
  int restarts_to_go = restart_interval;
  coeff_t last_dc_coeff[MAX_COMPS_IN_SCAN] = {0};
  DCTState s;

  const jpeg_scan_info* scan_info = &cinfo->scan_info[scan_index];
  // "Non-interleaved" means color data comes in separate scans, in other words
  // each scan can contain only one color component.
  const bool is_interleaved = (scan_info->comps_in_scan > 1);
  jpeg_component_info* base_comp =
      &cinfo->comp_info[scan_info->component_index[0]];
  // h_group / v_group act as numerators for converting number of blocks to
  // number of MCU. In interleaved mode it is 1, so MCU is represented with
  // max_*_samp_factor blocks. In non-interleaved mode we choose numerator to
  // be the samping factor, consequently MCU is always represented with single
  // block.
  const int h_group = is_interleaved ? 1 : base_comp->h_samp_factor;
  const int v_group = is_interleaved ? 1 : base_comp->v_samp_factor;
  int MCUs_per_row =
      DivCeil(cinfo->image_width * h_group, 8 * cinfo->max_h_samp_factor);
  int MCU_rows =
      DivCeil(cinfo->image_height * v_group, 8 * cinfo->max_v_samp_factor);
  const bool is_progressive = cinfo->progressive_mode;
  const int Al = scan_info->Al;
  const int Ah = scan_info->Ah;
  const int Ss = scan_info->Ss;
  const int Se = scan_info->Se;
  constexpr coeff_t kDummyBlock[DCTSIZE2] = {0};

  for (int mcu_y = 0; mcu_y < MCU_rows; ++mcu_y) {
    for (int mcu_x = 0; mcu_x < MCUs_per_row; ++mcu_x) {
      // Possibly emit a restart marker.
      if (restart_interval > 0 && restarts_to_go == 0) {
        ProcessFlush(&s);
        restarts_to_go = restart_interval;
        memset(last_dc_coeff, 0, sizeof(last_dc_coeff));
      }
      // Encode one MCU
      for (int i = 0; i < scan_info->comps_in_scan; ++i) {
        int comp_idx = scan_info->component_index[i];
        jpeg_component_info* comp = &cinfo->comp_info[comp_idx];
        int histo_idx = *histo_index + i;
        Histogram* dc_histo = &dc_histograms[histo_idx];
        Histogram* ac_histo = &ac_histograms[histo_idx];
        int n_blocks_y = is_interleaved ? comp->v_samp_factor : 1;
        int n_blocks_x = is_interleaved ? comp->h_samp_factor : 1;
        for (int iy = 0; iy < n_blocks_y; ++iy) {
          for (int ix = 0; ix < n_blocks_x; ++ix) {
            size_t block_y = mcu_y * n_blocks_y + iy;
            size_t block_x = mcu_x * n_blocks_x + ix;
            size_t block_idx = block_y * comp->width_in_blocks + block_x;
            size_t num_zero_runs = 0;
            const coeff_t* block = &coeffs[comp_idx][block_idx << 6];
            if (block_x >= comp->width_in_blocks ||
                block_y >= comp->height_in_blocks) {
              block = kDummyBlock;
            }
            bool ok;
            if (!is_progressive) {
              ok = ProcessDCTBlockSequential(block, dc_histo, ac_histo,
                                             num_zero_runs, last_dc_coeff + i);
            } else if (Ah == 0) {
              ok = ProcessDCTBlockProgressive(block, dc_histo, ac_histo, Ss, Se,
                                              Al, num_zero_runs, &s,
                                              last_dc_coeff + i);
            } else {
              ok = ProcessRefinementBits(block, ac_histo, Ss, Se, Al, &s);
            }
            if (!ok) return false;
          }
        }
      }
      --restarts_to_go;
    }
  }
  ProcessFlush(&s);
  *histo_index += scan_info->comps_in_scan;
  return true;
}

void ProcessJpeg(j_compress_ptr cinfo,
                 const std::vector<std::vector<jpegli::coeff_t> >& coeffs,
                 std::vector<Histogram>* dc_histograms,
                 std::vector<Histogram>* ac_histograms) {
  int histo_index = 0;
  for (int i = 0; i < cinfo->num_scans; ++i) {
    if (!ProcessScan(cinfo, coeffs, i, &histo_index, &(*dc_histograms)[0],
                     &(*ac_histograms)[0])) {
      JPEGLI_ERROR("Invalid scan.");
    }
  }
}

void ValidateHuffmanTable(j_compress_ptr cinfo, const JHUFF_TBL* table,
                          bool is_dc) {
  size_t total_symbols = 0;
  size_t total_p = 0;
  size_t max_depth = 0;
  for (size_t d = 1; d <= kJpegHuffmanMaxBitLength; ++d) {
    uint8_t count = table->bits[d];
    if (count) {
      total_symbols += count;
      total_p += (1u << (kJpegHuffmanMaxBitLength - d)) * count;
      max_depth = d;
    }
  }
  total_p += 1u << (kJpegHuffmanMaxBitLength - max_depth);  // sentinel symbol
  if (total_symbols == 0) {
    JPEGLI_ERROR("Empty Huffman table");
  }
  if (total_symbols > kJpegHuffmanAlphabetSize) {
    JPEGLI_ERROR("Too many symbols in Huffman table");
  }
  if (total_p != (1u << kJpegHuffmanMaxBitLength)) {
    JPEGLI_ERROR("Invalid bit length distribution");
  }
  uint8_t symbol_seen[kJpegHuffmanAlphabetSize] = {};
  for (size_t i = 0; i < total_symbols; ++i) {
    uint8_t symbol = table->huffval[i];
    if (symbol_seen[symbol]) {
      JPEGLI_ERROR("Duplicate symbol %d in Huffman table", symbol);
    }
    symbol_seen[symbol] = 1;
  }
}

void CopyHuffmanTable(j_compress_ptr cinfo, int index, bool is_dc,
                      std::vector<JPEGHuffmanCode>* huffman_codes) {
  const char* type = is_dc ? "DC" : "AC";
  if (index < 0 || index >= NUM_HUFF_TBLS) {
    JPEGLI_ERROR("Invalid %s Huffman table index %d", type, index);
  }
  JHUFF_TBL* table =
      is_dc ? cinfo->dc_huff_tbl_ptrs[index] : cinfo->ac_huff_tbl_ptrs[index];
  if (table == nullptr) {
    JPEGLI_ERROR("Missing %s Huffman table %d", table, index);
  }
  ValidateHuffmanTable(cinfo, table, is_dc);
  JPEGHuffmanCode huff;
  size_t max_depth = 0;
  for (size_t i = 1; i <= kJpegHuffmanMaxBitLength; ++i) {
    if (table->bits[i] != 0) max_depth = i;
    huff.counts[i] = table->bits[i];
  }
  ++huff.counts[max_depth];
  for (size_t i = 0; i < kJpegHuffmanAlphabetSize; ++i) {
    huff.values[i] = table->huffval[i];
  }
  huff.slot_id = index + (is_dc ? 0 : 0x10);
  huff.sent_table = table->sent_table;
  bool have_slot = false;
  for (size_t i = 0; i < huffman_codes->size(); ++i) {
    if ((*huffman_codes)[i].slot_id == huff.slot_id) have_slot = true;
  }
  if (!have_slot) {
    huffman_codes->emplace_back(std::move(huff));
  }
}

}  // namespace

void AddStandardHuffmanTables(j_compress_ptr cinfo, bool is_dc) {
  // Huffman tables from the JPEG standard.
  static constexpr JHUFF_TBL kStandardDCTables[2] = {
      // DC luma
      {{0, 0, 1, 5, 1, 1, 1, 1, 1, 1},
       {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11},
       FALSE},
      // DC chroma
      {{0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1},
       {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11},
       FALSE}};
  static constexpr JHUFF_TBL kStandardACTables[2] = {
      // AC luma
      {{0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 125},
       {0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06,
        0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
        0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0, 0x24, 0x33, 0x62, 0x72,
        0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
        0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45,
        0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
        0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75,
        0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
        0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3,
        0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
        0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9,
        0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
        0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4,
        0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa},
       FALSE},
      // AC chroma
      {{0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 119},
       {0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41,
        0x51, 0x07, 0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
        0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0, 0x15, 0x62, 0x72, 0xd1,
        0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
        0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44,
        0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
        0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74,
        0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
        0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a,
        0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
        0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
        0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
        0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4,
        0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa},
       FALSE}};
  const JHUFF_TBL* std_tables = is_dc ? kStandardDCTables : kStandardACTables;
  JHUFF_TBL** tables =
      is_dc ? cinfo->dc_huff_tbl_ptrs : cinfo->ac_huff_tbl_ptrs;
  for (int i = 0; i < 2; ++i) {
    if (tables[i] == nullptr) {
      tables[i] =
          jpegli_alloc_huff_table(reinterpret_cast<j_common_ptr>(cinfo));
      memcpy(tables[i], &std_tables[i], sizeof(JHUFF_TBL));
      ValidateHuffmanTable(cinfo, tables[i], is_dc);
    }
  }
}

void CopyHuffmanCodes(j_compress_ptr cinfo,
                      std::vector<JPEGHuffmanCode>* huffman_codes) {
  for (int c = 0; c < cinfo->num_components; ++c) {
    jpeg_component_info* comp = &cinfo->comp_info[c];
    CopyHuffmanTable(cinfo, comp->dc_tbl_no, /*is_dc=*/true, huffman_codes);
    CopyHuffmanTable(cinfo, comp->ac_tbl_no, /*is_dc=*/false, huffman_codes);
  }
  for (int i = 0; i < cinfo->num_scans; ++i) {
    const jpeg_scan_info* si = &cinfo->scan_info[i];
    ScanCodingInfo sci;
    for (int j = 0; j < si->comps_in_scan; ++j) {
      int ci = si->component_index[j];
      sci.dc_tbl_idx[j] = cinfo->comp_info[ci].dc_tbl_no;
      sci.ac_tbl_idx[j] = cinfo->comp_info[ci].ac_tbl_no + 4;
    }
    if (i == 0) {
      sci.num_huffman_codes = huffman_codes->size();
    }
    cinfo->master->scan_coding_info.emplace_back(std::move(sci));
  }
}

size_t RestartIntervalForScan(j_compress_ptr cinfo, size_t scan_index) {
  if (cinfo->restart_in_rows <= 0) {
    return cinfo->restart_interval;
  } else {
    const jpeg_scan_info* scan_info = &cinfo->scan_info[scan_index];
    const bool is_interleaved = (scan_info->comps_in_scan > 1);
    jpeg_component_info* base_comp =
        &cinfo->comp_info[scan_info->component_index[0]];
    const int h_group = is_interleaved ? 1 : base_comp->h_samp_factor;
    int MCUs_per_row =
        DivCeil(cinfo->image_width * h_group, 8 * cinfo->max_h_samp_factor);
    return std::min<size_t>(MCUs_per_row * cinfo->restart_in_rows, 65535u);
  }
}

void OptimizeHuffmanCodes(
    j_compress_ptr cinfo,
    const std::vector<std::vector<jpegli::coeff_t> >& coeffs,
    std::vector<JPEGHuffmanCode>* huffman_codes) {
  // Gather histograms.
  size_t num_histo = 0;
  for (int i = 0; i < cinfo->num_scans; ++i) {
    num_histo += cinfo->scan_info[i].comps_in_scan;
  }
  std::vector<Histogram> dc_histograms(num_histo);
  std::vector<Histogram> ac_histograms(num_histo);
  ProcessJpeg(cinfo, coeffs, &dc_histograms, &ac_histograms);

  // Cluster DC histograms.
  JpegClusteredHistograms dc_clusters;
  ClusterJpegHistograms(dc_histograms.data(), dc_histograms.size(),
                        &dc_clusters);

  // Cluster AC histograms.
  JpegClusteredHistograms ac_clusters;
  ClusterJpegHistograms(ac_histograms.data(), ac_histograms.size(),
                        &ac_clusters);

  // Add the first 4 DC and AC histograms in the first DHT segment.
  std::vector<uint32_t> dc_slot_histograms;
  std::vector<uint32_t> ac_slot_histograms;
  for (size_t i = 0; i < dc_clusters.histograms.size(); ++i) {
    if (i >= 4) break;
    JXL_ASSERT(dc_clusters.slot_ids[i] == i);
    AddJpegHuffmanCode(dc_clusters.histograms[i], i, huffman_codes);
    dc_slot_histograms.push_back(i);
  }
  for (size_t i = 0; i < ac_clusters.histograms.size(); ++i) {
    if (i >= 4) break;
    JXL_ASSERT(ac_clusters.slot_ids[i] == i);
    AddJpegHuffmanCode(ac_clusters.histograms[i], 0x10 + i, huffman_codes);
    ac_slot_histograms.push_back(i);
  }

  // Set the Huffman table indexes in the scan_infos and emit additional DHT
  // segments if necessary.
  size_t histogram_id = 0;
  size_t num_huffman_codes_sent = 0;
  for (int i = 0; i < cinfo->num_scans; ++i) {
    ScanCodingInfo sci;
    for (int c = 0; c < cinfo->scan_info[i].comps_in_scan; ++c) {
      SetJpegHuffmanCode(dc_clusters, histogram_id, 0, dc_slot_histograms,
                         &sci.dc_tbl_idx[c], huffman_codes);
      SetJpegHuffmanCode(ac_clusters, histogram_id, 0x10, ac_slot_histograms,
                         &sci.ac_tbl_idx[c], huffman_codes);
      ++histogram_id;
    }
    sci.num_huffman_codes = huffman_codes->size() - num_huffman_codes_sent;
    num_huffman_codes_sent = huffman_codes->size();
    cinfo->master->scan_coding_info.emplace_back(std::move(sci));
  }
}

}  // namespace jpegli
