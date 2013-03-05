/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* mfbt maths algorithms. */

#ifndef mozilla_MathAlgorithms_h_
#define mozilla_MathAlgorithms_h_

#include "mozilla/Assertions.h"
#include "mozilla/StandardInteger.h"
#include "mozilla/TypeTraits.h"

#include <limits.h>
#include <math.h>

namespace mozilla {

// Greatest Common Divisor
template<typename IntegerType>
MOZ_ALWAYS_INLINE IntegerType
EuclidGCD(IntegerType a, IntegerType b)
{
  // Euclid's algorithm; O(N) in the worst case.  (There are better
  // ways, but we don't need them for the current use of this algo.)
  MOZ_ASSERT(a > 0);
  MOZ_ASSERT(b > 0);

  while (a != b) {
    if (a > b) {
      a = a - b;
    } else {
      b = b - a;
    }
  }

  return a;
}

// Least Common Multiple
template<typename IntegerType>
MOZ_ALWAYS_INLINE IntegerType
EuclidLCM(IntegerType a, IntegerType b)
{
  // Divide first to reduce overflow risk.
  return (a / EuclidGCD(a, b)) * b;
}

namespace detail {

// For now mozilla::Abs only takes intN_T, the signed natural types, and
// float/double/long double.  Feel free to add overloads for other standard,
// signed types if you need them.

template<typename T>
struct SupportedForAbsFixed : FalseType {};

template<> struct SupportedForAbsFixed<int8_t> : TrueType {};
template<> struct SupportedForAbsFixed<int16_t> : TrueType {};
template<> struct SupportedForAbsFixed<int32_t> : TrueType {};
template<> struct SupportedForAbsFixed<int64_t> : TrueType {};

template<typename T>
struct SupportedForAbs : SupportedForAbsFixed<T> {};

template<> struct SupportedForAbs<char> : IntegralConstant<bool, char(-1) < char(0)> {};
template<> struct SupportedForAbs<signed char> : TrueType {};
template<> struct SupportedForAbs<short> : TrueType {};
template<> struct SupportedForAbs<int> : TrueType {};
template<> struct SupportedForAbs<long> : TrueType {};
template<> struct SupportedForAbs<long long> : TrueType {};
template<> struct SupportedForAbs<float> : TrueType {};
template<> struct SupportedForAbs<double> : TrueType {};
template<> struct SupportedForAbs<long double> : TrueType {};

} // namespace detail

template<typename T>
inline typename mozilla::EnableIf<detail::SupportedForAbs<T>::value, T>::Type
Abs(const T t)
{
  // The absolute value of the smallest possible value of a signed-integer type
  // won't fit in that type (on twos-complement systems -- and we're blithely
  // assuming we're on such systems, for the non-<stdint.h> types listed above),
  // so assert that the input isn't that value.
  //
  // This is the case if: the value is non-negative; or if adding one (giving a
  // value in the range [-maxvalue, 0]), then negating (giving a value in the
  // range [0, maxvalue]), doesn't produce maxvalue (because in twos-complement,
  // (minvalue + 1) == -maxvalue).
  MOZ_ASSERT(t >= 0 ||
             -(t + 1) != T((1ULL << (CHAR_BIT * sizeof(T) - 1)) - 1),
             "You can't negate the smallest possible negative integer!");
  return t >= 0 ? t : -t;
}

template<>
inline float
Abs<float>(const float f)
{
  return fabsf(f);
}

template<>
inline double
Abs<double>(const double d)
{
  return fabs(d);
}

template<>
inline long double
Abs<long double>(const long double d)
{
  return fabsl(d);
}

} /* namespace mozilla */

#endif  /* mozilla_MathAlgorithms_h_ */
