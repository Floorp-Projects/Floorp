/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BYTE_READER_H_
#define BYTE_READER_H_

#include "mozilla/EndianUtils.h"
#include "mozilla/Vector.h"
#include "nsTArray.h"
#include "MediaData.h"

namespace mp4_demuxer {

class MOZ_RAII ByteReader
{
public:
  ByteReader() : mPtr(nullptr), mRemaining(0) {}
  explicit ByteReader(const mozilla::Vector<uint8_t>& aData)
    : mPtr(aData.begin()), mRemaining(aData.length()), mLength(aData.length())
  {
  }
  ByteReader(const uint8_t* aData, size_t aSize)
    : mPtr(aData), mRemaining(aSize), mLength(aSize)
  {
  }
  template<size_t S>
  explicit ByteReader(const AutoTArray<uint8_t, S>& aData)
    : mPtr(aData.Elements()), mRemaining(aData.Length()), mLength(aData.Length())
  {
  }
  explicit ByteReader(const nsTArray<uint8_t>& aData)
    : mPtr(aData.Elements()), mRemaining(aData.Length()), mLength(aData.Length())
  {
  }
  explicit ByteReader(const mozilla::MediaByteBuffer* aData)
    : mPtr(aData->Elements()), mRemaining(aData->Length()), mLength(aData->Length())
  {
  }

  void SetData(const nsTArray<uint8_t>& aData)
  {
    MOZ_ASSERT(!mPtr && !mRemaining);
    mPtr = aData.Elements();
    mRemaining = aData.Length();
    mLength = mRemaining;
  }

  ~ByteReader()
  {
    NS_ASSERTION(!mRemaining, "Not all bytes have been processed");
  }

  size_t Offset()
  {
    return mLength - mRemaining;
  }

  // Make it explicit if we're not using the extra bytes.
  void DiscardRemaining()
  {
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
    return mozilla::BigEndian::readUint16(ptr);
  }

  int16_t ReadLE16()
  {
    auto ptr = Read(2);
    if (!ptr) {
      MOZ_ASSERT(false);
      return 0;
    }
    return mozilla::LittleEndian::readInt16(ptr);
  }

  uint32_t ReadU24()
  {
    auto ptr = Read(3);
    if (!ptr) {
      MOZ_ASSERT(false);
      return 0;
    }
    return ptr[0] << 16 | ptr[1] << 8 | ptr[2];
  }

  uint32_t Read24()
  {
    return (uint32_t)ReadU24();
  }

  int32_t ReadLE24()
  {
    auto ptr = Read(3);
    if (!ptr) {
      MOZ_ASSERT(false);
      return 0;
    }
    int32_t result = int32_t(ptr[2] << 16 | ptr[1] << 8 | ptr[0]);
    if (result & 0x00800000u) {
      result -= 0x1000000;
    }
    return result;
  }

  bool CanRead32() { return mRemaining >= 4; }

  uint32_t ReadU32()
  {
    auto ptr = Read(4);
    if (!ptr) {
      MOZ_ASSERT(false);
      return 0;
    }
    return mozilla::BigEndian::readUint32(ptr);
  }

  int32_t Read32()
  {
    auto ptr = Read(4);
    if (!ptr) {
      MOZ_ASSERT(false);
      return 0;
    }
    return mozilla::BigEndian::readInt32(ptr);
  }

  uint64_t ReadU64()
  {
    auto ptr = Read(8);
    if (!ptr) {
      MOZ_ASSERT(false);
      return 0;
    }
    return mozilla::BigEndian::readUint64(ptr);
  }

