/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_fuzzing_FuzzingTraits_h
#define mozilla_fuzzing_FuzzingTraits_h

#include "mozilla/Assertions.h"
#include <cmath>
#include <random>
#include <type_traits>

namespace mozilla {
namespace fuzzing {

class FuzzingTraits {
 public:
  static unsigned int Random(unsigned int aMax);
  static bool Sometimes(unsigned int aProbability);
  /**
   * Frequency() defines how many mutations of a kind shall be applied to a
   * target buffer by using a user definable factor. The higher the factor,
   * the less mutations are being made.
   */
  static size_t Frequency(const size_t aSize, const uint64_t aFactor);

  static std::mt19937_64& Rng();
};

/**
 * RandomNumericLimit returns either the min or max limit of an arithmetic
 * data type.
 */
template <typename T>
T RandomNumericLimit() {
  static_assert(std::is_arithmetic_v<T> == true,
                "T must be an arithmetic type");
  return FuzzingTraits::Sometimes(2) ? std::numeric_limits<T>::min()
                                     : std::numeric_limits<T>::max();
}

/**
 * RandomInteger generates negative and positive integers in 2**n increments.
 */
template <typename T>
T RandomInteger() {
  static_assert(std::is_integral_v<T> == true, "T must be an integral type");
  double r =
      static_cast<double>(FuzzingTraits::Random((sizeof(T) * CHAR_BIT) + 1));
  T x = static_cast<T>(pow(2.0, r)) - 1;
  if (std::numeric_limits<T>::is_signed && FuzzingTraits::Sometimes(2)) {
    return (x * -1) - 1;
  }
  return x;
}

/**
 * RandomIntegerRange returns a random integral within a [min, max] range.
 */
template <typename T>
T RandomIntegerRange(T min, T max) {
  static_assert(std::is_integral_v<T> == true, "T must be an integral type");
  MOZ_ASSERT(min < max);
  std::uniform_int_distribution<T> d(min, max);
  return d(FuzzingTraits::Rng());
}
/**
 * uniform_int_distribution is undefined for char/uchar. Need to handle them
 * separately.
 */
template <>
inline unsigned char RandomIntegerRange(unsigned char min, unsigned char max) {
  MOZ_ASSERT(min < max);
  std::uniform_int_distribution<unsigned short> d(min, max);
  return static_cast<unsigned char>(d(FuzzingTraits::Rng()));
}
template <>
inline char RandomIntegerRange(char min, char max) {
  MOZ_ASSERT(min < max);
  std::uniform_int_distribution<short> d(min, max);
  return static_cast<char>(d(FuzzingTraits::Rng()));
}

/**
 * RandomFloatingPointRange returns a random floating-point number within a
 * [min, max] range.
 */
template <typename T>
T RandomFloatingPointRange(T min, T max) {
  static_assert(std::is_floating_point_v<T> == true,
                "T must be a floating point type");
  MOZ_ASSERT(min < max);
  std::uniform_real_distribution<T> d(
      min, std::nextafter(max, std::numeric_limits<T>::max()));
  return d(FuzzingTraits::Rng());
}

/**
 * RandomFloatingPoint returns a random floating-point number in 2**n
 * increments.
 */
template <typename T>
T RandomFloatingPoint() {
  static_assert(std::is_floating_point_v<T> == true,
                "T must be a floating point type");
  int radix = RandomIntegerRange<int>(std::numeric_limits<T>::min_exponent,
                                      std::numeric_limits<T>::max_exponent);
  T x = static_cast<T>(pow(2.0, static_cast<double>(radix)));
  return x * RandomFloatingPointRange<T>(-1.0, 1.0);
}

}  // namespace fuzzing
}  // namespace mozilla

#endif
