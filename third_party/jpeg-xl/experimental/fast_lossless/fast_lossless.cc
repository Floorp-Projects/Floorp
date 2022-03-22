// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "fast_lossless.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <array>
#include <memory>
#include <queue>
#include <vector>

#if (!defined(__BYTE_ORDER__) || (__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__))
#error "system not known to be little endian"
#endif

struct BitWriter {
  void Allocate(size_t maximum_bit_size) {
    assert(data == nullptr);
    // Leave some padding.
    data.reset((uint8_t*)malloc(maximum_bit_size / 8 + 32));
  }

  void Write(uint32_t count, uint64_t bits) {
    buffer |= bits << bits_in_buffer;
    bits_in_buffer += count;
    memcpy(data.get() + bytes_written, &buffer, 8);
    size_t bytes_in_buffer = bits_in_buffer / 8;
    bits_in_buffer -= bytes_in_buffer * 8;
    buffer >>= bytes_in_buffer * 8;
    bytes_written += bytes_in_buffer;
  }

  void ZeroPadToByte() {
    if (bits_in_buffer != 0) {
      Write(8 - bits_in_buffer, 0);
    }
  }

  std::unique_ptr<uint8_t[], void (*)(void*)> data = {nullptr, free};
  size_t bytes_written = 0;
  size_t bits_in_buffer = 0;
  uint64_t buffer = 0;
};

constexpr size_t kLZ77Offset = 224;
constexpr size_t kLZ77MinLength = 16;

struct PrefixCode {
  static constexpr size_t kNumLZ77 = 17;
  static constexpr size_t kNumRaw = 15;

  alignas(32) uint8_t raw_nbits[16] = {};
  alignas(32) uint8_t raw_bits[16] = {};
  uint8_t lz77_nbits[kNumLZ77] = {};

  uint16_t lz77_bits[kNumLZ77] = {};

  static uint16_t BitReverse(size_t nbits, uint16_t bits) {
    constexpr uint16_t kNibbleLookup[16] = {
        0b0000, 0b1000, 0b0100, 0b1100, 0b0010, 0b1010, 0b0110, 0b1110,
        0b0001, 0b1001, 0b0101, 0b1101, 0b0011, 0b1011, 0b0111, 0b1111,
    };
    uint16_t rev16 = (kNibbleLookup[bits & 0xF] << 12) |
                     (kNibbleLookup[(bits >> 4) & 0xF] << 8) |
                     (kNibbleLookup[(bits >> 8) & 0xF] << 4) |
                     (kNibbleLookup[bits >> 12]);
    return rev16 >> (16 - nbits);
  }

  // Create the prefix codes given the code lengths.
  // Supports the code lengths being split into two halves.
  static void ComputeCanonicalCode(const uint8_t* first_chunk_nbits,
                                   uint8_t* first_chunk_bits,
                                   size_t first_chunk_size,
                                   const uint8_t* second_chunk_nbits,
                                   uint16_t* second_chunk_bits,
                                   size_t second_chunk_size) {
    uint8_t code_length_counts[16] = {};
    for (size_t i = 0; i < first_chunk_size; i++) {
      code_length_counts[first_chunk_nbits[i]]++;
      assert(first_chunk_nbits[i] <= 7);
      assert(first_chunk_nbits[i] > 0);
    }
    for (size_t i = 0; i < second_chunk_size; i++) {
      code_length_counts[second_chunk_nbits[i]]++;
    }

    uint16_t next_code[16] = {};

    uint16_t code = 0;
    for (size_t i = 1; i < 16; i++) {
      code = (code + code_length_counts[i - 1]) << 1;
      next_code[i] = code;
    }

    for (size_t i = 0; i < first_chunk_size; i++) {
      first_chunk_bits[i] =
          BitReverse(first_chunk_nbits[i], next_code[first_chunk_nbits[i]]++);
    }
    for (size_t i = 0; i < second_chunk_size; i++) {
      second_chunk_bits[i] =
          BitReverse(second_chunk_nbits[i], next_code[second_chunk_nbits[i]]++);
    }
  }

  PrefixCode(uint64_t* raw_counts, uint64_t* lz77_counts) {
    // "merge" together all the lz77 counts in a single symbol for the level 1
    // table (containing just the raw symbols, up to length 7).
    uint64_t level1_counts[kNumRaw + 1];
    memcpy(level1_counts, raw_counts, kNumRaw * sizeof(uint64_t));
    size_t numraw = kNumRaw;
    while (numraw > 0 && level1_counts[numraw - 1] == 0) numraw--;

    level1_counts[numraw] = 0;
    for (size_t i = 0; i < kNumLZ77; i++) {
      level1_counts[numraw] += lz77_counts[i];
    }
    uint8_t level1_nbits[kNumRaw + 1] = {};
    ComputeCodeLengths(level1_counts, numraw + 1, 7, level1_nbits);

    uint8_t level2_nbits[kNumLZ77] = {};
    ComputeCodeLengths(lz77_counts, kNumLZ77, 15 - level1_nbits[numraw],
                       level2_nbits);
    for (size_t i = 0; i < numraw; i++) {
      raw_nbits[i] = level1_nbits[i];
    }
    for (size_t i = 0; i < kNumLZ77; i++) {
      lz77_nbits[i] =
          level2_nbits[i] ? level1_nbits[numraw] + level2_nbits[i] : 0;
    }

    ComputeCanonicalCode(raw_nbits, raw_bits, numraw, lz77_nbits, lz77_bits,
                         kNumLZ77);
  }

  static void ComputeCodeLengths(uint64_t* freqs, size_t n, size_t limit,
                                 uint8_t* nbits) {
    if (n <= 1) return;
    assert(n <= (1 << limit));
    assert(n <= 32);
    int parent[64] = {};
    int height[64] = {};
    using QElem = std::pair<uint64_t, size_t>;
    std::priority_queue<QElem, std::vector<QElem>, std::greater<QElem>> q;
    // Standard Huffman code construction. On failure (i.e. if going beyond the
    // length limit), try again with halved frequencies.
    while (true) {
      size_t num_nodes = 0;
      for (size_t i = 0; i < n; i++) {
        if (freqs[i] == 0) continue;
        q.emplace(freqs[i], num_nodes++);
      }
      if (num_nodes <= 1) return;
      while (q.size() > 1) {
        QElem n1 = q.top();
        q.pop();
        QElem n2 = q.top();
        q.pop();
        size_t next = num_nodes++;
        parent[n1.second] = next;
        parent[n2.second] = next;
        q.emplace(n1.first + n2.first, next);
      }
      assert(q.size() == 1);
      q.pop();
      bool is_ok = true;
      for (size_t i = num_nodes - 1; i-- > 0;) {
        height[i] = height[parent[i]] + 1;
        is_ok &= height[i] <= limit;
      }
      if (is_ok) {
        num_nodes = 0;
        for (size_t i = 0; i < n; i++) {
          if (freqs[i] == 0) continue;
          nbits[i] = height[num_nodes++];
        }
        break;
      } else {
        for (size_t i = 0; i < n; i++) {
          freqs[i] = (freqs[i] + 1) >> 1;
        }
      }
    }
  }

