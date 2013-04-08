/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Endian.h"

using mozilla::BigEndian;
using mozilla::DebugOnly;
using mozilla::LittleEndian;
using mozilla::NativeEndian;

template<typename T>
void
TestSingleSwap(T value, T swappedValue)
{
#if MOZ_LITTLE_ENDIAN
  MOZ_ASSERT(NativeEndian::swapToBigEndian(value) == swappedValue);
  MOZ_ASSERT(NativeEndian::swapFromBigEndian(value) == swappedValue);
  MOZ_ASSERT(NativeEndian::swapToNetworkOrder(value) == swappedValue);
  MOZ_ASSERT(NativeEndian::swapFromNetworkOrder(value) == swappedValue);
#else
  MOZ_ASSERT(NativeEndian::swapToLittleEndian(value) == swappedValue);
  MOZ_ASSERT(NativeEndian::swapFromLittleEndian(value) == swappedValue);
#endif
}

template<typename T>
void
TestSingleNoSwap(T value, T notSwappedValue)
{
#if MOZ_LITTLE_ENDIAN
  MOZ_ASSERT(NativeEndian::swapToLittleEndian(value) == notSwappedValue);
  MOZ_ASSERT(NativeEndian::swapFromLittleEndian(value) == notSwappedValue);
#else
  MOZ_ASSERT(NativeEndian::swapToBigEndian(value) == notSwappedValue);
  MOZ_ASSERT(NativeEndian::swapFromBigEndian(value) == notSwappedValue);
  MOZ_ASSERT(NativeEndian::swapToNetworkOrder(value) == notSwappedValue);
  MOZ_ASSERT(NativeEndian::swapFromNetworkOrder(value) == notSwappedValue);
#endif
}

// Endian.h functions are declared as protected in an base class and
// then re-exported as public in public derived classes.  The
// standardese around explicit instantiation of templates is not clear
// in such cases.  Provide these wrappers to make things more explicit.
// For your own enlightenment, you may wish to peruse:
// http://gcc.gnu.org/bugzilla/show_bug.cgi?id=56152 and subsequently
// http://j.mp/XosS6S .
#define WRAP_COPYTO(NAME)                                       \
  template<typename T>                                          \
  void                                                          \
  NAME(void* dst, const T* src, unsigned int count)             \
  {                                                             \
    NativeEndian::NAME<T>(dst, src, count);                     \
  }

WRAP_COPYTO(copyAndSwapToLittleEndian)
WRAP_COPYTO(copyAndSwapToBigEndian)
WRAP_COPYTO(copyAndSwapToNetworkOrder)

#define WRAP_COPYFROM(NAME)                                     \
  template<typename T>                                          \
  void                                                          \
  NAME(T* dst, const void* src, unsigned int count)             \
  {                                                             \
    NativeEndian::NAME<T>(dst, src, count);                     \
  }

WRAP_COPYFROM(copyAndSwapFromLittleEndian)
WRAP_COPYFROM(copyAndSwapFromBigEndian)
WRAP_COPYFROM(copyAndSwapFromNetworkOrder)

#define WRAP_IN_PLACE(NAME)                                     \
  template<typename T>                                          \
  void                                                          \
  NAME(T* p, unsigned int count)                                \
  {                                                             \
    NativeEndian::NAME<T>(p, count);                            \
  }
WRAP_IN_PLACE(swapToLittleEndianInPlace)
WRAP_IN_PLACE(swapFromLittleEndianInPlace)
WRAP_IN_PLACE(swapToBigEndianInPlace)
WRAP_IN_PLACE(swapFromBigEndianInPlace)
WRAP_IN_PLACE(swapToNetworkOrderInPlace)
WRAP_IN_PLACE(swapFromNetworkOrderInPlace)

enum SwapExpectation {
  Swap,
  NoSwap
};

