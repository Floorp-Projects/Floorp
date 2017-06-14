/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_fuzzing_FuzzingTraits_h
#define mozilla_fuzzing_FuzzingTraits_h

#include "mozilla/Assertions.h"
#include "mozilla/TypeTraits.h"
#include <random>

namespace mozilla {
namespace fuzzing {

/**
 * RandomNumericLimit returns either the min or max limit of an arithmetic
 * data type.
 */
template <typename T>
T RandomNumericLimit()
{
  static_assert(mozilla::IsArithmetic<T>::value == true,
                "T must be an arithmetic type");
  return random() % 2 == 0 ? std::numeric_limits<T>::min()
                           : std::numeric_limits<T>::max();
}

/**
 * RandomInteger generates negative and positive integers in 2**n increments.
 */
template <typename T>
T RandomInteger()
{
  static_assert(mozilla::IsIntegral<T>::value == true,
                "T must be an integral type");
  double r = static_cast<double>(random() % ((sizeof(T) * CHAR_BIT) + 1));
  T x = static_cast<T>(pow(2.0, r)) - 1;
  if (std::numeric_limits<T>::is_signed && random() % 2 == 0) {
    return (x * -1) - 1;
  }
  return x;
}

/**
 * RandomIntegerRange returns a random integral within a [min, max] range.
 */
template <typename T>
T RandomIntegerRange(T min, T max)
{
  static_assert(mozilla::IsIntegral<T>::value == true,
                "T must be an integral type");
  MOZ_ASSERT(min < max);
  return static_cast<T>(random() % (max - min) + min);
}

/**
 * RandomFloatingPointRange returns a random floating-point number within a
 * [min, max] range.
 */
template <typename T>
T RandomFloatingPointRange(T min, T max)
{
  static_assert(mozilla::IsFloatingPoint<T>::value == true,
                "T must be a floating point type");
  MOZ_ASSERT(min < max);
  T x = static_cast<T>(random()) / static_cast<T>(RAND_MAX);
  return min + x * (max - min);
}

/**
 * RandomFloatingPoint returns a random floating-point number in 2**n
 * increments.
 */
template <typename T>
T RandomFloatingPoint()
{
  static_assert(mozilla::IsFloatingPoint<T>::value == true,
                "T must be a floating point type");
  int radix = RandomIntegerRange<int>(std::numeric_limits<T>::min_exponent,
                                      std::numeric_limits<T>::max_exponent);
  T x = static_cast<T>(pow(2.0, static_cast<double>(radix)));
  return x * RandomFloatingPointRange<T>(-1.0, 1.0);
}

class FuzzingTraits
{
public:
  static unsigned int Random(unsigned int aMax);
  static bool Sometimes(unsigned int aProbability);
  /**
   * Frequency() defines how many mutations of a kind shall be applied to a
   * target buffer by using a user definable factor. The higher the factor,
   * the less mutations are being made.
   */
  static size_t Frequency(const size_t aSize, const uint64_t aFactor);
};

} // namespace fuzzing
} // namespace mozilla

#endif