  void WriteTo(BitWriter* writer) const {
    uint64_t code_length_counts[18] = {};
    code_length_counts[17] = 3 + 2 * (kNumLZ77 - 1);
    for (size_t i = 0; i < kNumRaw; i++) {
      code_length_counts[raw_nbits[i]]++;
    }
    for (size_t i = 0; i < kNumLZ77; i++) {
      code_length_counts[lz77_nbits[i]]++;
    }
    uint8_t code_length_nbits[18] = {};
    ComputeCodeLengths(code_length_counts, 18, 5, code_length_nbits);
    writer->Write(2, 0b00);  // HSKIP = 0, i.e. don't skip code lengths.

    // As per Brotli RFC.
    uint8_t code_length_order[18] = {1, 2, 3, 4,  0,  5,  17, 6,  16,
                                     7, 8, 9, 10, 11, 12, 13, 14, 15};
    uint8_t code_length_length_nbits[] = {2, 4, 3, 2, 2, 4};
    uint8_t code_length_length_bits[] = {0, 7, 3, 2, 1, 15};

    // Encode lengths of code lengths.
    size_t num_code_lengths = 18;
    while (code_length_nbits[code_length_order[num_code_lengths - 1]] == 0) {
      num_code_lengths--;
    }
    for (size_t i = 0; i < num_code_lengths; i++) {
      int symbol = code_length_nbits[code_length_order[i]];
      writer->Write(code_length_length_nbits[symbol],
                    code_length_length_bits[symbol]);
    }

    // Compute the canonical codes for the codes that represent the lengths of
    // the actual codes for data.
    uint16_t code_length_bits[18] = {};
    ComputeCanonicalCode(nullptr, nullptr, 0, code_length_nbits,
                         code_length_bits, 18);
    // Encode raw bit code lengths.
    for (size_t i = 0; i < kNumRaw; i++) {
      writer->Write(code_length_nbits[raw_nbits[i]],
                    code_length_bits[raw_nbits[i]]);
    }
    size_t num_lz77 = kNumLZ77;
    while (lz77_nbits[num_lz77 - 1] == 0) {
      num_lz77--;
    }
    // Encode 0s until 224 (start of LZ77 symbols). This is in total 224-15 =
    // 209.
    static_assert(kLZ77Offset == 224, "");
    static_assert(kNumRaw == 15, "");
    writer->Write(code_length_nbits[17], code_length_bits[17]);
    writer->Write(3, 0b010);  // 5
    writer->Write(code_length_nbits[17], code_length_bits[17]);
    writer->Write(3, 0b000);  // (5-2)*8 + 3 = 27
    writer->Write(code_length_nbits[17], code_length_bits[17]);
    writer->Write(3, 0b110);  // (27-2)*8 + 9 = 209
    // Encode LZ77 symbols, with values 224+i*16.
    for (size_t i = 0; i < num_lz77; i++) {
      writer->Write(code_length_nbits[lz77_nbits[i]],
                    code_length_bits[lz77_nbits[i]]);
      if (i != num_lz77 - 1) {
        // Encode gap between LZ77 symbols: 15 zeros.
        writer->Write(code_length_nbits[17], code_length_bits[17]);
        writer->Write(3, 0b000);  // 3
        writer->Write(code_length_nbits[17], code_length_bits[17]);
        writer->Write(3, 0b100);  // (3-2)*8+7 = 15
      }
    }
  }
};

constexpr size_t kChunkSize = 16;

void EncodeHybridUint000(uint32_t value, uint32_t* token, uint32_t* nbits,
                         uint32_t* bits) {
  uint32_t n = 31 - __builtin_clz(value);
  *token = value ? n + 1 : 0;
  *nbits = value ? n : 0;
  *bits = value ? value - (1 << n) : 0;
}

void AppendWriter(BitWriter* dest, const BitWriter* src) {
  if (dest->bits_in_buffer == 0) {
    memcpy(dest->data.get() + dest->bytes_written, src->data.get(),
           src->bytes_written);
    dest->bytes_written += src->bytes_written;
  } else {
    size_t i = 0;
    uint64_t buf = dest->buffer;
    uint64_t bits_in_buffer = dest->bits_in_buffer;
    uint8_t* dest_buf = dest->data.get() + dest->bytes_written;
    // Copy 8 bytes at a time until we reach the border.
    for (; i + 8 < src->bytes_written; i += 8) {
      uint64_t chunk;
      memcpy(&chunk, src->data.get() + i, 8);
      uint64_t out = buf | (chunk << bits_in_buffer);
      memcpy(dest_buf + i, &out, 8);
      buf = chunk >> (64 - bits_in_buffer);
    }
    dest->buffer = buf;
    dest->bytes_written += i;
    for (; i < src->bytes_written; i++) {
      dest->Write(8, src->data[i]);
    }
  }
  dest->Write(src->bits_in_buffer, src->buffer);
}

