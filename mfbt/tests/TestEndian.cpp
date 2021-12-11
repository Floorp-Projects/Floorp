/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/EndianUtils.h"

#include <stddef.h>

using mozilla::BigEndian;
using mozilla::LittleEndian;
using mozilla::NativeEndian;

template <typename T>
void TestSingleSwap(T aValue, T aSwappedValue) {
#if MOZ_LITTLE_ENDIAN()
  MOZ_RELEASE_ASSERT(NativeEndian::swapToBigEndian(aValue) == aSwappedValue);
  MOZ_RELEASE_ASSERT(NativeEndian::swapFromBigEndian(aValue) == aSwappedValue);
  MOZ_RELEASE_ASSERT(NativeEndian::swapToNetworkOrder(aValue) == aSwappedValue);
  MOZ_RELEASE_ASSERT(NativeEndian::swapFromNetworkOrder(aValue) ==
                     aSwappedValue);
#else
  MOZ_RELEASE_ASSERT(NativeEndian::swapToLittleEndian(aValue) == aSwappedValue);
  MOZ_RELEASE_ASSERT(NativeEndian::swapFromLittleEndian(aValue) ==
                     aSwappedValue);
#endif
}

template <typename T>
void TestSingleNoSwap(T aValue, T aUnswappedValue) {
#if MOZ_LITTLE_ENDIAN()
  MOZ_RELEASE_ASSERT(NativeEndian::swapToLittleEndian(aValue) ==
                     aUnswappedValue);
  MOZ_RELEASE_ASSERT(NativeEndian::swapFromLittleEndian(aValue) ==
                     aUnswappedValue);
#else
  MOZ_RELEASE_ASSERT(NativeEndian::swapToBigEndian(aValue) == aUnswappedValue);
  MOZ_RELEASE_ASSERT(NativeEndian::swapFromBigEndian(aValue) ==
                     aUnswappedValue);
  MOZ_RELEASE_ASSERT(NativeEndian::swapToNetworkOrder(aValue) ==
                     aUnswappedValue);
  MOZ_RELEASE_ASSERT(NativeEndian::swapFromNetworkOrder(aValue) ==
                     aUnswappedValue);
#endif
}

// EndianUtils.h functions are declared as protected in a base class and
// then re-exported as public in public derived classes.  The
// standardese around explicit instantiation of templates is not clear
// in such cases.  Provide these wrappers to make things more explicit.
// For your own enlightenment, you may wish to peruse:
// http://gcc.gnu.org/bugzilla/show_bug.cgi?id=56152 and subsequently
// http://j.mp/XosS6S .
#define WRAP_COPYTO(NAME)                               \
  template <typename T>                                 \
  void NAME(void* aDst, const T* aSrc, size_t aCount) { \
    NativeEndian::NAME<T>(aDst, aSrc, aCount);          \
  }

WRAP_COPYTO(copyAndSwapToLittleEndian)
WRAP_COPYTO(copyAndSwapToBigEndian)
WRAP_COPYTO(copyAndSwapToNetworkOrder)

#define WRAP_COPYFROM(NAME)                             \
  template <typename T>                                 \
  void NAME(T* aDst, const void* aSrc, size_t aCount) { \
    NativeEndian::NAME<T>(aDst, aSrc, aCount);          \
  }

WRAP_COPYFROM(copyAndSwapFromLittleEndian)
WRAP_COPYFROM(copyAndSwapFromBigEndian)
WRAP_COPYFROM(copyAndSwapFromNetworkOrder)

#define WRAP_IN_PLACE(NAME)            \
  template <typename T>                \
  void NAME(T* aP, size_t aCount) {    \
    NativeEndian::NAME<T>(aP, aCount); \
  }
WRAP_IN_PLACE(swapToLittleEndianInPlace)
WRAP_IN_PLACE(swapFromLittleEndianInPlace)
WRAP_IN_PLACE(swapToBigEndianInPlace)
WRAP_IN_PLACE(swapFromBigEndianInPlace)
WRAP_IN_PLACE(swapToNetworkOrderInPlace)
WRAP_IN_PLACE(swapFromNetworkOrderInPlace)