template<typename T, size_t count>
void
TestBulkSwapToSub(enum SwapExpectation expectSwap,
                  const T (&values)[count],
                  void (*swapperFunc)(void*, const T*, unsigned int),
                  T (*readerFunc)(const void*))
{
  const size_t arraySize = 2 * count;
  const size_t bufferSize = arraySize * sizeof(T);
  static uint8_t buffer[bufferSize];
  const uint8_t fillValue = 0xa5;
  static uint8_t checkBuffer[bufferSize];

  MOZ_ASSERT(bufferSize > 2 * sizeof(T));

  memset(checkBuffer, fillValue, bufferSize);

  for (size_t startPosition = 0; startPosition < sizeof(T); ++startPosition) {
    for (size_t nValues = 0; nValues < count; ++nValues) {
      memset(buffer, fillValue, bufferSize);
      swapperFunc(buffer + startPosition, values, nValues);

      MOZ_ASSERT(memcmp(buffer, checkBuffer, startPosition) == 0);
      DebugOnly<size_t> valuesEndPosition = startPosition + sizeof(T) * nValues;
      MOZ_ASSERT(memcmp(buffer + valuesEndPosition,
                        checkBuffer + valuesEndPosition,
                        bufferSize - valuesEndPosition) == 0);
      if (expectSwap == NoSwap) {
        MOZ_ASSERT(memcmp(buffer + startPosition, values,
                          nValues * sizeof(T)) == 0);
      }
      for (size_t i = 0; i < nValues; ++i) {
        MOZ_ASSERT(readerFunc(buffer + startPosition + sizeof(T) * i) ==
                   values[i]);
      }
    }
  }
}

template<typename T, size_t count>
void
TestBulkSwapFromSub(enum SwapExpectation expectSwap,
                    const T (&values)[count],
                    void (*swapperFunc)(T*, const void*, unsigned int),
                    T (*readerFunc)(const void*))
{
  const size_t arraySize = 2 * count;
  const size_t bufferSize = arraySize * sizeof(T);
  static T buffer[arraySize];
  const uint8_t fillValue = 0xa5;
  static T checkBuffer[arraySize];

  memset(checkBuffer, fillValue, bufferSize);

  for (size_t startPosition = 0; startPosition < count; ++startPosition) {
    for (size_t nValues = 0; nValues < (count - startPosition); ++nValues) {
      memset(buffer, fillValue, bufferSize);
      swapperFunc(buffer + startPosition, values, nValues);

      MOZ_ASSERT(memcmp(buffer, checkBuffer, startPosition * sizeof(T)) == 0);
      DebugOnly<size_t> valuesEndPosition = startPosition + nValues;
      MOZ_ASSERT(memcmp(buffer + valuesEndPosition,
                        checkBuffer + valuesEndPosition,
                        (arraySize - valuesEndPosition) * sizeof(T)) == 0);
      if (expectSwap == NoSwap) {
        MOZ_ASSERT(memcmp(buffer + startPosition, values,
                          nValues * sizeof(T)) == 0);
      }
      for (size_t i = 0; i < nValues; ++i)
        MOZ_ASSERT(readerFunc(buffer + startPosition + i) == values[i]);
    }
  }
}


template<typename T, size_t count>
void
TestBulkInPlaceSub(enum SwapExpectation expectSwap,
                   const T (&values)[count],
                   void (*swapperFunc)(T* p, unsigned int),
                   T (*readerFunc)(const void*))
{
  const size_t bufferCount = 4 * count;
  const size_t bufferSize = bufferCount * sizeof(T);
  static T buffer[bufferCount];
  const T fillValue = 0xa5;
  static T checkBuffer[bufferCount];

  MOZ_ASSERT(bufferSize > 2 * sizeof(T));

  memset(checkBuffer, fillValue, bufferSize);

  for (size_t startPosition = 0; startPosition < count; ++startPosition) {
    for (size_t nValues = 0; nValues < count; ++nValues) {
      memset(buffer, fillValue, bufferSize);
      memcpy(buffer + startPosition, values, nValues * sizeof(T));
      swapperFunc(buffer + startPosition, nValues);

      MOZ_ASSERT(memcmp(buffer, checkBuffer, startPosition * sizeof(T)) == 0);
      DebugOnly<size_t> valuesEndPosition = startPosition + nValues;
      MOZ_ASSERT(memcmp(buffer + valuesEndPosition,
                        checkBuffer + valuesEndPosition,
                        bufferSize - valuesEndPosition * sizeof(T)) == 0);
      if (expectSwap == NoSwap) {
        MOZ_ASSERT(memcmp(buffer + startPosition, values,
                          nValues * sizeof(T)) == 0);
      }
      for (size_t i = 0; i < nValues; ++i)
        MOZ_ASSERT(readerFunc(buffer + startPosition + i) == values[i]);
    }
  }
}