void AssembleFrame(size_t width, size_t height, size_t nb_chans,
                   size_t bitdepth,
                   const std::vector<std::array<BitWriter, 4>>& group_data,
                   BitWriter* output) {
  size_t total_size_groups = 0;
  std::vector<size_t> group_sizes(group_data.size());
  for (size_t i = 0; i < group_data.size(); i++) {
    size_t sz = 0;
    for (size_t j = 0; j < nb_chans; j++) {
      const auto& writer = group_data[i][j];
      sz += writer.bytes_written * 8 + writer.bits_in_buffer;
    }
    sz = (sz + 7) / 8;
    group_sizes[i] = sz;
    total_size_groups += sz * 8;
  }
  output->Allocate(1000 + group_data.size() * 32 + total_size_groups);

  // Signature
  output->Write(16, 0x0AFF);

  // Size header, hand-crafted.
  // Not small
  output->Write(1, 0);

  auto wsz = [output](size_t size) {
    if (size - 1 < (1 << 9)) {
      output->Write(2, 0b00);
      output->Write(9, size - 1);
    } else if (size - 1 < (1 << 13)) {
      output->Write(2, 0b01);
      output->Write(13, size - 1);
    } else if (size - 1 < (1 << 18)) {
      output->Write(2, 0b10);
      output->Write(18, size - 1);
    } else {
      output->Write(2, 0b11);
      output->Write(30, size - 1);
    }
  };

  wsz(height);

  // No special ratio.
  output->Write(3, 0);

  wsz(width);

  // Hand-crafted ImageMetadata.
  output->Write(1, 0);  // all_default
  output->Write(1, 0);  // extra_fields
  output->Write(1, 0);  // bit_depth.floating_point_sample
  if (bitdepth == 8) {
    output->Write(2, 0b00);  // bit_depth.bits_per_sample = 8
  } else if (bitdepth == 10) {
    output->Write(2, 0b01);  // bit_depth.bits_per_sample = 10
  } else if (bitdepth == 12) {
    output->Write(2, 0b10);  // bit_depth.bits_per_sample = 12
  } else {
    output->Write(2, 0b11);  // 1 + u(6)
    output->Write(6, bitdepth - 1);
  }
  output->Write(1, 1);  // 16-bit-buffer sufficient
  bool have_alpha = (nb_chans == 2 || nb_chans == 4);
  if (have_alpha) {
    output->Write(2, 0b01);  // One extra channel
    output->Write(1, 1);     // ... all_default (ie. 8-bit alpha)
  } else {
    output->Write(2, 0b00);  // No extra channel
  }
  output->Write(1, 0);  // Not XYB
  if (nb_chans > 1) {
    output->Write(1, 1);  // color_encoding.all_default (sRGB)
  } else {
    output->Write(1, 0);     // color_encoding.all_default false
    output->Write(1, 0);     // color_encoding.want_icc false
    output->Write(2, 1);     // grayscale
    output->Write(2, 1);     // D65
    output->Write(1, 0);     // no gamma transfer function
    output->Write(2, 0b10);  // tf: 2 + u(4)
    output->Write(4, 11);    // tf of sRGB
    output->Write(2, 1);     // relative rendering intent
  }
  output->Write(2, 0b00);  // No extensions.

  output->Write(1, 1);  // all_default transform data

  // No ICC, no preview. Frame should start at byte boundery.
  output->ZeroPadToByte();

  auto wsz_fh = [output](size_t size) {
    if (size < (1 << 8)) {
      output->Write(2, 0b00);
      output->Write(8, size);
    } else if (size - 256 < (1 << 11)) {
      output->Write(2, 0b01);
      output->Write(11, size - 256);
    } else if (size - 2304 < (1 << 14)) {
      output->Write(2, 0b10);
      output->Write(14, size - 2304);
    } else {
      output->Write(2, 0b11);
      output->Write(30, size - 18688);
    }
  };

  // Handcrafted frame header.
  output->Write(1, 0);     // all_default
  output->Write(2, 0b00);  // regular frame
  output->Write(1, 1);     // modular
  output->Write(2, 0b00);  // default flags
  output->Write(1, 0);     // not YCbCr
  output->Write(2, 0b00);  // no upsampling
  if (have_alpha) {
    output->Write(2, 0b00);  // no alpha upsampling
  }
  output->Write(2, 0b01);  // default group size
  output->Write(2, 0b00);  // exactly one pass
  if (width % kChunkSize == 0) {
    output->Write(1, 0);  // no custom size or origin
  } else {
    output->Write(1, 1);  // custom size
    wsz_fh(0);            // x0 = 0
    wsz_fh(0);            // y0 = 0
    wsz_fh((width + kChunkSize - 1) / kChunkSize *
           kChunkSize);  // xsize rounded up to chunk size
    wsz_fh(height);      // ysize same
  }
  output->Write(2, 0b00);  // kReplace blending mode
  if (have_alpha) {
    output->Write(2, 0b00);  // kReplace blending mode for alpha channel
  }
  output->Write(1, 1);     // is_last
  output->Write(2, 0b00);  // a frame has no name
  output->Write(1, 0);     // loop filter is not all_default
  output->Write(1, 0);     // no gaborish
  output->Write(2, 0);     // 0 EPF iters
  output->Write(2, 0b00);  // No LF extensions
  output->Write(2, 0b00);  // No FH extensions

  output->Write(1, 0);      // No TOC permutation
  output->ZeroPadToByte();  // TOC is byte-aligned.
  for (size_t i = 0; i < group_data.size(); i++) {
    size_t sz = group_sizes[i];
    if (sz < (1 << 10)) {
      output->Write(2, 0b00);
      output->Write(10, sz);
    } else if (sz - 1024 < (1 << 14)) {
      output->Write(2, 0b01);
      output->Write(14, sz - 1024);
    } else if (sz - 17408 < (1 << 22)) {
      output->Write(2, 0b10);
      output->Write(22, sz - 17408);
    } else {
      output->Write(2, 0b11);
      output->Write(30, sz - 4211712);
    }
  }
  output->ZeroPadToByte();  // Groups are byte-aligned.

  for (size_t i = 0; i < group_data.size(); i++) {
    for (size_t j = 0; j < nb_chans; j++) {
      AppendWriter(output, &group_data[i][j]);
    }
    output->ZeroPadToByte();
  }
}

void PrepareDCGlobalCommon(bool is_single_group, size_t width, size_t height,
                           const PrefixCode& code, BitWriter* output) {
  output->Allocate(100000 + (is_single_group ? width * height * 16 : 0));
  // No patches, spline or noise.
  output->Write(1, 1);  // default DC dequantization factors (?)
  output->Write(1, 1);  // use global tree / histograms
  output->Write(1, 0);  // no lz77 for the tree

  output->Write(1, 1);   // simple code for the tree's context map
  output->Write(2, 0);   // all contexts clustered together
  output->Write(1, 1);   // use prefix code for tree
  output->Write(4, 15);  // don't do hybriduint for tree - 2 symbols anyway
  output->Write(7, 0b0100101);  // Alphabet size is 6: we need 0 and 5 (var16)
  output->Write(2, 1);          // simple prefix code
  output->Write(2, 1);          // with two symbols
  output->Write(3, 0);          // 0
  output->Write(3, 5);          // 5
  output->Write(5, 0b00010);    // tree repr: predictor is 5, all else 0

  output->Write(1, 1);     // Enable lz77 for the main bitstream
  output->Write(2, 0b00);  // lz77 offset 224
  static_assert(kLZ77Offset == 224, "");
  output->Write(10, 0b0000011111);  // lz77 min length 16
  static_assert(kLZ77MinLength == 16, "");
  output->Write(4, 4);  // 404 hybrid uint config for lz77: 4
  output->Write(3, 0);  // 0
  output->Write(3, 4);  // 4
  output->Write(1, 1);  // simple code for the context map
  output->Write(2, 1);  // two clusters
  output->Write(1, 1);  // raw/lz77 length histogram last
  output->Write(1, 0);  // distance histogram first
  output->Write(1, 1);  // use prefix codes
  output->Write(4, 0);  // 000 hybrid uint config for distances (only need 0)
  output->Write(4, 0);  // 000 hybrid uint config for symbols (only <= 10)
  // Distance alphabet size:
  output->Write(5, 0b00001);  // 2: just need 1 for RLE (i.e. distance 1)
  // Symbol + LZ77 alphabet size:
  output->Write(1, 1);    // > 1
  output->Write(4, 8);    // <= 512
  output->Write(8, 255);  // == 512

  // Distance histogram:
  output->Write(2, 1);  // simple prefix code
  output->Write(2, 0);  // with one symbol
  output->Write(1, 1);  // 1

  // Symbol + lz77 histogram:
  code.WriteTo(output);

  // Group header for global modular image.
  output->Write(1, 1);  // Global tree
  output->Write(1, 1);  // All default wp
}

void PrepareDCGlobal(bool is_single_group, size_t width, size_t height,
                     size_t nb_chans, size_t bitdepth, const PrefixCode& code,
                     BitWriter* output) {
  PrepareDCGlobalCommon(is_single_group, width, height, code, output);
  if (nb_chans > 2) {
    output->Write(2, 0b01);     // 1 transform
    output->Write(2, 0b00);     // RCT
    output->Write(5, 0b00000);  // Starting from ch 0
    output->Write(2, 0b00);     // YCoCg
  } else {
    output->Write(2, 0b00);  // no transforms
  }
  if (!is_single_group) {
    output->ZeroPadToByte();
  }
}

void EncodeHybridUint404_Mul16(uint32_t value, uint32_t* token_div16,
                               uint32_t* nbits, uint32_t* bits) {
  // NOTE: token in libjxl is actually << 4.
  uint32_t n = 31 - __builtin_clz(value);
  *token_div16 = value < 16 ? 0 : n - 3;
  *nbits = value < 16 ? 0 : n - 4;
  *bits = value < 16 ? 0 : (value >> 4) - (1 << *nbits);
}

