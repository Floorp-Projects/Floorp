/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BYTE_WRITER_H_
#define BYTE_WRITER_H_

#include "mozilla/EndianUtils.h"
#include "mozilla/Vector.h"
#include "nsTArray.h"

namespace mp4_demuxer {

class ByteWriter
{
public:
  explicit ByteWriter(mozilla::Vector<uint8_t>& aData)
    : mPtr(aData)
  {
  }
  ~ByteWriter()
  {
  }

  void WriteU8(uint8_t aByte)
  {
    Write(&aByte, 1);
  }

  void WriteU16(uint16_t aShort)
  {
    uint8_t c[2];
    mozilla::BigEndian::writeUint16(&c[0], aShort);
    Write(&c[0], 2);
  }

  void WriteU32(uint32_t aLong)
  {
    uint8_t c[4];
    mozilla::BigEndian::writeUint32(&c[0], aLong);
    Write(&c[0], 4);
  }

  void Write32(int32_t aLong)
  {
    uint8_t c[4];
    mozilla::BigEndian::writeInt32(&c[0], aLong);
    Write(&c[0], 4);
  }

  void WriteU64(uint64_t aLongLong)
  {
    uint8_t c[8];
    mozilla::BigEndian::writeUint64(&c[0], aLongLong);
    Write(&c[0], 8);
  }

  void Write64(int64_t aLongLong)
  {
    uint8_t c[8];
    mozilla::BigEndian::writeInt64(&c[0], aLongLong);
    Write(&c[0], 8);
  }

  void Write(const uint8_t* aSrc, size_t aCount)
  {
    MOZ_RELEASE_ASSERT(mPtr.append(aSrc, aCount));
  }

private:
  mozilla::Vector<uint8_t>& mPtr;
};

} // namespace mp4_demuxer

#endif
