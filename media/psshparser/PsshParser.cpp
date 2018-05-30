/*
 * Copyright 2015, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "PsshParser.h"

#include "mozilla/Assertions.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/Move.h"
#include <memory.h>
#include <algorithm>
#include <assert.h>
#include <limits>

// Stripped down version of mp4_demuxer::ByteReader, stripped down to make it
// easier to link into ClearKey DLL and gtest.
class ByteReader
{
public:
  ByteReader(const uint8_t* aData, size_t aSize)
    : mPtr(aData), mRemaining(aSize), mLength(aSize)
  {
  }

  size_t Offset() const
  {
    return mLength - mRemaining;
  }

  size_t Remaining() const { return mRemaining; }

  size_t Length() const { return mLength; }

  bool CanRead8() const { return mRemaining >= 1; }

  uint8_t ReadU8()
  {
    auto ptr = Read(1);
    if (!ptr) {
      MOZ_ASSERT(false);
      return 0;
    }
    return *ptr;
  }

  bool CanRead32() const { return mRemaining >= 4; }

  uint32_t ReadU32()
  {
    auto ptr = Read(4);
    if (!ptr) {
      MOZ_ASSERT(false);
      return 0;
    }
    return mozilla::BigEndian::readUint32(ptr);
  }

  const uint8_t* Read(size_t aCount)
  {
    if (aCount > mRemaining) {
      mRemaining = 0;
      return nullptr;
    }
    mRemaining -= aCount;

    const uint8_t* result = mPtr;
    mPtr += aCount;

    return result;
  }

  const uint8_t* Seek(size_t aOffset)
  {
    if (aOffset > mLength) {
      MOZ_ASSERT(false);
      return nullptr;
    }

    mPtr = mPtr - Offset() + aOffset;
    mRemaining = mLength - aOffset;
    return mPtr;
  }

private:
  const uint8_t* mPtr;
  size_t mRemaining;
  const size_t mLength;
};

#define FOURCC(a,b,c,d) ((a << 24) + (b << 16) + (c << 8) + d)

 // System ID identifying the cenc v2 pssh box format; specified at:
 // https://dvcs.w3.org/hg/html-media/raw-file/tip/encrypted-media/cenc-format.html
const uint8_t kSystemID[] = {
  0x10, 0x77, 0xef, 0xec, 0xc0, 0xb2, 0x4d, 0x02,
  0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2, 0xfb, 0x4b
};

bool
ParseCENCInitData(const uint8_t* aInitData,
                  uint32_t aInitDataSize,
                  std::vector<std::vector<uint8_t>>& aOutKeyIds)
{
  aOutKeyIds.clear();
  std::vector<std::vector<uint8_t>> keyIds;
  ByteReader reader(aInitData, aInitDataSize);
  while (reader.CanRead32()) {
    // Box size. For the common system Id, ignore this, as some useragents
    // handle invalid box sizes.
    const size_t start = reader.Offset();
    const size_t size = reader.ReadU32();
    if (size > std::numeric_limits<size_t>::max() - start) {
      // Ensure 'start + size' calculation below can't overflow.
      return false;
    }
    const size_t end = start + size;
    if (end > reader.Length()) {
      // Ridiculous sized box.
      return false;
    }

    // PSSH box type.
    if (!reader.CanRead32()) {
      return false;
    }
    uint32_t box = reader.ReadU32();
    if (box != FOURCC('p','s','s','h')) {
      return false;
    }

    // 1 byte version, 3 bytes flags.
    if (!reader.CanRead32()) {
      return false;
    }
    uint8_t version = reader.ReadU8();
    if (version != 1) {
      // Ignore pssh boxes with wrong version.
      reader.Seek(std::max<size_t>(reader.Offset(), end));
      continue;
    }
    reader.Read(3); // skip flags.

    // SystemID
    const uint8_t* sid = reader.Read(sizeof(kSystemID));
    if (!sid) {
      // Insufficient bytes to read SystemID.
      return false;
    }

    if (memcmp(kSystemID, sid, sizeof(kSystemID))) {
      // Ignore pssh boxes with wrong system ID.
      reader.Seek(std::max<size_t>(reader.Offset(), end));
      continue;
    }

    if (!reader.CanRead32()) {
      return false;
    }
    uint32_t kidCount = reader.ReadU32();

    if (kidCount * CENC_KEY_LEN > reader.Remaining()) {
      // Not enough bytes remaining to read all keys.
      return false;
    }

    for (uint32_t i = 0; i < kidCount; i++) {
      const uint8_t* kid = reader.Read(CENC_KEY_LEN);
      keyIds.push_back(std::vector<uint8_t>(kid, kid + CENC_KEY_LEN));
    }

    // Size of extra data. EME CENC format spec says datasize should
    // always be 0. We explicitly read the datasize, in case the box
    // size was 0, so that we get to the end of the box.
    if (!reader.CanRead32()) {
      return false;
    }
    reader.ReadU32();

    // Jump forwards to the end of the box, skipping any padding.
    if (size) {
      reader.Seek(end);
    }
  }
  aOutKeyIds = std::move(keyIds);
  return true;
}