#ifdef FASTLL_ENABLE_AVX2_INTRINSICS
#include <immintrin.h>
void EncodeChunk(const uint16_t* residuals, const PrefixCode& prefix_code,
                 BitWriter& output) {
  static_assert(kChunkSize == 16, "Chunk size must be 16");
  auto value = _mm256_load_si256((__m256i*)residuals);

  // we know that residuals[i] has at most 12 bits, so we just need 3 nibbles
  // and don't need to mask the third. However we do need to set the high
  // byte to 0xFF, which will make table lookups return 0.
  auto lo_nibble =
      _mm256_or_si256(_mm256_and_si256(value, _mm256_set1_epi16(0xF)),
                      _mm256_set1_epi16(0xFF00));
  auto mi_nibble = _mm256_or_si256(
      _mm256_and_si256(_mm256_srli_epi16(value, 4), _mm256_set1_epi16(0xF)),
      _mm256_set1_epi16(0xFF00));
  auto hi_nibble =
      _mm256_or_si256(_mm256_srli_epi16(value, 8), _mm256_set1_epi16(0xFF00));

  auto lo_lut = _mm256_broadcastsi128_si256(
      _mm_setr_epi8(0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4));
  auto mi_lut = _mm256_broadcastsi128_si256(
      _mm_setr_epi8(0, 5, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8));
  auto hi_lut = _mm256_broadcastsi128_si256(_mm_setr_epi8(
      0, 9, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12));

  auto lo_token = _mm256_shuffle_epi8(lo_lut, lo_nibble);
  auto mi_token = _mm256_shuffle_epi8(mi_lut, mi_nibble);
  auto hi_token = _mm256_shuffle_epi8(hi_lut, hi_nibble);

  auto token = _mm256_max_epi16(lo_token, _mm256_max_epi16(mi_token, hi_token));
  auto nbits = _mm256_subs_epu16(token, _mm256_set1_epi16(1));

  // Compute 1<<nbits.
  auto pow2_lo_lut = _mm256_broadcastsi128_si256(
      _mm_setr_epi8(1 << 0, 1 << 1, 1 << 2, 1 << 3, 1 << 4, 1 << 5, 1 << 6,
                    1u << 7, 0, 0, 0, 0, 0, 0, 0, 0));
  auto pow2_hi_lut = _mm256_broadcastsi128_si256(
      _mm_setr_epi8(0, 0, 0, 0, 0, 0, 0, 0, 1 << 0, 1 << 1, 1 << 2, 1 << 3,
                    1 << 4, 1 << 5, 1 << 6, 1u << 7));

  auto nbits_masked = _mm256_or_si256(nbits, _mm256_set1_epi16(0xFF00));

  auto nbits_pow2_lo = _mm256_shuffle_epi8(pow2_lo_lut, nbits_masked);
  auto nbits_pow2_hi = _mm256_shuffle_epi8(pow2_hi_lut, nbits_masked);

  auto nbits_pow2 =
      _mm256_or_si256(_mm256_slli_epi16(nbits_pow2_hi, 8), nbits_pow2_lo);

  auto bits = _mm256_subs_epu16(value, nbits_pow2);

  auto token_masked = _mm256_or_si256(token, _mm256_set1_epi16(0xFF00));

  // huff_nbits <= 6.
  auto huff_nbits =
      _mm256_shuffle_epi8(_mm256_broadcastsi128_si256(
                              _mm_load_si128((__m128i*)prefix_code.raw_nbits)),
                          token_masked);

  auto huff_bits =
      _mm256_shuffle_epi8(_mm256_broadcastsi128_si256(
                              _mm_load_si128((__m128i*)prefix_code.raw_bits)),
                          token_masked);

  auto huff_nbits_masked =
      _mm256_or_si256(huff_nbits, _mm256_set1_epi16(0xFF00));

  auto bits_shifted = _mm256_mullo_epi16(
      bits, _mm256_shuffle_epi8(pow2_lo_lut, huff_nbits_masked));

  nbits = _mm256_add_epi16(nbits, huff_nbits);
  bits = _mm256_or_si256(bits_shifted, huff_bits);

  // Merge nbits and bits from 16-bit to 32-bit lanes.
  auto nbits_hi16 = _mm256_srli_epi32(nbits, 16);
  auto nbits_lo16 = _mm256_and_si256(nbits, _mm256_set1_epi32(0xFFFF));
  auto bits_hi16 = _mm256_srli_epi32(bits, 16);
  auto bits_lo16 = _mm256_and_si256(bits, _mm256_set1_epi32(0xFFFF));

  nbits = _mm256_add_epi32(nbits_hi16, nbits_lo16);
  bits = _mm256_or_si256(_mm256_sllv_epi32(bits_hi16, nbits_lo16), bits_lo16);

  // Merge 32 -> 64 bit lanes.
  auto nbits_hi32 = _mm256_srli_epi64(nbits, 32);
  auto nbits_lo32 = _mm256_and_si256(nbits, _mm256_set1_epi64x(0xFFFFFFFF));
  auto bits_hi32 = _mm256_srli_epi64(bits, 32);
  auto bits_lo32 = _mm256_and_si256(bits, _mm256_set1_epi64x(0xFFFFFFFF));

  nbits = _mm256_add_epi64(nbits_hi32, nbits_lo32);
  bits = _mm256_or_si256(_mm256_sllv_epi64(bits_hi32, nbits_lo32), bits_lo32);

  alignas(32) uint64_t nbits_simd[4] = {};
  alignas(32) uint64_t bits_simd[4] = {};

  _mm256_store_si256((__m256i*)nbits_simd, nbits);
  _mm256_store_si256((__m256i*)bits_simd, bits);

  // Manually merge the buffer bits with the SIMD bits.
  // Necessary because Write() is only guaranteed to work with <=56 bits.
  // Trying to SIMD-fy this code results in slower speed (and definitely less
  // clarity).
  {
    for (size_t i = 0; i < 4; i++) {
      output.buffer |= bits_simd[i] << output.bits_in_buffer;
      memcpy(output.data.get() + output.bytes_written, &output.buffer, 8);
      // If >> 64, next_buffer is unused.
      uint64_t next_buffer = bits_simd[i] >> (64 - output.bits_in_buffer);
      output.bits_in_buffer += nbits_simd[i];
      // This `if` seems to be faster than using ternaries.
      if (output.bits_in_buffer >= 64) {
        output.buffer = next_buffer;
        output.bits_in_buffer -= 64;
        output.bytes_written += 8;
      }
    }
    memcpy(output.data.get() + output.bytes_written, &output.buffer, 8);
    size_t bytes_in_buffer = output.bits_in_buffer / 8;
    output.bits_in_buffer -= bytes_in_buffer * 8;
    output.buffer >>= bytes_in_buffer * 8;
    output.bytes_written += bytes_in_buffer;
  }
}
#endif

#ifdef FASTLL_ENABLE_NEON_INTRINSICS
#include <arm_neon.h>

