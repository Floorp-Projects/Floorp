// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jpegli/entropy_coding.h"

#include "lib/jpegli/encode_internal.h"
#include "lib/jpegli/error.h"
#include "lib/jpegli/huffman.h"
#include "lib/jxl/base/bits.h"

namespace jpegli {
namespace {

float HistogramCost(const Histogram& histo) {
  std::vector<uint32_t> counts(kJpegHuffmanAlphabetSize + 1);
  std::vector<uint8_t> depths(kJpegHuffmanAlphabetSize + 1);
  for (size_t i = 0; i < kJpegHuffmanAlphabetSize; ++i) {
    counts[i] = histo.count[i];
  }
  counts[kJpegHuffmanAlphabetSize] = 1;
  CreateHuffmanTree(counts.data(), counts.size(), kJpegHuffmanMaxBitLength,
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

void AddHistograms(const Histogram& a, const Histogram& b, Histogram* c) {
  for (size_t i = 0; i < kJpegHuffmanAlphabetSize; ++i) {
    c->count[i] = a.count[i] + b.count[i];
  }
}

bool IsEmptyHistogram(const Histogram& histo) {
  for (size_t i = 0; i < kJpegHuffmanAlphabetSize; ++i) {
    if (histo.count[i]) return false;
  }
  return true;
}

}  // namespace

void ClusterJpegHistograms(const Histogram* histograms, size_t num,
                           JpegClusteredHistograms* clusters) {
  clusters->histogram_indexes.resize(num);
  std::vector<uint32_t> slot_histograms;
  std::vector<float> slot_costs;
  for (size_t i = 0; i < num; ++i) {
    const Histogram& cur = histograms[i];
    if (IsEmptyHistogram(cur)) {
      continue;
    }
    float best_cost = HistogramCost(cur);
    size_t best_slot = slot_histograms.size();
    for (size_t j = 0; j < slot_histograms.size(); ++j) {
      size_t prev_idx = slot_histograms[j];
      const Histogram& prev = clusters->histograms[prev_idx];
      Histogram combined;
      AddHistograms(prev, cur, &combined);
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
      const Histogram& prev = clusters->histograms[histogram_index];
      AddHistograms(prev, cur, &clusters->histograms[histogram_index]);
      clusters->histogram_indexes[i] = histogram_index;
      JXL_ASSERT(clusters->slot_ids[histogram_index] == best_slot);
      slot_costs[best_slot] += best_cost;
    }
  }
}

void BuildJpegHuffmanCode(const Histogram& histo, JPEGHuffmanCode* huff) {
  std::vector<uint32_t> counts(kJpegHuffmanAlphabetSize + 1);
  std::vector<uint8_t> depths(kJpegHuffmanAlphabetSize + 1);
  for (size_t j = 0; j < kJpegHuffmanAlphabetSize; ++j) {
    counts[j] = histo.count[j];
  }
  counts[kJpegHuffmanAlphabetSize] = 1;
  CreateHuffmanTree(counts.data(), counts.size(), kJpegHuffmanMaxBitLength,
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

void AddJpegHuffmanCode(const Histogram& histogram, size_t slot_id,
                        JPEGHuffmanCode* huff_codes, size_t* num_huff_codes) {
  JPEGHuffmanCode huff_code = {};
  huff_code.slot_id = slot_id;
  BuildJpegHuffmanCode(histogram, &huff_code);
  memcpy(&huff_codes[*num_huff_codes], &huff_code, sizeof(huff_code));
  ++(*num_huff_codes);
}

namespace {
void SetJpegHuffmanCode(const JpegClusteredHistograms& clusters,
                        size_t histogram_id, size_t slot_id_offset,
                        std::vector<uint32_t>& slot_histograms,
                        uint32_t* slot_id, bool* is_baseline,
                        JPEGHuffmanCode* huff_codes, size_t* num_huff_codes) {
  JXL_ASSERT(histogram_id < clusters.histogram_indexes.size());
  uint32_t histogram_index = clusters.histogram_indexes[histogram_id];
  uint32_t id = clusters.slot_ids[histogram_index];
  if (id > 1) {
    *is_baseline = false;
  }
  *slot_id = id + (slot_id_offset / 4);
  if (slot_histograms[id] != histogram_index) {
    AddJpegHuffmanCode(clusters.histograms[histogram_index],
                       slot_id_offset + id, huff_codes, num_huff_codes);
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
                               Histogram* ac_histo, coeff_t* last_dc_coeff) {
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
  if (r > 0) {
    ++ac_histo->count[0];
  }
  return true;
}

bool ProcessDCTBlockProgressive(const coeff_t* coeffs, Histogram* dc_histo,
                                Histogram* ac_histo, int Ss, int Se, int Al,
                                DCTState* s, coeff_t* last_dc_coeff) {
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

void ProgressMonitorHistogramPass(j_compress_ptr cinfo, size_t scan_index,
                                  size_t mcu_y) {
  if (cinfo->progress == nullptr) {
    return;
  }
  cinfo->progress->completed_passes = 1 + scan_index;
  cinfo->progress->pass_counter = mcu_y;
  cinfo->progress->pass_limit = cinfo->total_iMCU_rows;
  (*cinfo->progress->progress_monitor)(reinterpret_cast<j_common_ptr>(cinfo));
}

bool ProcessScan(j_compress_ptr cinfo,
                 size_t scan_index, int* histo_index, Histogram* dc_histograms,
                 Histogram* ac_histograms) {
  jpeg_comp_master* m = cinfo->master;
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

  JBLOCKARRAY ba[MAX_COMPS_IN_SCAN];
  for (int mcu_y = 0; mcu_y < MCU_rows; ++mcu_y) {
    ProgressMonitorHistogramPass(cinfo, scan_index, mcu_y);
    for (int i = 0; i < scan_info->comps_in_scan; ++i) {
      int comp_idx = scan_info->component_index[i];
      jpeg_component_info* comp = &cinfo->comp_info[comp_idx];
      int n_blocks_y = is_interleaved ? comp->v_samp_factor : 1;
      int by0 = mcu_y * n_blocks_y;
      int block_rows_left = comp->height_in_blocks - by0;
      int max_block_rows = std::min(n_blocks_y, block_rows_left);
      ba[i] = (*cinfo->mem->access_virt_barray)(
          reinterpret_cast<j_common_ptr>(cinfo), m->coeff_buffers[comp_idx],
          by0, max_block_rows, false);
    }
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
            const coeff_t* block;
            if (block_x >= comp->width_in_blocks ||
                block_y >= comp->height_in_blocks) {
              block = kDummyBlock;
            } else {
              block = &ba[i][iy][block_x][0];
            }
            bool ok;
            if (!is_progressive) {
              ok = ProcessDCTBlockSequential(block, dc_histo, ac_histo,
                                             last_dc_coeff + i);
            } else if (Ah == 0) {
              ok = ProcessDCTBlockProgressive(block, dc_histo, ac_histo, Ss, Se,
                                              Al, &s, last_dc_coeff + i);
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
                 std::vector<Histogram>* dc_histograms,
                 std::vector<Histogram>* ac_histograms) {
  int histo_index = 0;
  for (int i = 0; i < cinfo->num_scans; ++i) {
    if (!ProcessScan(cinfo, i, &histo_index, &(*dc_histograms)[0],
                     &(*ac_histograms)[0])) {
      JPEGLI_ERROR("Invalid scan.");
    }
  }
}

void CopyHuffmanTable(j_compress_ptr cinfo, int index, bool is_dc,
                      JPEGHuffmanCode* huffman_codes,
                      size_t* num_huffman_codes) {
  const char* type = is_dc ? "DC" : "AC";
  if (index < 0 || index >= NUM_HUFF_TBLS) {
    JPEGLI_ERROR("Invalid %s Huffman table index %d", type, index);
  }
  JHUFF_TBL* table =
      is_dc ? cinfo->dc_huff_tbl_ptrs[index] : cinfo->ac_huff_tbl_ptrs[index];
  if (table == nullptr) {
    JPEGLI_ERROR("Missing %s Huffman table %d", type, index);
  }
  ValidateHuffmanTable(reinterpret_cast<j_common_ptr>(cinfo), table, is_dc);
  JPEGHuffmanCode huff = {};
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
  for (size_t i = 0; i < *num_huffman_codes; ++i) {
    if (huffman_codes[i].slot_id == huff.slot_id) have_slot = true;
  }
  if (!have_slot) {
    memcpy(&huffman_codes[*num_huffman_codes], &huff, sizeof(huff));
    ++(*num_huffman_codes);
  }
}

}  // namespace

void CopyHuffmanCodes(j_compress_ptr cinfo, bool* is_baseline) {
  jpeg_comp_master* m = cinfo->master;
  m->huffman_codes =
      Allocate<JPEGHuffmanCode>(cinfo, 2 * cinfo->num_components, JPOOL_IMAGE);
  for (int c = 0; c < cinfo->num_components; ++c) {
    jpeg_component_info* comp = &cinfo->comp_info[c];
    if (comp->dc_tbl_no > 1 || comp->ac_tbl_no > 1) {
      *is_baseline = false;
    }
    CopyHuffmanTable(cinfo, comp->dc_tbl_no, /*is_dc=*/true, m->huffman_codes,
                     &m->num_huffman_codes);
    CopyHuffmanTable(cinfo, comp->ac_tbl_no, /*is_dc=*/false, m->huffman_codes,
                     &m->num_huffman_codes);
  }
  for (int i = 0; i < cinfo->num_scans; ++i) {
    const jpeg_scan_info* si = &cinfo->scan_info[i];
    ScanCodingInfo sci = {};
    for (int j = 0; j < si->comps_in_scan; ++j) {
      int ci = si->component_index[j];
      sci.dc_tbl_idx[j] = cinfo->comp_info[ci].dc_tbl_no;
      sci.ac_tbl_idx[j] = cinfo->comp_info[ci].ac_tbl_no + 4;
    }
    if (i == 0) {
      sci.num_huffman_codes = m->num_huffman_codes;
    }
    memcpy(&m->scan_coding_info[i], &sci, sizeof(sci));
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

void OptimizeHuffmanCodes(j_compress_ptr cinfo, bool* is_baseline) {
  jpeg_comp_master* m = cinfo->master;
  // Gather histograms.
  size_t num_histo = 0;
  for (int i = 0; i < cinfo->num_scans; ++i) {
    num_histo += cinfo->scan_info[i].comps_in_scan;
  }
  std::vector<Histogram> dc_histograms(num_histo);
  std::vector<Histogram> ac_histograms(num_histo);
  ProcessJpeg(cinfo, &dc_histograms, &ac_histograms);

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
  m->huffman_codes = Allocate<JPEGHuffmanCode>(cinfo, num_histo, JPOOL_IMAGE);
  for (size_t i = 0; i < dc_clusters.histograms.size(); ++i) {
    if (i >= 4) break;
    JXL_ASSERT(dc_clusters.slot_ids[i] == i);
    AddJpegHuffmanCode(dc_clusters.histograms[i], i, m->huffman_codes,
                       &m->num_huffman_codes);
    dc_slot_histograms.push_back(i);
  }
  for (size_t i = 0; i < ac_clusters.histograms.size(); ++i) {
    if (i >= 4) break;
    JXL_ASSERT(ac_clusters.slot_ids[i] == i);
    AddJpegHuffmanCode(ac_clusters.histograms[i], 0x10 + i, m->huffman_codes,
                       &m->num_huffman_codes);
    ac_slot_histograms.push_back(i);
  }

  // Set the Huffman table indexes in the scan_infos and emit additional DHT
  // segments if necessary.
  size_t histogram_id = 0;
  size_t num_huffman_codes_sent = 0;
  for (int i = 0; i < cinfo->num_scans; ++i) {
    ScanCodingInfo sci = {};
    for (int c = 0; c < cinfo->scan_info[i].comps_in_scan; ++c) {
      SetJpegHuffmanCode(dc_clusters, histogram_id, 0, dc_slot_histograms,
                         &sci.dc_tbl_idx[c], is_baseline, m->huffman_codes,
                         &m->num_huffman_codes);
      SetJpegHuffmanCode(ac_clusters, histogram_id, 0x10, ac_slot_histograms,
                         &sci.ac_tbl_idx[c], is_baseline, m->huffman_codes,
                         &m->num_huffman_codes);
      ++histogram_id;
    }
    sci.num_huffman_codes = m->num_huffman_codes - num_huffman_codes_sent;
    num_huffman_codes_sent = m->num_huffman_codes;
    memcpy(&m->scan_coding_info[i], &sci, sizeof(sci));
  }
}

}  // namespace jpegli