template<typename T>
struct Reader
{
};

#define SPECIALIZE_READER(TYPE, READ_FUNC)                              \
  template<>                                                            \
  struct Reader<TYPE>                                                   \
  {                                                                     \
    static TYPE readLE(const void* p) { return LittleEndian::READ_FUNC(p); }    \
    static TYPE readBE(const void* p) { return BigEndian::READ_FUNC(p); } \
  };

SPECIALIZE_READER(uint16_t, readUint16)
SPECIALIZE_READER(uint32_t, readUint32)
SPECIALIZE_READER(uint64_t, readUint64)
SPECIALIZE_READER(int16_t, readInt16)
SPECIALIZE_READER(int32_t, readInt32)
SPECIALIZE_READER(int64_t, readInt64)

template<typename T, size_t count>
void
TestBulkSwap(const T (&bytes)[count])
{
#if MOZ_LITTLE_ENDIAN
  TestBulkSwapToSub(Swap, bytes, copyAndSwapToBigEndian<T>, Reader<T>::readBE);
  TestBulkSwapFromSub(Swap, bytes, copyAndSwapFromBigEndian<T>, Reader<T>::readBE);
  TestBulkSwapToSub(Swap, bytes, copyAndSwapToNetworkOrder<T>, Reader<T>::readBE);
  TestBulkSwapFromSub(Swap, bytes, copyAndSwapFromNetworkOrder<T>, Reader<T>::readBE);
#else
  TestBulkSwapToSub(Swap, bytes, copyAndSwapToLittleEndian<T>, Reader<T>::readLE);
  TestBulkSwapFromSub(Swap, bytes, copyAndSwapFromLittleEndian<T>, Reader<T>::readLE);
#endif
}

template<typename T, size_t count>
void
TestBulkNoSwap(const T (&bytes)[count])
{
#if MOZ_LITTLE_ENDIAN
  TestBulkSwapToSub(NoSwap, bytes, copyAndSwapToLittleEndian<T>, Reader<T>::readLE);
  TestBulkSwapFromSub(NoSwap, bytes, copyAndSwapFromLittleEndian<T>, Reader<T>::readLE);
#else
  TestBulkSwapToSub(NoSwap, bytes, copyAndSwapToBigEndian<T>, Reader<T>::readBE);
  TestBulkSwapFromSub(NoSwap, bytes, copyAndSwapFromBigEndian<T>, Reader<T>::readBE);
  TestBulkSwapToSub(NoSwap, bytes, copyAndSwapToNetworkOrder<T>, Reader<T>::readBE);
  TestBulkSwapFromSub(NoSwap, bytes, copyAndSwapFromNetworkOrder<T>, Reader<T>::readBE);
#endif
}

template<typename T, size_t count>
void
TestBulkInPlaceSwap(const T (&bytes)[count])
{
#if MOZ_LITTLE_ENDIAN
  TestBulkInPlaceSub(Swap, bytes, swapToBigEndianInPlace<T>, Reader<T>::readBE);
  TestBulkInPlaceSub(Swap, bytes, swapFromBigEndianInPlace<T>, Reader<T>::readBE);
  TestBulkInPlaceSub(Swap, bytes, swapToNetworkOrderInPlace<T>, Reader<T>::readBE);
  TestBulkInPlaceSub(Swap, bytes, swapFromNetworkOrderInPlace<T>, Reader<T>::readBE);
#else
  TestBulkInPlaceSub(Swap, bytes, swapToLittleEndianInPlace<T>, Reader<T>::readLE);
  TestBulkInPlaceSub(Swap, bytes, swapFromLittleEndianInPlace<T>, Reader<T>::readLE);
#endif
}

template<typename T, size_t count>
void
TestBulkInPlaceNoSwap(const T (&bytes)[count])
{
#if MOZ_LITTLE_ENDIAN
  TestBulkInPlaceSub(NoSwap, bytes, swapToLittleEndianInPlace<T>, Reader<T>::readLE);
  TestBulkInPlaceSub(NoSwap, bytes, swapFromLittleEndianInPlace<T>, Reader<T>::readLE);
#else
  TestBulkInPlaceSub(NoSwap, bytes, swapToBigEndianInPlace<T>, Reader<T>::readBE);
  TestBulkInPlaceSub(NoSwap, bytes, swapFromBigEndianInPlace<T>, Reader<T>::readBE);
  TestBulkInPlaceSub(NoSwap, bytes, swapToNetworkOrderInPlace<T>, Reader<T>::readBE);
  TestBulkInPlaceSub(NoSwap, bytes, swapFromNetworkOrderInPlace<T>, Reader<T>::readBE);
#endif
}

