/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RiceDeltaDecoder.h"

namespace {

////////////////////////////////////////////////////////////////////////
// BitBuffer is copied and modified from webrtc/base/bitbuffer.h
//

/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree (webrtc/base/bitbuffer.h/cc). An additional intellectual property
 *  rights grant can be found in the file PATENTS.  All contributing
 *  project authors may be found in the AUTHORS file in the root of
 *  the source tree.
 */

class BitBuffer {
 public:
  BitBuffer(const uint8_t* bytes, size_t byte_count);

  // Gets the current offset, in bytes/bits, from the start of the buffer. The
  // bit offset is the offset into the current byte, in the range [0,7].
  void GetCurrentOffset(size_t* out_byte_offset, size_t* out_bit_offset);

  // The remaining bits in the byte buffer.
  uint64_t RemainingBitCount() const;

  // Reads byte-sized values from the buffer. Returns false if there isn't
  // enough data left for the specified type.
  bool ReadUInt8(uint8_t* val);
  bool ReadUInt16(uint16_t* val);
  bool ReadUInt32(uint32_t* val);

  // Reads bit-sized values from the buffer. Returns false if there isn't enough
  // data left for the specified bit count..
  bool ReadBits(uint32_t* val, size_t bit_count);

  // Peeks bit-sized values from the buffer. Returns false if there isn't enough
  // data left for the specified number of bits. Doesn't move the current
  // offset.
  bool PeekBits(uint32_t* val, size_t bit_count);

  // Reads the exponential golomb encoded value at the current offset.
  // Exponential golomb values are encoded as:
  // 1) x = source val + 1
  // 2) In binary, write [countbits(x) - 1] 1s, then x
  // To decode, we count the number of leading 1 bits, read that many + 1 bits,
  // and increment the result by 1.
  // Returns false if there isn't enough data left for the specified type, or if
  // the value wouldn't fit in a uint32_t.
  bool ReadExponentialGolomb(uint32_t* val);
  // Reads signed exponential golomb values at the current offset. Signed
  // exponential golomb values are just the unsigned values mapped to the
  // sequence 0, 1, -1, 2, -2, etc. in order.
  bool ReadSignedExponentialGolomb(int32_t* val);

  // Moves current position |byte_count| bytes forward. Returns false if
  // there aren't enough bytes left in the buffer.
  bool ConsumeBytes(size_t byte_count);
  // Moves current position |bit_count| bits forward. Returns false if
  // there aren't enough bits left in the buffer.
  bool ConsumeBits(size_t bit_count);

  // Sets the current offset to the provied byte/bit offsets. The bit
  // offset is from the given byte, in the range [0,7].
  bool Seek(size_t byte_offset, size_t bit_offset);

 protected:
  const uint8_t* const bytes_;
  // The total size of |bytes_|.
  size_t byte_count_;
  // The current offset, in bytes, from the start of |bytes_|.
  size_t byte_offset_;
  // The current offset, in bits, into the current byte.
  size_t bit_offset_;
};

} // end of unnamed namespace

static void
ReverseByte(uint8_t& b)
{
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
}

namespace mozilla {
namespace safebrowsing {

RiceDeltaDecoder::RiceDeltaDecoder(uint8_t* aEncodedData,
                                   size_t aEncodedDataSize)
  : mEncodedData(aEncodedData)
  , mEncodedDataSize(aEncodedDataSize)
{
}

bool
RiceDeltaDecoder::Decode(uint32_t aRiceParameter,
                         uint32_t aFirstValue,
                         uint32_t aNumEntries,
                         uint32_t* aDecodedData)
{
  // Reverse each byte before reading bits from the byte buffer.
  for (size_t i = 0; i < mEncodedDataSize; i++) {
    ReverseByte(mEncodedData[i]);
  }

  BitBuffer bitBuffer(mEncodedData, mEncodedDataSize);

  // q = quotient
  // r = remainder
  // k = RICE parameter
  const uint32_t k = aRiceParameter;
  uint32_t previous = aFirstValue;
  for (uint32_t i = 0; i < aNumEntries; i++) {
    // Read the quotient of N.
    uint32_t q;
    if (!bitBuffer.ReadExponentialGolomb(&q)) {
      LOG(("Encoded data underflow!"));
      return false;
    }

    // Read the remainder of N, one bit at a time.
    uint32_t r = 0;
    for (uint32_t j = 0; j < k; j++) {
      uint32_t b = 0;
      if (!bitBuffer.ReadBits(&b, 1)) {
        // Insufficient bits. Just leave them as zeros.
        break;
      }
      // Add the bit to the right position so that it's in Little Endian order.
      r |= b << j;
    }

    // Caculate N from q,r,k.
    aDecodedData[i] = ((q << k) + r) + previous;
    previous = aDecodedData[i];
  }

  return true;
}

} // end of namespace mozilla
} // end of namespace safebrowsing

