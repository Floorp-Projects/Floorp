/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Utils_h
#define Utils_h

#include "mozilla/CheckedInt.h"
#include "mozilla/TemplateLib.h"

// Helper for log2 of powers of 2 at compile time.
template<size_t N>
struct Log2 : mozilla::tl::CeilingLog2<N>
{
  using mozilla::tl::CeilingLog2<N>::value;
  static_assert(1ULL << value == N, "Number is not a power of 2");
};
#define LOG2(N) Log2<N>::value

// Compare two addresses. Returns whether the first address is smaller (-1),
// equal (0) or greater (1) than the second address.
template<typename T>
int
CompareAddr(T* aAddr1, T* aAddr2)
{
  uintptr_t addr1 = reinterpret_cast<uintptr_t>(aAddr1);
  uintptr_t addr2 = reinterpret_cast<uintptr_t>(aAddr2);

  return (addr1 > addr2) - (addr1 < addr2);
}

// User-defined literals to make constants more legible
constexpr size_t operator"" _KiB(unsigned long long int aNum)
{
  return size_t(aNum) * 1024;
}

constexpr size_t operator"" _KiB(long double aNum)
{
  return size_t(aNum * 1024);
}

constexpr size_t operator"" _MiB(unsigned long long int aNum)
{
  return size_t(aNum) * 1024_KiB;
}

constexpr size_t operator"" _MiB(long double aNum)
{
  return size_t(aNum * 1024_KiB);
}

constexpr long double operator""_percent(long double aPercent)
{
  return aPercent / 100;
}

// Helper for (fast) comparison of fractions without involving divisions or
// floats.
class Fraction
{
public:
  explicit constexpr Fraction(size_t aNumerator, size_t aDenominator)
    : mNumerator(aNumerator)
    , mDenominator(aDenominator)
  {
  }

  MOZ_IMPLICIT constexpr Fraction(long double aValue)
    // We use an arbitrary power of two as denominator that provides enough
    // precision for our use case.
    : mNumerator(aValue * 4096)
    , mDenominator(4096)
  {
  }

  inline bool operator<(const Fraction& aOther) const
  {
#ifndef MOZ_DEBUG
    // We are comparing A / B < C / D, with all A, B, C and D being positive
    // numbers. Multiplying both sides with B * D, we have:
    // (A * B * D) / B < (C * B * D) / D, which can then be simplified as
    // A * D < C * B. When can thus compare our fractions without actually
    // doing any division.
    // This however assumes the multiplied quantities are small enough not
    // to overflow the multiplication. We use CheckedInt on debug builds
    // to enforce the assumption.
    return mNumerator * aOther.mDenominator < aOther.mNumerator * mDenominator;
#else
    mozilla::CheckedInt<size_t> numerator(mNumerator);
    mozilla::CheckedInt<size_t> denominator(mDenominator);
    // value() asserts when the multiplication overflowed.
    size_t lhs = (numerator * aOther.mDenominator).value();
    size_t rhs = (aOther.mNumerator * denominator).value();
    return lhs < rhs;
#endif
  }

  inline bool operator>(const Fraction& aOther) const { return aOther < *this; }

  inline bool operator>=(const Fraction& aOther) const
  {
    return !(*this < aOther);
  }

  inline bool operator<=(const Fraction& aOther) const
  {
    return !(*this > aOther);
  }

  inline bool operator==(const Fraction& aOther) const
  {
#ifndef MOZ_DEBUG
    // Same logic as operator<
    return mNumerator * aOther.mDenominator == aOther.mNumerator * mDenominator;
#else
    mozilla::CheckedInt<size_t> numerator(mNumerator);
    mozilla::CheckedInt<size_t> denominator(mDenominator);
    size_t lhs = (numerator * aOther.mDenominator).value();
    size_t rhs = (aOther.mNumerator * denominator).value();
    return lhs == rhs;
#endif
  }

  inline bool operator!=(const Fraction& aOther) const
  {
    return !(*this == aOther);
  }

private:
  size_t mNumerator;
  size_t mDenominator;
};

#endif