void EncodeChunk(const uint16_t* residuals, const PrefixCode& code,
                 BitWriter& output) {
  uint16x8_t res = vld1q_u16(residuals);
  uint16x8_t token = vsubq_u16(vdupq_n_u16(16), vclzq_u16(res));
  uint16x8_t nbits = vqsubq_u16(token, vdupq_n_u16(1));
  uint16x8_t bits = vqsubq_u16(res, vshlq_s16(vdupq_n_s16(1), nbits));
  uint16x8_t huff_bits =
      vandq_u16(vdupq_n_u16(0xFF), vqtbl1q_u8(vld1q_u8(code.raw_bits), token));
  uint16x8_t huff_nbits =
      vandq_u16(vdupq_n_u16(0xFF), vqtbl1q_u8(vld1q_u8(code.raw_nbits), token));
  bits = vorrq_u16(vshlq_u16(bits, huff_nbits), huff_bits);
  nbits = vaddq_u16(nbits, huff_nbits);

  // Merge nbits and bits from 16-bit to 32-bit lanes.
  uint32x4_t nbits_lo16 = vandq_u32(nbits, vdupq_n_u32(0xFFFF));
  uint32x4_t bits_hi16 = vshlq_u32(vshrq_n_u32(bits, 16), nbits_lo16);
  uint32x4_t bits_lo16 = vandq_u32(bits, vdupq_n_u32(0xFFFF));

  uint32x4_t nbits32 = vsraq_n_u32(nbits_lo16, nbits, 16);
  uint32x4_t bits32 = vorrq_u32(bits_hi16, bits_lo16);

  // Merging up to 64 bits is not faster.

  // Manually merge the buffer bits with the SIMD bits.
  // A bit faster.
  for (size_t i = 0; i < 4; i++) {
    output.buffer |= bits32[i] << output.bits_in_buffer;
    memcpy(output.data.get() + output.bytes_written, &output.buffer, 8);
    output.bits_in_buffer += nbits32[i];
    size_t bytes_in_buffer = output.bits_in_buffer / 8;
    output.bits_in_buffer -= bytes_in_buffer * 8;
    output.buffer >>= bytes_in_buffer * 8;
    output.bytes_written += bytes_in_buffer;
  }
}
#endif

template <size_t bytedepth>
struct ChunkEncoder {
  static void EncodeRle(size_t count, const PrefixCode& code,
                        BitWriter& output) {
    if (count == 0) return;
    count -= kLZ77MinLength;
    unsigned token_div16, nbits, bits;
    EncodeHybridUint404_Mul16(count, &token_div16, &nbits, &bits);
    output.Write(
        code.lz77_nbits[token_div16] + nbits,
        (bits << code.lz77_nbits[token_div16]) | code.lz77_bits[token_div16]);
  }

  inline void Chunk(size_t run, uint16_t* residuals) {
    EncodeRle(run, *code, *output);
#if defined(FASTLL_ENABLE_AVX2_INTRINSICS) && FASTLL_ENABLE_AVX2_INTRINSICS
    if (bytedepth == 1) {
      EncodeChunk(residuals, *code, *output);
      return;
    }
#elif defined(FASTLL_ENABLE_NEON_INTRINSICS) && FASTLL_ENABLE_NEON_INTRINSICS
    if (bytedepth == 1) {
      EncodeChunk(residuals, *code, *output);
      if (kChunkSize > 8) {
        EncodeChunk(residuals + 8, *code, *output);
      }
      return;
    }
#endif
    for (size_t ix = 0; ix < kChunkSize; ix++) {
      unsigned token, nbits, bits;
      EncodeHybridUint000(residuals[ix], &token, &nbits, &bits);
      output->Write(code->raw_nbits[token] + nbits,
                    code->raw_bits[token] | bits << code->raw_nbits[token]);
    }
  }

  inline void Finalize(size_t run) { EncodeRle(run, *code, *output); }

  const PrefixCode* code;
  BitWriter* output;
};

struct ChunkSampleCollector {
  void Rle(size_t count, uint64_t* lz77_counts) {
    if (count == 0) return;
    count -= kLZ77MinLength;
    unsigned token_div16, nbits, bits;
    EncodeHybridUint404_Mul16(count, &token_div16, &nbits, &bits);
    lz77_counts[token_div16]++;
  }

  inline void Chunk(size_t run, uint16_t* residuals) {
    // Run is broken. Encode the run and encode the individual vector.
    Rle(run, lz77_counts);
    for (size_t ix = 0; ix < kChunkSize; ix++) {
      unsigned token, nbits, bits;
      EncodeHybridUint000(residuals[ix], &token, &nbits, &bits);
      raw_counts[token]++;
    }
  }

  // don't count final run since we don't know how long it really is
  void Finalize(size_t run) {}

  uint64_t* raw_counts;
  uint64_t* lz77_counts;
};

constexpr uint16_t PackSigned(int16_t value) {
  return (static_cast<uint16_t>(value) << 1) ^
         ((static_cast<uint16_t>(~value) >> 15) - 1);
}

template <typename T>
struct ChannelRowProcessor {
  T* t;
  inline void ProcessChunk(const int16_t* row, const int16_t* row_left,
                           const int16_t* row_top, const int16_t* row_topleft) {
    bool continue_rle = true;
    alignas(32) uint16_t residuals[kChunkSize] = {};
    for (size_t ix = 0; ix < kChunkSize; ix++) {
      int16_t px = row[ix];
      int16_t left = row_left[ix];
      int16_t top = row_top[ix];
      int16_t topleft = row_topleft[ix];
      int16_t ac = left - topleft;
      int16_t ab = left - top;
      int16_t bc = top - topleft;
      int16_t grad = static_cast<int16_t>(static_cast<uint16_t>(ac) +
                                          static_cast<uint16_t>(top));
      int16_t d = ab ^ bc;
      int16_t clamp = d < 0 ? top : left;
      int16_t s = ac ^ bc;
      int16_t pred = s < 0 ? grad : clamp;
      residuals[ix] = PackSigned(px - pred);
      continue_rle &= residuals[ix] == last;
    }
    // Run continues, nothing to do.
    if (continue_rle) {
      run += kChunkSize;
    } else {
      // Run is broken. Encode the run and encode the individual vector.
      t->Chunk(run, residuals);
      run = 0;
    }
    last = residuals[kChunkSize - 1];
  }
  void ProcessRow(const int16_t* row, const int16_t* row_left,
                  const int16_t* row_top, const int16_t* row_topleft,
                  size_t xs) {
    for (size_t x = 0; x + kChunkSize <= xs; x += kChunkSize) {
      ProcessChunk(row + x, row_left + x, row_top + x, row_topleft + x);
    }
  }

  void Finalize() { t->Finalize(run); }
  size_t run = 0;
  uint16_t last = 0xFFFF;  // Can never appear
};

