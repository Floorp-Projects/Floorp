/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mp4_demuxer/BitReader.h"
#include <media/stagefright/foundation/ABitReader.h>

using namespace mozilla;
using namespace stagefright;

namespace mp4_demuxer
{

BitReader::BitReader(const mozilla::MediaByteBuffer* aBuffer)
  : mBitReader(new ABitReader(aBuffer->Elements(), aBuffer->Length()))
  , mSize(aBuffer->Length()) {}

BitReader::BitReader(const uint8_t* aBuffer, size_t aLength)
  : mBitReader(new ABitReader(aBuffer, aLength))
  , mSize(aLength) {}

BitReader::~BitReader() {}

uint32_t
BitReader::ReadBits(size_t aNum)
{
  MOZ_ASSERT(aNum <= 32);
  if (mBitReader->numBitsLeft() < aNum) {
    return 0;
  }
  return mBitReader->getBits(aNum);
}

// Read unsigned integer Exp-Golomb-coded.
uint32_t
BitReader::ReadUE()
{
  uint32_t i = 0;

  while (ReadBit() == 0 && i < 32) {
    i++;
  }
  if (i == 32) {
    // This can happen if the data is invalid, or if it's
    // short, since ReadBit() will return 0 when it runs
    // off the end of the buffer.
    NS_WARNING("Invalid H.264 data");
    return 0;
  }
  uint32_t r = ReadBits(i);
  r += (1 << i) - 1;
  return r;
}

// Read signed integer Exp-Golomb-coded.
int32_t
BitReader::ReadSE()
{
  int32_t r = ReadUE();
  if (r & 1) {
    return (r+1) / 2;
  } else {
    return -r / 2;
  }
}

uint64_t
BitReader::ReadU64()
{
  uint64_t hi = ReadU32();
  uint32_t lo = ReadU32();
  return (hi << 32) | lo;
}

uint64_t
BitReader::ReadUTF8()
{
  int64_t val = ReadBits(8);
  uint32_t top = (val & 0x80) >> 1;

  if ((val & 0xc0) == 0x80 || val >= 0xFE) {
    // error.
    return -1;
  }
  while (val & top) {
    int tmp = ReadBits(8) - 128;
    if (tmp >> 6) {
      // error.
      return -1;
    }
    val = (val << 6) + tmp;
    top <<= 5;
  }
  val &= (top << 1) - 1;
  return val;
}

size_t
BitReader::BitCount() const
{
  return mSize * 8 - mBitReader->numBitsLeft();
}

} // namespace mp4_demuxer
