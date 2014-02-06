/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/FloatingPoint.h"

#include <math.h>

using mozilla::DoublesAreIdentical;
using mozilla::DoubleExponentBias;
using mozilla::DoubleEqualsInt32;
using mozilla::DoubleIsInt32;
using mozilla::ExponentComponent;
using mozilla::FuzzyEqualsAdditive;
using mozilla::FuzzyEqualsMultiplicative;
using mozilla::IsFinite;
using mozilla::IsInfinite;
using mozilla::IsNaN;
using mozilla::IsNegative;
using mozilla::IsNegativeZero;
using mozilla::NegativeInfinity;
using mozilla::PositiveInfinity;
using mozilla::SpecificFloatNaN;
using mozilla::SpecificNaN;
using mozilla::UnspecifiedNaN;

static void
ShouldBeIdentical(double d1, double d2)
{
  MOZ_ASSERT(DoublesAreIdentical(d1, d2));
  MOZ_ASSERT(DoublesAreIdentical(d2, d1));
}

static void
ShouldNotBeIdentical(double d1, double d2)
{
  MOZ_ASSERT(!DoublesAreIdentical(d1, d2));
  MOZ_ASSERT(!DoublesAreIdentical(d2, d1));
}

static void
TestDoublesAreIdentical()
{
  ShouldBeIdentical(+0.0, +0.0);
  ShouldBeIdentical(-0.0, -0.0);
  ShouldNotBeIdentical(+0.0, -0.0);

  ShouldBeIdentical(1.0, 1.0);
  ShouldNotBeIdentical(-1.0, 1.0);
  ShouldBeIdentical(4294967295.0, 4294967295.0);
  ShouldNotBeIdentical(-4294967295.0, 4294967295.0);
  ShouldBeIdentical(4294967296.0, 4294967296.0);
  ShouldBeIdentical(4294967297.0, 4294967297.0);
  ShouldBeIdentical(1e300, 1e300);

  ShouldBeIdentical(PositiveInfinity(), PositiveInfinity());
  ShouldBeIdentical(NegativeInfinity(), NegativeInfinity());
  ShouldNotBeIdentical(PositiveInfinity(), NegativeInfinity());

  ShouldNotBeIdentical(-0.0, NegativeInfinity());
  ShouldNotBeIdentical(+0.0, NegativeInfinity());
  ShouldNotBeIdentical(1e300, NegativeInfinity());
  ShouldNotBeIdentical(3.141592654, NegativeInfinity());

  ShouldBeIdentical(UnspecifiedNaN(), UnspecifiedNaN());
  ShouldBeIdentical(-UnspecifiedNaN(), UnspecifiedNaN());
  ShouldBeIdentical(UnspecifiedNaN(), -UnspecifiedNaN());

  ShouldBeIdentical(SpecificNaN(0, 17), SpecificNaN(0, 42));
  ShouldBeIdentical(SpecificNaN(1, 17), SpecificNaN(1, 42));
  ShouldBeIdentical(SpecificNaN(0, 17), SpecificNaN(1, 42));
  ShouldBeIdentical(SpecificNaN(1, 17), SpecificNaN(0, 42));

  const uint64_t Mask = 0xfffffffffffffULL;
  for (unsigned i = 0; i < 52; i++) {
    for (unsigned j = 0; j < 52; j++) {
      for (unsigned sign = 0; i < 2; i++) {
        ShouldBeIdentical(SpecificNaN(0, 1ULL << i), SpecificNaN(sign, 1ULL << j));
        ShouldBeIdentical(SpecificNaN(1, 1ULL << i), SpecificNaN(sign, 1ULL << j));

        ShouldBeIdentical(SpecificNaN(0, Mask & ~(1ULL << i)),
                          SpecificNaN(sign, Mask & ~(1ULL << j)));
        ShouldBeIdentical(SpecificNaN(1, Mask & ~(1ULL << i)),
                          SpecificNaN(sign, Mask & ~(1ULL << j)));
      }
    }
  }
  ShouldBeIdentical(SpecificNaN(0, 17), SpecificNaN(0, 0x8000000000000ULL));
  ShouldBeIdentical(SpecificNaN(0, 17), SpecificNaN(0, 0x4000000000000ULL));
  ShouldBeIdentical(SpecificNaN(0, 17), SpecificNaN(0, 0x2000000000000ULL));
  ShouldBeIdentical(SpecificNaN(0, 17), SpecificNaN(0, 0x1000000000000ULL));
  ShouldBeIdentical(SpecificNaN(0, 17), SpecificNaN(0, 0x0800000000000ULL));
  ShouldBeIdentical(SpecificNaN(0, 17), SpecificNaN(0, 0x0400000000000ULL));
  ShouldBeIdentical(SpecificNaN(0, 17), SpecificNaN(0, 0x0200000000000ULL));
  ShouldBeIdentical(SpecificNaN(0, 17), SpecificNaN(0, 0x0100000000000ULL));
  ShouldBeIdentical(SpecificNaN(0, 17), SpecificNaN(0, 0x0080000000000ULL));
  ShouldBeIdentical(SpecificNaN(0, 17), SpecificNaN(0, 0x0040000000000ULL));
  ShouldBeIdentical(SpecificNaN(0, 17), SpecificNaN(0, 0x0020000000000ULL));
  ShouldBeIdentical(SpecificNaN(0, 17), SpecificNaN(0, 0x0010000000000ULL));
  ShouldBeIdentical(SpecificNaN(1, 17), SpecificNaN(0, 0xff0ffffffffffULL));
  ShouldBeIdentical(SpecificNaN(1, 17), SpecificNaN(0, 0xfffffffffff0fULL));

  ShouldNotBeIdentical(UnspecifiedNaN(), +0.0);
  ShouldNotBeIdentical(UnspecifiedNaN(), -0.0);
  ShouldNotBeIdentical(UnspecifiedNaN(), 1.0);
  ShouldNotBeIdentical(UnspecifiedNaN(), -1.0);
  ShouldNotBeIdentical(UnspecifiedNaN(), PositiveInfinity());
  ShouldNotBeIdentical(UnspecifiedNaN(), NegativeInfinity());
}

