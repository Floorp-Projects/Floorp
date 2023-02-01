/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Casting.h"
#include "mozilla/ThreadSafety.h"

#include <stdint.h>
#include <cstdint>
#include <limits>
#include <type_traits>

using mozilla::AssertedCast;
using mozilla::BitwiseCast;
using mozilla::detail::IsInBounds;

static const uint8_t floatMantissaBitsPlusOne = 24;
static const uint8_t doubleMantissaBitsPlusOne = 53;

template <typename Uint, typename Ulong, bool = (sizeof(Uint) == sizeof(Ulong))>
struct UintUlongBitwiseCast;

template <typename Uint, typename Ulong>
struct UintUlongBitwiseCast<Uint, Ulong, true> {
  static void test() {
    MOZ_RELEASE_ASSERT(BitwiseCast<Ulong>(Uint(8675309)) == Ulong(8675309));
  }
};

template <typename Uint, typename Ulong>
struct UintUlongBitwiseCast<Uint, Ulong, false> {
  static void test() {}
};

static void TestBitwiseCast() {
  MOZ_RELEASE_ASSERT(BitwiseCast<int>(int(8675309)) == int(8675309));
  UintUlongBitwiseCast<unsigned int, unsigned long>::test();
}

static void TestSameSize() {
  MOZ_RELEASE_ASSERT((IsInBounds<int16_t, int16_t>(int16_t(0))));
  MOZ_RELEASE_ASSERT((IsInBounds<int16_t, int16_t>(int16_t(INT16_MIN))));
  MOZ_RELEASE_ASSERT((IsInBounds<int16_t, int16_t>(int16_t(INT16_MAX))));
  MOZ_RELEASE_ASSERT((IsInBounds<uint16_t, uint16_t>(uint16_t(UINT16_MAX))));
  MOZ_RELEASE_ASSERT((IsInBounds<uint16_t, int16_t>(uint16_t(0))));
  MOZ_RELEASE_ASSERT((!IsInBounds<uint16_t, int16_t>(uint16_t(-1))));
  MOZ_RELEASE_ASSERT((!IsInBounds<int16_t, uint16_t>(int16_t(-1))));
  MOZ_RELEASE_ASSERT((IsInBounds<int16_t, uint16_t>(int16_t(INT16_MAX))));
  MOZ_RELEASE_ASSERT((!IsInBounds<int16_t, uint16_t>(int16_t(INT16_MIN))));
  MOZ_RELEASE_ASSERT((IsInBounds<int32_t, uint32_t>(int32_t(INT32_MAX))));
  MOZ_RELEASE_ASSERT((!IsInBounds<int32_t, uint32_t>(int32_t(INT32_MIN))));
}

static void TestToBiggerSize() {
  MOZ_RELEASE_ASSERT((IsInBounds<int16_t, int32_t>(int16_t(0))));
  MOZ_RELEASE_ASSERT((IsInBounds<int16_t, int32_t>(int16_t(INT16_MIN))));
  MOZ_RELEASE_ASSERT((IsInBounds<int16_t, int32_t>(int16_t(INT16_MAX))));
  MOZ_RELEASE_ASSERT((IsInBounds<uint16_t, uint32_t>(uint16_t(UINT16_MAX))));
  MOZ_RELEASE_ASSERT((IsInBounds<uint16_t, int32_t>(uint16_t(0))));
  MOZ_RELEASE_ASSERT((IsInBounds<uint16_t, int32_t>(uint16_t(-1))));
  MOZ_RELEASE_ASSERT((!IsInBounds<int16_t, uint32_t>(int16_t(-1))));
  MOZ_RELEASE_ASSERT((IsInBounds<int16_t, uint32_t>(int16_t(INT16_MAX))));
  MOZ_RELEASE_ASSERT((!IsInBounds<int16_t, uint32_t>(int16_t(INT16_MIN))));
  MOZ_RELEASE_ASSERT((IsInBounds<int32_t, uint64_t>(int32_t(INT32_MAX))));
  MOZ_RELEASE_ASSERT((!IsInBounds<int32_t, uint64_t>(int32_t(INT32_MIN))));
}