enum SwapExpectation { Swap, NoSwap };

template <typename T, size_t Count>
void TestBulkSwapToSub(enum SwapExpectation aExpectSwap,
                       const T (&aValues)[Count],
                       void (*aSwapperFunc)(void*, const T*, size_t),
                       T (*aReaderFunc)(const void*)) {
  const size_t arraySize = 2 * Count;
  const size_t bufferSize = arraySize * sizeof(T);
  static uint8_t buffer[bufferSize];
  const uint8_t fillValue = 0xa5;
  static uint8_t checkBuffer[bufferSize];

  MOZ_RELEASE_ASSERT(bufferSize > 2 * sizeof(T));

  memset(checkBuffer, fillValue, bufferSize);

  for (size_t startPosition = 0; startPosition < sizeof(T); ++startPosition) {
    for (size_t nValues = 0; nValues < Count; ++nValues) {
      memset(buffer, fillValue, bufferSize);
      aSwapperFunc(buffer + startPosition, aValues, nValues);

      MOZ_RELEASE_ASSERT(memcmp(buffer, checkBuffer, startPosition) == 0);
      size_t valuesEndPosition = startPosition + sizeof(T) * nValues;
      MOZ_RELEASE_ASSERT(memcmp(buffer + valuesEndPosition,
                                checkBuffer + valuesEndPosition,
                                bufferSize - valuesEndPosition) == 0);
      if (aExpectSwap == NoSwap) {
        MOZ_RELEASE_ASSERT(
            memcmp(buffer + startPosition, aValues, nValues * sizeof(T)) == 0);
      }
      for (size_t i = 0; i < nValues; ++i) {
        MOZ_RELEASE_ASSERT(
            aReaderFunc(buffer + startPosition + sizeof(T) * i) == aValues[i]);
      }
    }
  }
}

template <typename T, size_t Count>
void TestBulkSwapFromSub(enum SwapExpectation aExpectSwap,
                         const T (&aValues)[Count],
                         void (*aSwapperFunc)(T*, const void*, size_t),
                         T (*aReaderFunc)(const void*)) {
  const size_t arraySize = 2 * Count;
  const size_t bufferSize = arraySize * sizeof(T);
  static T buffer[arraySize];
  const uint8_t fillValue = 0xa5;
  static T checkBuffer[arraySize];

  memset(checkBuffer, fillValue, bufferSize);

  for (size_t startPosition = 0; startPosition < Count; ++startPosition) {
    for (size_t nValues = 0; nValues < (Count - startPosition); ++nValues) {
      memset(buffer, fillValue, bufferSize);
      aSwapperFunc(buffer + startPosition, aValues, nValues);

      MOZ_RELEASE_ASSERT(
          memcmp(buffer, checkBuffer, startPosition * sizeof(T)) == 0);
      size_t valuesEndPosition = startPosition + nValues;
      MOZ_RELEASE_ASSERT(
          memcmp(buffer + valuesEndPosition, checkBuffer + valuesEndPosition,
                 (arraySize - valuesEndPosition) * sizeof(T)) == 0);
      if (aExpectSwap == NoSwap) {
        MOZ_RELEASE_ASSERT(
            memcmp(buffer + startPosition, aValues, nValues * sizeof(T)) == 0);
      }
      for (size_t i = 0; i < nValues; ++i) {
        MOZ_RELEASE_ASSERT(aReaderFunc(buffer + startPosition + i) ==
                           aValues[i]);
      }
    }
  }
}