static void
TestExponentComponent()
{
  MOZ_ASSERT(ExponentComponent(0.0) == -int_fast16_t(DoubleExponentBias));
  MOZ_ASSERT(ExponentComponent(-0.0) == -int_fast16_t(DoubleExponentBias));
  MOZ_ASSERT(ExponentComponent(0.125) == -3);
  MOZ_ASSERT(ExponentComponent(0.5) == -1);
  MOZ_ASSERT(ExponentComponent(1.0) == 0);
  MOZ_ASSERT(ExponentComponent(1.5) == 0);
  MOZ_ASSERT(ExponentComponent(2.0) == 1);
  MOZ_ASSERT(ExponentComponent(7) == 2);
  MOZ_ASSERT(ExponentComponent(PositiveInfinity()) == DoubleExponentBias + 1);
  MOZ_ASSERT(ExponentComponent(NegativeInfinity()) == DoubleExponentBias + 1);
  MOZ_ASSERT(ExponentComponent(UnspecifiedNaN()) == DoubleExponentBias + 1);
}

static void
TestPredicates()
{
  MOZ_ASSERT(IsNaN(UnspecifiedNaN()));
  MOZ_ASSERT(IsNaN(SpecificNaN(1, 17)));;
  MOZ_ASSERT(IsNaN(SpecificNaN(0, 0xfffffffffff0fULL)));
  MOZ_ASSERT(!IsNaN(0));
  MOZ_ASSERT(!IsNaN(-0.0));
  MOZ_ASSERT(!IsNaN(1.0));
  MOZ_ASSERT(!IsNaN(PositiveInfinity()));
  MOZ_ASSERT(!IsNaN(NegativeInfinity()));

  MOZ_ASSERT(IsInfinite(PositiveInfinity()));
  MOZ_ASSERT(IsInfinite(NegativeInfinity()));
  MOZ_ASSERT(!IsInfinite(UnspecifiedNaN()));
  MOZ_ASSERT(!IsInfinite(0));
  MOZ_ASSERT(!IsInfinite(-0.0));
  MOZ_ASSERT(!IsInfinite(1.0));

  MOZ_ASSERT(!IsFinite(PositiveInfinity()));
  MOZ_ASSERT(!IsFinite(NegativeInfinity()));
  MOZ_ASSERT(!IsFinite(UnspecifiedNaN()));
  MOZ_ASSERT(IsFinite(0));
  MOZ_ASSERT(IsFinite(-0.0));
  MOZ_ASSERT(IsFinite(1.0));

  MOZ_ASSERT(!IsNegative(PositiveInfinity()));
  MOZ_ASSERT(IsNegative(NegativeInfinity()));
  MOZ_ASSERT(IsNegative(-0.0));
  MOZ_ASSERT(!IsNegative(0.0));
  MOZ_ASSERT(IsNegative(-1.0));
  MOZ_ASSERT(!IsNegative(1.0));

  MOZ_ASSERT(!IsNegativeZero(PositiveInfinity()));
  MOZ_ASSERT(!IsNegativeZero(NegativeInfinity()));
  MOZ_ASSERT(!IsNegativeZero(SpecificNaN(1, 17)));;
  MOZ_ASSERT(!IsNegativeZero(SpecificNaN(1, 0xfffffffffff0fULL)));
  MOZ_ASSERT(!IsNegativeZero(SpecificNaN(0, 17)));;
  MOZ_ASSERT(!IsNegativeZero(SpecificNaN(0, 0xfffffffffff0fULL)));
  MOZ_ASSERT(!IsNegativeZero(UnspecifiedNaN()));
  MOZ_ASSERT(IsNegativeZero(-0.0));
  MOZ_ASSERT(!IsNegativeZero(0.0));
  MOZ_ASSERT(!IsNegativeZero(-1.0));
  MOZ_ASSERT(!IsNegativeZero(1.0));

  int32_t i;
  MOZ_ASSERT(DoubleIsInt32(0.0, &i)); MOZ_ASSERT(i == 0);
  MOZ_ASSERT(!DoubleIsInt32(-0.0, &i));
  MOZ_ASSERT(DoubleEqualsInt32(0.0, &i)); MOZ_ASSERT(i == 0);
  MOZ_ASSERT(DoubleEqualsInt32(-0.0, &i)); MOZ_ASSERT(i == 0);
  MOZ_ASSERT(DoubleIsInt32(INT32_MIN, &i)); MOZ_ASSERT(i == INT32_MIN);
  MOZ_ASSERT(DoubleIsInt32(INT32_MAX, &i)); MOZ_ASSERT(i == INT32_MAX);
  MOZ_ASSERT(DoubleEqualsInt32(INT32_MIN, &i)); MOZ_ASSERT(i == INT32_MIN);
  MOZ_ASSERT(DoubleEqualsInt32(INT32_MAX, &i)); MOZ_ASSERT(i == INT32_MAX);
  MOZ_ASSERT(!DoubleIsInt32(0.5, &i));
  MOZ_ASSERT(!DoubleIsInt32(double(INT32_MAX) + 0.1, &i));
  MOZ_ASSERT(!DoubleIsInt32(double(INT32_MIN) - 0.1, &i));
  MOZ_ASSERT(!DoubleIsInt32(NegativeInfinity(), &i));
  MOZ_ASSERT(!DoubleIsInt32(PositiveInfinity(), &i));
  MOZ_ASSERT(!DoubleIsInt32(UnspecifiedNaN(), &i));
  MOZ_ASSERT(!DoubleEqualsInt32(0.5, &i));
  MOZ_ASSERT(!DoubleEqualsInt32(double(INT32_MAX) + 0.1, &i));
  MOZ_ASSERT(!DoubleEqualsInt32(double(INT32_MIN) - 0.1, &i));
  MOZ_ASSERT(!DoubleEqualsInt32(NegativeInfinity(), &i));
  MOZ_ASSERT(!DoubleEqualsInt32(PositiveInfinity(), &i));
  MOZ_ASSERT(!DoubleEqualsInt32(UnspecifiedNaN(), &i));
}