template <typename Processor, size_t nb_chans, size_t bytedepth>
void ProcessImageArea(const unsigned char* rgba, size_t x0, size_t y0,
                      size_t oxs, size_t xs, size_t yskip, size_t ys,
                      size_t row_stride, Processor* processors) {
  constexpr size_t kPadding = 16;

  int16_t group_data[nb_chans][2][256 + kPadding * 2] = {};
  int16_t allzero[nb_chans] = {};
  int16_t allone[nb_chans];
  auto get_pixel = [&](size_t x, size_t y, size_t channel) {
    int16_t p = rgba[row_stride * (y0 + y) + (x0 + x) * nb_chans * bytedepth +
                     channel * bytedepth];
    if (bytedepth == 2) {
      p <<= 8;
      p |= rgba[row_stride * (y0 + y) + (x0 + x) * nb_chans * 2 + channel * 2 +
                1];
    }
    return p;
  };

  for (size_t i = 0; i < nb_chans; i++) allone[i] = 0xffff;
  for (size_t y = 0; y < ys; y++) {
    // Pre-fill rows with YCoCg converted pixels.
    for (size_t x = 0; x < oxs; x++) {
      if (nb_chans < 3) {
        int16_t luma = get_pixel(x, y, 0);
        group_data[0][y & 1][x + kPadding] = luma;
        if (nb_chans == 2) {
          int16_t a = get_pixel(x, y, 1);
          group_data[1][y & 1][x + kPadding] = a;
        }
      } else {
        int16_t r = get_pixel(x, y, 0);
        int16_t g = get_pixel(x, y, 1);
        int16_t b = get_pixel(x, y, 2);
        if (nb_chans == 4) {
          int16_t a = get_pixel(x, y, 3);
          group_data[3][y & 1][x + kPadding] = a;
          group_data[1][y & 1][x + kPadding] = a ? r - b : 0;
          int16_t tmp = b + (group_data[1][y & 1][x + kPadding] >> 1);
          group_data[2][y & 1][x + kPadding] = a ? g - tmp : 0;
          group_data[0][y & 1][x + kPadding] =
              a ? tmp + (group_data[2][y & 1][x + kPadding] >> 1) : 0;
        } else {
          group_data[1][y & 1][x + kPadding] = r - b;
          int16_t tmp = b + (group_data[1][y & 1][x + kPadding] >> 1);
          group_data[2][y & 1][x + kPadding] = g - tmp;
          group_data[0][y & 1][x + kPadding] =
              tmp + (group_data[2][y & 1][x + kPadding] >> 1);
        }
      }
      for (size_t c = 0; c < nb_chans; c++) {
        allzero[c] |= group_data[c][y & 1][x + kPadding];
        allone[c] &= group_data[c][y & 1][x + kPadding];
      }
    }
    // Deal with x == 0.
    for (size_t c = 0; c < nb_chans; c++) {
      group_data[c][y & 1][kPadding - 1] =
          y > 0 ? group_data[c][(y - 1) & 1][kPadding] : 0;
      // Fix topleft.
      group_data[c][(y - 1) & 1][kPadding - 1] =
          y > 0 ? group_data[c][(y - 1) & 1][kPadding] : 0;
    }
    // Fill in padding.
    for (size_t c = 0; c < nb_chans; c++) {
      for (size_t x = oxs; x < xs; x++) {
        group_data[c][y & 1][kPadding + x] =
            group_data[c][y & 1][kPadding + oxs - 1];
      }
    }
    if (y < yskip) continue;
    for (size_t c = 0; c < nb_chans; c++) {
      if (y > 0 && (allzero[c] == 0 || (allone[c] == 0xff && bytedepth == 1))) {
        processors[c].run += xs;
        continue;
      }

      // Get pointers to px/left/top/topleft data to speedup loop.
      const int16_t* row = &group_data[c][y & 1][kPadding];
      const int16_t* row_left = &group_data[c][y & 1][kPadding - 1];
      const int16_t* row_top =
          y == 0 ? row_left : &group_data[c][(y - 1) & 1][kPadding];
      const int16_t* row_topleft =
          y == 0 ? row_left : &group_data[c][(y - 1) & 1][kPadding - 1];

      processors[c].ProcessRow(row, row_left, row_top, row_topleft, xs);
    }
  }
  for (size_t c = 0; c < nb_chans; c++) {
    processors[c].Finalize();
  }
}

template <size_t nb_chans, size_t bytedepth>
void WriteACSection(const unsigned char* rgba, size_t x0, size_t y0, size_t oxs,
                    size_t ys, size_t row_stride, bool is_single_group,
                    const PrefixCode& code, std::array<BitWriter, 4>& output) {
  size_t xs = (oxs + kChunkSize - 1) / kChunkSize * kChunkSize;
  for (size_t i = 0; i < nb_chans; i++) {
    if (is_single_group && i == 0) continue;
    output[i].Allocate(16 * xs * ys * bytedepth + 4);
  }
  if (!is_single_group) {
    // Group header for modular image.
    // When the image is single-group, the global modular image is the one that
    // contains the pixel data, and there is no group header.
    output[0].Write(1, 1);     // Global tree
    output[0].Write(1, 1);     // All default wp
    output[0].Write(2, 0b00);  // 0 transforms
  }

  ChunkEncoder<bytedepth> encoders[nb_chans];
  ChannelRowProcessor<ChunkEncoder<bytedepth>> row_encoders[nb_chans];
  for (size_t c = 0; c < nb_chans; c++) {
    row_encoders[c].t = &encoders[c];
    encoders[c].output = &output[c];
    encoders[c].code = &code;
  }
  ProcessImageArea<ChannelRowProcessor<ChunkEncoder<bytedepth>>, nb_chans,
                   bytedepth>(rgba, x0, y0, oxs, xs, 0, ys, row_stride,
                              row_encoders);
}

constexpr int kHashExp = 16;
constexpr uint32_t kHashSize = 1 << kHashExp;
constexpr uint32_t kHashMultiplier = 2654435761;
constexpr int kMaxColors = 512;

// can be any function that returns a value in 0 .. kHashSize-1
// has to map 0 to 0
inline uint32_t pixel_hash(uint32_t p) {
  return (p * kHashMultiplier) >> (32 - kHashExp);
}

template <typename Processor, size_t nb_chans>
void ProcessImageAreaPalette(const unsigned char* rgba, size_t x0, size_t y0,
                             size_t oxs, size_t xs, size_t yskip, size_t ys,
                             size_t row_stride, const int16_t* lookup,
                             Processor* processors) {
  constexpr size_t kPadding = 16;

  int16_t group_data[2][256 + kPadding * 2] = {};
  Processor& row_encoder = processors[0];

  for (size_t y = 0; y < ys; y++) {
    // Pre-fill rows with palette converted pixels.
    const unsigned char* inrow = rgba + row_stride * (y0 + y) + x0 * nb_chans;
    for (size_t x = 0; x < oxs; x++) {
      uint32_t p = 0;
      memcpy(&p, inrow + x * nb_chans, nb_chans);
      group_data[y & 1][x + kPadding] = lookup[pixel_hash(p)];
    }
    // Deal with x == 0.
    group_data[y & 1][kPadding - 1] =
        y > 0 ? group_data[(y - 1) & 1][kPadding] : 0;
    // Fix topleft.
    group_data[(y - 1) & 1][kPadding - 1] =
        y > 0 ? group_data[(y - 1) & 1][kPadding] : 0;
    // Fill in padding.
    for (size_t x = oxs; x < xs; x++) {
      group_data[y & 1][kPadding + x] = group_data[y & 1][kPadding + oxs - 1];
    }
    // Get pointers to px/left/top/topleft data to speedup loop.
    const int16_t* row = &group_data[y & 1][kPadding];
    const int16_t* row_left = &group_data[y & 1][kPadding - 1];
    const int16_t* row_top =
        y == 0 ? row_left : &group_data[(y - 1) & 1][kPadding];
    const int16_t* row_topleft =
        y == 0 ? row_left : &group_data[(y - 1) & 1][kPadding - 1];

    row_encoder.ProcessRow(row, row_left, row_top, row_topleft, xs);
  }
  row_encoder.Finalize();
}