static void TestToSmallerSize() {
  MOZ_RELEASE_ASSERT((IsInBounds<int16_t, int8_t>(int16_t(0))));
  MOZ_RELEASE_ASSERT((!IsInBounds<int16_t, int8_t>(int16_t(INT16_MIN))));
  MOZ_RELEASE_ASSERT((!IsInBounds<int16_t, int8_t>(int16_t(INT16_MAX))));
  MOZ_RELEASE_ASSERT((!IsInBounds<uint16_t, uint8_t>(uint16_t(UINT16_MAX))));
  MOZ_RELEASE_ASSERT((IsInBounds<uint16_t, int8_t>(uint16_t(0))));
  MOZ_RELEASE_ASSERT((!IsInBounds<uint16_t, int8_t>(uint16_t(-1))));
  MOZ_RELEASE_ASSERT((!IsInBounds<int16_t, uint8_t>(int16_t(-1))));
  MOZ_RELEASE_ASSERT((!IsInBounds<int16_t, uint8_t>(int16_t(INT16_MAX))));
  MOZ_RELEASE_ASSERT((!IsInBounds<int16_t, uint8_t>(int16_t(INT16_MIN))));
  MOZ_RELEASE_ASSERT((!IsInBounds<int32_t, uint16_t>(int32_t(INT32_MAX))));
  MOZ_RELEASE_ASSERT((!IsInBounds<int32_t, uint16_t>(int32_t(INT32_MIN))));

  // Boundary cases
  MOZ_RELEASE_ASSERT((!IsInBounds<int64_t, int32_t>(int64_t(INT32_MIN) - 1)));
  MOZ_RELEASE_ASSERT((IsInBounds<int64_t, int32_t>(int64_t(INT32_MIN))));
  MOZ_RELEASE_ASSERT((IsInBounds<int64_t, int32_t>(int64_t(INT32_MIN) + 1)));
  MOZ_RELEASE_ASSERT((IsInBounds<int64_t, int32_t>(int64_t(INT32_MAX) - 1)));
  MOZ_RELEASE_ASSERT((IsInBounds<int64_t, int32_t>(int64_t(INT32_MAX))));
  MOZ_RELEASE_ASSERT((!IsInBounds<int64_t, int32_t>(int64_t(INT32_MAX) + 1)));

  MOZ_RELEASE_ASSERT((!IsInBounds<int64_t, uint32_t>(int64_t(-1))));
  MOZ_RELEASE_ASSERT((IsInBounds<int64_t, uint32_t>(int64_t(0))));
  MOZ_RELEASE_ASSERT((IsInBounds<int64_t, uint32_t>(int64_t(1))));
  MOZ_RELEASE_ASSERT((IsInBounds<int64_t, uint32_t>(int64_t(UINT32_MAX) - 1)));
  MOZ_RELEASE_ASSERT((IsInBounds<int64_t, uint32_t>(int64_t(UINT32_MAX))));
  MOZ_RELEASE_ASSERT((!IsInBounds<int64_t, uint32_t>(int64_t(UINT32_MAX) + 1)));
}

template <typename In, typename Out>
void checkBoundariesFloating(In aEpsilon = {}, Out aIntegerOffset = {}) {
  // Check the max value of the input float can't be represented as an integer.
  // This is true for all floating point and integer width.
  MOZ_RELEASE_ASSERT((!IsInBounds<In, Out>(std::numeric_limits<In>::max())));
  // Check that the max value of the integer, as a float, minus an offset that
  // depends on the magnitude, can be represented as an integer.
  MOZ_RELEASE_ASSERT((IsInBounds<In, Out>(
      static_cast<In>(std::numeric_limits<Out>::max() - aIntegerOffset))));
  // Check that the max value of the integer, plus a number that depends on the
  // magnitude of the number, can't be represented as this integer (because it
  // becomes too big).
  MOZ_RELEASE_ASSERT((!IsInBounds<In, Out>(
      aEpsilon + static_cast<In>(std::numeric_limits<Out>::max()))));
  if constexpr (std::is_signed_v<In>) {
    // Same for negative numbers.
    MOZ_RELEASE_ASSERT(
        (!IsInBounds<In, Out>(std::numeric_limits<In>::lowest())));
    MOZ_RELEASE_ASSERT((IsInBounds<In, Out>(
        static_cast<In>(std::numeric_limits<Out>::lowest()))));
    MOZ_RELEASE_ASSERT((!IsInBounds<In, Out>(
        static_cast<In>(std::numeric_limits<Out>::lowest()) - aEpsilon)));
  } else {
    // Check for negative floats and unsigned integer types.
    MOZ_RELEASE_ASSERT((!IsInBounds<In, Out>(static_cast<In>(-1))));
  }
}