static void
TestFloatsAreApproximatelyEqual()
{
  float epsilon = mozilla::detail::FuzzyEqualsEpsilon<float>::value();
  float lessThanEpsilon = epsilon / 2.0f;
  float moreThanEpsilon = epsilon * 2.0f;

  // Additive tests using the default epsilon
  // ... around 1.0
  MOZ_ASSERT(FuzzyEqualsAdditive(1.0f, 1.0f + lessThanEpsilon));
  MOZ_ASSERT(FuzzyEqualsAdditive(1.0f, 1.0f - lessThanEpsilon));
  MOZ_ASSERT(FuzzyEqualsAdditive(1.0f, 1.0f + epsilon));
  MOZ_ASSERT(FuzzyEqualsAdditive(1.0f, 1.0f - epsilon));
  MOZ_ASSERT(!FuzzyEqualsAdditive(1.0f, 1.0f + moreThanEpsilon));
  MOZ_ASSERT(!FuzzyEqualsAdditive(1.0f, 1.0f - moreThanEpsilon));
  // ... around 1.0e2 (this is near the upper bound of the range where
  // adding moreThanEpsilon will still be representable and return false)
  MOZ_ASSERT(FuzzyEqualsAdditive(1.0e2f, 1.0e2f + lessThanEpsilon));
  MOZ_ASSERT(FuzzyEqualsAdditive(1.0e2f, 1.0e2f + epsilon));
  MOZ_ASSERT(!FuzzyEqualsAdditive(1.0e2f, 1.0e2f + moreThanEpsilon));
  // ... around 1.0e-10
  MOZ_ASSERT(FuzzyEqualsAdditive(1.0e-10f, 1.0e-10f + lessThanEpsilon));
  MOZ_ASSERT(FuzzyEqualsAdditive(1.0e-10f, 1.0e-10f + epsilon));
  MOZ_ASSERT(!FuzzyEqualsAdditive(1.0e-10f, 1.0e-10f + moreThanEpsilon));
  // ... straddling 0
  MOZ_ASSERT(FuzzyEqualsAdditive(1.0e-6f, -1.0e-6f));
  MOZ_ASSERT(!FuzzyEqualsAdditive(1.0e-5f, -1.0e-5f));
  // Using a small epsilon
  MOZ_ASSERT(FuzzyEqualsAdditive(1.0e-5f, 1.0e-5f + 1.0e-10f, 1.0e-9f));
  MOZ_ASSERT(!FuzzyEqualsAdditive(1.0e-5f, 1.0e-5f + 1.0e-10f, 1.0e-11f));
  // Using a big epsilon
  MOZ_ASSERT(FuzzyEqualsAdditive(1.0e20f, 1.0e20f + 1.0e15f, 1.0e16f));
  MOZ_ASSERT(!FuzzyEqualsAdditive(1.0e20f, 1.0e20f + 1.0e15f, 1.0e14f));

  // Multiplicative tests using the default epsilon
  // ... around 1.0
  MOZ_ASSERT(FuzzyEqualsMultiplicative(1.0f, 1.0f + lessThanEpsilon));
  MOZ_ASSERT(FuzzyEqualsMultiplicative(1.0f, 1.0f - lessThanEpsilon));
  MOZ_ASSERT(FuzzyEqualsMultiplicative(1.0f, 1.0f + epsilon));
  MOZ_ASSERT(!FuzzyEqualsMultiplicative(1.0f, 1.0f - epsilon));
  MOZ_ASSERT(!FuzzyEqualsMultiplicative(1.0f, 1.0f + moreThanEpsilon));
  MOZ_ASSERT(!FuzzyEqualsMultiplicative(1.0f, 1.0f - moreThanEpsilon));
  // ... around 1.0e10
  MOZ_ASSERT(FuzzyEqualsMultiplicative(1.0e10f, 1.0e10f + (lessThanEpsilon * 1.0e10f)));
  MOZ_ASSERT(!FuzzyEqualsMultiplicative(1.0e10f, 1.0e10f + (moreThanEpsilon * 1.0e10f)));
  // ... around 1.0e-10
  MOZ_ASSERT(FuzzyEqualsMultiplicative(1.0e-10f, 1.0e-10f + (lessThanEpsilon * 1.0e-10f)));
  MOZ_ASSERT(!FuzzyEqualsMultiplicative(1.0e-10f, 1.0e-10f + (moreThanEpsilon * 1.0e-10f)));
  // ... straddling 0
  MOZ_ASSERT(!FuzzyEqualsMultiplicative(1.0e-6f, -1.0e-6f));
  MOZ_ASSERT(FuzzyEqualsMultiplicative(1.0e-6f, -1.0e-6f, 1.0e2f));
  // Using a small epsilon
  MOZ_ASSERT(FuzzyEqualsMultiplicative(1.0e-5f, 1.0e-5f + 1.0e-10f, 1.0e-4f));
  MOZ_ASSERT(!FuzzyEqualsMultiplicative(1.0e-5f, 1.0e-5f + 1.0e-10f, 1.0e-5f));
  // Using a big epsilon
  MOZ_ASSERT(FuzzyEqualsMultiplicative(1.0f, 2.0f, 1.0f));
  MOZ_ASSERT(!FuzzyEqualsMultiplicative(1.0f, 2.0f, 0.1f));

  // "real world case"
  float oneThird = 10.0f / 3.0f;
  MOZ_ASSERT(FuzzyEqualsAdditive(10.0f, 3.0f * oneThird));
  MOZ_ASSERT(FuzzyEqualsMultiplicative(10.0f, 3.0f * oneThird));
  // NaN check
  MOZ_ASSERT(!FuzzyEqualsAdditive(SpecificFloatNaN(1, 1), SpecificFloatNaN(1, 1)));
  MOZ_ASSERT(!FuzzyEqualsAdditive(SpecificFloatNaN(1, 2), SpecificFloatNaN(0, 8)));
  MOZ_ASSERT(!FuzzyEqualsMultiplicative(SpecificFloatNaN(1, 1), SpecificFloatNaN(1, 1)));
  MOZ_ASSERT(!FuzzyEqualsMultiplicative(SpecificFloatNaN(1, 2), SpecificFloatNaN(0, 200)));
}