template <typename T, size_t Count>
void TestBulkInPlaceSub(enum SwapExpectation aExpectSwap,
                        const T (&aValues)[Count],
                        void (*aSwapperFunc)(T*, size_t),
                        T (*aReaderFunc)(const void*)) {
  const size_t bufferCount = 4 * Count;
  const size_t bufferSize = bufferCount * sizeof(T);
  static T buffer[bufferCount];
  const T fillValue = 0xa5;
  static T checkBuffer[bufferCount];

  MOZ_RELEASE_ASSERT(bufferSize > 2 * sizeof(T));

  memset(checkBuffer, fillValue, bufferSize);

  for (size_t startPosition = 0; startPosition < Count; ++startPosition) {
    for (size_t nValues = 0; nValues < Count; ++nValues) {
      memset(buffer, fillValue, bufferSize);
      memcpy(buffer + startPosition, aValues, nValues * sizeof(T));
      aSwapperFunc(buffer + startPosition, nValues);

      MOZ_RELEASE_ASSERT(
          memcmp(buffer, checkBuffer, startPosition * sizeof(T)) == 0);
      size_t valuesEndPosition = startPosition + nValues;
      MOZ_RELEASE_ASSERT(
          memcmp(buffer + valuesEndPosition, checkBuffer + valuesEndPosition,
                 bufferSize - valuesEndPosition * sizeof(T)) == 0);
      if (aExpectSwap == NoSwap) {
        MOZ_RELEASE_ASSERT(
            memcmp(buffer + startPosition, aValues, nValues * sizeof(T)) == 0);
      }
      for (size_t i = 0; i < nValues; ++i) {
        MOZ_RELEASE_ASSERT(aReaderFunc(buffer + startPosition + i) ==
                           aValues[i]);
      }
    }
  }
}

template <typename T>
struct Reader {};

#define SPECIALIZE_READER(TYPE, READ_FUNC)                                     \
  template <>                                                                  \
  struct Reader<TYPE> {                                                        \
    static TYPE readLE(const void* aP) { return LittleEndian::READ_FUNC(aP); } \
    static TYPE readBE(const void* aP) { return BigEndian::READ_FUNC(aP); }    \
  };

SPECIALIZE_READER(uint16_t, readUint16)
SPECIALIZE_READER(uint32_t, readUint32)
SPECIALIZE_READER(uint64_t, readUint64)
SPECIALIZE_READER(int16_t, readInt16)
SPECIALIZE_READER(int32_t, readInt32)
SPECIALIZE_READER(int64_t, readInt64)

template <typename T, size_t Count>
void TestBulkSwap(const T (&aBytes)[Count]) {
#if MOZ_LITTLE_ENDIAN()
  TestBulkSwapToSub(Swap, aBytes, copyAndSwapToBigEndian<T>, Reader<T>::readBE);
  TestBulkSwapFromSub(Swap, aBytes, copyAndSwapFromBigEndian<T>,
                      Reader<T>::readBE);
  TestBulkSwapToSub(Swap, aBytes, copyAndSwapToNetworkOrder<T>,
                    Reader<T>::readBE);
  TestBulkSwapFromSub(Swap, aBytes, copyAndSwapFromNetworkOrder<T>,
                      Reader<T>::readBE);
#else
  TestBulkSwapToSub(Swap, aBytes, copyAndSwapToLittleEndian<T>,
                    Reader<T>::readLE);
  TestBulkSwapFromSub(Swap, aBytes, copyAndSwapFromLittleEndian<T>,
                      Reader<T>::readLE);
#endif
}

template <typename T, size_t Count>
void TestBulkNoSwap(const T (&aBytes)[Count]) {
#if MOZ_LITTLE_ENDIAN()
  TestBulkSwapToSub(NoSwap, aBytes, copyAndSwapToLittleEndian<T>,
                    Reader<T>::readLE);
  TestBulkSwapFromSub(NoSwap, aBytes, copyAndSwapFromLittleEndian<T>,
                      Reader<T>::readLE);
#else
  TestBulkSwapToSub(NoSwap, aBytes, copyAndSwapToBigEndian<T>,
                    Reader<T>::readBE);
  TestBulkSwapFromSub(NoSwap, aBytes, copyAndSwapFromBigEndian<T>,
                      Reader<T>::readBE);
  TestBulkSwapToSub(NoSwap, aBytes, copyAndSwapToNetworkOrder<T>,
                    Reader<T>::readBE);
  TestBulkSwapFromSub(NoSwap, aBytes, copyAndSwapFromNetworkOrder<T>,
                      Reader<T>::readBE);
#endif
}

