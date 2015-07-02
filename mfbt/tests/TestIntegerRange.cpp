/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/IntegerRange.h"

#include <stddef.h>

using mozilla::IsSame;
using mozilla::MakeRange;
using mozilla::Reversed;

const size_t kMaxNumber = 50;
const size_t kArraySize = 256;

template<typename IntType>
static IntType
GenerateNumber()
{
  return static_cast<IntType>(rand() % kMaxNumber + 1);
}

template<typename IntType>
static void
TestSingleParamRange(const IntType aN)
{
  IntType array[kArraySize];
  IntType* ptr = array;
  for (auto i : MakeRange(aN)) {
    static_assert(IsSame<decltype(i), IntType>::value,
                  "type of the loop var and the param should be the same");
    *ptr++ = i;
  }

  MOZ_RELEASE_ASSERT(ptr - array == static_cast<ptrdiff_t>(aN),
                     "Should iterates N items");
  for (size_t i = 0; i < static_cast<size_t>(aN); i++) {
    MOZ_RELEASE_ASSERT(array[i] == static_cast<IntType>(i),
                       "Values should equal to the index");
  }
}

template<typename IntType>
static void
TestSingleParamReverseRange(const IntType aN)
{
  IntType array[kArraySize];
  IntType* ptr = array;
  for (auto i : Reversed(MakeRange(aN))) {
    static_assert(IsSame<decltype(i), IntType>::value,
                  "type of the loop var and the param should be the same");
    *ptr++ = i;
  }

  MOZ_RELEASE_ASSERT(ptr - array == static_cast<ptrdiff_t>(aN),
                     "Should iterates N items");
  for (size_t i = 0; i < static_cast<size_t>(aN); i++) {
    MOZ_RELEASE_ASSERT(array[i] == static_cast<IntType>(aN - i - 1),
                       "Values should be the reverse of their index");
  }
}

template<typename IntType>
static void
TestSingleParamIntegerRange()
{
  const auto kN = GenerateNumber<IntType>();
  TestSingleParamRange<IntType>(0);
  TestSingleParamReverseRange<IntType>(0);
  TestSingleParamRange<IntType>(kN);
  TestSingleParamReverseRange<IntType>(kN);
}

template<typename IntType1, typename IntType2>
static void
TestDoubleParamRange(const IntType1 aBegin, const IntType2 aEnd)
{
  IntType2 array[kArraySize];
  IntType2* ptr = array;
  for (auto i : MakeRange(aBegin, aEnd)) {
    static_assert(IsSame<decltype(i), IntType2>::value, "type of the loop var "
                  "should be same as that of the second param");
    *ptr++ = i;
  }

  MOZ_RELEASE_ASSERT(ptr - array == static_cast<ptrdiff_t>(aEnd - aBegin),
                     "Should iterates (aEnd - aBegin) times");
  for (size_t i = 0; i < static_cast<size_t>(aEnd - aBegin); i++) {
    MOZ_RELEASE_ASSERT(array[i] == static_cast<IntType2>(aBegin + i),
                       "Should iterate integers in [aBegin, aEnd) in order");
  }
}

template<typename IntType1, typename IntType2>
static void
TestDoubleParamReverseRange(const IntType1 aBegin, const IntType2 aEnd)
{
  IntType2 array[kArraySize];
  IntType2* ptr = array;
  for (auto i : Reversed(MakeRange(aBegin, aEnd))) {
    static_assert(IsSame<decltype(i), IntType2>::value, "type of the loop var "
                  "should be same as that of the second param");
    *ptr++ = i;
  }

  MOZ_RELEASE_ASSERT(ptr - array == static_cast<ptrdiff_t>(aEnd - aBegin),
                     "Should iterates (aEnd - aBegin) times");
  for (size_t i = 0; i < static_cast<size_t>(aEnd - aBegin); i++) {
    MOZ_RELEASE_ASSERT(array[i] == static_cast<IntType2>(aEnd - i - 1),
                       "Should iterate integers in [aBegin, aEnd) in reverse order");
  }
}

template<typename IntType1, typename IntType2>
static void
TestDoubleParamIntegerRange()
{
  const auto kStart = GenerateNumber<IntType1>();
  const auto kEnd = static_cast<IntType2>(kStart + GenerateNumber<IntType2>());
  TestDoubleParamRange(kStart, static_cast<IntType2>(kStart));
  TestDoubleParamReverseRange(kStart, static_cast<IntType2>(kStart));
  TestDoubleParamRange(kStart, kEnd);
  TestDoubleParamReverseRange(kStart, kEnd);
}

int
main()
{
  TestSingleParamIntegerRange<int8_t>();
  TestSingleParamIntegerRange<int16_t>();
  TestSingleParamIntegerRange<int32_t>();
  TestSingleParamIntegerRange<int64_t>();

  TestSingleParamIntegerRange<uint8_t>();
  TestSingleParamIntegerRange<uint16_t>();
  TestSingleParamIntegerRange<uint32_t>();
  TestSingleParamIntegerRange<uint64_t>();

  TestDoubleParamIntegerRange<int8_t, int8_t>();
  TestDoubleParamIntegerRange<int16_t, int16_t>();
  TestDoubleParamIntegerRange<int32_t, int32_t>();
  TestDoubleParamIntegerRange<int64_t, int64_t>();

  TestDoubleParamIntegerRange<uint8_t, uint8_t>();
  TestDoubleParamIntegerRange<uint16_t, uint16_t>();
  TestDoubleParamIntegerRange<uint32_t, uint32_t>();
  TestDoubleParamIntegerRange<uint64_t, uint64_t>();

  TestDoubleParamIntegerRange<int8_t, int16_t>();
  TestDoubleParamIntegerRange<int16_t, int32_t>();
  TestDoubleParamIntegerRange<int32_t, int64_t>();
  TestDoubleParamIntegerRange<int64_t, int8_t>();

  TestDoubleParamIntegerRange<uint8_t, uint64_t>();
  TestDoubleParamIntegerRange<uint16_t, uint8_t>();
  TestDoubleParamIntegerRange<uint32_t, uint16_t>();
  TestDoubleParamIntegerRange<uint64_t, uint32_t>();

  return 0;
}
