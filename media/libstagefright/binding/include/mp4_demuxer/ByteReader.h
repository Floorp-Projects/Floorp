/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BYTE_READER_H_
#define BYTE_READER_H_

#include "mozilla/Vector.h"
#include "nsTArray.h"

namespace mp4_demuxer
{

class ByteReader
{
public:
  ByteReader(const mozilla::Vector<uint8_t>& aData)
    : mPtr(&aData[0]), mRemaining(aData.length())
  {
  }
  ByteReader(const uint8_t* aData, size_t aSize)
    : mPtr(aData), mRemaining(aSize)
  {
  }

  ~ByteReader()
  {
    MOZ_ASSERT(!mRemaining);
  }

  // Make it explicit if we're not using the extra bytes.
  void DiscardRemaining() {
    mRemaining = 0;
  }

  size_t Remaining() const { return mRemaining; }

  bool CanRead8() { return mRemaining >= 1; }

  uint8_t ReadU8()
  {
    auto ptr = Read(1);
    if (!ptr) {
      MOZ_ASSERT(false);
      return 0;
    }
    return *ptr;
  }

  bool CanRead16() { return mRemaining >= 2; }

  uint16_t ReadU16()
  {
    auto ptr = Read(2);
    if (!ptr) {
      MOZ_ASSERT(false);
      return 0;
    }
    return ptr[0] << 8 | ptr[1];
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

  template <typename T> bool CanReadType() { return mRemaining >= sizeof(T); }

  template <typename T> T ReadType()
  {
    auto ptr = Read(sizeof(T));
    if (!ptr) {
      MOZ_ASSERT(false);
      return 0;
    }
    return *reinterpret_cast<const T*>(ptr);
  }

  template <typename T>
  bool ReadArray(nsTArray<T>& aDest, size_t aLength)
  {
    auto ptr = Read(aLength * sizeof(T));
    if (!ptr) {
      return false;
    }

    aDest.Clear();
    aDest.AppendElements(reinterpret_cast<const T*>(ptr), aLength);
    return true;
  }

private:
  const uint8_t* mPtr;
  size_t mRemaining;
};
}

#endif
