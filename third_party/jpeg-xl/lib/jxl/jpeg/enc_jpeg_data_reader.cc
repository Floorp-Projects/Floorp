// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/jpeg/enc_jpeg_data_reader.h"

#include <inttypes.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/common.h"
#include "lib/jxl/jpeg/enc_jpeg_huffman_decode.h"
#include "lib/jxl/jpeg/jpeg_data.h"

// By default only print debug messages when JXL_DEBUG_ON_ERROR is enabled.
#ifndef JXL_DEBUG_JPEG_DATA_READER
#define JXL_DEBUG_JPEG_DATA_READER JXL_DEBUG_ON_ERROR
#endif  // JXL_DEBUG_JPEG_DATA_READER

#define JXL_JPEG_DEBUG(format, ...) \
  JXL_DEBUG(JXL_DEBUG_JPEG_DATA_READER, format, ##__VA_ARGS__)

namespace jxl {
namespace jpeg {

namespace {
static const int kBrunsliMaxSampling = 15;
static const size_t kBrunsliMaxNumBlocks = 1ull << 24;

// Macros for commonly used error conditions.

#define JXL_JPEG_VERIFY_LEN(n)                            \
  if (*pos + (n) > len) {                                 \
    JXL_JPEG_DEBUG("Unexpected end of input: pos=%" PRIuS \
                   " need=%d len=%" PRIuS,                \
                   *pos, static_cast<int>(n), len);       \
    jpg->error = JPEGReadError::UNEXPECTED_EOF;           \
    return false;                                         \
  }

#define JXL_JPEG_VERIFY_INPUT(var, low, high, code)                \
  if ((var) < (low) || (var) > (high)) {                           \
    JXL_JPEG_DEBUG("Invalid " #var ": %d", static_cast<int>(var)); \
    jpg->error = JPEGReadError::INVALID_##code;                    \
    return false;                                                  \
  }

#define JXL_JPEG_VERIFY_MARKER_END()                         \
  if (start_pos + marker_len != *pos) {                      \
    JXL_JPEG_DEBUG("Invalid marker length: declared=%" PRIuS \
                   " actual=%" PRIuS,                        \
                   marker_len, (*pos - start_pos));          \
    jpg->error = JPEGReadError::WRONG_MARKER_SIZE;           \
    return false;                                            \
  }

#define JXL_JPEG_EXPECT_MARKER()                                            \
  if (pos + 2 > len || data[pos] != 0xff) {                                 \
    JXL_JPEG_DEBUG("Marker byte (0xff) expected, found: 0x%.2x pos=%" PRIuS \
                   " len=%" PRIuS,                                          \
                   (pos < len ? data[pos] : 0), pos, len);                  \
    jpg->error = JPEGReadError::MARKER_BYTE_NOT_FOUND;                      \
    return false;                                                           \
  }

inline int ReadUint8(const uint8_t* data, size_t* pos) {
  return data[(*pos)++];
}

inline int ReadUint16(const uint8_t* data, size_t* pos) {
  int v = (data[*pos] << 8) + data[*pos + 1];
  *pos += 2;
  return v;
}

// Reads the Start of Frame (SOF) marker segment and fills in *jpg with the
// parsed data.
bool ProcessSOF(const uint8_t* data, const size_t len, JpegReadMode mode,
                size_t* pos, JPEGData* jpg) {
  if (jpg->width != 0) {
    JXL_JPEG_DEBUG("Duplicate SOF marker.");
    jpg->error = JPEGReadError::DUPLICATE_SOF;
    return false;
  }
  const size_t start_pos = *pos;
  JXL_JPEG_VERIFY_LEN(8);
  size_t marker_len = ReadUint16(data, pos);
  int precision = ReadUint8(data, pos);
  int height = ReadUint16(data, pos);
  int width = ReadUint16(data, pos);
  int num_components = ReadUint8(data, pos);
  JXL_JPEG_VERIFY_INPUT(precision, 8, 8, PRECISION);
  JXL_JPEG_VERIFY_INPUT(height, 1, kMaxDimPixels, HEIGHT);
  JXL_JPEG_VERIFY_INPUT(width, 1, kMaxDimPixels, WIDTH);
  JXL_JPEG_VERIFY_INPUT(num_components, 1, kMaxComponents, NUMCOMP);
  JXL_JPEG_VERIFY_LEN(3 * num_components);
  jpg->height = height;
  jpg->width = width;
  jpg->components.resize(num_components);

  // Read sampling factors and quant table index for each component.
  std::vector<bool> ids_seen(256, false);
  int max_h_samp_factor = 1;
  int max_v_samp_factor = 1;
  for (size_t i = 0; i < jpg->components.size(); ++i) {
    const int id = ReadUint8(data, pos);
    if (ids_seen[id]) {  // (cf. section B.2.2, syntax of Ci)
      JXL_JPEG_DEBUG("Duplicate ID %d in SOF.", id);
      jpg->error = JPEGReadError::DUPLICATE_COMPONENT_ID;
      return false;
    }
    ids_seen[id] = true;
    jpg->components[i].id = id;
    int factor = ReadUint8(data, pos);
    int h_samp_factor = factor >> 4;
    int v_samp_factor = factor & 0xf;
    JXL_JPEG_VERIFY_INPUT(h_samp_factor, 1, kBrunsliMaxSampling, SAMP_FACTOR);
    JXL_JPEG_VERIFY_INPUT(v_samp_factor, 1, kBrunsliMaxSampling, SAMP_FACTOR);
    jpg->components[i].h_samp_factor = h_samp_factor;
    jpg->components[i].v_samp_factor = v_samp_factor;
    jpg->components[i].quant_idx = ReadUint8(data, pos);
    max_h_samp_factor = std::max(max_h_samp_factor, h_samp_factor);
    max_v_samp_factor = std::max(max_v_samp_factor, v_samp_factor);
  }

  // We have checked above that none of the sampling factors are 0, so the max
  // sampling factors can not be 0.
  int MCU_rows = DivCeil(jpg->height, max_v_samp_factor * 8);
  int MCU_cols = DivCeil(jpg->width, max_h_samp_factor * 8);
  // Compute the block dimensions for each component.
  for (size_t i = 0; i < jpg->components.size(); ++i) {
    JPEGComponent* c = &jpg->components[i];
    if (max_h_samp_factor % c->h_samp_factor != 0 ||
        max_v_samp_factor % c->v_samp_factor != 0) {
      JXL_JPEG_DEBUG("Non-integral subsampling ratios.");
      jpg->error = JPEGReadError::INVALID_SAMPLING_FACTORS;
      return false;
    }
    c->width_in_blocks = MCU_cols * c->h_samp_factor;
    c->height_in_blocks = MCU_rows * c->v_samp_factor;
    const uint64_t num_blocks =
        static_cast<uint64_t>(c->width_in_blocks) * c->height_in_blocks;
    if (num_blocks > kBrunsliMaxNumBlocks) {
      JXL_JPEG_DEBUG("Image too large.");
      jpg->error = JPEGReadError::IMAGE_TOO_LARGE;
      return false;
    }
    if (mode == JpegReadMode::kReadAll) {
      c->coeffs.resize(num_blocks * kDCTBlockSize);
    }
  }
  JXL_JPEG_VERIFY_MARKER_END();
  return true;
}

// Reads the Start of Scan (SOS) marker segment and fills in *scan_info with the
// parsed data.
bool ProcessSOS(const uint8_t* data, const size_t len, size_t* pos,
                JPEGData* jpg) {
  const size_t start_pos = *pos;
  JXL_JPEG_VERIFY_LEN(3);
  size_t marker_len = ReadUint16(data, pos);
  size_t comps_in_scan = ReadUint8(data, pos);
  JXL_JPEG_VERIFY_INPUT(comps_in_scan, 1, jpg->components.size(),
                        COMPS_IN_SCAN);

  JPEGScanInfo scan_info;
  scan_info.num_components = comps_in_scan;
  JXL_JPEG_VERIFY_LEN(2 * comps_in_scan);
  std::vector<bool> ids_seen(256, false);
  for (size_t i = 0; i < comps_in_scan; ++i) {
    uint32_t id = ReadUint8(data, pos);
    if (ids_seen[id]) {  // (cf. section B.2.3, regarding CSj)
      JXL_JPEG_DEBUG("Duplicate ID %d in SOS.", id);
      jpg->error = JPEGReadError::DUPLICATE_COMPONENT_ID;
      return false;
    }
    ids_seen[id] = true;
    bool found_index = false;
    for (size_t j = 0; j < jpg->components.size(); ++j) {
      if (jpg->components[j].id == id) {
        scan_info.components[i].comp_idx = j;
        found_index = true;
      }
    }
    if (!found_index) {
      JXL_JPEG_DEBUG("SOS marker: Could not find component with id %d", id);
      jpg->error = JPEGReadError::COMPONENT_NOT_FOUND;
      return false;
    }
    int c = ReadUint8(data, pos);
    int dc_tbl_idx = c >> 4;
    int ac_tbl_idx = c & 0xf;
    JXL_JPEG_VERIFY_INPUT(dc_tbl_idx, 0, 3, HUFFMAN_INDEX);
    JXL_JPEG_VERIFY_INPUT(ac_tbl_idx, 0, 3, HUFFMAN_INDEX);
    scan_info.components[i].dc_tbl_idx = dc_tbl_idx;
    scan_info.components[i].ac_tbl_idx = ac_tbl_idx;
  }
  JXL_JPEG_VERIFY_LEN(3);
  scan_info.Ss = ReadUint8(data, pos);
  scan_info.Se = ReadUint8(data, pos);
  JXL_JPEG_VERIFY_INPUT(static_cast<int>(scan_info.Ss), 0, 63, START_OF_SCAN);
  JXL_JPEG_VERIFY_INPUT(scan_info.Se, scan_info.Ss, 63, END_OF_SCAN);
  int c = ReadUint8(data, pos);
  scan_info.Ah = c >> 4;
  scan_info.Al = c & 0xf;
  if (scan_info.Ah != 0 && scan_info.Al != scan_info.Ah - 1) {
    // section G.1.1.1.2 : Successive approximation control only improves
    // by one bit at a time. But it's not always respected, so we just issue
    // a warning.
    JXL_WARNING("Invalid progressive parameters: Al=%d Ah=%d", scan_info.Al,
                scan_info.Ah);
  }
  // Check that all the Huffman tables needed for this scan are defined.
  for (size_t i = 0; i < comps_in_scan; ++i) {
    bool found_dc_table = false;
    bool found_ac_table = false;
    for (size_t j = 0; j < jpg->huffman_code.size(); ++j) {
      uint32_t slot_id = jpg->huffman_code[j].slot_id;
      if (slot_id == scan_info.components[i].dc_tbl_idx) {
        found_dc_table = true;
      } else if (slot_id == scan_info.components[i].ac_tbl_idx + 16) {
        found_ac_table = true;
      }
    }
    if (scan_info.Ss == 0 && !found_dc_table) {
      JXL_JPEG_DEBUG(
          "SOS marker: Could not find DC Huffman table with index %d",
          scan_info.components[i].dc_tbl_idx);
      jpg->error = JPEGReadError::HUFFMAN_TABLE_NOT_FOUND;
      return false;
    }
    if (scan_info.Se > 0 && !found_ac_table) {
      JXL_JPEG_DEBUG(
          "SOS marker: Could not find AC Huffman table with index %d",
          scan_info.components[i].ac_tbl_idx);
      jpg->error = JPEGReadError::HUFFMAN_TABLE_NOT_FOUND;
      return false;
    }
  }
  jpg->scan_info.push_back(scan_info);
  JXL_JPEG_VERIFY_MARKER_END();
  return true;
}

// Reads the Define Huffman Table (DHT) marker segment and fills in *jpg with
// the parsed data. Builds the Huffman decoding table in either dc_huff_lut or
// ac_huff_lut, depending on the type and solt_id of Huffman code being read.
bool ProcessDHT(const uint8_t* data, const size_t len, JpegReadMode mode,
                std::vector<HuffmanTableEntry>* dc_huff_lut,
                std::vector<HuffmanTableEntry>* ac_huff_lut, size_t* pos,
                JPEGData* jpg) {
  const size_t start_pos = *pos;
  JXL_JPEG_VERIFY_LEN(2);
  size_t marker_len = ReadUint16(data, pos);
  if (marker_len == 2) {
    JXL_JPEG_DEBUG("DHT marker: no Huffman table found");
    jpg->error = JPEGReadError::EMPTY_DHT;
    return false;
  }
  while (*pos < start_pos + marker_len) {
    JXL_JPEG_VERIFY_LEN(1 + kJpegHuffmanMaxBitLength);
    JPEGHuffmanCode huff;
    huff.slot_id = ReadUint8(data, pos);
    int huffman_index = huff.slot_id;
    int is_ac_table = (huff.slot_id & 0x10) != 0;
    HuffmanTableEntry* huff_lut;
    if (is_ac_table) {
      huffman_index -= 0x10;
      JXL_JPEG_VERIFY_INPUT(huffman_index, 0, 3, HUFFMAN_INDEX);
      huff_lut = &(*ac_huff_lut)[huffman_index * kJpegHuffmanLutSize];
    } else {
      JXL_JPEG_VERIFY_INPUT(huffman_index, 0, 3, HUFFMAN_INDEX);
      huff_lut = &(*dc_huff_lut)[huffman_index * kJpegHuffmanLutSize];
    }
    huff.counts[0] = 0;
    int total_count = 0;
    int space = 1 << kJpegHuffmanMaxBitLength;
    int max_depth = 1;
    for (size_t i = 1; i <= kJpegHuffmanMaxBitLength; ++i) {
      int count = ReadUint8(data, pos);
      if (count != 0) {
        max_depth = i;
      }
      huff.counts[i] = count;
      total_count += count;
      space -= count * (1 << (kJpegHuffmanMaxBitLength - i));
    }
    if (is_ac_table) {
      JXL_JPEG_VERIFY_INPUT(total_count, 0, kJpegHuffmanAlphabetSize,
                            HUFFMAN_CODE);
    } else {
      JXL_JPEG_VERIFY_INPUT(total_count, 0, kJpegDCAlphabetSize, HUFFMAN_CODE);
    }
    JXL_JPEG_VERIFY_LEN(total_count);
    std::vector<bool> values_seen(256, false);
    for (int i = 0; i < total_count; ++i) {
      int value = ReadUint8(data, pos);
      if (!is_ac_table) {
        JXL_JPEG_VERIFY_INPUT(value, 0, kJpegDCAlphabetSize - 1, HUFFMAN_CODE);
      }
      if (values_seen[value]) {
        JXL_JPEG_DEBUG("Duplicate Huffman code value %d", value);
        jpg->error = JPEGReadError::INVALID_HUFFMAN_CODE;
        return false;
      }
      values_seen[value] = true;
      huff.values[i] = value;
    }
    // Add an invalid symbol that will have the all 1 code.
    ++huff.counts[max_depth];
    huff.values[total_count] = kJpegHuffmanAlphabetSize;
    space -= (1 << (kJpegHuffmanMaxBitLength - max_depth));
    if (space < 0) {
      JXL_JPEG_DEBUG("Invalid Huffman code lengths.");
      jpg->error = JPEGReadError::INVALID_HUFFMAN_CODE;
      return false;
    } else if (space > 0 && huff_lut[0].value != 0xffff) {
      // Re-initialize the values to an invalid symbol so that we can recognize
      // it when reading the bit stream using a Huffman code with space > 0.
      for (int i = 0; i < kJpegHuffmanLutSize; ++i) {
        huff_lut[i].bits = 0;
        huff_lut[i].value = 0xffff;
      }
    }
    huff.is_last = (*pos == start_pos + marker_len);
    if (mode == JpegReadMode::kReadAll) {
      BuildJpegHuffmanTable(&huff.counts[0], &huff.values[0], huff_lut);
    }
    jpg->huffman_code.push_back(huff);
  }
  JXL_JPEG_VERIFY_MARKER_END();
  return true;
}

// Reads the Define Quantization Table (DQT) marker segment and fills in *jpg
// with the parsed data.
bool ProcessDQT(const uint8_t* data, const size_t len, size_t* pos,
                JPEGData* jpg) {
  const size_t start_pos = *pos;
  JXL_JPEG_VERIFY_LEN(2);
  size_t marker_len = ReadUint16(data, pos);
  if (marker_len == 2) {
    JXL_JPEG_DEBUG("DQT marker: no quantization table found");
    jpg->error = JPEGReadError::EMPTY_DQT;
    return false;
  }
  while (*pos < start_pos + marker_len && jpg->quant.size() < kMaxQuantTables) {
    JXL_JPEG_VERIFY_LEN(1);
    int quant_table_index = ReadUint8(data, pos);
    int quant_table_precision = quant_table_index >> 4;
    JXL_JPEG_VERIFY_INPUT(quant_table_precision, 0, 1, QUANT_TBL_PRECISION);
    quant_table_index &= 0xf;
    JXL_JPEG_VERIFY_INPUT(quant_table_index, 0, 3, QUANT_TBL_INDEX);
    JXL_JPEG_VERIFY_LEN((quant_table_precision + 1) * kDCTBlockSize);
    JPEGQuantTable table;
    table.index = quant_table_index;
    table.precision = quant_table_precision;
    for (size_t i = 0; i < kDCTBlockSize; ++i) {
      int quant_val =
          quant_table_precision ? ReadUint16(data, pos) : ReadUint8(data, pos);
      JXL_JPEG_VERIFY_INPUT(quant_val, 1, 65535, QUANT_VAL);
      table.values[kJPEGNaturalOrder[i]] = quant_val;
    }
    table.is_last = (*pos == start_pos + marker_len);
    jpg->quant.push_back(table);
  }
  JXL_JPEG_VERIFY_MARKER_END();
  return true;
}

// Reads the DRI marker and saves the restart interval into *jpg.
bool ProcessDRI(const uint8_t* data, const size_t len, size_t* pos,
                bool* found_dri, JPEGData* jpg) {
  if (*found_dri) {
    JXL_JPEG_DEBUG("Duplicate DRI marker.");
    jpg->error = JPEGReadError::DUPLICATE_DRI;
    return false;
  }
  *found_dri = true;
  const size_t start_pos = *pos;
  JXL_JPEG_VERIFY_LEN(4);
  size_t marker_len = ReadUint16(data, pos);
  int restart_interval = ReadUint16(data, pos);
  jpg->restart_interval = restart_interval;
  JXL_JPEG_VERIFY_MARKER_END();
  return true;
}

// Saves the APP marker segment as a string to *jpg.
bool ProcessAPP(const uint8_t* data, const size_t len, size_t* pos,
                JPEGData* jpg) {
  JXL_JPEG_VERIFY_LEN(2);
  size_t marker_len = ReadUint16(data, pos);
  JXL_JPEG_VERIFY_INPUT(marker_len, 2, 65535, MARKER_LEN);
  JXL_JPEG_VERIFY_LEN(marker_len - 2);
  JXL_DASSERT(*pos >= 3);
  // Save the marker type together with the app data.
  const uint8_t* app_str_start = data + *pos - 3;
  std::vector<uint8_t> app_str(app_str_start, app_str_start + marker_len + 1);
  *pos += marker_len - 2;
  jpg->app_data.push_back(app_str);
  return true;
}

// Saves the COM marker segment as a string to *jpg.
bool ProcessCOM(const uint8_t* data, const size_t len, size_t* pos,
                JPEGData* jpg) {
  JXL_JPEG_VERIFY_LEN(2);
  size_t marker_len = ReadUint16(data, pos);
  JXL_JPEG_VERIFY_INPUT(marker_len, 2, 65535, MARKER_LEN);
  JXL_JPEG_VERIFY_LEN(marker_len - 2);
  const uint8_t* com_str_start = data + *pos - 3;
  std::vector<uint8_t> com_str(com_str_start, com_str_start + marker_len + 1);
  *pos += marker_len - 2;
  jpg->com_data.push_back(com_str);
  return true;
}

// Helper structure to read bits from the entropy coded data segment.
struct BitReaderState {
  BitReaderState(const uint8_t* data, const size_t len, size_t pos)
      : data_(data), len_(len) {
    Reset(pos);
  }

  void Reset(size_t pos) {
    pos_ = pos;
    val_ = 0;
    bits_left_ = 0;
    next_marker_pos_ = len_ - 2;
    FillBitWindow();
  }

  // Returns the next byte and skips the 0xff/0x00 escape sequences.
  uint8_t GetNextByte() {
    if (pos_ >= next_marker_pos_) {
      ++pos_;
      return 0;
    }
    uint8_t c = data_[pos_++];
    if (c == 0xff) {
      uint8_t escape = data_[pos_];
      if (escape == 0) {
        ++pos_;
      } else {
        // 0xff was followed by a non-zero byte, which means that we found the
        // start of the next marker segment.
        next_marker_pos_ = pos_ - 1;
      }
    }
    return c;
  }

  void FillBitWindow() {
    if (bits_left_ <= 16) {
      while (bits_left_ <= 56) {
        val_ <<= 8;
        val_ |= (uint64_t)GetNextByte();
        bits_left_ += 8;
      }
    }
  }

  int ReadBits(int nbits) {
    FillBitWindow();
    uint64_t val = (val_ >> (bits_left_ - nbits)) & ((1ULL << nbits) - 1);
    bits_left_ -= nbits;
    return val;
  }

  // Sets *pos to the next stream position where parsing should continue.
  // Enqueue the padding bits seen (0 or 1).
  // Returns false if there is inconsistent or invalid padding or the stream
  // ended too early.
  bool FinishStream(JPEGData* jpg, size_t* pos) {
    int npadbits = bits_left_ & 7;
    if (npadbits > 0) {
      uint64_t padmask = (1ULL << npadbits) - 1;
      uint64_t padbits = (val_ >> (bits_left_ - npadbits)) & padmask;
      if (padbits != padmask) {
        jpg->has_zero_padding_bit = true;
      }
      for (int i = npadbits - 1; i >= 0; --i) {
        jpg->padding_bits.push_back((padbits >> i) & 1);
      }
    }
    // Give back some bytes that we did not use.
    int unused_bytes_left = bits_left_ >> 3;
    while (unused_bytes_left-- > 0) {
      --pos_;
      // If we give back a 0 byte, we need to check if it was a 0xff/0x00 escape
      // sequence, and if yes, we need to give back one more byte.
      if (pos_ < next_marker_pos_ && data_[pos_] == 0 &&
          data_[pos_ - 1] == 0xff) {
        --pos_;
      }
    }
    if (pos_ > next_marker_pos_) {
      // Data ran out before the scan was complete.
      JXL_JPEG_DEBUG("Unexpected end of scan.");
      return false;
    }
    *pos = pos_;
    return true;
  }

  const uint8_t* data_;
  const size_t len_;
  size_t pos_;
  uint64_t val_;
  int bits_left_;
  size_t next_marker_pos_;
};

// Returns the next Huffman-coded symbol.
int ReadSymbol(const HuffmanTableEntry* table, BitReaderState* br) {
  int nbits;
  br->FillBitWindow();
  int val = (br->val_ >> (br->bits_left_ - 8)) & 0xff;
  table += val;
  nbits = table->bits - 8;
  if (nbits > 0) {
    br->bits_left_ -= 8;
    table += table->value;
    val = (br->val_ >> (br->bits_left_ - nbits)) & ((1 << nbits) - 1);
    table += val;
  }
  br->bits_left_ -= table->bits;
  return table->value;
}

/**
 * Returns the DC diff or AC value for extra bits value x and prefix code s.
 *
 * CCITT Rec. T.81 (1992 E)
 * Table F.1 – Difference magnitude categories for DC coding
 *  SSSS | DIFF values
 * ------+--------------------------
 *     0 | 0
 *     1 | –1, 1
 *     2 | –3, –2, 2, 3
 *     3 | –7..–4, 4..7
 * ......|..........................
 *    11 | –2047..–1024, 1024..2047
 *
 * CCITT Rec. T.81 (1992 E)
 * Table F.2 – Categories assigned to coefficient values
 * [ Same as Table F.1, but does not include SSSS equal to 0 and 11]
 *
 *
 * CCITT Rec. T.81 (1992 E)
 * F.1.2.1.1 Structure of DC code table
 * For each category,... additional bits... appended... to uniquely identify
 * which difference... occurred... When DIFF is positive... SSSS... bits of DIFF
 * are appended. When DIFF is negative... SSSS... bits of (DIFF – 1) are
 * appended... Most significant bit... is 0 for negative differences and 1 for
 * positive differences.
 *
 * In other words the upper half of extra bits range represents DIFF as is.
 * The lower half represents the negative DIFFs with an offset.
 */
int HuffExtend(int x, int s) {
  JXL_DASSERT(s >= 1);
  int half = 1 << (s - 1);
  if (x >= half) {
    JXL_DASSERT(x < (1 << s));
    return x;
  } else {
    return x - (1 << s) + 1;
  }
}

// Decodes one 8x8 block of DCT coefficients from the bit stream.
bool DecodeDCTBlock(const HuffmanTableEntry* dc_huff,
                    const HuffmanTableEntry* ac_huff, int Ss, int Se, int Al,
                    int* eobrun, bool* reset_state, int* num_zero_runs,
                    BitReaderState* br, JPEGData* jpg, coeff_t* last_dc_coeff,
                    coeff_t* coeffs) {
  // Nowadays multiplication is even faster than variable shift.
  int Am = 1 << Al;
  bool eobrun_allowed = Ss > 0;
  if (Ss == 0) {
    int s = ReadSymbol(dc_huff, br);
    if (s >= kJpegDCAlphabetSize) {
      JXL_JPEG_DEBUG("Invalid Huffman symbol %d  for DC coefficient.", s);
      jpg->error = JPEGReadError::INVALID_SYMBOL;
      return false;
    }
    int diff = 0;
    if (s > 0) {
      int bits = br->ReadBits(s);
      diff = HuffExtend(bits, s);
    }
    int coeff = diff + *last_dc_coeff;
    const int dc_coeff = coeff * Am;
    coeffs[0] = dc_coeff;
    // TODO(eustas): is there a more elegant / explicit way to check this?
    if (dc_coeff != coeffs[0]) {
      JXL_JPEG_DEBUG("Invalid DC coefficient %d", dc_coeff);
      jpg->error = JPEGReadError::NON_REPRESENTABLE_DC_COEFF;
      return false;
    }
    *last_dc_coeff = coeff;
    ++Ss;
  }
  if (Ss > Se) {
    return true;
  }
  if (*eobrun > 0) {
    --(*eobrun);
    return true;
  }
  *num_zero_runs = 0;
  for (int k = Ss; k <= Se; k++) {
    int sr = ReadSymbol(ac_huff, br);
    if (sr >= kJpegHuffmanAlphabetSize) {
      JXL_JPEG_DEBUG("Invalid Huffman symbol %d for AC coefficient %d", sr, k);
      jpg->error = JPEGReadError::INVALID_SYMBOL;
      return false;
    }
    int r = sr >> 4;
    int s = sr & 15;
    if (s > 0) {
      k += r;
      if (k > Se) {
        JXL_JPEG_DEBUG("Out-of-band coefficient %d band was %d-%d", k, Ss, Se);
        jpg->error = JPEGReadError::OUT_OF_BAND_COEFF;
        return false;
      }
      if (s + Al >= kJpegDCAlphabetSize) {
        JXL_JPEG_DEBUG(
            "Out of range AC coefficient value: s = %d Al = %d k = %d", s, Al,
            k);
        jpg->error = JPEGReadError::NON_REPRESENTABLE_AC_COEFF;
        return false;
      }
      int bits = br->ReadBits(s);
      int coeff = HuffExtend(bits, s);
      coeffs[kJPEGNaturalOrder[k]] = coeff * Am;
      *num_zero_runs = 0;
    } else if (r == 15) {
      k += 15;
      ++(*num_zero_runs);
    } else {
      if (eobrun_allowed && k == Ss && *eobrun == 0) {
        // We have two end-of-block runs right after each other, so we signal
        // the jpeg encoder to force a state reset at this point.
        *reset_state = true;
      }
      *eobrun = 1 << r;
      if (r > 0) {
        if (!eobrun_allowed) {
          JXL_JPEG_DEBUG("End-of-block run crossing DC coeff.");
          jpg->error = JPEGReadError::EOB_RUN_TOO_LONG;
          return false;
        }
        *eobrun += br->ReadBits(r);
      }
      break;
    }
  }
  --(*eobrun);
  return true;
}

bool RefineDCTBlock(const HuffmanTableEntry* ac_huff, int Ss, int Se, int Al,
                    int* eobrun, bool* reset_state, BitReaderState* br,
                    JPEGData* jpg, coeff_t* coeffs) {
  // Nowadays multiplication is even faster than variable shift.
  int Am = 1 << Al;
  bool eobrun_allowed = Ss > 0;
  if (Ss == 0) {
    int s = br->ReadBits(1);
    coeff_t dc_coeff = coeffs[0];
    dc_coeff |= s * Am;
    coeffs[0] = dc_coeff;
    ++Ss;
  }
  if (Ss > Se) {
    return true;
  }
  int p1 = Am;
  int m1 = -Am;
  int k = Ss;
  int r;
  int s;
  bool in_zero_run = false;
  if (*eobrun <= 0) {
    for (; k <= Se; k++) {
      s = ReadSymbol(ac_huff, br);
      if (s >= kJpegHuffmanAlphabetSize) {
        JXL_JPEG_DEBUG("Invalid Huffman symbol %d for AC coefficient %d", s, k);
        jpg->error = JPEGReadError::INVALID_SYMBOL;
        return false;
      }
      r = s >> 4;
      s &= 15;
      if (s) {
        if (s != 1) {
          JXL_JPEG_DEBUG("Invalid Huffman symbol %d for AC coefficient %d", s,
                         k);
          jpg->error = JPEGReadError::INVALID_SYMBOL;
          return false;
        }
        s = br->ReadBits(1) ? p1 : m1;
        in_zero_run = false;
      } else {
        if (r != 15) {
          if (eobrun_allowed && k == Ss && *eobrun == 0) {
            // We have two end-of-block runs right after each other, so we
            // signal the jpeg encoder to force a state reset at this point.
            *reset_state = true;
          }
          *eobrun = 1 << r;
          if (r > 0) {
            if (!eobrun_allowed) {
              JXL_JPEG_DEBUG("End-of-block run crossing DC coeff.");
              jpg->error = JPEGReadError::EOB_RUN_TOO_LONG;
              return false;
            }
            *eobrun += br->ReadBits(r);
          }
          break;
        }
        in_zero_run = true;
      }
      do {
        coeff_t thiscoef = coeffs[kJPEGNaturalOrder[k]];
        if (thiscoef != 0) {
          if (br->ReadBits(1)) {
            if ((thiscoef & p1) == 0) {
              if (thiscoef >= 0) {
                thiscoef += p1;
              } else {
                thiscoef += m1;
              }
            }
          }
          coeffs[kJPEGNaturalOrder[k]] = thiscoef;
        } else {
          if (--r < 0) {
            break;
          }
        }
        k++;
      } while (k <= Se);
      if (s) {
        if (k > Se) {
          JXL_JPEG_DEBUG("Out-of-band coefficient %d band was %d-%d", k, Ss,
                         Se);
          jpg->error = JPEGReadError::OUT_OF_BAND_COEFF;
          return false;
        }
        coeffs[kJPEGNaturalOrder[k]] = s;
      }
    }
  }
  if (in_zero_run) {
    JXL_JPEG_DEBUG("Extra zero run before end-of-block.");
    jpg->error = JPEGReadError::EXTRA_ZERO_RUN;
    return false;
  }
  if (*eobrun > 0) {
    for (; k <= Se; k++) {
      coeff_t thiscoef = coeffs[kJPEGNaturalOrder[k]];
      if (thiscoef != 0) {
        if (br->ReadBits(1)) {
          if ((thiscoef & p1) == 0) {
            if (thiscoef >= 0) {
              thiscoef += p1;
            } else {
              thiscoef += m1;
            }
          }
        }
        coeffs[kJPEGNaturalOrder[k]] = thiscoef;
      }
    }
  }
  --(*eobrun);
  return true;
}

bool ProcessRestart(const uint8_t* data, const size_t len,
                    int* next_restart_marker, BitReaderState* br,
                    JPEGData* jpg) {
  size_t pos = 0;
  if (!br->FinishStream(jpg, &pos)) {
    jpg->error = JPEGReadError::INVALID_SCAN;
    return false;
  }
  int expected_marker = 0xd0 + *next_restart_marker;
  JXL_JPEG_EXPECT_MARKER();
  int marker = data[pos + 1];
  if (marker != expected_marker) {
    JXL_JPEG_DEBUG("Did not find expected restart marker %d actual %d",
                   expected_marker, marker);
    jpg->error = JPEGReadError::WRONG_RESTART_MARKER;
    return false;
  }
  br->Reset(pos + 2);
  *next_restart_marker += 1;
  *next_restart_marker &= 0x7;
  return true;
}

bool ProcessScan(const uint8_t* data, const size_t len,
                 const std::vector<HuffmanTableEntry>& dc_huff_lut,
                 const std::vector<HuffmanTableEntry>& ac_huff_lut,
                 uint16_t scan_progression[kMaxComponents][kDCTBlockSize],
                 bool is_progressive, size_t* pos, JPEGData* jpg) {
  if (!ProcessSOS(data, len, pos, jpg)) {
    return false;
  }
  JPEGScanInfo* scan_info = &jpg->scan_info.back();
  bool is_interleaved = (scan_info->num_components > 1);
  int max_h_samp_factor = 1;
  int max_v_samp_factor = 1;
  for (size_t i = 0; i < jpg->components.size(); ++i) {
    max_h_samp_factor =
        std::max(max_h_samp_factor, jpg->components[i].h_samp_factor);
    max_v_samp_factor =
        std::max(max_v_samp_factor, jpg->components[i].v_samp_factor);
  }

  int MCU_rows = DivCeil(jpg->height, max_v_samp_factor * 8);
  int MCUs_per_row = DivCeil(jpg->width, max_h_samp_factor * 8);
  if (!is_interleaved) {
    const JPEGComponent& c = jpg->components[scan_info->components[0].comp_idx];
    MCUs_per_row = DivCeil(jpg->width * c.h_samp_factor, 8 * max_h_samp_factor);
    MCU_rows = DivCeil(jpg->height * c.v_samp_factor, 8 * max_v_samp_factor);
  }
  coeff_t last_dc_coeff[kMaxComponents] = {0};
  BitReaderState br(data, len, *pos);
  int restarts_to_go = jpg->restart_interval;
  int next_restart_marker = 0;
  int eobrun = -1;
  int block_scan_index = 0;
  const int Al = is_progressive ? scan_info->Al : 0;
  const int Ah = is_progressive ? scan_info->Ah : 0;
  const int Ss = is_progressive ? scan_info->Ss : 0;
  const int Se = is_progressive ? scan_info->Se : 63;
  const uint16_t scan_bitmask = Ah == 0 ? (0xffff << Al) : (1u << Al);
  const uint16_t refinement_bitmask = (1 << Al) - 1;
  for (size_t i = 0; i < scan_info->num_components; ++i) {
    int comp_idx = scan_info->components[i].comp_idx;
    for (int k = Ss; k <= Se; ++k) {
      if (scan_progression[comp_idx][k] & scan_bitmask) {
        JXL_JPEG_DEBUG(
            "Overlapping scans: component=%d k=%d prev_mask: %u cur_mask %u",
            comp_idx, k, scan_progression[i][k], scan_bitmask);
        jpg->error = JPEGReadError::OVERLAPPING_SCANS;
        return false;
      }
      if (scan_progression[comp_idx][k] & refinement_bitmask) {
        JXL_JPEG_DEBUG(
            "Invalid scan order, a more refined scan was already done: "
            "component=%d k=%d prev_mask=%u cur_mask=%u",
            comp_idx, k, scan_progression[i][k], scan_bitmask);
        jpg->error = JPEGReadError::INVALID_SCAN_ORDER;
        return false;
      }
      scan_progression[comp_idx][k] |= scan_bitmask;
    }
  }
  if (Al > 10) {
    JXL_JPEG_DEBUG("Scan parameter Al=%d is not supported.", Al);
    jpg->error = JPEGReadError::NON_REPRESENTABLE_AC_COEFF;
    return false;
  }
  for (int mcu_y = 0; mcu_y < MCU_rows; ++mcu_y) {
    for (int mcu_x = 0; mcu_x < MCUs_per_row; ++mcu_x) {
      // Handle the restart intervals.
      if (jpg->restart_interval > 0) {
        if (restarts_to_go == 0) {
          if (ProcessRestart(data, len, &next_restart_marker, &br, jpg)) {
            restarts_to_go = jpg->restart_interval;
            memset(static_cast<void*>(last_dc_coeff), 0, sizeof(last_dc_coeff));
            if (eobrun > 0) {
              JXL_JPEG_DEBUG("End-of-block run too long.");
              jpg->error = JPEGReadError::EOB_RUN_TOO_LONG;
              return false;
            }
            eobrun = -1;  // fresh start
          } else {
            return false;
          }
        }
        --restarts_to_go;
      }
      // Decode one MCU.
      for (size_t i = 0; i < scan_info->num_components; ++i) {
        JPEGComponentScanInfo* si = &scan_info->components[i];
        JPEGComponent* c = &jpg->components[si->comp_idx];
        const HuffmanTableEntry* dc_lut =
            &dc_huff_lut[si->dc_tbl_idx * kJpegHuffmanLutSize];
        const HuffmanTableEntry* ac_lut =
            &ac_huff_lut[si->ac_tbl_idx * kJpegHuffmanLutSize];
        int nblocks_y = is_interleaved ? c->v_samp_factor : 1;
        int nblocks_x = is_interleaved ? c->h_samp_factor : 1;
        for (int iy = 0; iy < nblocks_y; ++iy) {
          for (int ix = 0; ix < nblocks_x; ++ix) {
            int block_y = mcu_y * nblocks_y + iy;
            int block_x = mcu_x * nblocks_x + ix;
            int block_idx = block_y * c->width_in_blocks + block_x;
            bool reset_state = false;
            int num_zero_runs = 0;
            coeff_t* coeffs = &c->coeffs[block_idx * kDCTBlockSize];
            if (Ah == 0) {
              if (!DecodeDCTBlock(dc_lut, ac_lut, Ss, Se, Al, &eobrun,
                                  &reset_state, &num_zero_runs, &br, jpg,
                                  &last_dc_coeff[si->comp_idx], coeffs)) {
                return false;
              }
            } else {
              if (!RefineDCTBlock(ac_lut, Ss, Se, Al, &eobrun, &reset_state,
                                  &br, jpg, coeffs)) {
                return false;
              }
            }
            if (reset_state) {
              scan_info->reset_points.emplace_back(block_scan_index);
            }
            if (num_zero_runs > 0) {
              JPEGScanInfo::ExtraZeroRunInfo info;
              info.block_idx = block_scan_index;
              info.num_extra_zero_runs = num_zero_runs;
              scan_info->extra_zero_runs.push_back(info);
            }
            ++block_scan_index;
          }
        }
      }
    }
  }
  if (eobrun > 0) {
    JXL_JPEG_DEBUG("End-of-block run too long.");
    jpg->error = JPEGReadError::EOB_RUN_TOO_LONG;
    return false;
  }
  if (!br.FinishStream(jpg, pos)) {
    jpg->error = JPEGReadError::INVALID_SCAN;
    return false;
  }
  if (*pos > len) {
    JXL_JPEG_DEBUG("Unexpected end of file during scan. pos=%" PRIuS
                   " len=%" PRIuS,
                   *pos, len);
    jpg->error = JPEGReadError::UNEXPECTED_EOF;
    return false;
  }
  return true;
}

// Changes the quant_idx field of the components to refer to the index of the
// quant table in the jpg->quant array.
bool FixupIndexes(JPEGData* jpg) {
  for (size_t i = 0; i < jpg->components.size(); ++i) {
    JPEGComponent* c = &jpg->components[i];
    bool found_index = false;
    for (size_t j = 0; j < jpg->quant.size(); ++j) {
      if (jpg->quant[j].index == c->quant_idx) {
        c->quant_idx = j;
        found_index = true;
        break;
      }
    }
    if (!found_index) {
      JXL_JPEG_DEBUG("Quantization table with index %u not found",
                     c->quant_idx);
      jpg->error = JPEGReadError::QUANT_TABLE_NOT_FOUND;
      return false;
    }
  }
  return true;
}

size_t FindNextMarker(const uint8_t* data, const size_t len, size_t pos) {
  // kIsValidMarker[i] == 1 means (0xc0 + i) is a valid marker.
  static const uint8_t kIsValidMarker[] = {
      1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,
      1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
  };
  size_t num_skipped = 0;
  while (pos + 1 < len && (data[pos] != 0xff || data[pos + 1] < 0xc0 ||
                           !kIsValidMarker[data[pos + 1] - 0xc0])) {
    ++pos;
    ++num_skipped;
  }
  return num_skipped;
}

}  // namespace

bool ReadJpeg(const uint8_t* data, const size_t len, JpegReadMode mode,
              JPEGData* jpg) {
  size_t pos = 0;
  // Check SOI marker.
  JXL_JPEG_EXPECT_MARKER();
  int marker = data[pos + 1];
  pos += 2;
  if (marker != 0xd8) {
    JXL_JPEG_DEBUG("Did not find expected SOI marker, actual=%d", marker);
    jpg->error = JPEGReadError::SOI_NOT_FOUND;
    return false;
  }
  int lut_size = kMaxHuffmanTables * kJpegHuffmanLutSize;
  std::vector<HuffmanTableEntry> dc_huff_lut(lut_size);
  std::vector<HuffmanTableEntry> ac_huff_lut(lut_size);
  bool found_sof = false;
  bool found_dri = false;
  uint16_t scan_progression[kMaxComponents][kDCTBlockSize] = {{0}};

  jpg->padding_bits.resize(0);
  bool is_progressive = false;  // default
  do {
    // Read next marker.
    size_t num_skipped = FindNextMarker(data, len, pos);
    if (num_skipped > 0) {
      // Add a fake marker to indicate arbitrary in-between-markers data.
      jpg->marker_order.push_back(0xff);
      jpg->inter_marker_data.emplace_back(data + pos, data + pos + num_skipped);
      pos += num_skipped;
    }
    JXL_JPEG_EXPECT_MARKER();
    marker = data[pos + 1];
    pos += 2;
    bool ok = true;
    switch (marker) {
      case 0xc0:
      case 0xc1:
      case 0xc2:
        is_progressive = (marker == 0xc2);
        ok = ProcessSOF(data, len, mode, &pos, jpg);
        found_sof = true;
        break;
      case 0xc4:
        ok = ProcessDHT(data, len, mode, &dc_huff_lut, &ac_huff_lut, &pos, jpg);
        break;
      case 0xd0:
      case 0xd1:
      case 0xd2:
      case 0xd3:
      case 0xd4:
      case 0xd5:
      case 0xd6:
      case 0xd7:
        // RST markers do not have any data.
        break;
      case 0xd9:
        // Found end marker.
        break;
      case 0xda:
        if (mode == JpegReadMode::kReadAll) {
          ok = ProcessScan(data, len, dc_huff_lut, ac_huff_lut,
                           scan_progression, is_progressive, &pos, jpg);
        }
        break;
      case 0xdb:
        ok = ProcessDQT(data, len, &pos, jpg);
        break;
      case 0xdd:
        ok = ProcessDRI(data, len, &pos, &found_dri, jpg);
        break;
      case 0xe0:
      case 0xe1:
      case 0xe2:
      case 0xe3:
      case 0xe4:
      case 0xe5:
      case 0xe6:
      case 0xe7:
      case 0xe8:
      case 0xe9:
      case 0xea:
      case 0xeb:
      case 0xec:
      case 0xed:
      case 0xee:
      case 0xef:
        if (mode != JpegReadMode::kReadTables) {
          ok = ProcessAPP(data, len, &pos, jpg);
        }
        break;
      case 0xfe:
        if (mode != JpegReadMode::kReadTables) {
          ok = ProcessCOM(data, len, &pos, jpg);
        }
        break;
      default:
        JXL_JPEG_DEBUG("Unsupported marker: %d pos=%" PRIuS " len=%" PRIuS,
                       marker, pos, len);
        jpg->error = JPEGReadError::UNSUPPORTED_MARKER;
        ok = false;
        break;
    }
    if (!ok) {
      return false;
    }
    jpg->marker_order.push_back(marker);
    if (mode == JpegReadMode::kReadHeader && found_sof) {
      break;
    }
  } while (marker != 0xd9);

  if (!found_sof) {
    JXL_JPEG_DEBUG("Missing SOF marker.");
    jpg->error = JPEGReadError::SOF_NOT_FOUND;
    return false;
  }

  // Supplemental checks.
  if (mode == JpegReadMode::kReadAll) {
    if (pos < len) {
      jpg->tail_data = std::vector<uint8_t>(data + pos, data + len);
    }
    if (!FixupIndexes(jpg)) {
      return false;
    }
    if (jpg->huffman_code.empty()) {
      // Section B.2.4.2: "If a table has never been defined for a particular
      // destination, then when this destination is specified in a scan header,
      // the results are unpredictable."
      JXL_JPEG_DEBUG("Need at least one Huffman code table.");
      jpg->error = JPEGReadError::HUFFMAN_TABLE_ERROR;
      return false;
    }
    if (jpg->huffman_code.size() >= kMaxDHTMarkers) {
      JXL_JPEG_DEBUG("Too many Huffman tables.");
      jpg->error = JPEGReadError::HUFFMAN_TABLE_ERROR;
      return false;
    }
  }
  return true;
}

}  // namespace jpeg
}  // namespace jxl
