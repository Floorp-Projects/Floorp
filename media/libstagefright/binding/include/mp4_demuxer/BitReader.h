/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BIT_READER_H_
#define BIT_READER_H_

#include "nsAutoPtr.h"
#include "MediaData.h"

namespace stagefright { class ABitReader; }

namespace mp4_demuxer
{

class BitReader
{
public:
  explicit BitReader(const mozilla::MediaByteBuffer* aBuffer);
  BitReader(const uint8_t* aBuffer, size_t aLength);
  ~BitReader();
  uint32_t ReadBits(size_t aNum);
  uint32_t ReadBit() { return ReadBits(1); }
  uint32_t ReadU32() { return ReadBits(32); }
  uint64_t ReadU64();

  // Read the UTF-8 sequence and convert it to its 64-bit UCS-4 encoded form.
  // Return 0xfffffffffffffff if sequence was invalid.
  uint64_t ReadUTF8();
  // Read unsigned integer Exp-Golomb-coded.
  uint32_t ReadUE();
  // Read signed integer Exp-Golomb-coded.
  int32_t ReadSE();

  // Return the number of bits parsed so far;
  size_t BitCount() const;

private:
  nsAutoPtr<stagefright::ABitReader> mBitReader;
  const size_t mSize;
};

} // namespace mp4_demuxer

#endif // BIT_READER_H_