namespace {
//////////////////////////////////////////////////////////////////////////
// The BitBuffer impl is copied and modified from webrtc/base/bitbuffer.cc
//

// Returns the lowest (right-most) |bit_count| bits in |byte|.
uint8_t LowestBits(uint8_t byte, size_t bit_count) {
  return byte & ((1 << bit_count) - 1);
}

// Returns the highest (left-most) |bit_count| bits in |byte|, shifted to the
// lowest bits (to the right).
uint8_t HighestBits(uint8_t byte, size_t bit_count) {
  MOZ_ASSERT(bit_count < 8u);
  uint8_t shift = 8 - static_cast<uint8_t>(bit_count);
  uint8_t mask = 0xFF << shift;
  return (byte & mask) >> shift;
}

BitBuffer::BitBuffer(const uint8_t* bytes, size_t byte_count)
    : bytes_(bytes), byte_count_(byte_count), byte_offset_(), bit_offset_() {
  MOZ_ASSERT(static_cast<uint64_t>(byte_count_) <=
             std::numeric_limits<uint32_t>::max());
}

uint64_t BitBuffer::RemainingBitCount() const {
  return (static_cast<uint64_t>(byte_count_) - byte_offset_) * 8 - bit_offset_;
}

bool BitBuffer::ReadUInt8(uint8_t* val) {
  uint32_t bit_val;
  if (!ReadBits(&bit_val, sizeof(uint8_t) * 8)) {
    return false;
  }
  MOZ_ASSERT(bit_val <= std::numeric_limits<uint8_t>::max());
  *val = static_cast<uint8_t>(bit_val);
  return true;
}

bool BitBuffer::ReadUInt16(uint16_t* val) {
  uint32_t bit_val;
  if (!ReadBits(&bit_val, sizeof(uint16_t) * 8)) {
    return false;
  }
  MOZ_ASSERT(bit_val <= std::numeric_limits<uint16_t>::max());
  *val = static_cast<uint16_t>(bit_val);
  return true;
}

bool BitBuffer::ReadUInt32(uint32_t* val) {
  return ReadBits(val, sizeof(uint32_t) * 8);
}

bool BitBuffer::PeekBits(uint32_t* val, size_t bit_count) {
  if (!val || bit_count > RemainingBitCount() || bit_count > 32) {
    return false;
  }
  const uint8_t* bytes = bytes_ + byte_offset_;
  size_t remaining_bits_in_current_byte = 8 - bit_offset_;
  uint32_t bits = LowestBits(*bytes++, remaining_bits_in_current_byte);
  // If we're reading fewer bits than what's left in the current byte, just
  // return the portion of this byte that we need.
  if (bit_count < remaining_bits_in_current_byte) {
    *val = HighestBits(bits, bit_offset_ + bit_count);
    return true;
  }
  // Otherwise, subtract what we've read from the bit count and read as many
  // full bytes as we can into bits.
  bit_count -= remaining_bits_in_current_byte;
  while (bit_count >= 8) {
    bits = (bits << 8) | *bytes++;
    bit_count -= 8;
  }
  // Whatever we have left is smaller than a byte, so grab just the bits we need
  // and shift them into the lowest bits.
  if (bit_count > 0) {
    bits <<= bit_count;
    bits |= HighestBits(*bytes, bit_count);
  }
  *val = bits;
  return true;
}

bool BitBuffer::ReadBits(uint32_t* val, size_t bit_count) {
  return PeekBits(val, bit_count) && ConsumeBits(bit_count);
}

bool BitBuffer::ConsumeBytes(size_t byte_count) {
  return ConsumeBits(byte_count * 8);
}

bool BitBuffer::ConsumeBits(size_t bit_count) {
  if (bit_count > RemainingBitCount()) {
    return false;
  }

  byte_offset_ += (bit_offset_ + bit_count) / 8;
  bit_offset_ = (bit_offset_ + bit_count) % 8;
  return true;
}

bool BitBuffer::ReadExponentialGolomb(uint32_t* val) {
  if (!val) {
    return false;
  }

  *val = 0;

  // Count the number of leading 0 bits by peeking/consuming them one at a time.
  size_t one_bit_count = 0;
  uint32_t peeked_bit;
  while (PeekBits(&peeked_bit, 1) && peeked_bit == 1) {
    one_bit_count++;
    ConsumeBits(1);
  }
  if (!ConsumeBits(1)) {
    return false; // The stream is incorrectly terminated at '1'.
  }

  *val = one_bit_count;
  return true;
}

bool BitBuffer::ReadSignedExponentialGolomb(int32_t* val) {
  uint32_t unsigned_val;
  if (!ReadExponentialGolomb(&unsigned_val)) {
    return false;
  }
  if ((unsigned_val & 1) == 0) {
    *val = -static_cast<int32_t>(unsigned_val / 2);
  } else {
    *val = (unsigned_val + 1) / 2;
  }
  return true;
}

void BitBuffer::GetCurrentOffset(
    size_t* out_byte_offset, size_t* out_bit_offset) {
  MOZ_ASSERT(out_byte_offset != NULL);
  MOZ_ASSERT(out_bit_offset != NULL);
  *out_byte_offset = byte_offset_;
  *out_bit_offset = bit_offset_;
}

bool BitBuffer::Seek(size_t byte_offset, size_t bit_offset) {
  if (byte_offset > byte_count_ || bit_offset > 7 ||
      (byte_offset == byte_count_ && bit_offset > 0)) {
    return false;
  }
  byte_offset_ = byte_offset;
  bit_offset_ = bit_offset;
  return true;
}
}