template <typename T, size_t Count>
void TestBulkInPlaceSwap(const T (&aBytes)[Count]) {
#if MOZ_LITTLE_ENDIAN()
  TestBulkInPlaceSub(Swap, aBytes, swapToBigEndianInPlace<T>,
                     Reader<T>::readBE);
  TestBulkInPlaceSub(Swap, aBytes, swapFromBigEndianInPlace<T>,
                     Reader<T>::readBE);
  TestBulkInPlaceSub(Swap, aBytes, swapToNetworkOrderInPlace<T>,
                     Reader<T>::readBE);
  TestBulkInPlaceSub(Swap, aBytes, swapFromNetworkOrderInPlace<T>,
                     Reader<T>::readBE);
#else
  TestBulkInPlaceSub(Swap, aBytes, swapToLittleEndianInPlace<T>,
                     Reader<T>::readLE);
  TestBulkInPlaceSub(Swap, aBytes, swapFromLittleEndianInPlace<T>,
                     Reader<T>::readLE);
#endif
}

template <typename T, size_t Count>
void TestBulkInPlaceNoSwap(const T (&aBytes)[Count]) {
#if MOZ_LITTLE_ENDIAN()
  TestBulkInPlaceSub(NoSwap, aBytes, swapToLittleEndianInPlace<T>,
                     Reader<T>::readLE);
  TestBulkInPlaceSub(NoSwap, aBytes, swapFromLittleEndianInPlace<T>,
                     Reader<T>::readLE);
#else
  TestBulkInPlaceSub(NoSwap, aBytes, swapToBigEndianInPlace<T>,
                     Reader<T>::readBE);
  TestBulkInPlaceSub(NoSwap, aBytes, swapFromBigEndianInPlace<T>,
                     Reader<T>::readBE);
  TestBulkInPlaceSub(NoSwap, aBytes, swapToNetworkOrderInPlace<T>,
                     Reader<T>::readBE);
  TestBulkInPlaceSub(NoSwap, aBytes, swapFromNetworkOrderInPlace<T>,
                     Reader<T>::readBE);
#endif
}