static void
TestDoublesAreApproximatelyEqual()
{
  double epsilon = mozilla::detail::FuzzyEqualsEpsilon<double>::value();
  double lessThanEpsilon = epsilon / 2.0;
  double moreThanEpsilon = epsilon * 2.0;

  // Additive tests using the default epsilon
  // ... around 1.0
  MOZ_ASSERT(FuzzyEqualsAdditive(1.0, 1.0 + lessThanEpsilon));
  MOZ_ASSERT(FuzzyEqualsAdditive(1.0, 1.0 - lessThanEpsilon));
  MOZ_ASSERT(FuzzyEqualsAdditive(1.0, 1.0 + epsilon));
  MOZ_ASSERT(FuzzyEqualsAdditive(1.0, 1.0 - epsilon));
  MOZ_ASSERT(!FuzzyEqualsAdditive(1.0, 1.0 + moreThanEpsilon));
  MOZ_ASSERT(!FuzzyEqualsAdditive(1.0, 1.0 - moreThanEpsilon));
  // ... around 1.0e4 (this is near the upper bound of the range where
  // adding moreThanEpsilon will still be representable and return false)
  MOZ_ASSERT(FuzzyEqualsAdditive(1.0e4, 1.0e4 + lessThanEpsilon));
  MOZ_ASSERT(FuzzyEqualsAdditive(1.0e4, 1.0e4 + epsilon));
  MOZ_ASSERT(!FuzzyEqualsAdditive(1.0e4, 1.0e4 + moreThanEpsilon));
  // ... around 1.0e-25
  MOZ_ASSERT(FuzzyEqualsAdditive(1.0e-25, 1.0e-25 + lessThanEpsilon));
  MOZ_ASSERT(FuzzyEqualsAdditive(1.0e-25, 1.0e-25 + epsilon));
  MOZ_ASSERT(!FuzzyEqualsAdditive(1.0e-25, 1.0e-25 + moreThanEpsilon));
  // ... straddling 0
  MOZ_ASSERT(FuzzyEqualsAdditive(1.0e-13, -1.0e-13));
  MOZ_ASSERT(!FuzzyEqualsAdditive(1.0e-12, -1.0e-12));
  // Using a small epsilon
  MOZ_ASSERT(FuzzyEqualsAdditive(1.0e-15, 1.0e-15 + 1.0e-30, 1.0e-29));
  MOZ_ASSERT(!FuzzyEqualsAdditive(1.0e-15, 1.0e-15 + 1.0e-30, 1.0e-31));
  // Using a big epsilon
  MOZ_ASSERT(FuzzyEqualsAdditive(1.0e40, 1.0e40 + 1.0e25, 1.0e26));
  MOZ_ASSERT(!FuzzyEqualsAdditive(1.0e40, 1.0e40 + 1.0e25, 1.0e24));

  // Multiplicative tests using the default epsilon
  // ... around 1.0
  MOZ_ASSERT(FuzzyEqualsMultiplicative(1.0, 1.0 + lessThanEpsilon));
  MOZ_ASSERT(FuzzyEqualsMultiplicative(1.0, 1.0 - lessThanEpsilon));
  MOZ_ASSERT(FuzzyEqualsMultiplicative(1.0, 1.0 + epsilon));
  MOZ_ASSERT(!FuzzyEqualsMultiplicative(1.0, 1.0 - epsilon));
  MOZ_ASSERT(!FuzzyEqualsMultiplicative(1.0, 1.0 + moreThanEpsilon));
  MOZ_ASSERT(!FuzzyEqualsMultiplicative(1.0, 1.0 - moreThanEpsilon));
  // ... around 1.0e30
  MOZ_ASSERT(FuzzyEqualsMultiplicative(1.0e30, 1.0e30 + (lessThanEpsilon * 1.0e30)));
  MOZ_ASSERT(!FuzzyEqualsMultiplicative(1.0e30, 1.0e30 + (moreThanEpsilon * 1.0e30)));
  // ... around 1.0e-30
  MOZ_ASSERT(FuzzyEqualsMultiplicative(1.0e-30, 1.0e-30 + (lessThanEpsilon * 1.0e-30)));
  MOZ_ASSERT(!FuzzyEqualsMultiplicative(1.0e-30, 1.0e-30 + (moreThanEpsilon * 1.0e-30)));
  // ... straddling 0
  MOZ_ASSERT(!FuzzyEqualsMultiplicative(1.0e-6, -1.0e-6));
  MOZ_ASSERT(FuzzyEqualsMultiplicative(1.0e-6, -1.0e-6, 1.0e2));
  // Using a small epsilon
  MOZ_ASSERT(FuzzyEqualsMultiplicative(1.0e-15, 1.0e-15 + 1.0e-30, 1.0e-15));
  MOZ_ASSERT(!FuzzyEqualsMultiplicative(1.0e-15, 1.0e-15 + 1.0e-30, 1.0e-16));
  // Using a big epsilon
  MOZ_ASSERT(FuzzyEqualsMultiplicative(1.0e40, 2.0e40, 1.0));
  MOZ_ASSERT(!FuzzyEqualsMultiplicative(1.0e40, 2.0e40, 0.1));

  // "real world case"
  double oneThird = 10.0 / 3.0;
  MOZ_ASSERT(FuzzyEqualsAdditive(10.0, 3.0 * oneThird));
  MOZ_ASSERT(FuzzyEqualsMultiplicative(10.0, 3.0 * oneThird));
  // NaN check
  MOZ_ASSERT(!FuzzyEqualsAdditive(SpecificNaN(1, 1), SpecificNaN(1, 1)));
  MOZ_ASSERT(!FuzzyEqualsAdditive(SpecificNaN(1, 2), SpecificNaN(0, 8)));
  MOZ_ASSERT(!FuzzyEqualsMultiplicative(SpecificNaN(1, 1), SpecificNaN(1, 1)));
  MOZ_ASSERT(!FuzzyEqualsMultiplicative(SpecificNaN(1, 2), SpecificNaN(0, 200)));
}

static void
TestAreApproximatelyEqual()
{
  TestFloatsAreApproximatelyEqual();
  TestDoublesAreApproximatelyEqual();
}

int
main()
{
  TestDoublesAreIdentical();
  TestExponentComponent();
  TestPredicates();
  TestAreApproximatelyEqual();
}