  int64_t Read64()
  {
    auto ptr = Read(8);
    if (!ptr) {
      MOZ_ASSERT(false);
      return 0;
    }
    return mozilla::BigEndian::readInt64(ptr);
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

  const uint8_t* Rewind(size_t aCount)
  {
    MOZ_ASSERT(aCount <= Offset());
    size_t rewind = Offset();
    if (aCount < rewind) {
      rewind = aCount;
    }
    mRemaining += rewind;
    mPtr -= rewind;
    return mPtr;
  }

  uint8_t PeekU8()
  {
    auto ptr = Peek(1);
    if (!ptr) {
      MOZ_ASSERT(false);
      return 0;
    }
    return *ptr;
  }

  uint16_t PeekU16()
  {
    auto ptr = Peek(2);
    if (!ptr) {
      MOZ_ASSERT(false);
      return 0;
    }
    return mozilla::BigEndian::readUint16(ptr);
  }

  uint32_t PeekU24()
  {
    auto ptr = Peek(3);
    if (!ptr) {
      MOZ_ASSERT(false);
      return 0;
    }
    return ptr[0] << 16 | ptr[1] << 8 | ptr[2];
  }

  uint32_t Peek24()
  {
    return (uint32_t)PeekU24();
  }

  uint32_t PeekU32()
  {
    auto ptr = Peek(4);
    if (!ptr) {
      MOZ_ASSERT(false);
      return 0;
    }
    return mozilla::BigEndian::readUint32(ptr);
  }

  int32_t Peek32()
  {
    auto ptr = Peek(4);
    if (!ptr) {
      MOZ_ASSERT(false);
      return 0;
    }
    return mozilla::BigEndian::readInt32(ptr);
  }

  uint64_t PeekU64()
  {
    auto ptr = Peek(8);
    if (!ptr) {
      MOZ_ASSERT(false);
      return 0;
    }
    return mozilla::BigEndian::readUint64(ptr);
  }

  int64_t Peek64()
  {
    auto ptr = Peek(8);
    if (!ptr) {
      MOZ_ASSERT(false);
      return 0;
    }
    return mozilla::BigEndian::readInt64(ptr);
  }

  const uint8_t* Peek(size_t aCount)
  {
    if (aCount > mRemaining) {
      return nullptr;
    }
    return mPtr;
  }

  const uint8_t* Seek(size_t aOffset)
  {
    if (aOffset >= mLength) {
      MOZ_ASSERT(false);
      return nullptr;
    }

    mPtr = mPtr - Offset() + aOffset;
    mRemaining = mLength - aOffset;
    return mPtr;
  }

  const uint8_t* Reset()
  {
    mPtr -= Offset();
    mRemaining = mLength;
    return mPtr;
  }

  uint32_t Align()
  {
    return 4 - ((intptr_t)mPtr & 3);
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
  MOZ_MUST_USE bool ReadArray(nsTArray<T>& aDest, size_t aLength)
  {
    auto ptr = Read(aLength * sizeof(T));
    if (!ptr) {
      return false;
    }

    aDest.Clear();
    aDest.AppendElements(reinterpret_cast<const T*>(ptr), aLength);
    return true;
  }

  template <typename T>
  MOZ_MUST_USE bool ReadArray(FallibleTArray<T>& aDest, size_t aLength)
  {
    auto ptr = Read(aLength * sizeof(T));
    if (!ptr) {
      return false;
    }

    aDest.Clear();
    if (!aDest.SetCapacity(aLength, mozilla::fallible)) {
      return false;
    }
    MOZ_ALWAYS_TRUE(aDest.AppendElements(reinterpret_cast<const T*>(ptr),
                                         aLength,
                                         mozilla::fallible));
    return true;
  }

private:
  const uint8_t* mPtr;
  size_t mRemaining;
  size_t mLength;
};

// A ByteReader that automatically discards remaining data before destruction.
class MOZ_RAII AutoByteReader : public ByteReader
{
public:
  AutoByteReader(const uint8_t* aData, size_t aSize)
    : ByteReader(aData, aSize)
  {
  }
  ~AutoByteReader()
  {
    ByteReader::DiscardRemaining();
  }

  // Prevent unneeded DiscardRemaining() calls.
  void DiscardRemaining() = delete;
};

} // namespace mp4_demuxer

#endif
