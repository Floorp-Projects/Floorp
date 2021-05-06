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

#include "lib/jxl/enc_bit_writer.h"

#include <string.h>  // memcpy

#include "lib/jxl/base/byte_order.h"
#include "lib/jxl/dec_bit_reader.h"

namespace jxl {

BitWriter::Allotment::Allotment(BitWriter* JXL_RESTRICT writer, size_t max_bits)
    : max_bits_(max_bits) {
  if (writer == nullptr) return;
  prev_bits_written_ = writer->BitsWritten();
  const size_t prev_bytes = writer->storage_.size();
  const size_t next_bytes = DivCeil(max_bits, kBitsPerByte);
  writer->storage_.resize(prev_bytes + next_bytes);
  parent_ = writer->current_allotment_;
  writer->current_allotment_ = this;
}

BitWriter::Allotment::~Allotment() {
  if (!called_) {
    // Not calling is a bug - unused storage will not be reclaimed.
    JXL_ABORT("Did not call Allotment::ReclaimUnused");
  }
}

void BitWriter::Allotment::FinishedHistogram(BitWriter* JXL_RESTRICT writer) {
  if (writer == nullptr) return;
  JXL_ASSERT(!called_);              // Call before ReclaimUnused
  JXL_ASSERT(histogram_bits_ == 0);  // Do not call twice
  JXL_ASSERT(writer->BitsWritten() >= prev_bits_written_);
  histogram_bits_ = writer->BitsWritten() - prev_bits_written_;
}

void BitWriter::Allotment::PrivateReclaim(BitWriter* JXL_RESTRICT writer,
                                          size_t* JXL_RESTRICT used_bits,
                                          size_t* JXL_RESTRICT unused_bits) {
  JXL_ASSERT(!called_);  // Do not call twice
  called_ = true;
  if (writer == nullptr) return;

  JXL_ASSERT(writer->BitsWritten() >= prev_bits_written_);
  *used_bits = writer->BitsWritten() - prev_bits_written_;
  JXL_ASSERT(*used_bits <= max_bits_);
  *unused_bits = max_bits_ - *used_bits;

  // Reclaim unused bytes whole bytes from writer's allotment.
  const size_t unused_bytes = *unused_bits / kBitsPerByte;  // truncate
  JXL_ASSERT(writer->storage_.size() >= unused_bytes);
  writer->storage_.resize(writer->storage_.size() - unused_bytes);
  writer->current_allotment_ = parent_;
  // Ensure we don't also charge the parent for these bits.
  auto parent = parent_;
  while (parent != nullptr) {
    parent->prev_bits_written_ += *used_bits;
    parent = parent->parent_;
  }
}

void BitWriter::AppendByteAligned(const Span<const uint8_t>& span) {
  if (!span.size()) return;
  storage_.resize(storage_.size() + span.size() + 1);  // extra zero padding

  // Concatenate by copying bytes because both source and destination are bytes.
  JXL_ASSERT(BitsWritten() % kBitsPerByte == 0);
  size_t pos = BitsWritten() / kBitsPerByte;
  memcpy(storage_.data() + pos, span.data(), span.size());
  pos += span.size();
  storage_[pos++] = 0;  // for next Write
  JXL_ASSERT(pos <= storage_.size());
  bits_written_ += span.size() * kBitsPerByte;
}

void BitWriter::AppendByteAligned(const BitWriter& other) {
  JXL_ASSERT(other.BitsWritten() % kBitsPerByte == 0);
  JXL_ASSERT(other.BitsWritten() / kBitsPerByte != 0);

  AppendByteAligned(other.GetSpan());
}

void BitWriter::AppendByteAligned(const std::vector<BitWriter>& others) {
  // Total size to add so we can preallocate
  size_t other_bytes = 0;
  for (const BitWriter& writer : others) {
    JXL_ASSERT(writer.BitsWritten() % kBitsPerByte == 0);
    other_bytes += writer.BitsWritten() / kBitsPerByte;
  }
  if (other_bytes == 0) {
    // No bytes to append: this happens for example when creating per-group
    // storage for groups, but not writing anything in them for e.g. lossless
    // images with no alpha. Do nothing.
    return;
  }
  storage_.resize(storage_.size() + other_bytes + 1);  // extra zero padding

  // Concatenate by copying bytes because both source and destination are bytes.
  JXL_ASSERT(BitsWritten() % kBitsPerByte == 0);
  size_t pos = BitsWritten() / kBitsPerByte;
  for (const BitWriter& writer : others) {
    const Span<const uint8_t> span = writer.GetSpan();
    memcpy(storage_.data() + pos, span.data(), span.size());
    pos += span.size();
  }
  storage_[pos++] = 0;  // for next Write
  JXL_ASSERT(pos <= storage_.size());
  bits_written_ += other_bytes * kBitsPerByte;
}

// TODO(lode): avoid code duplication
void BitWriter::AppendByteAligned(
    const std::vector<std::unique_ptr<BitWriter>>& others) {
  // Total size to add so we can preallocate
  size_t other_bytes = 0;
  for (const auto& writer : others) {
    JXL_ASSERT(writer->BitsWritten() % kBitsPerByte == 0);
    other_bytes += writer->BitsWritten() / kBitsPerByte;
  }
  if (other_bytes == 0) {
    // No bytes to append: this happens for example when creating per-group
    // storage for groups, but not writing anything in them for e.g. lossless
    // images with no alpha. Do nothing.
    return;
  }
  storage_.resize(storage_.size() + other_bytes + 1);  // extra zero padding

  // Concatenate by copying bytes because both source and destination are bytes.
  JXL_ASSERT(BitsWritten() % kBitsPerByte == 0);
  size_t pos = BitsWritten() / kBitsPerByte;
  for (const auto& writer : others) {
    const Span<const uint8_t> span = writer->GetSpan();
    memcpy(storage_.data() + pos, span.data(), span.size());
    pos += span.size();
  }
  storage_[pos++] = 0;  // for next Write
  JXL_ASSERT(pos <= storage_.size());
  bits_written_ += other_bytes * kBitsPerByte;
}

BitWriter& BitWriter::operator+=(const BitWriter& other) {
  // Required for correctness, otherwise owned[bits_written_] is out of bounds.
  if (other.bits_written_ == 0) return *this;
  const size_t other_bytes = DivCeil(other.bits_written_, kBitsPerByte);
  const size_t prev_bytes = storage_.size();
  storage_.resize(prev_bytes + other_bytes + 1);  // extra zero padding

  if (bits_written_ % kBitsPerByte == 0) {
    // Only copy fully-initialized bytes.
    const size_t full_bytes = other.bits_written_ / kBitsPerByte;  // truncated
    memcpy(&storage_[bits_written_ / kBitsPerByte], other.storage_.data(),
           full_bytes);
    storage_[bits_written_ / kBitsPerByte + full_bytes] = 0;  // for next Write
    bits_written_ += full_bytes * kBitsPerByte;

    const size_t leftovers = other.bits_written_ % kBitsPerByte;
    if (leftovers != 0) {
      BitReader reader(Span<const uint8_t>(other.storage_.data() + full_bytes,
                                           other_bytes - full_bytes));
      Write(leftovers, reader.ReadBits(leftovers));
      JXL_CHECK(reader.Close());
    }
    return *this;
  }

  constexpr size_t N = kMaxBitsPerCall < BitReader::kMaxBitsPerCall
                           ? kMaxBitsPerCall
                           : BitReader::kMaxBitsPerCall;

  // Do not use GetSpan because other may not be byte-aligned.
  BitReader reader(other.storage_);
  size_t i = 0;
  for (; i + N <= other.bits_written_; i += N) {
    Write(N, reader.ReadFixedBits<N>());
  }
  const size_t leftovers = other.bits_written_ - i;
  if (leftovers != 0) {
    Write(leftovers, reader.ReadBits(leftovers));
  }
  JXL_CHECK(reader.Close());
  return *this;
}

// Example: let's assume that 3 bits (Rs below) have been written already:
// BYTE+0       BYTE+1       BYTE+2
// 0000 0RRR    ???? ????    ???? ????
//
// Now, we could write up to 5 bits by just shifting them left by 3 bits and
// OR'ing to BYTE-0.
//
// For n > 5 bits, we write the lowest 5 bits as above, then write the next
// lowest bits into BYTE+1 starting from its lower bits and so on.
void BitWriter::Write(size_t n_bits, uint64_t bits) {
  JXL_DASSERT((bits >> n_bits) == 0);
  JXL_DASSERT(n_bits <= kMaxBitsPerCall);
  uint8_t* p = &storage_[bits_written_ / kBitsPerByte];
  const size_t bits_in_first_byte = bits_written_ % kBitsPerByte;
  bits <<= bits_in_first_byte;
#if JXL_BYTE_ORDER_LITTLE
  uint64_t v = *p;
  // Last (partial) or next byte to write must be zero-initialized!
  // PaddedBytes initializes the first, and Write/Append maintain this.
  JXL_DASSERT(v >> bits_in_first_byte == 0);
  v |= bits;
  memcpy(p, &v, sizeof(v));  // Write bytes: possibly more than n_bits/8
#else
  *p++ |= static_cast<uint8_t>(bits & 0xFF);
  for (size_t bits_left_to_write = n_bits + bits_in_first_byte;
       bits_left_to_write >= 9; bits_left_to_write -= 8) {
    bits >>= 8;
    *p++ = static_cast<uint8_t>(bits & 0xFF);
  }
  *p = 0;
#endif
  bits_written_ += n_bits;
}

BitWriter& BitWriter::operator+=(const PaddedBytes& other) {
  const size_t other_bytes = other.size();
  // Required for correctness, otherwise owned[bits_written_] is out of bounds.
  if (other_bytes == 0) return *this;
  const size_t other_bits = other_bytes * kBitsPerByte;

  storage_.resize(storage_.size() + other_bytes + 1);
  if (bits_written_ % kBitsPerByte == 0) {
    memcpy(&storage_[bits_written_ / kBitsPerByte], other.data(), other_bytes);
    storage_[bits_written_ / kBitsPerByte + other_bytes] = 0;  // for next Write
    bits_written_ += other_bits;
    return *this;
  }
  constexpr size_t N = kMaxBitsPerCall < BitReader::kMaxBitsPerCall
                           ? kMaxBitsPerCall
                           : BitReader::kMaxBitsPerCall;

  BitReader reader(other);
  size_t i = 0;
  for (; i + N <= other_bits; i += N) {
    Write(N, reader.ReadFixedBits<N>());
  }
  const size_t leftovers = other_bits - i;
  Write(leftovers, reader.ReadBits(leftovers));
  JXL_CHECK(reader.Close());
  return *this;
}

}  // namespace jxl