int main() {
  static const uint8_t unsigned_bytes[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                             0x07, 0x08, 0x01, 0x02, 0x03, 0x04,
                                             0x05, 0x06, 0x07, 0x08};
  static const int8_t signed_bytes[16] = {
      -0x0f, -0x0e, -0x0d, -0x0c, -0x0b, -0x0a, -0x09, -0x08,
      -0x0f, -0x0e, -0x0d, -0x0c, -0x0b, -0x0a, -0x09, -0x08};
  static const uint16_t uint16_values[8] = {0x0102, 0x0304, 0x0506, 0x0708,
                                            0x0102, 0x0304, 0x0506, 0x0708};
  static const int16_t int16_values[8] = {
      int16_t(0xf1f2), int16_t(0xf3f4), int16_t(0xf5f6), int16_t(0xf7f8),
      int16_t(0xf1f2), int16_t(0xf3f4), int16_t(0xf5f6), int16_t(0xf7f8)};
  static const uint32_t uint32_values[4] = {0x01020304, 0x05060708, 0x01020304,
                                            0x05060708};
  static const int32_t int32_values[4] = {
      int32_t(0xf1f2f3f4), int32_t(0xf5f6f7f8), int32_t(0xf1f2f3f4),
      int32_t(0xf5f6f7f8)};
  static const uint64_t uint64_values[2] = {0x0102030405060708,
                                            0x0102030405060708};
  static const int64_t int64_values[2] = {int64_t(0xf1f2f3f4f5f6f7f8),
                                          int64_t(0xf1f2f3f4f5f6f7f8)};
  uint8_t buffer[8];

  MOZ_RELEASE_ASSERT(LittleEndian::readUint16(&unsigned_bytes[0]) == 0x0201);
  MOZ_RELEASE_ASSERT(BigEndian::readUint16(&unsigned_bytes[0]) == 0x0102);

  MOZ_RELEASE_ASSERT(LittleEndian::readUint32(&unsigned_bytes[0]) ==
                     0x04030201U);
  MOZ_RELEASE_ASSERT(BigEndian::readUint32(&unsigned_bytes[0]) == 0x01020304U);

  MOZ_RELEASE_ASSERT(LittleEndian::readUint64(&unsigned_bytes[0]) ==
                     0x0807060504030201ULL);
  MOZ_RELEASE_ASSERT(BigEndian::readUint64(&unsigned_bytes[0]) ==
                     0x0102030405060708ULL);

  if (sizeof(uintptr_t) == 8) {
    // MSVC warning C4309 is "'static_cast': truncation of constant value" and
    // will hit for the literal casts below in 32-bit builds -- in dead code,
    // because only the other arm of this |if| runs.  Turn off the warning for
    // these two uses in dead code.
#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4309)
#endif
    MOZ_RELEASE_ASSERT(LittleEndian::readUintptr(&unsigned_bytes[0]) ==
                       static_cast<uintptr_t>(0x0807060504030201ULL));
    MOZ_RELEASE_ASSERT(BigEndian::readUintptr(&unsigned_bytes[0]) ==
                       static_cast<uintptr_t>(0x0102030405060708ULL));
#ifdef _MSC_VER
#  pragma warning(pop)
#endif
  } else {
    MOZ_RELEASE_ASSERT(LittleEndian::readUintptr(&unsigned_bytes[0]) ==
                       0x04030201U);
    MOZ_RELEASE_ASSERT(BigEndian::readUintptr(&unsigned_bytes[0]) ==
                       0x01020304U);
  }

  LittleEndian::writeUint16(&buffer[0], 0x0201);
  MOZ_RELEASE_ASSERT(memcmp(&unsigned_bytes[0], &buffer[0], sizeof(uint16_t)) ==
                     0);
  BigEndian::writeUint16(&buffer[0], 0x0102);
  MOZ_RELEASE_ASSERT(memcmp(&unsigned_bytes[0], &buffer[0], sizeof(uint16_t)) ==
                     0);

  LittleEndian::writeUint32(&buffer[0], 0x04030201U);
  MOZ_RELEASE_ASSERT(memcmp(&unsigned_bytes[0], &buffer[0], sizeof(uint32_t)) ==
                     0);
  BigEndian::writeUint32(&buffer[0], 0x01020304U);
  MOZ_RELEASE_ASSERT(memcmp(&unsigned_bytes[0], &buffer[0], sizeof(uint32_t)) ==
                     0);

  LittleEndian::writeUint64(&buffer[0], 0x0807060504030201ULL);
  MOZ_RELEASE_ASSERT(memcmp(&unsigned_bytes[0], &buffer[0], sizeof(uint64_t)) ==
                     0);
  BigEndian::writeUint64(&buffer[0], 0x0102030405060708ULL);
  MOZ_RELEASE_ASSERT(memcmp(&unsigned_bytes[0], &buffer[0], sizeof(uint64_t)) ==
                     0);

  memset(&buffer[0], 0xff, sizeof(buffer));
  LittleEndian::writeUintptr(&buffer[0], uintptr_t(0x0807060504030201ULL));
  MOZ_RELEASE_ASSERT(
      memcmp(&unsigned_bytes[0], &buffer[0], sizeof(uintptr_t)) == 0);
  if (sizeof(uintptr_t) == 4) {
    MOZ_RELEASE_ASSERT(LittleEndian::readUint32(&buffer[4]) == 0xffffffffU);
  }

  memset(&buffer[0], 0xff, sizeof(buffer));
  if (sizeof(uintptr_t) == 8) {
    BigEndian::writeUintptr(&buffer[0], uintptr_t(0x0102030405060708ULL));
  } else {
    BigEndian::writeUintptr(&buffer[0], uintptr_t(0x01020304U));
    MOZ_RELEASE_ASSERT(LittleEndian::readUint32(&buffer[4]) == 0xffffffffU);
  }
  MOZ_RELEASE_ASSERT(
      memcmp(&unsigned_bytes[0], &buffer[0], sizeof(uintptr_t)) == 0);

  MOZ_RELEASE_ASSERT(LittleEndian::readInt16(&signed_bytes[0]) ==
                     int16_t(0xf2f1));
  MOZ_RELEASE_ASSERT(BigEndian::readInt16(&signed_bytes[0]) == int16_t(0xf1f2));

  MOZ_RELEASE_ASSERT(LittleEndian::readInt32(&signed_bytes[0]) ==
                     int32_t(0xf4f3f2f1));
  MOZ_RELEASE_ASSERT(BigEndian::readInt32(&signed_bytes[0]) ==
                     int32_t(0xf1f2f3f4));

  MOZ_RELEASE_ASSERT(LittleEndian::readInt64(&signed_bytes[0]) ==
                     int64_t(0xf8f7f6f5f4f3f2f1LL));
  MOZ_RELEASE_ASSERT(BigEndian::readInt64(&signed_bytes[0]) ==
                     int64_t(0xf1f2f3f4f5f6f7f8LL));

  if (sizeof(uintptr_t) == 8) {
    MOZ_RELEASE_ASSERT(LittleEndian::readIntptr(&signed_bytes[0]) ==
                       intptr_t(0xf8f7f6f5f4f3f2f1LL));
    MOZ_RELEASE_ASSERT(BigEndian::readIntptr(&signed_bytes[0]) ==
                       intptr_t(0xf1f2f3f4f5f6f7f8LL));
  } else {
    MOZ_RELEASE_ASSERT(LittleEndian::readIntptr(&signed_bytes[0]) ==
                       intptr_t(0xf4f3f2f1));
    MOZ_RELEASE_ASSERT(BigEndian::readIntptr(&signed_bytes[0]) ==
                       intptr_t(0xf1f2f3f4));
  }

  LittleEndian::writeInt16(&buffer[0], int16_t(0xf2f1));
  MOZ_RELEASE_ASSERT(memcmp(&signed_bytes[0], &buffer[0], sizeof(int16_t)) ==
                     0);
  BigEndian::writeInt16(&buffer[0], int16_t(0xf1f2));
  MOZ_RELEASE_ASSERT(memcmp(&signed_bytes[0], &buffer[0], sizeof(int16_t)) ==
                     0);

  LittleEndian::writeInt32(&buffer[0], 0xf4f3f2f1);
  MOZ_RELEASE_ASSERT(memcmp(&signed_bytes[0], &buffer[0], sizeof(int32_t)) ==
                     0);
  BigEndian::writeInt32(&buffer[0], 0xf1f2f3f4);
  MOZ_RELEASE_ASSERT(memcmp(&signed_bytes[0], &buffer[0], sizeof(int32_t)) ==
                     0);

  LittleEndian::writeInt64(&buffer[0], 0xf8f7f6f5f4f3f2f1LL);
  MOZ_RELEASE_ASSERT(memcmp(&signed_bytes[0], &buffer[0], sizeof(int64_t)) ==
                     0);
  BigEndian::writeInt64(&buffer[0], 0xf1f2f3f4f5f6f7f8LL);
  MOZ_RELEASE_ASSERT(memcmp(&signed_bytes[0], &buffer[0], sizeof(int64_t)) ==
                     0);

  memset(&buffer[0], 0xff, sizeof(buffer));
  LittleEndian::writeIntptr(&buffer[0], intptr_t(0xf8f7f6f5f4f3f2f1LL));
  MOZ_RELEASE_ASSERT(memcmp(&signed_bytes[0], &buffer[0], sizeof(intptr_t)) ==
                     0);
  if (sizeof(intptr_t) == 4) {
    MOZ_RELEASE_ASSERT(LittleEndian::readUint32(&buffer[4]) == 0xffffffffU);
  }

  memset(&buffer[0], 0xff, sizeof(buffer));
  if (sizeof(intptr_t) == 8) {
    BigEndian::writeIntptr(&buffer[0], intptr_t(0xf1f2f3f4f5f6f7f8LL));
  } else {
    BigEndian::writeIntptr(&buffer[0], intptr_t(0xf1f2f3f4));
    MOZ_RELEASE_ASSERT(LittleEndian::readUint32(&buffer[4]) == 0xffffffffU);
  }
  MOZ_RELEASE_ASSERT(memcmp(&signed_bytes[0], &buffer[0], sizeof(intptr_t)) ==
                     0);

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