int
main()
{
  static const uint8_t unsigned_bytes[16] = { 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8,
                                              0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8 };
  static const int8_t signed_bytes[16] = { -0x0f, -0x0e, -0x0d, -0x0c, -0x0b, -0x0a, -0x09, -0x08,
                                           -0x0f, -0x0e, -0x0d, -0x0c, -0x0b, -0x0a, -0x09, -0x08 };
  static const uint16_t uint16_values[8] = { 0x102, 0x304, 0x506, 0x708, 0x102, 0x304, 0x506, 0x708 };
  static const int16_t int16_values[8] = { int16_t(0xf1f2), int16_t(0xf3f4), int16_t(0xf5f6), int16_t(0xf7f8),
                                           int16_t(0xf1f2), int16_t(0xf3f4), int16_t(0xf5f6), int16_t(0xf7f8) };
  static const uint32_t uint32_values[4] = { 0x1020304, 0x5060708, 0x1020304, 0x5060708 };
  static const int32_t int32_values[4] = { int32_t(0xf1f2f3f4), int32_t(0xf5f6f7f8),
                                           int32_t(0xf1f2f3f4), int32_t(0xf5f6f7f8) };
  static const uint64_t uint64_values[2] = { 0x102030405060708, 0x102030405060708 };
  static const int64_t int64_values[2] = { int64_t(0xf1f2f3f4f5f6f7f8),
                                           int64_t(0xf1f2f3f4f5f6f7f8) };
  uint8_t buffer[8];

  MOZ_ASSERT(LittleEndian::readUint16(&unsigned_bytes[0]) == 0x201);
  MOZ_ASSERT(BigEndian::readUint16(&unsigned_bytes[0]) == 0x102);

  MOZ_ASSERT(LittleEndian::readUint32(&unsigned_bytes[0]) == 0x4030201U);
  MOZ_ASSERT(BigEndian::readUint32(&unsigned_bytes[0]) == 0x1020304U);

  MOZ_ASSERT(LittleEndian::readUint64(&unsigned_bytes[0]) == 0x807060504030201ULL);
  MOZ_ASSERT(BigEndian::readUint64(&unsigned_bytes[0]) == 0x102030405060708ULL);

  LittleEndian::writeUint16(&buffer[0], 0x201);
  MOZ_ASSERT(memcmp(&unsigned_bytes[0], &buffer[0], sizeof(uint16_t)) == 0);
  BigEndian::writeUint16(&buffer[0], 0x102);
  MOZ_ASSERT(memcmp(&unsigned_bytes[0], &buffer[0], sizeof(uint16_t)) == 0);

  LittleEndian::writeUint32(&buffer[0], 0x4030201U);
  MOZ_ASSERT(memcmp(&unsigned_bytes[0], &buffer[0], sizeof(uint32_t)) == 0);
  BigEndian::writeUint32(&buffer[0], 0x1020304U);
  MOZ_ASSERT(memcmp(&unsigned_bytes[0], &buffer[0], sizeof(uint32_t)) == 0);

  LittleEndian::writeUint64(&buffer[0], 0x807060504030201ULL);
  MOZ_ASSERT(memcmp(&unsigned_bytes[0], &buffer[0], sizeof(uint64_t)) == 0);
  BigEndian::writeUint64(&buffer[0], 0x102030405060708ULL);
  MOZ_ASSERT(memcmp(&unsigned_bytes[0], &buffer[0], sizeof(uint64_t)) == 0);

  MOZ_ASSERT(LittleEndian::readInt16(&signed_bytes[0]) == int16_t(0xf2f1));
  MOZ_ASSERT(BigEndian::readInt16(&signed_bytes[0]) == int16_t(0xf1f2));

  MOZ_ASSERT(LittleEndian::readInt32(&signed_bytes[0]) == int32_t(0xf4f3f2f1));
  MOZ_ASSERT(BigEndian::readInt32(&signed_bytes[0]) == int32_t(0xf1f2f3f4));

  MOZ_ASSERT(LittleEndian::readInt64(&signed_bytes[0]) == int64_t(0xf8f7f6f5f4f3f2f1LL));
  MOZ_ASSERT(BigEndian::readInt64(&signed_bytes[0]) == int64_t(0xf1f2f3f4f5f6f7f8LL));

  LittleEndian::writeInt16(&buffer[0], 0xf2f1);
  MOZ_ASSERT(memcmp(&signed_bytes[0], &buffer[0], sizeof(int16_t)) == 0);
  BigEndian::writeInt16(&buffer[0], 0xf1f2);
  MOZ_ASSERT(memcmp(&signed_bytes[0], &buffer[0], sizeof(int16_t)) == 0);

  LittleEndian::writeInt32(&buffer[0], 0xf4f3f2f1);
  MOZ_ASSERT(memcmp(&signed_bytes[0], &buffer[0], sizeof(int32_t)) == 0);
  BigEndian::writeInt32(&buffer[0], 0xf1f2f3f4);
  MOZ_ASSERT(memcmp(&signed_bytes[0], &buffer[0], sizeof(int32_t)) == 0);

  LittleEndian::writeInt64(&buffer[0], 0xf8f7f6f5f4f3f2f1LL);
  MOZ_ASSERT(memcmp(&signed_bytes[0], &buffer[0], sizeof(int64_t)) == 0);
  BigEndian::writeInt64(&buffer[0], 0xf1f2f3f4f5f6f7f8LL);
  MOZ_ASSERT(memcmp(&signed_bytes[0], &buffer[0], sizeof(int64_t)) == 0);

  TestSingleSwap(uint16_t(0xf2f1), uint16_t(0xf1f2));
  TestSingleSwap(uint32_t(0xf4f3f2f1), uint32_t(0xf1f2f3f4));
  TestSingleSwap(uint64_t(0xf8f7f6f5f4f3f2f1), uint64_t(0xf1f2f3f4f5f6f7f8));

  TestSingleSwap(int16_t(0xf2f1), int16_t(0xf1f2));
  TestSingleSwap(int32_t(0xf4f3f2f1), int32_t(0xf1f2f3f4));
  TestSingleSwap(int64_t(0xf8f7f6f5f4f3f2f1), int64_t(0xf1f2f3f4f5f6f7f8));

  TestSingleNoSwap(uint16_t(0xf2f1), uint16_t(0xf2f1));
  TestSingleNoSwap(uint32_t(0xf4f3f2f1), uint32_t(0xf4f3f2f1));
  TestSingleNoSwap(uint64_t(0xf8f7f6f5f4f3f2f1), uint64_t(0xf8f7f6f5f4f3f2f1));

  TestSingleNoSwap(int16_t(0xf2f1), int16_t(0xf2f1));
  TestSingleNoSwap(int32_t(0xf4f3f2f1), int32_t(0xf4f3f2f1));
  TestSingleNoSwap(int64_t(0xf8f7f6f5f4f3f2f1), int64_t(0xf8f7f6f5f4f3f2f1));

  TestBulkSwap(uint16_values);
  TestBulkSwap(int16_values);
  TestBulkSwap(uint32_values);
  TestBulkSwap(int32_values);
  TestBulkSwap(uint64_values);
  TestBulkSwap(int64_values);

  TestBulkNoSwap(uint16_values);
  TestBulkNoSwap(int16_values);
  TestBulkNoSwap(uint32_values);
  TestBulkNoSwap(int32_values);
  TestBulkNoSwap(uint64_values);
  TestBulkNoSwap(int64_values);

  TestBulkInPlaceSwap(uint16_values);
  TestBulkInPlaceSwap(int16_values);
  TestBulkInPlaceSwap(uint32_values);
  TestBulkInPlaceSwap(int32_values);
  TestBulkInPlaceSwap(uint64_values);
  TestBulkInPlaceSwap(int64_values);

  TestBulkInPlaceNoSwap(uint16_values);
  TestBulkInPlaceNoSwap(int16_values);
  TestBulkInPlaceNoSwap(uint32_values);
  TestBulkInPlaceNoSwap(int32_values);
  TestBulkInPlaceNoSwap(uint64_values);
  TestBulkInPlaceNoSwap(int64_values);

  return 0;
}
