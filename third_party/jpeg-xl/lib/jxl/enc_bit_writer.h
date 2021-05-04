// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LIB_JXL_ENC_BIT_WRITER_H_
#define LIB_JXL_ENC_BIT_WRITER_H_

// BitWriter class: unbuffered writes using unaligned 64-bit stores.

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/common.h"

namespace jxl {

struct BitWriter {
  // Upper bound on `n_bits` in each call to Write. We shift a 64-bit word by
  // 7 bits (max already valid bits in the last byte) and at least 1 bit is
  // needed to zero-initialize the bit-stream ahead (i.e. if 7 bits are valid
  // and we write 57 bits, then the next write will access a byte that was not
  // yet zero-initialized).
  static constexpr size_t kMaxBitsPerCall = 56;

  BitWriter() : bits_written_(0) {}

  // Disallow copying - may lead to bugs.
  BitWriter(const BitWriter&) = delete;
  BitWriter& operator=(const BitWriter&) = delete;
  BitWriter(BitWriter&&) = default;
  BitWriter& operator=(BitWriter&&) = default;

  explicit BitWriter(PaddedBytes&& donor)
      : bits_written_(donor.size() * kBitsPerByte),
        storage_(std::move(donor)) {}

  size_t BitsWritten() const { return bits_written_; }

  Span<const uint8_t> GetSpan() const {
    // Callers must ensure byte alignment to avoid uninitialized bits.
    JXL_ASSERT(bits_written_ % kBitsPerByte == 0);
    return Span<const uint8_t>(storage_.data(), bits_written_ / kBitsPerByte);
  }

  // Example usage: bytes = std::move(writer).TakeBytes(); Useful for the
  // top-level encoder which returns PaddedBytes, not a BitWriter.
  // *this must be an rvalue reference and is invalid afterwards.
  PaddedBytes&& TakeBytes() && {
    // Callers must ensure byte alignment to avoid uninitialized bits.
    JXL_ASSERT(bits_written_ % kBitsPerByte == 0);
    storage_.resize(bits_written_ / kBitsPerByte);
    return std::move(storage_);
  }

  // Must be byte-aligned before calling.
  void AppendByteAligned(const Span<const uint8_t>& span);
  // NOTE: no allotment needed, the other BitWriters have already been charged.
  void AppendByteAligned(const BitWriter& other);
  void AppendByteAligned(const std::vector<std::unique_ptr<BitWriter>>& others);
  void AppendByteAligned(const std::vector<BitWriter>& others);

  class Allotment {
   public:
    // Expands a BitWriter's storage. Must happen before calling Write or
    // ZeroPadToByte. Must call ReclaimUnused after writing to reclaim the
    // unused storage so that BitWriter memory use remains tightly bounded.
    Allotment(BitWriter* JXL_RESTRICT writer, size_t max_bits);
    ~Allotment();

    size_t MaxBits() const { return max_bits_; }

    // Call after writing a histogram, but before ReclaimUnused.
    void FinishedHistogram(BitWriter* JXL_RESTRICT writer);

    size_t HistogramBits() const {
      JXL_ASSERT(called_);
      return histogram_bits_;
    }

    // Do not call directly - use ::ReclaimAndCharge instead, which ensures
    // the bits are charged to a layer.
    void PrivateReclaim(BitWriter* JXL_RESTRICT writer,
                        size_t* JXL_RESTRICT used_bits,
                        size_t* JXL_RESTRICT unused_bits);

   private:
    size_t prev_bits_written_;
    const size_t max_bits_;
    size_t histogram_bits_ = 0;
    bool called_ = false;
    Allotment* parent_;
  };

  // WARNING: think twice before using this. Concatenating two BitWriters that
  // pad to bytes is NOT the same as one contiguous BitWriter.
  BitWriter& operator+=(const BitWriter& other);

  // TODO(janwas): remove once all callers use BitWriter
  BitWriter& operator+=(const PaddedBytes& other);

  // Writes bits into bytes in increasing addresses, and within a byte
  // least-significant-bit first.
  //
  // The function can write up to 56 bits in one go.
  void Write(size_t n_bits, uint64_t bits);

  // This should only rarely be used - e.g. when the current location will be
  // referenced via byte offset (TOCs point to groups), or byte-aligned reading
  // is required for speed. WARNING: this interacts badly with operator+=,
  // see above.
  void ZeroPadToByte() {
    const size_t remainder_bits =
        RoundUpBitsToByteMultiple(bits_written_) - bits_written_;
    if (remainder_bits == 0) return;
    Write(remainder_bits, 0);
    JXL_ASSERT(bits_written_ % kBitsPerByte == 0);
  }

  // TODO(janwas): remove? only called from ANS
  void RewindStorage(const size_t pos0) {
    JXL_ASSERT(pos0 <= bits_written_);
    bits_written_ = pos0;
    static const uint8_t kRewindMasks[8] = {0x0, 0x1,  0x3,  0x7,
                                            0xf, 0x1f, 0x3f, 0x7f};
    storage_[pos0 >> 3] &= kRewindMasks[pos0 & 7];
  }

 private:
  size_t bits_written_;
  PaddedBytes storage_;
  Allotment* current_allotment_ = nullptr;
};

}  // namespace jxl

#endif  // LIB_JXL_ENC_BIT_WRITER_H_
