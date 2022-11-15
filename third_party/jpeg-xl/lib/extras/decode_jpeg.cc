// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/decode_jpeg.h"

#include <cmath>

#include "hwy/aligned_allocator.h"
#include "lib/extras/dec_group_jpeg.h"
#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/common.h"
#include "lib/jxl/image.h"

namespace jxl {
namespace extras {

namespace {

// Padding for horizontal chroma upsampling.
constexpr size_t kPaddingLeft = CacheAligned::kAlignment;
constexpr size_t kPaddingRight = 1;
constexpr size_t kTempOutputLen = 1024;
constexpr int kMaxSampling = 2;
constexpr int kMaxHuffmanTables = 4;
constexpr size_t kJpegHuffmanMaxBitLength = 16;
constexpr int kJpegHuffmanAlphabetSize = 256;
constexpr int kMaxQuantTables = 4;
constexpr int kJpegDCAlphabetSize = 12;
constexpr int kMaxDimPixels = 65535;
// Max 14 block per MCU (when 1 channel is subsampled)
// Max 64 nonzero coefficients per block
// Max 16 symbol bits plus 11 extra bits per nonzero symbol
// Max 2 bytes per 8 bits (worst case is all bytes are escaped 0xff)
constexpr int kMaxMCUByteSize = 6048;
constexpr uint8_t kIccProfileTag[12] = "ICC_PROFILE";

constexpr int kJpegHuffmanRootTableBits = 8;
// Maximum huffman lookup table size.
// According to zlib/examples/enough.c, 758 entries are always enough for
// an alphabet of 257 symbols (256 + 1 special symbol for the all 1s code) and
// max bit length 16 if the root table has 8 bits.
constexpr int kJpegHuffmanLutSize = 758;

/* clang-format off */
constexpr uint32_t kJPEGNaturalOrder[80] = {
  0,   1,  8, 16,  9,  2,  3, 10,
  17, 24, 32, 25, 18, 11,  4,  5,
  12, 19, 26, 33, 40, 48, 41, 34,
  27, 20, 13,  6,  7, 14, 21, 28,
  35, 42, 49, 56, 57, 50, 43, 36,
  29, 22, 15, 23, 30, 37, 44, 51,
  58, 59, 52, 45, 38, 31, 39, 46,
  53, 60, 61, 54, 47, 55, 62, 63,
  // extra entries for safety in decoder
  63, 63, 63, 63, 63, 63, 63, 63,
  63, 63, 63, 63, 63, 63, 63, 63
};

/* clang-format on */

#ifdef JXL_CRASH_ON_ERROR
#define JPEG_ERROR(format, ...)                                              \
  (::jxl::Debug(("%s:%d: " format "\n"), __FILE__, __LINE__, ##__VA_ARGS__), \
   ::jxl::Abort(), JpegDecoder::Status::kError)

#else  // JXL_CRASH_ON_ERROR
#define JPEG_ERROR(format, ...)                                                \
  (((JXL_DEBUG_ON_ERROR) &&                                                    \
    ::jxl::Debug(("%s:%d: " format "\n"), __FILE__, __LINE__, ##__VA_ARGS__)), \
   JpegDecoder::Status::kError)
#endif  // JXL_CRASH_ON_ERROR

JpegDecoder::Status ConvertStatus(JpegDecoder::Status status) { return status; }

JpegDecoder::Status ConvertStatus(jxl::Status status) {
  return status ? JpegDecoder::Status::kSuccess : JpegDecoder::Status::kError;
}

#define JPEG_RETURN_IF_ERROR(expr)                                \
  {                                                               \
    JpegDecoder::Status status_ = ConvertStatus(expr);            \
    if (status_ != JpegDecoder::Status::kSuccess) return status_; \
  }

// Macros for commonly used error conditions.

#define JPEG_VERIFY_LEN(n)                                   \
  if (pos + (n) > len) {                                     \
    return JPEG_ERROR("Unexpected end of input: pos=%" PRIuS \
                      " need=%d len=%" PRIuS,                \
                      pos, static_cast<int>(n), len);        \
  }

#define JPEG_VERIFY_INPUT(var, low, high)                             \
  if ((var) < (low) || (var) > (high)) {                              \
    return JPEG_ERROR("Invalid " #var ": %d", static_cast<int>(var)); \
  }

#define JPEG_VERIFY_MARKER_END()                                \
  if (pos != len) {                                             \
    return JPEG_ERROR("Invalid marker length: declared=%" PRIuS \
                      " actual=%" PRIuS,                        \
                      len, pos);                                \
  }

inline int ReadUint8(const uint8_t* data, size_t* pos) {
  return data[(*pos)++];
}

inline int ReadUint16(const uint8_t* data, size_t* pos) {
  int v = (data[*pos] << 8) + data[*pos + 1];
  *pos += 2;
  return v;
}

// Helper structure to read bits from the entropy coded data segment.
struct BitReaderState {
  BitReaderState(const uint8_t* data, const size_t len, size_t pos)
      : data_(data), len_(len), start_pos_(pos) {
    Reset(pos);
  }

  void Reset(size_t pos) {
    pos_ = pos;
    val_ = 0;
    bits_left_ = 0;
    next_marker_pos_ = len_;
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
      uint8_t escape = pos_ < len_ ? data_[pos_] : 0;
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

  // Sets *pos to the next stream position, and *bit_pos to the bit position
  // within the next byte where parsing should continue.
  // Returns false if the stream ended too early.
  bool FinishStream(size_t* pos, size_t* bit_pos) {
    *bit_pos = (8 - (bits_left_ & 7)) & 7;
    // Give back some bytes that we did not use.
    int unused_bytes_left = DivCeil(bits_left_, 8);
    while (unused_bytes_left-- > 0) {
      --pos_;
      // If we give back a 0 byte, we need to check if it was a 0xff/0x00 escape
      // sequence, and if yes, we need to give back one more byte.
      if (((pos_ == len_) || (pos_ < next_marker_pos_ && data_[pos_] == 0)) &&
          (data_[pos_ - 1] == 0xff)) {
        --pos_;
      }
    }
    if (pos_ > next_marker_pos_) {
      *pos = next_marker_pos_;
      // Data ran out before the scan was complete.
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
  size_t start_pos_;
};

// Returns the table width of the next 2nd level table, count is the histogram
// of bit lengths for the remaining symbols, len is the code length of the next
// processed symbol.
static inline int NextTableBitSize(const int* count, int len) {
  int left = 1 << (len - kJpegHuffmanRootTableBits);
  while (len < static_cast<int>(kJpegHuffmanMaxBitLength)) {
    left -= count[len];
    if (left <= 0) break;
    ++len;
    left <<= 1;
  }
  return len - kJpegHuffmanRootTableBits;
}

void BuildJpegHuffmanTable(const uint32_t* count, const uint32_t* symbols,
                           HuffmanTableEntry* lut) {
  HuffmanTableEntry code;    // current table entry
  HuffmanTableEntry* table;  // next available space in table
  int len;                   // current code length
  int idx;                   // symbol index
  int key;                   // prefix code
  int reps;                  // number of replicate key values in current table
  int low;                   // low bits for current root entry
  int table_bits;            // key length of current table
  int table_size;            // size of current table

  // Make a local copy of the input bit length histogram.
  int tmp_count[kJpegHuffmanMaxBitLength + 1] = {0};
  int total_count = 0;
  for (len = 1; len <= static_cast<int>(kJpegHuffmanMaxBitLength); ++len) {
    tmp_count[len] = count[len];
    total_count += tmp_count[len];
  }

  table = lut;
  table_bits = kJpegHuffmanRootTableBits;
  table_size = 1 << table_bits;

  // Special case code with only one value.
  if (total_count == 1) {
    code.bits = 0;
    code.value = symbols[0];
    for (key = 0; key < table_size; ++key) {
      table[key] = code;
    }
    return;
  }

  // Fill in root table.
  key = 0;
  idx = 0;
  for (len = 1; len <= kJpegHuffmanRootTableBits; ++len) {
    for (; tmp_count[len] > 0; --tmp_count[len]) {
      code.bits = len;
      code.value = symbols[idx++];
      reps = 1 << (kJpegHuffmanRootTableBits - len);
      while (reps--) {
        table[key++] = code;
      }
    }
  }

  // Fill in 2nd level tables and add pointers to root table.
  table += table_size;
  table_size = 0;
  low = 0;
  for (len = kJpegHuffmanRootTableBits + 1;
       len <= static_cast<int>(kJpegHuffmanMaxBitLength); ++len) {
    for (; tmp_count[len] > 0; --tmp_count[len]) {
      // Start a new sub-table if the previous one is full.
      if (low >= table_size) {
        table += table_size;
        table_bits = NextTableBitSize(tmp_count, len);
        table_size = 1 << table_bits;
        low = 0;
        lut[key].bits = table_bits + kJpegHuffmanRootTableBits;
        lut[key].value = (table - lut) - key;
        ++key;
      }
      code.bits = len - kJpegHuffmanRootTableBits;
      code.value = symbols[idx++];
      reps = 1 << (table_bits - code.bits);
      while (reps--) {
        table[low++] = code;
      }
    }
  }
}

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
                    int* eobrun, BitReaderState* br, coeff_t* last_dc_coeff,
                    coeff_t* coeffs) {
  // Nowadays multiplication is even faster than variable shift.
  int Am = 1 << Al;
  bool eobrun_allowed = Ss > 0;
  if (Ss == 0) {
    int s = ReadSymbol(dc_huff, br);
    if (s >= kJpegDCAlphabetSize) {
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
  for (int k = Ss; k <= Se; k++) {
    int sr = ReadSymbol(ac_huff, br);
    if (sr >= kJpegHuffmanAlphabetSize) {
      return false;
    }
    int r = sr >> 4;
    int s = sr & 15;
    if (s > 0) {
      k += r;
      if (k > Se) {
        return false;
      }
      if (s + Al >= kJpegDCAlphabetSize) {
        return false;
      }
      int bits = br->ReadBits(s);
      int coeff = HuffExtend(bits, s);
      coeffs[kJPEGNaturalOrder[k]] = coeff * Am;
    } else if (r == 15) {
      k += 15;
    } else {
      *eobrun = 1 << r;
      if (r > 0) {
        if (!eobrun_allowed) {
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
                    int* eobrun, BitReaderState* br, coeff_t* coeffs) {
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
        return false;
      }
      r = s >> 4;
      s &= 15;
      if (s) {
        if (s != 1) {
          return false;
        }
        s = br->ReadBits(1) ? p1 : m1;
        in_zero_run = false;
      } else {
        if (r != 15) {
          *eobrun = 1 << r;
          if (r > 0) {
            if (!eobrun_allowed) {
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
          return false;
        }
        coeffs[kJPEGNaturalOrder[k]] = s;
      }
    }
  }
  if (in_zero_run) {
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

// See the following article for the details:
// J. R. Price and M. Rabbani, "Dequantization bias for JPEG decompression"
// Proceedings International Conference on Information Technology: Coding and
// Computing (Cat. No.PR00540), 2000, pp. 30-35, doi: 10.1109/ITCC.2000.844179.
void ComputeOptimalLaplacianBiases(const int num_blocks, const int* nonzeros,
                                   const int* sumabs, float* biases) {
  for (size_t k = 1; k < kDCTBlockSize; ++k) {
    // Notation adapted from the article
    size_t N = num_blocks;
    size_t N1 = nonzeros[k];
    size_t N0 = num_blocks - N1;
    size_t S = sumabs[k];
    // Compute gamma from N0, N1, N, S (eq. 11), with A and B being just
    // temporary grouping of terms.
    float A = 4.0 * S + 2.0 * N;
    float B = 4.0 * S - 2.0 * N1;
    float gamma = (-1.0 * N0 + std::sqrt(N0 * N0 * 1.0 + A * B)) / A;
    float gamma2 = gamma * gamma;
    // The bias is computed from gamma with (eq. 5), where the quantization
    // multiplier Q can be factored out and thus the bias can be applied
    // directly on the quantized coefficient.
    biases[k] =
        0.5 * (((1.0 + gamma2) / (1.0 - gamma2)) + 1.0 / std::log(gamma));
  }
}

}  // namespace

JpegDecoder::Status JpegDecoder::SetInput(const uint8_t* data, size_t size) {
  if (next_in_ != nullptr) {
    return JPEG_ERROR("No input is needed.");
  }
  next_in_ = data;
  avail_in_ = size;
  return Status::kSuccess;
}

void JpegDecoder::AdvanceInput(size_t size) {
  JXL_DASSERT(avail_in_ >= size);
  next_in_ += size;
  avail_in_ -= size;
}

void JpegDecoder::AdvanceCodestream(size_t size) {
  if (codestream_copy_.empty()) {
    if (size <= avail_in_) {
      AdvanceInput(size);
    } else {
      codestream_pos_ = size - avail_in_;
      AdvanceInput(avail_in_);
    }
  } else {
    codestream_pos_ += size;
    if (codestream_pos_ + codestream_unconsumed_ >= codestream_copy_.size()) {
      size_t advance = std::min(
          codestream_unconsumed_,
          codestream_unconsumed_ + codestream_pos_ - codestream_copy_.size());
      AdvanceInput(advance);
      codestream_pos_ -= std::min(codestream_pos_, codestream_copy_.size());
      codestream_unconsumed_ = 0;
      codestream_copy_.clear();
    }
  }
}

JpegDecoder::Status JpegDecoder::RequestMoreInput() {
  if (codestream_copy_.empty()) {
    codestream_copy_.insert(codestream_copy_.end(), next_in_,
                            next_in_ + avail_in_);
    AdvanceInput(avail_in_);
  } else {
    AdvanceInput(codestream_unconsumed_);
    codestream_unconsumed_ = 0;
  }
  JXL_DASSERT(avail_in_ == 0);
  next_in_ = nullptr;
  return Status::kNeedMoreInput;
}

JpegDecoder::Status JpegDecoder::GetCodestreamInput(const uint8_t** data,
                                                    size_t* len) {
  if (codestream_copy_.empty() && codestream_pos_ > 0) {
    size_t skip = std::min<size_t>(codestream_pos_, avail_in_);
    AdvanceInput(skip);
    codestream_pos_ -= skip;
    if (codestream_pos_ > 0) {
      return RequestMoreInput();
    }
  }
  JXL_DASSERT(codestream_pos_ <= codestream_copy_.size());
  JXL_DASSERT(codestream_unconsumed_ <= codestream_copy_.size());
  if (codestream_copy_.empty()) {
    if (avail_in_ == 0) {
      return RequestMoreInput();
    }
    *data = next_in_;
    *len = avail_in_;
    return Status::kSuccess;
  } else {
    codestream_copy_.insert(codestream_copy_.end(),
                            next_in_ + codestream_unconsumed_,
                            next_in_ + avail_in_);
    codestream_unconsumed_ = avail_in_;
    *data = codestream_copy_.data() + codestream_pos_;
    *len = codestream_copy_.size() - codestream_pos_;
    return Status::kSuccess;
  }
}

JpegDecoder::Status JpegDecoder::SetOutput(PackedImage* image) {
  if (!found_sof_) {
    return JPEG_ERROR("SOF header was not found.");
  }
  if (image->xsize != xsize_ || image->ysize != ysize_) {
    return JPEG_ERROR("Invalid image dimensions.");
  }
  if (image->format.num_channels != components_.size()) {
    return JPEG_ERROR("Invalid number of channels.");
  }
  output_ = image;
  return Status::kSuccess;
}

JpegDecoder::Status JpegDecoder::ReadHeaders() {
  const uint8_t* data;
  size_t len;
  size_t pos = 0;
  JPEG_RETURN_IF_ERROR(GetCodestreamInput(&data, &len));

  while (!found_sof_) {
    if (state_ == State::kStart) {
      // Look for the SOI marker.
      if (len < 2) return RequestMoreInput();
      if (data[0] != 0xff || data[1] != 0xd8) {
        return JPEG_ERROR("Did not find SOI marker.");
      }
      pos += 2;
      AdvanceCodestream(2);
      found_soi_ = true;
      state_ = State::kProcessMarkers;
    } else if (state_ == State::kProcessMarkers) {
      JPEG_RETURN_IF_ERROR(ProcessMarker(data, len, &pos));
    } else {
      return JPEG_ERROR("ReadHeaders: Unexpected state");
    }
  }

  return Status::kSuccess;
}

JpegDecoder::Status JpegDecoder::ReadScanLines(size_t* num_output_rows,
                                               size_t max_output_rows) {
  if (max_output_rows == 0 || state_ == State::kEnd) {
    return Status::kSuccess;
  }
  *num_output_rows = 0;

  const uint8_t* data;
  size_t len = 0;
  size_t pos = 0;
  if (!found_eoi_) {
    JPEG_RETURN_IF_ERROR(GetCodestreamInput(&data, &len));
  }

  for (;;) {
    if (state_ == State::kProcessMarkers) {
      JPEG_RETURN_IF_ERROR(ProcessMarker(data, len, &pos));
    } else if (state_ == State::kScan) {
      JPEG_RETURN_IF_ERROR(ProcessScan(data, len, &pos));
    } else if (state_ == State::kRender) {
      ProcessOutput(num_output_rows, max_output_rows);
      if (*num_output_rows == max_output_rows) {
        return Status::kSuccess;
      }
    } else if (state_ == State::kEnd) {
      return Status::kSuccess;
    } else {
      return JPEG_ERROR("ReadScanLines: Unexpected state");
    }
  }
}

JpegDecoder::Status JpegDecoder::ProcessMarker(const uint8_t* data, size_t len,
                                               size_t* pos) {
  // kIsValidMarker[i] == 1 means (0xc0 + i) is a valid marker.
  static const uint8_t kIsValidMarker[] = {
      1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,
      1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
  };
  // Skip bytes between markers.
  size_t num_skipped = 0;
  while (*pos + 1 < len && (data[*pos] != 0xff || data[*pos + 1] < 0xc0 ||
                            !kIsValidMarker[data[*pos + 1] - 0xc0])) {
    ++(*pos);
    ++num_skipped;
  }
  if (*pos + 2 > len) {
    return RequestMoreInput();
  }
  if (num_skipped > 0) {
    AdvanceCodestream(num_skipped);
  }
  uint8_t marker = data[*pos + 1];
  if (marker == 0xd9) {
    found_eoi_ = true;
    if (is_progressive_) {
      PrepareForOutput();
      state_ = State::kRender;
    } else {
      state_ = State::kEnd;
    }
    AdvanceCodestream(2);
    return Status::kSuccess;
  }
  if (*pos + 4 > len) {
    return RequestMoreInput();
  }
  const uint8_t* marker_data = &data[*pos];
  size_t marker_len = (data[*pos + 2] << 8) + data[*pos + 3] + 2;
  if (marker_len < 4) {
    return JPEG_ERROR("Invalid marker length");
  }
  if (*pos + marker_len > len) {
    return RequestMoreInput();
  }
  if (marker == 0xc0 || marker == 0xc1 || marker == 0xc2) {
    JPEG_RETURN_IF_ERROR(ProcessSOF(marker_data, marker_len));
  } else if (marker == 0xc4) {
    JPEG_RETURN_IF_ERROR(ProcessDHT(marker_data, marker_len));
  } else if (marker == 0xda) {
    JPEG_RETURN_IF_ERROR(ProcessSOS(marker_data, marker_len));
  } else if (marker == 0xdb) {
    JPEG_RETURN_IF_ERROR(ProcessDQT(marker_data, marker_len));
  } else if (marker == 0xdd) {
    JPEG_RETURN_IF_ERROR(ProcessDRI(marker_data, marker_len));
  } else if (marker >= 0xe0 && marker <= 0xef) {
    JPEG_RETURN_IF_ERROR(ProcessAPP(marker_data, marker_len));
  } else if (marker == 0xfe) {
    JPEG_RETURN_IF_ERROR(ProcessCOM(marker_data, marker_len));
  } else {
    return JPEG_ERROR("Unexpected marker 0x%x", marker);
  }
  *pos += marker_len;
  AdvanceCodestream(marker_len);
  return Status::kSuccess;
}

JpegDecoder::Status JpegDecoder::ProcessSOF(const uint8_t* data, size_t len) {
  if (!found_soi_) {
    return JPEG_ERROR("Unexpected SOF marker.");
  }
  if (found_sof_) {
    return JPEG_ERROR("Duplicate SOF marker.");
  }
  found_sof_ = true;
  is_progressive_ = (data[1] == 0xc2);
  size_t pos = 4;
  JPEG_VERIFY_LEN(6);
  int precision = ReadUint8(data, &pos);
  ysize_ = ReadUint16(data, &pos);
  xsize_ = ReadUint16(data, &pos);
  int num_components = ReadUint8(data, &pos);
  JPEG_VERIFY_INPUT(precision, 8, 8);
  JPEG_VERIFY_INPUT(ysize_, 1, kMaxDimPixels);
  JPEG_VERIFY_INPUT(xsize_, 1, kMaxDimPixels);
  JPEG_VERIFY_INPUT(num_components, 1, kMaxComponents);
  JPEG_VERIFY_LEN(3 * num_components);
  components_.resize(num_components);

  // Read sampling factors and quant table index for each component.
  std::vector<bool> ids_seen(256, false);
  max_h_samp_ = 1;
  max_v_samp_ = 1;
  for (size_t i = 0; i < components_.size(); ++i) {
    JPEGComponent* c = &components_[i];
    const int id = ReadUint8(data, &pos);
    if (ids_seen[id]) {  // (cf. section B.2.2, syntax of Ci)
      return JPEG_ERROR("Duplicate ID %d in SOF.", id);
    }
    ids_seen[id] = true;
    c->id = id;
    int factor = ReadUint8(data, &pos);
    int h_samp_factor = factor >> 4;
    int v_samp_factor = factor & 0xf;
    JPEG_VERIFY_INPUT(h_samp_factor, 1, kMaxSampling);
    JPEG_VERIFY_INPUT(v_samp_factor, 1, kMaxSampling);
    c->h_samp_factor = h_samp_factor;
    c->v_samp_factor = v_samp_factor;
    max_h_samp_ = std::max(max_h_samp_, h_samp_factor);
    max_v_samp_ = std::max(max_v_samp_, v_samp_factor);
    uint8_t quant_tbl_idx = ReadUint8(data, &pos);
    bool found_quant_tbl = false;
    for (size_t j = 0; j < quant_.size(); ++j) {
      if (quant_[j].index == quant_tbl_idx) {
        c->quant_idx = j;
        found_quant_tbl = true;
        break;
      }
    }
    if (!found_quant_tbl) {
      return JPEG_ERROR("Quantization table with index %u not found",
                        quant_tbl_idx);
    }
  }
  JPEG_VERIFY_MARKER_END();

  if (num_components == 1) {
    is_ycbcr_ = true;
  }
  if (!found_app0_ && num_components == 3 && components_[0].id == 'R' &&
      components_[1].id == 'G' && components_[2].id == 'B') {
    is_ycbcr_ = false;
  }

  // We have checked above that none of the sampling factors are 0, so the max
  // sampling factors can not be 0.
  iMCU_height_ = max_v_samp_ * kBlockDim;
  iMCU_width_ = max_h_samp_ * kBlockDim;
  iMCU_rows_ = DivCeil(ysize_, iMCU_height_);
  iMCU_cols_ = DivCeil(xsize_, iMCU_width_);
  // Compute the block dimensions for each component.
  for (size_t i = 0; i < components_.size(); ++i) {
    JPEGComponent* c = &components_[i];
    if (max_h_samp_ % c->h_samp_factor != 0 ||
        max_v_samp_ % c->v_samp_factor != 0) {
      return JPEG_ERROR("Non-integral subsampling ratios.");
    }
    c->width_in_blocks = iMCU_cols_ * c->h_samp_factor;
    c->height_in_blocks = iMCU_rows_ * c->v_samp_factor;
    const uint64_t num_blocks =
        static_cast<uint64_t>(c->width_in_blocks) * c->height_in_blocks;
    c->coeffs = hwy::AllocateAligned<coeff_t>(num_blocks * kDCTBlockSize);
    memset(c->coeffs.get(), 0, num_blocks * kDCTBlockSize * sizeof(coeff_t));
  }
  memset(scan_progression_, 0, sizeof(scan_progression_));
  return Status::kSuccess;
}

JpegDecoder::Status JpegDecoder::ProcessSOS(const uint8_t* data, size_t len) {
  if (!found_sof_) {
    return JPEG_ERROR("Unexpected SOS marker.");
  }
  size_t pos = 4;
  JPEG_VERIFY_LEN(1);
  size_t comps_in_scan = ReadUint8(data, &pos);
  JPEG_VERIFY_INPUT(comps_in_scan, 1, components_.size());

  scan_info_.num_components = comps_in_scan;
  JPEG_VERIFY_LEN(2 * comps_in_scan);
  bool is_interleaved = (scan_info_.num_components > 1);
  std::vector<bool> ids_seen(256, false);
  for (size_t i = 0; i < scan_info_.num_components; ++i) {
    JPEGComponentScanInfo* si = &scan_info_.components[i];
    uint32_t id = ReadUint8(data, &pos);
    if (ids_seen[id]) {  // (cf. section B.2.3, regarding CSj)
      return JPEG_ERROR("Duplicate ID %d in SOS.", id);
    }
    ids_seen[id] = true;
    JPEGComponent* comp = nullptr;
    for (size_t j = 0; j < components_.size(); ++j) {
      if (components_[j].id == id) {
        si->comp_idx = j;
        comp = &components_[j];
      }
    }
    if (!comp) {
      return JPEG_ERROR("SOS marker: Could not find component with id %d", id);
    }
    int c = ReadUint8(data, &pos);
    si->dc_tbl_idx = c >> 4;
    si->ac_tbl_idx = c & 0xf;
    JPEG_VERIFY_INPUT(static_cast<int>(si->dc_tbl_idx), 0, 3);
    JPEG_VERIFY_INPUT(static_cast<int>(si->ac_tbl_idx), 0, 3);
    si->mcu_xsize_blocks = is_interleaved ? comp->h_samp_factor : 1;
    si->mcu_ysize_blocks = is_interleaved ? comp->v_samp_factor : 1;
  }
  JPEG_VERIFY_LEN(3);
  scan_info_.Ss = ReadUint8(data, &pos);
  scan_info_.Se = ReadUint8(data, &pos);
  JPEG_VERIFY_INPUT(static_cast<int>(scan_info_.Ss), 0, 63);
  JPEG_VERIFY_INPUT(scan_info_.Se, scan_info_.Ss, 63);
  int c = ReadUint8(data, &pos);
  scan_info_.Ah = c >> 4;
  scan_info_.Al = c & 0xf;
  JPEG_VERIFY_MARKER_END();

  if (scan_info_.Ah != 0 && scan_info_.Al != scan_info_.Ah - 1) {
    // section G.1.1.1.2 : Successive approximation control only improves
    // by one bit at a time.
    JPEG_ERROR("Invalid progressive parameters: Al=%d Ah=%d", scan_info_.Al,
               scan_info_.Ah);
  }
  if (!is_progressive_) {
    scan_info_.Ss = 0;
    scan_info_.Se = 63;
    scan_info_.Ah = 0;
    scan_info_.Al = 0;
  }
  const uint16_t scan_bitmask =
      scan_info_.Ah == 0 ? (0xffff << scan_info_.Al) : (1u << scan_info_.Al);
  const uint16_t refinement_bitmask = (1 << scan_info_.Al) - 1;
  for (size_t i = 0; i < scan_info_.num_components; ++i) {
    int comp_idx = scan_info_.components[i].comp_idx;
    for (uint32_t k = scan_info_.Ss; k <= scan_info_.Se; ++k) {
      if (scan_progression_[comp_idx][k] & scan_bitmask) {
        return JPEG_ERROR(
            "Overlapping scans: component=%d k=%d prev_mask: %u cur_mask %u",
            comp_idx, k, scan_progression_[i][k], scan_bitmask);
      }
      if (scan_progression_[comp_idx][k] & refinement_bitmask) {
        return JPEG_ERROR(
            "Invalid scan order, a more refined scan was already done: "
            "component=%d k=%d prev_mask=%u cur_mask=%u",
            comp_idx, k, scan_progression_[i][k], scan_bitmask);
      }
      scan_progression_[comp_idx][k] |= scan_bitmask;
    }
  }
  if (scan_info_.Al > 10) {
    return JPEG_ERROR("Scan parameter Al=%d is not supported.", scan_info_.Al);
  }
  // Check that all the Huffman tables needed for this scan are defined.
  for (size_t i = 0; i < comps_in_scan; ++i) {
    if (scan_info_.Ss == 0 &&
        !huff_slot_defined_[scan_info_.components[i].dc_tbl_idx]) {
      return JPEG_ERROR(
          "SOS marker: Could not find DC Huffman table with index %d",
          scan_info_.components[i].dc_tbl_idx);
    }
    if (scan_info_.Se > 0 &&
        !huff_slot_defined_[scan_info_.components[i].ac_tbl_idx + 16]) {
      return JPEG_ERROR(
          "SOS marker: Could not find AC Huffman table with index %d",
          scan_info_.components[i].ac_tbl_idx);
    }
  }
  scan_info_.MCU_rows = iMCU_rows_;
  scan_info_.MCU_cols = iMCU_cols_;
  if (!is_interleaved) {
    const JPEGComponent& c = components_[scan_info_.components[0].comp_idx];
    scan_info_.MCU_cols = DivCeil(xsize_ * c.h_samp_factor, iMCU_width_);
    scan_info_.MCU_rows = DivCeil(ysize_ * c.v_samp_factor, iMCU_height_);
  }
  memset(last_dc_coeff_, 0, sizeof(last_dc_coeff_));
  restarts_to_go_ = restart_interval_;
  next_restart_marker_ = 0;
  eobrun_ = -1;
  scan_mcu_row_ = 0;
  scan_mcu_col_ = 0;
  codestream_bits_ahead_ = 0;
  size_t mcu_size = 0;
  for (size_t i = 0; i < scan_info_.num_components; ++i) {
    JPEGComponentScanInfo* si = &scan_info_.components[i];
    mcu_size += si->mcu_ysize_blocks * si->mcu_xsize_blocks;
  }
  mcu_.coeffs.resize(mcu_size * kDCTBlockSize);
  if (!is_progressive_) {
    PrepareForOutput();
  }
  state_ = State::kScan;
  return Status::kSuccess;
}

// Reads the Define Huffman Table (DHT) marker segment and builds the Huffman
// decoding table in either dc_huff_lut_ or ac_huff_lut_, depending on the type
// and solt_id of Huffman code being read.
JpegDecoder::Status JpegDecoder::ProcessDHT(const uint8_t* data, size_t len) {
  constexpr int kLutSize = kMaxHuffmanTables * kJpegHuffmanLutSize;
  dc_huff_lut_.resize(kLutSize);
  ac_huff_lut_.resize(kLutSize);
  size_t pos = 4;
  if (pos == len) {
    return JPEG_ERROR("DHT marker: no Huffman table found");
  }
  while (pos < len) {
    JPEG_VERIFY_LEN(1 + kJpegHuffmanMaxBitLength);
    // The index of the Huffman code in the current set of Huffman codes. For AC
    // component Huffman codes, 0x10 is added to the index.
    int slot_id = ReadUint8(data, &pos);
    huff_slot_defined_[slot_id] = 1;
    int huffman_index = slot_id;
    int is_ac_table = (slot_id & 0x10) != 0;
    HuffmanTableEntry* huff_lut;
    if (is_ac_table) {
      huffman_index -= 0x10;
      JPEG_VERIFY_INPUT(huffman_index, 0, 3);
      huff_lut = &ac_huff_lut_[huffman_index * kJpegHuffmanLutSize];
    } else {
      JPEG_VERIFY_INPUT(huffman_index, 0, 3);
      huff_lut = &dc_huff_lut_[huffman_index * kJpegHuffmanLutSize];
    }
    // Bit length histogram.
    std::array<uint32_t, kJpegHuffmanMaxBitLength + 1> counts = {};
    counts[0] = 0;
    int total_count = 0;
    int space = 1 << kJpegHuffmanMaxBitLength;
    int max_depth = 1;
    for (size_t i = 1; i <= kJpegHuffmanMaxBitLength; ++i) {
      int count = ReadUint8(data, &pos);
      if (count != 0) {
        max_depth = i;
      }
      counts[i] = count;
      total_count += count;
      space -= count * (1 << (kJpegHuffmanMaxBitLength - i));
    }
    if (is_ac_table) {
      JPEG_VERIFY_INPUT(total_count, 0, kJpegHuffmanAlphabetSize);
    } else {
      JPEG_VERIFY_INPUT(total_count, 0, kJpegDCAlphabetSize);
    }
    JPEG_VERIFY_LEN(total_count);
    // Symbol values sorted by increasing bit lengths.
    std::array<uint32_t, kJpegHuffmanAlphabetSize + 1> values = {};
    std::vector<bool> values_seen(256, false);
    for (int i = 0; i < total_count; ++i) {
      int value = ReadUint8(data, &pos);
      if (!is_ac_table) {
        JPEG_VERIFY_INPUT(value, 0, kJpegDCAlphabetSize - 1);
      }
      if (values_seen[value]) {
        return JPEG_ERROR("Duplicate Huffman code value %d", value);
      }
      values_seen[value] = true;
      values[i] = value;
    }
    // Add an invalid symbol that will have the all 1 code.
    ++counts[max_depth];
    values[total_count] = kJpegHuffmanAlphabetSize;
    space -= (1 << (kJpegHuffmanMaxBitLength - max_depth));
    if (space < 0) {
      return JPEG_ERROR("Invalid Huffman code lengths.");
    } else if (space > 0 && huff_lut[0].value != 0xffff) {
      // Re-initialize the values to an invalid symbol so that we can recognize
      // it when reading the bit stream using a Huffman code with space > 0.
      for (int i = 0; i < kJpegHuffmanLutSize; ++i) {
        huff_lut[i].bits = 0;
        huff_lut[i].value = 0xffff;
      }
    }
    BuildJpegHuffmanTable(&counts[0], &values[0], huff_lut);
  }
  JPEG_VERIFY_MARKER_END();
  return Status::kSuccess;
}

JpegDecoder::Status JpegDecoder::ProcessDQT(const uint8_t* data, size_t len) {
  size_t pos = 4;
  if (pos == len) {
    return JPEG_ERROR("DQT marker: no quantization table found");
  }
  while (pos < len && quant_.size() < kMaxQuantTables) {
    JPEG_VERIFY_LEN(1);
    int quant_table_index = ReadUint8(data, &pos);
    int precision = quant_table_index >> 4;
    JPEG_VERIFY_INPUT(precision, 0, 1);
    quant_table_index &= 0xf;
    JPEG_VERIFY_INPUT(quant_table_index, 0, 3);
    JPEG_VERIFY_LEN((precision + 1) * kDCTBlockSize);
    JPEGQuantTable table;
    table.index = quant_table_index;
    for (size_t i = 0; i < kDCTBlockSize; ++i) {
      int quant_val =
          precision ? ReadUint16(data, &pos) : ReadUint8(data, &pos);
      JPEG_VERIFY_INPUT(quant_val, 1, 65535);
      table.values[kJPEGNaturalOrder[i]] = quant_val;
    }
    quant_.push_back(table);
  }
  JPEG_VERIFY_MARKER_END();
  return Status::kSuccess;
}

JpegDecoder::Status JpegDecoder::ProcessDRI(const uint8_t* data, size_t len) {
  if (found_dri_) {
    return JPEG_ERROR("Duplicate DRI marker.");
  }
  found_dri_ = true;
  size_t pos = 4;
  JPEG_VERIFY_LEN(2);
  restart_interval_ = ReadUint16(data, &pos);
  JPEG_VERIFY_MARKER_END();
  return Status::kSuccess;
}

JpegDecoder::Status JpegDecoder::ProcessAPP(const uint8_t* data, size_t len) {
  const uint8_t marker = data[1];
  const uint8_t* payload = data + 4;
  size_t payload_size = len - 4;
  if (marker == 0xE0) {
    found_app0_ = true;
    is_ycbcr_ = true;
  } else if (!found_app0_ && marker == 0xEE && payload_size == 12 &&
             memcmp(payload, "Adobe", 5) == 0 && payload[11] == 0) {
    is_ycbcr_ = false;
  }
  if (marker == 0xE2) {
    if (payload_size >= sizeof(kIccProfileTag) &&
        memcmp(payload, kIccProfileTag, sizeof(kIccProfileTag)) == 0) {
      payload += sizeof(kIccProfileTag);
      payload_size -= sizeof(kIccProfileTag);
      if (payload_size < 2) {
        return JPEG_ERROR("ICC chunk is too small.");
      }
      uint8_t index = payload[0];
      uint8_t total = payload[1];
      ++icc_index_;
      if (icc_index_ != index) {
        return JPEG_ERROR("Invalid ICC chunk order.");
      }
      JPEG_RETURN_IF_ERROR(total != 0);
      if (icc_total_ == 0) {
        icc_total_ = total;
      } else {
        JPEG_RETURN_IF_ERROR(icc_total_ == total);
      }
      if (icc_index_ > icc_total_) {
        return JPEG_ERROR("Invalid ICC chunk index.");
      }
      icc_profile_.insert(icc_profile_.end(), payload + 2,
                          payload + payload_size);
    }
  }
  return Status::kSuccess;
}

JpegDecoder::Status JpegDecoder::ProcessCOM(const uint8_t* data, size_t len) {
  return Status::kSuccess;
}

void JpegDecoder::SaveMCUCodingState() {
  memcpy(mcu_.last_dc_coeff, last_dc_coeff_, sizeof(last_dc_coeff_));
  mcu_.eobrun = eobrun_;
  size_t offset = 0;
  for (size_t i = 0; i < scan_info_.num_components; ++i) {
    JPEGComponentScanInfo* si = &scan_info_.components[i];
    JPEGComponent* c = &components_[si->comp_idx];
    int block_x = scan_mcu_col_ * si->mcu_xsize_blocks;
    for (uint32_t iy = 0; iy < si->mcu_ysize_blocks; ++iy) {
      int block_y = scan_mcu_row_ * si->mcu_ysize_blocks + iy;
      size_t ncoeffs = si->mcu_xsize_blocks * kDCTBlockSize;
      int block_idx = (block_y * c->width_in_blocks + block_x) * kDCTBlockSize;
      coeff_t* coeffs = &c->coeffs[block_idx];
      memcpy(&mcu_.coeffs[offset], coeffs, ncoeffs * sizeof(coeffs[0]));
      offset += ncoeffs;
    }
  }
}

void JpegDecoder::RestoreMCUCodingState() {
  memcpy(last_dc_coeff_, mcu_.last_dc_coeff, sizeof(last_dc_coeff_));
  eobrun_ = mcu_.eobrun;
  size_t offset = 0;
  for (size_t i = 0; i < scan_info_.num_components; ++i) {
    JPEGComponentScanInfo* si = &scan_info_.components[i];
    JPEGComponent* c = &components_[si->comp_idx];
    int block_x = scan_mcu_col_ * si->mcu_xsize_blocks;
    for (uint32_t iy = 0; iy < si->mcu_ysize_blocks; ++iy) {
      int block_y = scan_mcu_row_ * si->mcu_ysize_blocks + iy;
      size_t ncoeffs = si->mcu_xsize_blocks * kDCTBlockSize;
      int block_idx = (block_y * c->width_in_blocks + block_x) * kDCTBlockSize;
      coeff_t* coeffs = &c->coeffs[block_idx];
      memcpy(coeffs, &mcu_.coeffs[offset], ncoeffs * sizeof(coeffs[0]));
      offset += ncoeffs;
    }
  }
}

JpegDecoder::Status JpegDecoder::ProcessScan(const uint8_t* data, size_t len,
                                             size_t* pos) {
  for (; scan_mcu_col_ < scan_info_.MCU_cols; ++scan_mcu_col_) {
    // Handle the restart intervals.
    if (restart_interval_ > 0 && restarts_to_go_ == 0) {
      if (eobrun_ > 0) {
        return JPEG_ERROR("End-of-block run too long.");
      }
      if (codestream_bits_ahead_ > 0) {
        ++(*pos);
        AdvanceCodestream(1);
        codestream_bits_ahead_ = 0;
      }
      if (*pos + 2 > len) {
        return RequestMoreInput();
      }
      int expected_marker = 0xd0 + next_restart_marker_;
      int marker = data[*pos + 1];
      if (marker != expected_marker) {
        return JPEG_ERROR("Did not find expected restart marker %d actual %d",
                          expected_marker, marker);
      }
      next_restart_marker_ += 1;
      next_restart_marker_ &= 0x7;
      restarts_to_go_ = restart_interval_;
      memset(last_dc_coeff_, 0, sizeof(last_dc_coeff_));
      eobrun_ = -1;  // fresh start
      *pos += 2;
      AdvanceCodestream(2);
    }

    size_t start_pos = *pos;
    BitReaderState br(data, len, start_pos);
    if (codestream_bits_ahead_ > 0) {
      br.ReadBits(codestream_bits_ahead_);
    }
    if (start_pos + kMaxMCUByteSize > len) {
      SaveMCUCodingState();
    }

    // Decode one MCU.
    bool scan_ok = true;
    for (size_t i = 0; i < scan_info_.num_components; ++i) {
      JPEGComponentScanInfo* si = &scan_info_.components[i];
      JPEGComponent* c = &components_[si->comp_idx];
      const HuffmanTableEntry* dc_lut =
          &dc_huff_lut_[si->dc_tbl_idx * kJpegHuffmanLutSize];
      const HuffmanTableEntry* ac_lut =
          &ac_huff_lut_[si->ac_tbl_idx * kJpegHuffmanLutSize];
      for (uint32_t iy = 0; iy < si->mcu_ysize_blocks; ++iy) {
        int block_y = scan_mcu_row_ * si->mcu_ysize_blocks + iy;
        for (uint32_t ix = 0; ix < si->mcu_xsize_blocks; ++ix) {
          int block_x = scan_mcu_col_ * si->mcu_xsize_blocks + ix;
          int block_idx = block_y * c->width_in_blocks + block_x;
          coeff_t* coeffs = &c->coeffs[block_idx * kDCTBlockSize];
          if (scan_info_.Ah == 0) {
            if (!DecodeDCTBlock(dc_lut, ac_lut, scan_info_.Ss, scan_info_.Se,
                                scan_info_.Al, &eobrun_, &br,
                                &last_dc_coeff_[si->comp_idx], coeffs)) {
              scan_ok = false;
            }
          } else {
            if (!RefineDCTBlock(ac_lut, scan_info_.Ss, scan_info_.Se,
                                scan_info_.Al, &eobrun_, &br, coeffs)) {
              scan_ok = false;
            }
          }
        }
      }
    }
    size_t bit_pos;
    bool stream_ok = br.FinishStream(pos, &bit_pos);
    if (*pos + 2 > len) {
      // If reading stopped within the last two bytes, we have to request more
      // input even if FinishStream() returned true, since the Huffman code
      // reader could have peaked ahead some bits past the current input chunk
      // and thus the last prefix code length could have been wrong. We can do
      // this because a valid JPEG bit stream has two extra bytes at the end.
      RestoreMCUCodingState();
      return RequestMoreInput();
    }
    if (!scan_ok) {
      return JPEG_ERROR("Failed to decode DCT block");
    }
    if (!stream_ok) {
      // We hit a marker during parsing.
      JXL_DASSERT(data[*pos] == 0xff);
      JXL_DASSERT(data[*pos + 1] != 0);
      return JPEG_ERROR("Unexpected end of scan.");
    }
    codestream_bits_ahead_ = bit_pos;
    AdvanceCodestream(*pos - start_pos);
    if (restarts_to_go_ > 0) {
      --restarts_to_go_;
    }
  }
  ++scan_mcu_row_;
  scan_mcu_col_ = 0;
  if (scan_mcu_row_ == scan_info_.MCU_rows) {
    // Current scan is done, skip any remaining bits in the last byte.
    if (codestream_bits_ahead_ > 0) {
      ++(*pos);
      AdvanceCodestream(1);
      codestream_bits_ahead_ = 0;
    }
    if (eobrun_ > 0) {
      return JPEG_ERROR("End-of-block run too long.");
    }
    if (is_progressive_) {
      state_ = State::kProcessMarkers;
    }
  }
  if (!is_progressive_) {
    state_ = State::kRender;
  }
  return Status::kSuccess;
}

void JpegDecoder::PrepareForOutput() {
  size_t stride = iMCU_cols_ * iMCU_width_ + kPaddingLeft + kPaddingRight;
  MCU_row_buf_ = Image3F(stride, iMCU_height_);
  const size_t nbcomp = components_.size();
  for (size_t c = 0; c < nbcomp; ++c) {
    const auto& comp = components_[c];
    if (comp.v_samp_factor < max_v_samp_) {
      component_order_.emplace_back(c);
      chroma_.emplace_back(ImageF(stride, 3 * kBlockDim));
    }
  }
  for (size_t c = 0; c < nbcomp; ++c) {
    const auto& comp = components_[c];
    if (comp.v_samp_factor == max_v_samp_) {
      component_order_.emplace_back(c);
    }
  }
  idct_scratch_ = hwy::AllocateAligned<float>(kDCTBlockSize * 2);
  upsample_scratch_ = hwy::AllocateAligned<float>(stride);
  size_t bytes_per_channel =
      PackedImage::BitsPerChannel(output_->format.data_type) / 8;
  size_t bytes_per_sample = nbcomp * bytes_per_channel;
  output_scratch_ =
      hwy::AllocateAligned<uint8_t>(bytes_per_sample * kTempOutputLen);
  nonzeros_ = hwy::AllocateAligned<int>(nbcomp * kDCTBlockSize);
  sumabs_ = hwy::AllocateAligned<int>(nbcomp * kDCTBlockSize);
  memset(nonzeros_.get(), 0, nbcomp * kDCTBlockSize * sizeof(nonzeros_[0]));
  memset(sumabs_.get(), 0, nbcomp * kDCTBlockSize * sizeof(sumabs_[0]));
  num_processed_blocks_.resize(nbcomp);
  biases_ = hwy::AllocateAligned<float>(nbcomp * kDCTBlockSize);
  memset(biases_.get(), 0, nbcomp * kDCTBlockSize * sizeof(biases_[0]));
  output_mcu_row_ = 0;
  output_ci_ = 0;
  output_row_ = 0;
  MCU_buf_ready_rows_ = 0;
  const float kDequantScale = 1.0f / (8 * 255);
  dequant_ = hwy::AllocateAligned<float>(nbcomp * kDCTBlockSize);
  for (size_t c = 0; c < nbcomp; c++) {
    const auto& comp = components_[c];
    const int32_t* quant = quant_[comp.quant_idx].values.data();
    for (size_t k = 0; k < kDCTBlockSize; ++k) {
      dequant_[c * kDCTBlockSize + k] = quant[k] * kDequantScale;
    }
  }
}

void JpegDecoder::ProcessOutput(size_t* num_output_rows,
                                size_t max_output_rows) {
  const size_t nbcomp = components_.size();
  size_t xsize_blocks = DivCeil(xsize_, kBlockDim);
  size_t mcu_y = output_mcu_row_;
  for (; output_ci_ < components_.size(); ++output_ci_) {
    size_t c = component_order_[output_ci_];
    size_t k0 = c * kDCTBlockSize;
    auto& comp = components_[c];
    bool hups = comp.h_samp_factor < max_h_samp_;
    bool vups = comp.v_samp_factor < max_v_samp_;
    size_t nblocks_y = comp.v_samp_factor;
    ImageF* output = vups ? &chroma_[output_ci_] : &MCU_row_buf_.Plane(c);
    size_t mcu_y0 = vups ? (mcu_y * kBlockDim) % output->ysize() : 0;
    if (output_ci_ == chroma_.size() && mcu_y > 0) {
      // For the previous MCU row we have everything we need at this point,
      // including the chroma components for the current MCU row that was used
      // in upsampling, so we can do the color conversion and the interleaved
      // output.
      if (MCU_buf_ready_rows_ == 0) {
        MCU_buf_ready_rows_ = iMCU_height_;
        MCU_buf_current_row_ = 0;
      }
      while (MCU_buf_current_row_ < MCU_buf_ready_rows_ &&
             *num_output_rows < max_output_rows && output_row_ < ysize_) {
        float* rows[3];
        for (size_t c = 0; c < components_.size(); ++c) {
          rows[c] =
              MCU_row_buf_.PlaneRow(c, MCU_buf_current_row_) + kPaddingLeft;
        }
        if (is_ycbcr_ && nbcomp == 3) {
          YCbCrToRGB(rows[0], rows[1], rows[2], xsize_blocks * kBlockDim);
        } else {
          for (size_t c = 0; c < components_.size(); ++c) {
            // Libjpeg encoder converts all unsigned input values to signed
            // ones, i.e. for 8 bit input from [0..255] to [-128..127]. For
            // YCbCr jpegs this is undone in the YCbCr -> RGB conversion above
            // by adding 128 to Y channel, but for grayscale and RGB jpegs we
            // need to undo it here channel by channel.
            DecenterRow(rows[c], xsize_blocks * kBlockDim);
          }
        }
        for (size_t x0 = 0; x0 < xsize_; x0 += kTempOutputLen) {
          size_t len = std::min(xsize_ - x0, kTempOutputLen);
          WriteToPackedImage(rows, x0, output_row_, len, output_scratch_.get(),
                             output_);
        }
        ++output_row_;
        ++(*num_output_rows);
        ++MCU_buf_current_row_;
      }
      if (output_row_ == ysize_) {
        state_ = is_progressive_ ? State::kEnd : State::kProcessMarkers;
        return;
      }
      if (*num_output_rows == max_output_rows) {
        return;
      }
      MCU_buf_ready_rows_ = 0;
    }
    if (mcu_y < iMCU_rows_) {
      if (!hups && !vups) {
        size_t num_coeffs = comp.width_in_blocks * kDCTBlockSize;
        size_t offset = mcu_y * comp.width_in_blocks * kDCTBlockSize;
        // Update statistics for this MCU row.
        GatherBlockStats(&comp.coeffs[offset], num_coeffs, &nonzeros_[k0],
                         &sumabs_[k0]);
        num_processed_blocks_[c] += comp.width_in_blocks;
        if (mcu_y % 4 == 3) {
          // Re-compute optimal biases every few MCU-rows.
          ComputeOptimalLaplacianBiases(num_processed_blocks_[c],
                                        &nonzeros_[k0], &sumabs_[k0],
                                        &biases_[k0]);
        }
      }
      for (size_t iy = 0; iy < nblocks_y; ++iy) {
        size_t by = mcu_y * nblocks_y + iy;
        size_t y0 = mcu_y0 + iy * kBlockDim;
        int16_t* JXL_RESTRICT row_in =
            &comp.coeffs[by * comp.width_in_blocks * kDCTBlockSize];
        float* JXL_RESTRICT row_out = output->Row(y0) + kPaddingLeft;
        for (size_t bx = 0; bx < comp.width_in_blocks; ++bx) {
          DecodeJpegBlock(&row_in[bx * kDCTBlockSize], &dequant_[k0],
                          &biases_[k0], idct_scratch_.get(),
                          &row_out[bx * kBlockDim], output->PixelsPerRow());
        }
        if (hups) {
          for (size_t y = 0; y < kBlockDim; ++y) {
            float* JXL_RESTRICT row = output->Row(y0 + y) + kPaddingLeft;
            Upsample2Horizontal(row, upsample_scratch_.get(),
                                xsize_blocks * kBlockDim);
            memcpy(row, upsample_scratch_.get(),
                   xsize_blocks * kBlockDim * sizeof(row[0]));
          }
        }
      }
    }
    if (vups) {
      auto y_idx = [&](size_t mcu_y, ssize_t y) {
        return (output->ysize() + mcu_y * kBlockDim + y) % output->ysize();
      };
      if (mcu_y == 0) {
        // Copy the first row of the current MCU row to the last row of the
        // previous one.
        memcpy(output->Row(y_idx(mcu_y, -1)), output->Row(y_idx(mcu_y, 0)),
               output->PixelsPerRow() * sizeof(output->Row(0)[0]));
      }
      if (mcu_y == iMCU_rows_) {
        // Copy the last row of the current MCU row to the  first row of the
        // next  one.
        memcpy(output->Row(y_idx(mcu_y + 1, 0)),
               output->Row(y_idx(mcu_y, kBlockDim - 1)),
               output->PixelsPerRow() * sizeof(output->Row(0)[0]));
      }
      if (mcu_y > 0) {
        for (size_t y = 0; y < kBlockDim; ++y) {
          size_t y_top = y_idx(mcu_y - 1, y - 1);
          size_t y_cur = y_idx(mcu_y - 1, y);
          size_t y_bot = y_idx(mcu_y - 1, y + 1);
          size_t y_out0 = 2 * y;
          size_t y_out1 = 2 * y + 1;
          Upsample2Vertical(output->Row(y_top) + kPaddingLeft,
                            output->Row(y_cur) + kPaddingLeft,
                            output->Row(y_bot) + kPaddingLeft,
                            MCU_row_buf_.PlaneRow(c, y_out0) + kPaddingLeft,
                            MCU_row_buf_.PlaneRow(c, y_out1) + kPaddingLeft,
                            xsize_blocks * kBlockDim);
        }
      }
    }
  }
  ++output_mcu_row_;
  output_ci_ = 0;
  if (!is_progressive_ && output_mcu_row_ < iMCU_rows_) {
    state_ = State::kScan;
  }
  JXL_DASSERT(output_mcu_row_ <= iMCU_rows_);
}

Status DecodeJpeg(const std::vector<uint8_t>& compressed,
                  JxlDataType output_data_type, ThreadPool* pool,
                  PackedPixelFile* ppf) {
  JpegDecoder dec;
  dec.SetInput(compressed.data(), compressed.size());

  if (dec.ReadHeaders() != JpegDecoder::Status::kSuccess) {
    return JXL_FAILURE("Failed to read header data.");
  }

  ppf->info.xsize = dec.xsize();
  ppf->info.ysize = dec.ysize();
  ppf->info.num_color_channels = dec.num_channels();
  ppf->info.bits_per_sample = PackedImage::BitsPerChannel(output_data_type);

  ppf->icc = dec.icc_profile();
  if (ppf->icc.empty()) {
    ppf->color_encoding.color_space =
        dec.num_channels() == 1 ? JXL_COLOR_SPACE_GRAY : JXL_COLOR_SPACE_RGB;
    ppf->color_encoding.white_point = JXL_WHITE_POINT_D65;
    ppf->color_encoding.primaries = JXL_PRIMARIES_SRGB;
    ppf->color_encoding.transfer_function = JXL_TRANSFER_FUNCTION_SRGB;
    ppf->color_encoding.rendering_intent = JXL_RENDERING_INTENT_RELATIVE;
  }

  JxlPixelFormat format = {static_cast<uint32_t>(dec.num_channels()),
                           output_data_type, JXL_LITTLE_ENDIAN, 0};
  ppf->frames.emplace_back(dec.xsize(), dec.ysize(), format);
  auto& frame = ppf->frames.back();

  if (dec.SetOutput(&frame.color) != JpegDecoder::Status::kSuccess) {
    return JXL_FAILURE("Failed to set output image");
  }

  size_t num_output_rows = 0;
  if (dec.ReadScanLines(&num_output_rows, dec.ysize()) !=
          JpegDecoder::Status::kSuccess ||
      num_output_rows != dec.ysize()) {
    return JXL_FAILURE("Failed to read image data.");
  }

  return true;
}

}  // namespace extras
}  // namespace jxl