void TestFloatConversion() {
  MOZ_RELEASE_ASSERT((!IsInBounds<uint64_t, float>(UINT64_MAX)));
  MOZ_RELEASE_ASSERT((!IsInBounds<uint32_t, float>(UINT32_MAX)));
  MOZ_RELEASE_ASSERT((IsInBounds<uint16_t, float>(UINT16_MAX)));
  MOZ_RELEASE_ASSERT((IsInBounds<uint8_t, float>(UINT8_MAX)));

  MOZ_RELEASE_ASSERT((!IsInBounds<int64_t, float>(INT64_MAX)));
  MOZ_RELEASE_ASSERT((!IsInBounds<int64_t, float>(INT64_MIN)));
  MOZ_RELEASE_ASSERT((!IsInBounds<int32_t, float>(INT32_MAX)));
  MOZ_RELEASE_ASSERT((!IsInBounds<int32_t, float>(INT32_MIN)));
  MOZ_RELEASE_ASSERT((IsInBounds<int16_t, float>(INT16_MAX)));
  MOZ_RELEASE_ASSERT((IsInBounds<int16_t, float>(INT16_MIN)));
  MOZ_RELEASE_ASSERT((IsInBounds<int8_t, float>(INT8_MAX)));
  MOZ_RELEASE_ASSERT((IsInBounds<int8_t, float>(INT8_MIN)));

  MOZ_RELEASE_ASSERT((!IsInBounds<uint64_t, double>(UINT64_MAX)));
  MOZ_RELEASE_ASSERT((IsInBounds<uint32_t, double>(UINT32_MAX)));
  MOZ_RELEASE_ASSERT((IsInBounds<uint16_t, double>(UINT16_MAX)));
  MOZ_RELEASE_ASSERT((IsInBounds<uint8_t, double>(UINT8_MAX)));

  MOZ_RELEASE_ASSERT((!IsInBounds<int64_t, double>(INT64_MAX)));
  MOZ_RELEASE_ASSERT((!IsInBounds<int64_t, double>(INT64_MIN)));
  MOZ_RELEASE_ASSERT((IsInBounds<int32_t, double>(INT32_MAX)));
  MOZ_RELEASE_ASSERT((IsInBounds<int32_t, double>(INT32_MIN)));
  MOZ_RELEASE_ASSERT((IsInBounds<int16_t, double>(INT16_MAX)));
  MOZ_RELEASE_ASSERT((IsInBounds<int16_t, double>(INT16_MIN)));
  MOZ_RELEASE_ASSERT((IsInBounds<int8_t, double>(INT8_MAX)));
  MOZ_RELEASE_ASSERT((IsInBounds<int8_t, double>(INT8_MIN)));

  // Floor check
  MOZ_RELEASE_ASSERT((IsInBounds<float, uint64_t>(4.3)));
  MOZ_RELEASE_ASSERT((AssertedCast<uint64_t>(4.3f) == 4u));
  MOZ_RELEASE_ASSERT((IsInBounds<float, uint32_t>(4.3)));
  MOZ_RELEASE_ASSERT((AssertedCast<uint32_t>(4.3f) == 4u));
  MOZ_RELEASE_ASSERT((IsInBounds<float, uint16_t>(4.3)));
  MOZ_RELEASE_ASSERT((AssertedCast<uint16_t>(4.3f) == 4u));
  MOZ_RELEASE_ASSERT((IsInBounds<float, uint8_t>(4.3)));
  MOZ_RELEASE_ASSERT((AssertedCast<uint8_t>(4.3f) == 4u));

  MOZ_RELEASE_ASSERT((IsInBounds<float, int64_t>(4.3)));
  MOZ_RELEASE_ASSERT((AssertedCast<int64_t>(4.3f) == 4u));
  MOZ_RELEASE_ASSERT((IsInBounds<float, int32_t>(4.3)));
  MOZ_RELEASE_ASSERT((AssertedCast<int32_t>(4.3f) == 4u));
  MOZ_RELEASE_ASSERT((IsInBounds<float, int16_t>(4.3)));
  MOZ_RELEASE_ASSERT((AssertedCast<int16_t>(4.3f) == 4u));
  MOZ_RELEASE_ASSERT((IsInBounds<float, int8_t>(4.3)));
  MOZ_RELEASE_ASSERT((AssertedCast<int8_t>(4.3f) == 4u));

  MOZ_RELEASE_ASSERT((IsInBounds<float, int64_t>(-4.3)));
  MOZ_RELEASE_ASSERT((AssertedCast<int64_t>(-4.3f) == -4));
  MOZ_RELEASE_ASSERT((IsInBounds<float, int32_t>(-4.3)));
  MOZ_RELEASE_ASSERT((AssertedCast<int32_t>(-4.3f) == -4));
  MOZ_RELEASE_ASSERT((IsInBounds<float, int16_t>(-4.3)));
  MOZ_RELEASE_ASSERT((AssertedCast<int16_t>(-4.3f) == -4));
  MOZ_RELEASE_ASSERT((IsInBounds<float, int8_t>(-4.3)));
  MOZ_RELEASE_ASSERT((AssertedCast<int8_t>(-4.3f) == -4));

  // Bound check for float to unsigned integer conversion. The parameters are
  // espilons and offsets allowing to check boundaries, that depend on the
  // magnitude of the numbers.
  checkBoundariesFloating<double, uint64_t>(2049.);
  checkBoundariesFloating<double, uint32_t>(1.);
  checkBoundariesFloating<double, uint16_t>(1.);
  checkBoundariesFloating<double, uint8_t>(1.);
  // Large number because of the lack of precision of floats at this magnitude
  checkBoundariesFloating<float, uint64_t>(1.1e12f);
  checkBoundariesFloating<float, uint32_t>(1.f, 128u);
  checkBoundariesFloating<float, uint16_t>(1.f);
  checkBoundariesFloating<float, uint8_t>(1.f);

  checkBoundariesFloating<double, int64_t>(1025.);
  checkBoundariesFloating<double, int32_t>(1.);
  checkBoundariesFloating<double, int16_t>(1.);
  checkBoundariesFloating<double, int8_t>(1.);
  // Large number because of the lack of precision of floats at this magnitude
  checkBoundariesFloating<float, int64_t>(1.1e12f);
  checkBoundariesFloating<float, int32_t>(256.f, 64u);
  checkBoundariesFloating<float, int16_t>(1.f);
  checkBoundariesFloating<float, int8_t>(1.f);

  // Integer to floating point, boundary cases
  MOZ_RELEASE_ASSERT(!(IsInBounds<int64_t, float>(
      int64_t(std::pow(2, floatMantissaBitsPlusOne)) + 1)));
  MOZ_RELEASE_ASSERT((IsInBounds<int64_t, float>(
      int64_t(std::pow(2, floatMantissaBitsPlusOne)))));
  MOZ_RELEASE_ASSERT((IsInBounds<int64_t, float>(
      int64_t(std::pow(2, floatMantissaBitsPlusOne)) - 1)));

  MOZ_RELEASE_ASSERT(!(IsInBounds<int64_t, float>(
      int64_t(-std::pow(2, floatMantissaBitsPlusOne)) - 1)));
  MOZ_RELEASE_ASSERT((IsInBounds<int64_t, float>(
      int64_t(-std::pow(2, floatMantissaBitsPlusOne)))));
  MOZ_RELEASE_ASSERT((IsInBounds<int64_t, float>(
      int64_t(-std::pow(2, floatMantissaBitsPlusOne)) + 1)));

  MOZ_RELEASE_ASSERT(!(IsInBounds<int64_t, double>(
      uint64_t(std::pow(2, doubleMantissaBitsPlusOne)) + 1)));
  MOZ_RELEASE_ASSERT((IsInBounds<int64_t, double>(
      uint64_t(std::pow(2, doubleMantissaBitsPlusOne)))));
  MOZ_RELEASE_ASSERT((IsInBounds<int64_t, double>(
      uint64_t(std::pow(2, doubleMantissaBitsPlusOne)) - 1)));

  MOZ_RELEASE_ASSERT(!(IsInBounds<int64_t, double>(
      int64_t(-std::pow(2, doubleMantissaBitsPlusOne)) - 1)));
  MOZ_RELEASE_ASSERT((IsInBounds<int64_t, double>(
      int64_t(-std::pow(2, doubleMantissaBitsPlusOne)))));
  MOZ_RELEASE_ASSERT((IsInBounds<int64_t, double>(
      int64_t(-std::pow(2, doubleMantissaBitsPlusOne)) + 1)));

  MOZ_RELEASE_ASSERT(!(IsInBounds<uint64_t, double>(UINT64_MAX)));
  MOZ_RELEASE_ASSERT(!(IsInBounds<int64_t, double>(INT64_MAX)));
  MOZ_RELEASE_ASSERT(!(IsInBounds<int64_t, double>(INT64_MIN)));

  MOZ_RELEASE_ASSERT(
      !(IsInBounds<double, float>(std::numeric_limits<double>::max())));
  MOZ_RELEASE_ASSERT(
      !(IsInBounds<double, float>(-std::numeric_limits<double>::max())));
}

int main() {
  TestBitwiseCast();

  TestSameSize();
  TestToBiggerSize();
  TestToSmallerSize();
  TestFloatConversion();

  return 0;
}