template <size_t nb_chans>
void WriteACSectionPalette(const unsigned char* rgba, size_t x0, size_t y0,
                           size_t oxs, size_t ys, size_t row_stride,
                           bool is_single_group, const PrefixCode& code,
                           const int16_t* lookup, BitWriter& output) {
  size_t xs = (oxs + kChunkSize - 1) / kChunkSize * kChunkSize;

  if (!is_single_group) {
    output.Allocate(16 * xs * ys + 4);
    // Group header for modular image.
    // When the image is single-group, the global modular image is the one that
    // contains the pixel data, and there is no group header.
    output.Write(1, 1);     // Global tree
    output.Write(1, 1);     // All default wp
    output.Write(2, 0b00);  // 0 transforms
  }

  ChunkEncoder<1> encoder;
  ChannelRowProcessor<ChunkEncoder<1>> row_encoder;

  row_encoder.t = &encoder;
  encoder.output = &output;
  encoder.code = &code;
  ProcessImageAreaPalette<ChannelRowProcessor<ChunkEncoder<1>>, nb_chans>(
      rgba, x0, y0, oxs, xs, 0, ys, row_stride, lookup, &row_encoder);
}

template <size_t nb_chans, size_t bytedepth>
void CollectSamples(const unsigned char* rgba, size_t x0, size_t y0, size_t xs,
                    size_t row_stride, size_t row_count, uint64_t* raw_counts,
                    uint64_t* lz77_counts, bool palette,
                    const int16_t* lookup) {
  ChunkSampleCollector sample_collectors[nb_chans];
  ChannelRowProcessor<ChunkSampleCollector> row_sample_collectors[nb_chans];
  for (size_t c = 0; c < nb_chans; c++) {
    row_sample_collectors[c].t = &sample_collectors[c];
    sample_collectors[c].raw_counts = raw_counts;
    sample_collectors[c].lz77_counts = lz77_counts;
  }
  if (palette) {
    assert(bytedepth == 1);
    ProcessImageAreaPalette<ChannelRowProcessor<ChunkSampleCollector>,
                            nb_chans>(rgba, x0, y0, xs, xs, 1, 1 + row_count,
                                      row_stride, lookup,
                                      row_sample_collectors);
  } else {
    ProcessImageArea<ChannelRowProcessor<ChunkSampleCollector>, nb_chans,
                     bytedepth>(rgba, x0, y0, xs, xs, 1, 1 + row_count,
                                row_stride, row_sample_collectors);
  }
}

void PrepareDCGlobalPalette(bool is_single_group, size_t width, size_t height,
                            const PrefixCode& code,
                            const std::vector<uint32_t>& palette,
                            size_t pcolors_real, BitWriter* output) {
  PrepareDCGlobalCommon(is_single_group, width, height, code, output);
  output->Write(2, 0b01);     // 1 transform
  output->Write(2, 0b01);     // Palette
  output->Write(5, 0b00000);  // Starting from ch 0
  output->Write(2, 0b10);     // 4-channel palette (RGBA)
  size_t pcolors = (pcolors_real + kChunkSize - 1) / kChunkSize * kChunkSize;
  // pcolors <= kMaxColors + kChunkSize - 1
  static_assert(kMaxColors + kChunkSize < 1281,
                "add code to signal larger palette sizes");
  if (pcolors < 256) {
    output->Write(2, 0b00);
    output->Write(8, pcolors);
  } else {
    output->Write(2, 0b01);
    output->Write(10, pcolors - 256);
  }

  output->Write(2, 0b00);  // nb_deltas == 0
  output->Write(4, 0);     // Zero predictor for delta palette
  // Encode palette
  ChunkEncoder<1> encoder;
  ChannelRowProcessor<ChunkEncoder<1>> row_encoder;
  row_encoder.t = &encoder;
  encoder.output = output;
  encoder.code = &code;
  int16_t p[4][32 + 1024] = {};
  uint8_t prgba[4];
  int i = 0;
  int have_zero = 0;
  if (palette[pcolors_real - 1] == 0) have_zero = 1;
  for (; i < pcolors; i++) {
    if (i < pcolors_real) {
      memcpy(prgba, &palette[i], 4);
    }
    p[0][16 + i + have_zero] = prgba[0];
    p[1][16 + i + have_zero] = prgba[1];
    p[2][16 + i + have_zero] = prgba[2];
    p[3][16 + i + have_zero] = prgba[3];
  }
  p[0][15] = 0;
  row_encoder.ProcessRow(p[0] + 16, p[0] + 15, p[0] + 15, p[0] + 15, pcolors);
  p[1][15] = p[0][16];
  p[0][15] = p[0][16];
  row_encoder.ProcessRow(p[1] + 16, p[1] + 15, p[0] + 16, p[0] + 15, pcolors);
  p[2][15] = p[1][16];
  p[1][15] = p[1][16];
  row_encoder.ProcessRow(p[2] + 16, p[2] + 15, p[1] + 16, p[1] + 15, pcolors);
  p[3][15] = p[2][16];
  p[2][15] = p[2][16];
  row_encoder.ProcessRow(p[3] + 16, p[3] + 15, p[2] + 16, p[2] + 15, pcolors);
  row_encoder.Finalize();

  if (!is_single_group) {
    output->ZeroPadToByte();
  }
}

template <size_t nb_chans, size_t bytedepth>
size_t LLEnc(const unsigned char* rgba, size_t width, size_t stride,
             size_t height, size_t bitdepth, int effort,
             unsigned char** output) {
  size_t bytes_per_sample = (bitdepth > 8 ? 2 : 1);
  assert(bytedepth == bytes_per_sample);
  assert(width != 0);
  assert(height != 0);
  assert(stride >= nb_chans * bytes_per_sample * width);
  (void)bytes_per_sample;

  // Count colors to try palette
  std::vector<uint32_t> palette(kHashSize);
  palette[0] = 1;
  int16_t lookup[kHashSize];
  lookup[0] = 0;
  int pcolors = 0;
  bool collided =
      effort < 2 || bitdepth != 8 || nb_chans < 4;  // todo: also do rgb palette
  for (size_t y = 0; y < height && !collided; y++) {
    const unsigned char* r = rgba + stride * y;
    size_t x = 0;
    if (nb_chans == 4) {
      // this is just an unrolling of the next loop
      for (; x + 7 < width; x += 8) {
        uint32_t p[8], index[8];
        memcpy(p, r + x * 4, 32);
        for (int i = 0; i < 8; i++) index[i] = pixel_hash(p[i]);
        for (int i = 0; i < 8; i++) {
          uint32_t init_entry = index[i] ? 0 : 1;
          if (init_entry != palette[index[i]] && p[i] != palette[index[i]]) {
            collided = true;
          }
        }
        for (int i = 0; i < 8; i++) palette[index[i]] = p[i];
      }
      for (; x < width; x++) {
        uint32_t p;
        memcpy(&p, r + x * 4, 4);
        uint32_t index = pixel_hash(p);
        uint32_t init_entry = index ? 0 : 1;
        if (init_entry != palette[index] && p != palette[index]) {
          collided = true;
        }
        palette[index] = p;
      }
    } else {
      for (; x < width; x++) {
        uint32_t p = 0;
        memcpy(&p, r + x * nb_chans, nb_chans);
        uint32_t index = pixel_hash(p);
        uint32_t init_entry = index ? 0 : 1;
        if (init_entry != palette[index] && p != palette[index]) {
          collided = true;
        }
        palette[index] = p;
      }
    }
  }

  int nb_entries = 0;
  if (!collided) {
    if (palette[0] == 0) pcolors = 1;
    if (palette[0] == 1) palette[0] = 0;
    bool have_color = false;
    uint8_t minG = 255, maxG = 0;
    for (int k = 0; k < kHashSize; k++) {
      if (palette[k] == 0) continue;
      uint8_t p[4];
      memcpy(p, &palette[k], 4);
      // move entries to front so sort has less work
      palette[nb_entries] = palette[k];
      if (p[0] != p[1] || p[0] != p[2]) have_color = true;
      if (p[1] < minG) minG = p[1];
      if (p[1] > maxG) maxG = p[1];
      nb_entries++;
      // don't do palette if too many colors are needed
      if (nb_entries + pcolors > kMaxColors) {
        collided = true;
        break;
      }
    }
    if (!have_color) {
      // don't do palette if it's just grayscale without many holes
      if (maxG - minG < nb_entries * 1.4f) collided = true;
    }
  }
  if (!collided) {
    std::sort(
        palette.begin(), palette.begin() + nb_entries,
        [](uint32_t ap, uint32_t bp) {
          if (ap == 0) return false;
          if (bp == 0) return true;
          uint8_t a[4], b[4];
          memcpy(a, &ap, 4);
          memcpy(b, &bp, 4);
          float ay, by;
          ay = (0.299f * a[0] + 0.587f * a[1] + 0.114f * a[2] + 0.01f) * a[3];
          by = (0.299f * b[0] + 0.587f * b[1] + 0.114f * b[2] + 0.01f) * b[3];
          return ay < by;  // sort on alpha*luma
        });
    for (int k = 0; k < nb_entries; k++) {
      if (palette[k] == 0) break;
      lookup[pixel_hash(palette[k])] = pcolors++;
    }
  }

  // Width gets padded to kChunkSize, but this computation doesn't change
  // because of that.
  size_t num_groups_x = (width + 255) / 256;
  size_t num_groups_y = (height + 255) / 256;
  size_t num_dc_groups_x = (width + 2047) / 2048;
  size_t num_dc_groups_y = (height + 2047) / 2048;

  uint64_t raw_counts[16] = {};
  uint64_t lz77_counts[17] = {};

  // sample the middle (effort * 2) rows of every group
  for (size_t g = 0; g < num_groups_y * num_groups_x; g++) {
    size_t xg = g % num_groups_x;
    size_t yg = g / num_groups_x;
    int y_offset = yg * 256;
    int y_max = std::min<size_t>(height - yg * 256, 256);
    int y_begin = y_offset + std::max<int>(0, y_max - 2 * effort) / 2;
    int y_count =
        std::min<int>(2 * effort * y_max / 256, y_offset + y_max - y_begin - 1);
    int x_max =
        std::min<size_t>(width - xg * 256, 256) / kChunkSize * kChunkSize;
    CollectSamples<nb_chans, bytedepth>(rgba, xg * 256, y_begin, x_max, stride,
                                        y_count, raw_counts, lz77_counts,
                                        !collided, lookup);
  }

  uint64_t base_raw_counts[16] = {3843, 852, 1270, 1214, 1014, 727, 481, 300,
                                  159,  51,  5,    1,    1,    1,   1,   1};

  bool doing_ycocg = nb_chans > 2 && collided;
  for (size_t i = bitdepth + 2 + (doing_ycocg ? 1 : 0); i < 16; i++) {
    base_raw_counts[i] = 0;
  }
  uint64_t base_lz77_counts[17] = {
      // short runs will be sampled, but long ones won't.
      // near full-group run is quite common (e.g. all-opaque alpha)
      18, 12, 9, 11, 15, 2, 2, 1, 1, 1, 1, 2, 300, 0, 0, 0, 0};

  for (size_t i = 0; i < 16; i++) {
    raw_counts[i] = (raw_counts[i] << 8) + base_raw_counts[i];
  }
  if (!collided) {
    unsigned token, nbits, bits;
    EncodeHybridUint000(PackSigned(pcolors - 1), &token, &nbits, &bits);
    // ensure all palette indices can actually be encoded
    for (size_t i = 0; i < token + 1; i++)
      raw_counts[i] = std::max<uint64_t>(raw_counts[i], 1);
    // these tokens are only used for the palette itself so they can get a bad
    // code
    for (size_t i = token + 1; i < 10; i++) raw_counts[i] = 1;
  }
  for (size_t i = 0; i < 17; i++) {
    lz77_counts[i] = (lz77_counts[i] << 8) + base_lz77_counts[i];
  }
  alignas(32) PrefixCode hcode(raw_counts, lz77_counts);

  BitWriter writer;

  bool onegroup = num_groups_x == 1 && num_groups_y == 1;

  size_t num_groups = onegroup ? 1
                               : (2 + num_dc_groups_x * num_dc_groups_y +
                                  num_groups_x * num_groups_y);

  std::vector<std::array<BitWriter, 4>> group_data(num_groups);
  if (collided) {
    PrepareDCGlobal(onegroup, width, height, nb_chans, bitdepth, hcode,
                    &group_data[0][0]);
  } else {
    PrepareDCGlobalPalette(onegroup, width, height, hcode, palette, pcolors,
                           &group_data[0][0]);
  }
#pragma omp parallel for
  for (size_t g = 0; g < num_groups_y * num_groups_x; g++) {
    size_t xg = g % num_groups_x;
    size_t yg = g / num_groups_x;
    size_t group_id =
        onegroup ? 0 : (2 + num_dc_groups_x * num_dc_groups_y + g);
    size_t xs = std::min<size_t>(width - xg * 256, 256);
    size_t ys = std::min<size_t>(height - yg * 256, 256);
    size_t x0 = xg * 256;
    size_t y0 = yg * 256;
    auto& gd = group_data[group_id];
    if (collided) {
      WriteACSection<nb_chans, bytedepth>(rgba, x0, y0, xs, ys, stride,
                                          onegroup, hcode, gd);

    } else {
      WriteACSectionPalette<nb_chans>(rgba, x0, y0, xs, ys, stride, onegroup,
                                      hcode, lookup, gd[0]);
    }
  }

  AssembleFrame(width, height, nb_chans, bitdepth, group_data, &writer);

  *output = writer.data.release();
  return writer.bytes_written;
}

size_t FastLosslessEncode(const unsigned char* rgba, size_t width,
                          size_t stride, size_t height, size_t nb_chans,
                          size_t bitdepth, int effort, unsigned char** output) {
  assert(bitdepth <= 12);
  assert(bitdepth > 0);
  assert(nb_chans <= 4);
  assert(nb_chans != 0);
  if (bitdepth <= 8) {
    if (nb_chans == 1) {
      return LLEnc<1, 1>(rgba, width, stride, height, bitdepth, effort, output);
    }
    if (nb_chans == 2) {
      return LLEnc<2, 1>(rgba, width, stride, height, bitdepth, effort, output);
    }
    if (nb_chans == 3) {
      return LLEnc<3, 1>(rgba, width, stride, height, bitdepth, effort, output);
    }
    if (nb_chans == 4) {
      return LLEnc<4, 1>(rgba, width, stride, height, bitdepth, effort, output);
    }
  } else {
    if (nb_chans == 1) {
      return LLEnc<1, 2>(rgba, width, stride, height, bitdepth, effort, output);
    }
    if (nb_chans == 2) {
      return LLEnc<2, 2>(rgba, width, stride, height, bitdepth, effort, output);
    }
    if (nb_chans == 3) {
      return LLEnc<3, 2>(rgba, width, stride, height, bitdepth, effort, output);
    }
    if (nb_chans == 4) {
      return LLEnc<4, 2>(rgba, width, stride, height, bitdepth, effort, output);
    }
  }
  return 0;
}
