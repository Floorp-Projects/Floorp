/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/FloatingPoint.h"

#include <math.h>

using mozilla::ExponentComponent;
using mozilla::FloatingPoint;
using mozilla::FuzzyEqualsAdditive;
using mozilla::FuzzyEqualsMultiplicative;
using mozilla::IsFinite;
using mozilla::IsInfinite;
using mozilla::IsNaN;
using mozilla::IsNegative;
using mozilla::IsNegativeZero;
using mozilla::NegativeInfinity;
using mozilla::NumberEqualsInt32;
using mozilla::NumberIsInt32;
using mozilla::NumbersAreIdentical;
using mozilla::PositiveInfinity;
using mozilla::SpecificNaN;
using mozilla::UnspecifiedNaN;

template<typename T>
static void
ShouldBeIdentical(T d1, T d2)
{
  MOZ_RELEASE_ASSERT(NumbersAreIdentical(d1, d2));
  MOZ_RELEASE_ASSERT(NumbersAreIdentical(d2, d1));
}

template<typename T>
static void
ShouldNotBeIdentical(T d1, T d2)
{
  MOZ_RELEASE_ASSERT(!NumbersAreIdentical(d1, d2));
  MOZ_RELEASE_ASSERT(!NumbersAreIdentical(d2, d1));
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

  ShouldBeIdentical(PositiveInfinity<double>(), PositiveInfinity<double>());
  ShouldBeIdentical(NegativeInfinity<double>(), NegativeInfinity<double>());
  ShouldNotBeIdentical(PositiveInfinity<double>(), NegativeInfinity<double>());

  ShouldNotBeIdentical(-0.0, NegativeInfinity<double>());
  ShouldNotBeIdentical(+0.0, NegativeInfinity<double>());
  ShouldNotBeIdentical(1e300, NegativeInfinity<double>());
  ShouldNotBeIdentical(3.141592654, NegativeInfinity<double>());

  ShouldBeIdentical(UnspecifiedNaN<double>(), UnspecifiedNaN<double>());
  ShouldBeIdentical(-UnspecifiedNaN<double>(), UnspecifiedNaN<double>());
  ShouldBeIdentical(UnspecifiedNaN<double>(), -UnspecifiedNaN<double>());

  ShouldBeIdentical(SpecificNaN<double>(0, 17), SpecificNaN<double>(0, 42));
  ShouldBeIdentical(SpecificNaN<double>(1, 17), SpecificNaN<double>(1, 42));
  ShouldBeIdentical(SpecificNaN<double>(0, 17), SpecificNaN<double>(1, 42));
  ShouldBeIdentical(SpecificNaN<double>(1, 17), SpecificNaN<double>(0, 42));

  const uint64_t Mask = 0xfffffffffffffULL;
  for (unsigned i = 0; i < 52; i++) {
    for (unsigned j = 0; j < 52; j++) {
      for (unsigned sign = 0; i < 2; i++) {
        ShouldBeIdentical(SpecificNaN<double>(0, 1ULL << i), SpecificNaN<double>(sign, 1ULL << j));
        ShouldBeIdentical(SpecificNaN<double>(1, 1ULL << i), SpecificNaN<double>(sign, 1ULL << j));

        ShouldBeIdentical(SpecificNaN<double>(0, Mask & ~(1ULL << i)),
                          SpecificNaN<double>(sign, Mask & ~(1ULL << j)));
        ShouldBeIdentical(SpecificNaN<double>(1, Mask & ~(1ULL << i)),
                          SpecificNaN<double>(sign, Mask & ~(1ULL << j)));
      }
    }
  }
  ShouldBeIdentical(SpecificNaN<double>(0, 17), SpecificNaN<double>(0, 0x8000000000000ULL));
  ShouldBeIdentical(SpecificNaN<double>(0, 17), SpecificNaN<double>(0, 0x4000000000000ULL));
  ShouldBeIdentical(SpecificNaN<double>(0, 17), SpecificNaN<double>(0, 0x2000000000000ULL));
  ShouldBeIdentical(SpecificNaN<double>(0, 17), SpecificNaN<double>(0, 0x1000000000000ULL));
  ShouldBeIdentical(SpecificNaN<double>(0, 17), SpecificNaN<double>(0, 0x0800000000000ULL));
  ShouldBeIdentical(SpecificNaN<double>(0, 17), SpecificNaN<double>(0, 0x0400000000000ULL));
  ShouldBeIdentical(SpecificNaN<double>(0, 17), SpecificNaN<double>(0, 0x0200000000000ULL));
  ShouldBeIdentical(SpecificNaN<double>(0, 17), SpecificNaN<double>(0, 0x0100000000000ULL));
  ShouldBeIdentical(SpecificNaN<double>(0, 17), SpecificNaN<double>(0, 0x0080000000000ULL));
  ShouldBeIdentical(SpecificNaN<double>(0, 17), SpecificNaN<double>(0, 0x0040000000000ULL));
  ShouldBeIdentical(SpecificNaN<double>(0, 17), SpecificNaN<double>(0, 0x0020000000000ULL));
  ShouldBeIdentical(SpecificNaN<double>(0, 17), SpecificNaN<double>(0, 0x0010000000000ULL));
  ShouldBeIdentical(SpecificNaN<double>(1, 17), SpecificNaN<double>(0, 0xff0ffffffffffULL));
  ShouldBeIdentical(SpecificNaN<double>(1, 17), SpecificNaN<double>(0, 0xfffffffffff0fULL));

  ShouldNotBeIdentical(UnspecifiedNaN<double>(), +0.0);
  ShouldNotBeIdentical(UnspecifiedNaN<double>(), -0.0);
  ShouldNotBeIdentical(UnspecifiedNaN<double>(), 1.0);
  ShouldNotBeIdentical(UnspecifiedNaN<double>(), -1.0);
  ShouldNotBeIdentical(UnspecifiedNaN<double>(), PositiveInfinity<double>());
  ShouldNotBeIdentical(UnspecifiedNaN<double>(), NegativeInfinity<double>());
}

static void
TestFloatsAreIdentical()
{
  ShouldBeIdentical(+0.0f, +0.0f);
  ShouldBeIdentical(-0.0f, -0.0f);
  ShouldNotBeIdentical(+0.0f, -0.0f);

  ShouldBeIdentical(1.0f, 1.0f);
  ShouldNotBeIdentical(-1.0f, 1.0f);
  ShouldBeIdentical(8388607.0f, 8388607.0f);
  ShouldNotBeIdentical(-8388607.0f, 8388607.0f);
  ShouldBeIdentical(8388608.0f, 8388608.0f);
  ShouldBeIdentical(8388609.0f, 8388609.0f);
  ShouldBeIdentical(1e36f, 1e36f);

  ShouldBeIdentical(PositiveInfinity<float>(), PositiveInfinity<float>());
  ShouldBeIdentical(NegativeInfinity<float>(), NegativeInfinity<float>());
  ShouldNotBeIdentical(PositiveInfinity<float>(), NegativeInfinity<float>());

  ShouldNotBeIdentical(-0.0f, NegativeInfinity<float>());
  ShouldNotBeIdentical(+0.0f, NegativeInfinity<float>());
  ShouldNotBeIdentical(1e36f, NegativeInfinity<float>());
  ShouldNotBeIdentical(3.141592654f, NegativeInfinity<float>());

  ShouldBeIdentical(UnspecifiedNaN<float>(), UnspecifiedNaN<float>());
  ShouldBeIdentical(-UnspecifiedNaN<float>(), UnspecifiedNaN<float>());
  ShouldBeIdentical(UnspecifiedNaN<float>(), -UnspecifiedNaN<float>());

  ShouldBeIdentical(SpecificNaN<float>(0, 17), SpecificNaN<float>(0, 42));
  ShouldBeIdentical(SpecificNaN<float>(1, 17), SpecificNaN<float>(1, 42));
  ShouldBeIdentical(SpecificNaN<float>(0, 17), SpecificNaN<float>(1, 42));
  ShouldBeIdentical(SpecificNaN<float>(1, 17), SpecificNaN<float>(0, 42));

  const uint32_t Mask = 0x7fffffUL;
  for (unsigned i = 0; i < 23; i++) {
    for (unsigned j = 0; j < 23; j++) {
      for (unsigned sign = 0; i < 2; i++) {
        ShouldBeIdentical(SpecificNaN<float>(0, 1UL << i), SpecificNaN<float>(sign, 1UL << j));
        ShouldBeIdentical(SpecificNaN<float>(1, 1UL << i), SpecificNaN<float>(sign, 1UL << j));

        ShouldBeIdentical(SpecificNaN<float>(0, Mask & ~(1UL << i)),
                          SpecificNaN<float>(sign, Mask & ~(1UL << j)));
        ShouldBeIdentical(SpecificNaN<float>(1, Mask & ~(1UL << i)),
                          SpecificNaN<float>(sign, Mask & ~(1UL << j)));
      }
    }
  }
  ShouldBeIdentical(SpecificNaN<float>(0, 17), SpecificNaN<float>(0, 0x700000));
  ShouldBeIdentical(SpecificNaN<float>(0, 17), SpecificNaN<float>(0, 0x400000));
  ShouldBeIdentical(SpecificNaN<float>(0, 17), SpecificNaN<float>(0, 0x200000));
  ShouldBeIdentical(SpecificNaN<float>(0, 17), SpecificNaN<float>(0, 0x100000));
  ShouldBeIdentical(SpecificNaN<float>(0, 17), SpecificNaN<float>(0, 0x080000));
  ShouldBeIdentical(SpecificNaN<float>(0, 17), SpecificNaN<float>(0, 0x040000));
  ShouldBeIdentical(SpecificNaN<float>(0, 17), SpecificNaN<float>(0, 0x020000));
  ShouldBeIdentical(SpecificNaN<float>(0, 17), SpecificNaN<float>(0, 0x010000));
  ShouldBeIdentical(SpecificNaN<float>(0, 17), SpecificNaN<float>(0, 0x008000));
  ShouldBeIdentical(SpecificNaN<float>(0, 17), SpecificNaN<float>(0, 0x004000));
  ShouldBeIdentical(SpecificNaN<float>(0, 17), SpecificNaN<float>(0, 0x002000));
  ShouldBeIdentical(SpecificNaN<float>(0, 17), SpecificNaN<float>(0, 0x001000));
  ShouldBeIdentical(SpecificNaN<float>(1, 17), SpecificNaN<float>(0, 0x7f0fff));
  ShouldBeIdentical(SpecificNaN<float>(1, 17), SpecificNaN<float>(0, 0x7fff0f));

  ShouldNotBeIdentical(UnspecifiedNaN<float>(), +0.0f);
  ShouldNotBeIdentical(UnspecifiedNaN<float>(), -0.0f);
  ShouldNotBeIdentical(UnspecifiedNaN<float>(), 1.0f);
  ShouldNotBeIdentical(UnspecifiedNaN<float>(), -1.0f);
  ShouldNotBeIdentical(UnspecifiedNaN<float>(), PositiveInfinity<float>());
  ShouldNotBeIdentical(UnspecifiedNaN<float>(), NegativeInfinity<float>());
}

static void
TestAreIdentical()
{
  TestDoublesAreIdentical();
  TestFloatsAreIdentical();
}

static void
TestDoubleExponentComponent()
{
  MOZ_RELEASE_ASSERT(ExponentComponent(0.0) == -int_fast16_t(FloatingPoint<double>::ExponentBias));
  MOZ_RELEASE_ASSERT(ExponentComponent(-0.0) == -int_fast16_t(FloatingPoint<double>::ExponentBias));
  MOZ_RELEASE_ASSERT(ExponentComponent(0.125) == -3);
  MOZ_RELEASE_ASSERT(ExponentComponent(0.5) == -1);
  MOZ_RELEASE_ASSERT(ExponentComponent(1.0) == 0);
  MOZ_RELEASE_ASSERT(ExponentComponent(1.5) == 0);
  MOZ_RELEASE_ASSERT(ExponentComponent(2.0) == 1);
  MOZ_RELEASE_ASSERT(ExponentComponent(7.0) == 2);
  MOZ_RELEASE_ASSERT(ExponentComponent(PositiveInfinity<double>()) == FloatingPoint<double>::ExponentBias + 1);
  MOZ_RELEASE_ASSERT(ExponentComponent(NegativeInfinity<double>()) == FloatingPoint<double>::ExponentBias + 1);
  MOZ_RELEASE_ASSERT(ExponentComponent(UnspecifiedNaN<double>()) == FloatingPoint<double>::ExponentBias + 1);
}

static void
TestFloatExponentComponent()
{
  MOZ_RELEASE_ASSERT(ExponentComponent(0.0f) == -int_fast16_t(FloatingPoint<float>::ExponentBias));
  MOZ_RELEASE_ASSERT(ExponentComponent(-0.0f) == -int_fast16_t(FloatingPoint<float>::ExponentBias));
  MOZ_RELEASE_ASSERT(ExponentComponent(0.125f) == -3);
  MOZ_RELEASE_ASSERT(ExponentComponent(0.5f) == -1);
  MOZ_RELEASE_ASSERT(ExponentComponent(1.0f) == 0);
  MOZ_RELEASE_ASSERT(ExponentComponent(1.5f) == 0);
  MOZ_RELEASE_ASSERT(ExponentComponent(2.0f) == 1);
  MOZ_RELEASE_ASSERT(ExponentComponent(7.0f) == 2);
  MOZ_RELEASE_ASSERT(ExponentComponent(PositiveInfinity<float>()) == FloatingPoint<float>::ExponentBias + 1);
  MOZ_RELEASE_ASSERT(ExponentComponent(NegativeInfinity<float>()) == FloatingPoint<float>::ExponentBias + 1);
  MOZ_RELEASE_ASSERT(ExponentComponent(UnspecifiedNaN<float>()) == FloatingPoint<float>::ExponentBias + 1);
}

static void
TestExponentComponent()
{
  TestDoubleExponentComponent();
  TestFloatExponentComponent();
}

static void
TestDoublesPredicates()
{
  MOZ_RELEASE_ASSERT(IsNaN(UnspecifiedNaN<double>()));
  MOZ_RELEASE_ASSERT(IsNaN(SpecificNaN<double>(1, 17)));;
  MOZ_RELEASE_ASSERT(IsNaN(SpecificNaN<double>(0, 0xfffffffffff0fULL)));
  MOZ_RELEASE_ASSERT(!IsNaN(0.0));
  MOZ_RELEASE_ASSERT(!IsNaN(-0.0));
  MOZ_RELEASE_ASSERT(!IsNaN(1.0));
  MOZ_RELEASE_ASSERT(!IsNaN(PositiveInfinity<double>()));
  MOZ_RELEASE_ASSERT(!IsNaN(NegativeInfinity<double>()));

  MOZ_RELEASE_ASSERT(IsInfinite(PositiveInfinity<double>()));
  MOZ_RELEASE_ASSERT(IsInfinite(NegativeInfinity<double>()));
  MOZ_RELEASE_ASSERT(!IsInfinite(UnspecifiedNaN<double>()));
  MOZ_RELEASE_ASSERT(!IsInfinite(0.0));
  MOZ_RELEASE_ASSERT(!IsInfinite(-0.0));
  MOZ_RELEASE_ASSERT(!IsInfinite(1.0));

  MOZ_RELEASE_ASSERT(!IsFinite(PositiveInfinity<double>()));
  MOZ_RELEASE_ASSERT(!IsFinite(NegativeInfinity<double>()));
  MOZ_RELEASE_ASSERT(!IsFinite(UnspecifiedNaN<double>()));
  MOZ_RELEASE_ASSERT(IsFinite(0.0));
  MOZ_RELEASE_ASSERT(IsFinite(-0.0));
  MOZ_RELEASE_ASSERT(IsFinite(1.0));

  MOZ_RELEASE_ASSERT(!IsNegative(PositiveInfinity<double>()));
  MOZ_RELEASE_ASSERT(IsNegative(NegativeInfinity<double>()));
  MOZ_RELEASE_ASSERT(IsNegative(-0.0));
  MOZ_RELEASE_ASSERT(!IsNegative(0.0));
  MOZ_RELEASE_ASSERT(IsNegative(-1.0));
  MOZ_RELEASE_ASSERT(!IsNegative(1.0));

  MOZ_RELEASE_ASSERT(!IsNegativeZero(PositiveInfinity<double>()));
  MOZ_RELEASE_ASSERT(!IsNegativeZero(NegativeInfinity<double>()));
  MOZ_RELEASE_ASSERT(!IsNegativeZero(SpecificNaN<double>(1, 17)));;
  MOZ_RELEASE_ASSERT(!IsNegativeZero(SpecificNaN<double>(1, 0xfffffffffff0fULL)));
  MOZ_RELEASE_ASSERT(!IsNegativeZero(SpecificNaN<double>(0, 17)));;
  MOZ_RELEASE_ASSERT(!IsNegativeZero(SpecificNaN<double>(0, 0xfffffffffff0fULL)));
  MOZ_RELEASE_ASSERT(!IsNegativeZero(UnspecifiedNaN<double>()));
  MOZ_RELEASE_ASSERT(IsNegativeZero(-0.0));
  MOZ_RELEASE_ASSERT(!IsNegativeZero(0.0));
  MOZ_RELEASE_ASSERT(!IsNegativeZero(-1.0));
  MOZ_RELEASE_ASSERT(!IsNegativeZero(1.0));

  int32_t i;
  MOZ_RELEASE_ASSERT(NumberIsInt32(0.0, &i)); MOZ_RELEASE_ASSERT(i == 0);
  MOZ_RELEASE_ASSERT(!NumberIsInt32(-0.0, &i));
  MOZ_RELEASE_ASSERT(NumberEqualsInt32(0.0, &i)); MOZ_RELEASE_ASSERT(i == 0);
  MOZ_RELEASE_ASSERT(NumberEqualsInt32(-0.0, &i)); MOZ_RELEASE_ASSERT(i == 0);
  MOZ_RELEASE_ASSERT(NumberIsInt32(double(INT32_MIN), &i)); MOZ_RELEASE_ASSERT(i == INT32_MIN);
  MOZ_RELEASE_ASSERT(NumberIsInt32(double(INT32_MAX), &i)); MOZ_RELEASE_ASSERT(i == INT32_MAX);
  MOZ_RELEASE_ASSERT(NumberEqualsInt32(double(INT32_MIN), &i)); MOZ_RELEASE_ASSERT(i == INT32_MIN);
  MOZ_RELEASE_ASSERT(NumberEqualsInt32(double(INT32_MAX), &i)); MOZ_RELEASE_ASSERT(i == INT32_MAX);
  MOZ_RELEASE_ASSERT(!NumberIsInt32(0.5, &i));
  MOZ_RELEASE_ASSERT(!NumberIsInt32(double(INT32_MAX) + 0.1, &i));
  MOZ_RELEASE_ASSERT(!NumberIsInt32(double(INT32_MIN) - 0.1, &i));
  MOZ_RELEASE_ASSERT(!NumberIsInt32(NegativeInfinity<double>(), &i));
  MOZ_RELEASE_ASSERT(!NumberIsInt32(PositiveInfinity<double>(), &i));
  MOZ_RELEASE_ASSERT(!NumberIsInt32(UnspecifiedNaN<double>(), &i));
  MOZ_RELEASE_ASSERT(!NumberEqualsInt32(0.5, &i));
  MOZ_RELEASE_ASSERT(!NumberEqualsInt32(double(INT32_MAX) + 0.1, &i));
  MOZ_RELEASE_ASSERT(!NumberEqualsInt32(double(INT32_MIN) - 0.1, &i));
  MOZ_RELEASE_ASSERT(!NumberEqualsInt32(NegativeInfinity<double>(), &i));
  MOZ_RELEASE_ASSERT(!NumberEqualsInt32(PositiveInfinity<double>(), &i));
  MOZ_RELEASE_ASSERT(!NumberEqualsInt32(UnspecifiedNaN<double>(), &i));
}

static void
TestFloatsPredicates()
{
  MOZ_RELEASE_ASSERT(IsNaN(UnspecifiedNaN<float>()));
  MOZ_RELEASE_ASSERT(IsNaN(SpecificNaN<float>(1, 17)));;
  MOZ_RELEASE_ASSERT(IsNaN(SpecificNaN<float>(0, 0x7fff0fUL)));
  MOZ_RELEASE_ASSERT(!IsNaN(0.0f));
  MOZ_RELEASE_ASSERT(!IsNaN(-0.0f));
  MOZ_RELEASE_ASSERT(!IsNaN(1.0f));
  MOZ_RELEASE_ASSERT(!IsNaN(PositiveInfinity<float>()));
  MOZ_RELEASE_ASSERT(!IsNaN(NegativeInfinity<float>()));

  MOZ_RELEASE_ASSERT(IsInfinite(PositiveInfinity<float>()));
  MOZ_RELEASE_ASSERT(IsInfinite(NegativeInfinity<float>()));
  MOZ_RELEASE_ASSERT(!IsInfinite(UnspecifiedNaN<float>()));
  MOZ_RELEASE_ASSERT(!IsInfinite(0.0f));
  MOZ_RELEASE_ASSERT(!IsInfinite(-0.0f));
  MOZ_RELEASE_ASSERT(!IsInfinite(1.0f));

  MOZ_RELEASE_ASSERT(!IsFinite(PositiveInfinity<float>()));
  MOZ_RELEASE_ASSERT(!IsFinite(NegativeInfinity<float>()));
  MOZ_RELEASE_ASSERT(!IsFinite(UnspecifiedNaN<float>()));
  MOZ_RELEASE_ASSERT(IsFinite(0.0f));
  MOZ_RELEASE_ASSERT(IsFinite(-0.0f));
  MOZ_RELEASE_ASSERT(IsFinite(1.0f));

  MOZ_RELEASE_ASSERT(!IsNegative(PositiveInfinity<float>()));
  MOZ_RELEASE_ASSERT(IsNegative(NegativeInfinity<float>()));
  MOZ_RELEASE_ASSERT(IsNegative(-0.0f));
  MOZ_RELEASE_ASSERT(!IsNegative(0.0f));
  MOZ_RELEASE_ASSERT(IsNegative(-1.0f));
  MOZ_RELEASE_ASSERT(!IsNegative(1.0f));

  MOZ_RELEASE_ASSERT(!IsNegativeZero(PositiveInfinity<float>()));
  MOZ_RELEASE_ASSERT(!IsNegativeZero(NegativeInfinity<float>()));
  MOZ_RELEASE_ASSERT(!IsNegativeZero(SpecificNaN<float>(1, 17)));;
  MOZ_RELEASE_ASSERT(!IsNegativeZero(SpecificNaN<float>(1, 0x7fff0fUL)));
  MOZ_RELEASE_ASSERT(!IsNegativeZero(SpecificNaN<float>(0, 17)));;
  MOZ_RELEASE_ASSERT(!IsNegativeZero(SpecificNaN<float>(0, 0x7fff0fUL)));
  MOZ_RELEASE_ASSERT(!IsNegativeZero(UnspecifiedNaN<float>()));
  MOZ_RELEASE_ASSERT(IsNegativeZero(-0.0f));
  MOZ_RELEASE_ASSERT(!IsNegativeZero(0.0f));
  MOZ_RELEASE_ASSERT(!IsNegativeZero(-1.0f));
  MOZ_RELEASE_ASSERT(!IsNegativeZero(1.0f));

  int32_t i;
  const int32_t BIG = 2097151;
  MOZ_RELEASE_ASSERT(NumberIsInt32(0.0f, &i)); MOZ_RELEASE_ASSERT(i == 0);
  MOZ_RELEASE_ASSERT(!NumberIsInt32(-0.0f, &i));
  MOZ_RELEASE_ASSERT(NumberEqualsInt32(0.0f, &i)); MOZ_RELEASE_ASSERT(i == 0);
  MOZ_RELEASE_ASSERT(NumberEqualsInt32(-0.0f, &i)); MOZ_RELEASE_ASSERT(i == 0);
  MOZ_RELEASE_ASSERT(NumberIsInt32(float(INT32_MIN), &i)); MOZ_RELEASE_ASSERT(i == INT32_MIN);
  MOZ_RELEASE_ASSERT(NumberIsInt32(float(BIG), &i)); MOZ_RELEASE_ASSERT(i == BIG);
  MOZ_RELEASE_ASSERT(NumberEqualsInt32(float(INT32_MIN), &i)); MOZ_RELEASE_ASSERT(i == INT32_MIN);
  MOZ_RELEASE_ASSERT(NumberEqualsInt32(float(BIG), &i)); MOZ_RELEASE_ASSERT(i == BIG);
  MOZ_RELEASE_ASSERT(!NumberIsInt32(0.5f, &i));
  MOZ_RELEASE_ASSERT(!NumberIsInt32(float(BIG) + 0.1f, &i));
  MOZ_RELEASE_ASSERT(!NumberIsInt32(NegativeInfinity<float>(), &i));
  MOZ_RELEASE_ASSERT(!NumberIsInt32(PositiveInfinity<float>(), &i));
  MOZ_RELEASE_ASSERT(!NumberIsInt32(UnspecifiedNaN<float>(), &i));
  MOZ_RELEASE_ASSERT(!NumberEqualsInt32(0.5f, &i));
  MOZ_RELEASE_ASSERT(!NumberEqualsInt32(float(BIG) + 0.1f, &i));
  MOZ_RELEASE_ASSERT(!NumberEqualsInt32(NegativeInfinity<float>(), &i));
  MOZ_RELEASE_ASSERT(!NumberEqualsInt32(PositiveInfinity<float>(), &i));
  MOZ_RELEASE_ASSERT(!NumberEqualsInt32(UnspecifiedNaN<float>(), &i));
}

static void
TestPredicates()
{
  TestFloatsPredicates();
  TestDoublesPredicates();
}

static void
TestFloatsAreApproximatelyEqual()
{
  float epsilon = mozilla::detail::FuzzyEqualsEpsilon<float>::value();
  float lessThanEpsilon = epsilon / 2.0f;
  float moreThanEpsilon = epsilon * 2.0f;

  // Additive tests using the default epsilon
  // ... around 1.0
  MOZ_RELEASE_ASSERT(FuzzyEqualsAdditive(1.0f, 1.0f + lessThanEpsilon));
  MOZ_RELEASE_ASSERT(FuzzyEqualsAdditive(1.0f, 1.0f - lessThanEpsilon));
  MOZ_RELEASE_ASSERT(FuzzyEqualsAdditive(1.0f, 1.0f + epsilon));
  MOZ_RELEASE_ASSERT(FuzzyEqualsAdditive(1.0f, 1.0f - epsilon));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsAdditive(1.0f, 1.0f + moreThanEpsilon));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsAdditive(1.0f, 1.0f - moreThanEpsilon));
  // ... around 1.0e2 (this is near the upper bound of the range where
  // adding moreThanEpsilon will still be representable and return false)
  MOZ_RELEASE_ASSERT(FuzzyEqualsAdditive(1.0e2f, 1.0e2f + lessThanEpsilon));
  MOZ_RELEASE_ASSERT(FuzzyEqualsAdditive(1.0e2f, 1.0e2f + epsilon));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsAdditive(1.0e2f, 1.0e2f + moreThanEpsilon));
  // ... around 1.0e-10
  MOZ_RELEASE_ASSERT(FuzzyEqualsAdditive(1.0e-10f, 1.0e-10f + lessThanEpsilon));
  MOZ_RELEASE_ASSERT(FuzzyEqualsAdditive(1.0e-10f, 1.0e-10f + epsilon));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsAdditive(1.0e-10f, 1.0e-10f + moreThanEpsilon));
  // ... straddling 0
  MOZ_RELEASE_ASSERT(FuzzyEqualsAdditive(1.0e-6f, -1.0e-6f));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsAdditive(1.0e-5f, -1.0e-5f));
  // Using a small epsilon
  MOZ_RELEASE_ASSERT(FuzzyEqualsAdditive(1.0e-5f, 1.0e-5f + 1.0e-10f, 1.0e-9f));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsAdditive(1.0e-5f, 1.0e-5f + 1.0e-10f, 1.0e-11f));
  // Using a big epsilon
  MOZ_RELEASE_ASSERT(FuzzyEqualsAdditive(1.0e20f, 1.0e20f + 1.0e15f, 1.0e16f));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsAdditive(1.0e20f, 1.0e20f + 1.0e15f, 1.0e14f));

  // Multiplicative tests using the default epsilon
  // ... around 1.0
  MOZ_RELEASE_ASSERT(FuzzyEqualsMultiplicative(1.0f, 1.0f + lessThanEpsilon));
  MOZ_RELEASE_ASSERT(FuzzyEqualsMultiplicative(1.0f, 1.0f - lessThanEpsilon));
  MOZ_RELEASE_ASSERT(FuzzyEqualsMultiplicative(1.0f, 1.0f + epsilon));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsMultiplicative(1.0f, 1.0f - epsilon));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsMultiplicative(1.0f, 1.0f + moreThanEpsilon));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsMultiplicative(1.0f, 1.0f - moreThanEpsilon));
  // ... around 1.0e10
  MOZ_RELEASE_ASSERT(FuzzyEqualsMultiplicative(1.0e10f, 1.0e10f + (lessThanEpsilon * 1.0e10f)));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsMultiplicative(1.0e10f, 1.0e10f + (moreThanEpsilon * 1.0e10f)));
  // ... around 1.0e-10
  MOZ_RELEASE_ASSERT(FuzzyEqualsMultiplicative(1.0e-10f, 1.0e-10f + (lessThanEpsilon * 1.0e-10f)));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsMultiplicative(1.0e-10f, 1.0e-10f + (moreThanEpsilon * 1.0e-10f)));
  // ... straddling 0
  MOZ_RELEASE_ASSERT(!FuzzyEqualsMultiplicative(1.0e-6f, -1.0e-6f));
  MOZ_RELEASE_ASSERT(FuzzyEqualsMultiplicative(1.0e-6f, -1.0e-6f, 1.0e2f));
  // Using a small epsilon
  MOZ_RELEASE_ASSERT(FuzzyEqualsMultiplicative(1.0e-5f, 1.0e-5f + 1.0e-10f, 1.0e-4f));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsMultiplicative(1.0e-5f, 1.0e-5f + 1.0e-10f, 1.0e-5f));
  // Using a big epsilon
  MOZ_RELEASE_ASSERT(FuzzyEqualsMultiplicative(1.0f, 2.0f, 1.0f));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsMultiplicative(1.0f, 2.0f, 0.1f));

  // "real world case"
  float oneThird = 10.0f / 3.0f;
  MOZ_RELEASE_ASSERT(FuzzyEqualsAdditive(10.0f, 3.0f * oneThird));
  MOZ_RELEASE_ASSERT(FuzzyEqualsMultiplicative(10.0f, 3.0f * oneThird));
  // NaN check
  MOZ_RELEASE_ASSERT(!FuzzyEqualsAdditive(SpecificNaN<float>(1, 1), SpecificNaN<float>(1, 1)));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsAdditive(SpecificNaN<float>(1, 2), SpecificNaN<float>(0, 8)));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsMultiplicative(SpecificNaN<float>(1, 1), SpecificNaN<float>(1, 1)));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsMultiplicative(SpecificNaN<float>(1, 2), SpecificNaN<float>(0, 200)));
}

static void
TestDoublesAreApproximatelyEqual()
{
  double epsilon = mozilla::detail::FuzzyEqualsEpsilon<double>::value();
  double lessThanEpsilon = epsilon / 2.0;
  double moreThanEpsilon = epsilon * 2.0;

  // Additive tests using the default epsilon
  // ... around 1.0
  MOZ_RELEASE_ASSERT(FuzzyEqualsAdditive(1.0, 1.0 + lessThanEpsilon));
  MOZ_RELEASE_ASSERT(FuzzyEqualsAdditive(1.0, 1.0 - lessThanEpsilon));
  MOZ_RELEASE_ASSERT(FuzzyEqualsAdditive(1.0, 1.0 + epsilon));
  MOZ_RELEASE_ASSERT(FuzzyEqualsAdditive(1.0, 1.0 - epsilon));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsAdditive(1.0, 1.0 + moreThanEpsilon));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsAdditive(1.0, 1.0 - moreThanEpsilon));
  // ... around 1.0e4 (this is near the upper bound of the range where
  // adding moreThanEpsilon will still be representable and return false)
  MOZ_RELEASE_ASSERT(FuzzyEqualsAdditive(1.0e4, 1.0e4 + lessThanEpsilon));
  MOZ_RELEASE_ASSERT(FuzzyEqualsAdditive(1.0e4, 1.0e4 + epsilon));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsAdditive(1.0e4, 1.0e4 + moreThanEpsilon));
  // ... around 1.0e-25
  MOZ_RELEASE_ASSERT(FuzzyEqualsAdditive(1.0e-25, 1.0e-25 + lessThanEpsilon));
  MOZ_RELEASE_ASSERT(FuzzyEqualsAdditive(1.0e-25, 1.0e-25 + epsilon));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsAdditive(1.0e-25, 1.0e-25 + moreThanEpsilon));
  // ... straddling 0
  MOZ_RELEASE_ASSERT(FuzzyEqualsAdditive(1.0e-13, -1.0e-13));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsAdditive(1.0e-12, -1.0e-12));
  // Using a small epsilon
  MOZ_RELEASE_ASSERT(FuzzyEqualsAdditive(1.0e-15, 1.0e-15 + 1.0e-30, 1.0e-29));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsAdditive(1.0e-15, 1.0e-15 + 1.0e-30, 1.0e-31));
  // Using a big epsilon
  MOZ_RELEASE_ASSERT(FuzzyEqualsAdditive(1.0e40, 1.0e40 + 1.0e25, 1.0e26));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsAdditive(1.0e40, 1.0e40 + 1.0e25, 1.0e24));

  // Multiplicative tests using the default epsilon
  // ... around 1.0
  MOZ_RELEASE_ASSERT(FuzzyEqualsMultiplicative(1.0, 1.0 + lessThanEpsilon));
  MOZ_RELEASE_ASSERT(FuzzyEqualsMultiplicative(1.0, 1.0 - lessThanEpsilon));
  MOZ_RELEASE_ASSERT(FuzzyEqualsMultiplicative(1.0, 1.0 + epsilon));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsMultiplicative(1.0, 1.0 - epsilon));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsMultiplicative(1.0, 1.0 + moreThanEpsilon));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsMultiplicative(1.0, 1.0 - moreThanEpsilon));
  // ... around 1.0e30
  MOZ_RELEASE_ASSERT(FuzzyEqualsMultiplicative(1.0e30, 1.0e30 + (lessThanEpsilon * 1.0e30)));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsMultiplicative(1.0e30, 1.0e30 + (moreThanEpsilon * 1.0e30)));
  // ... around 1.0e-30
  MOZ_RELEASE_ASSERT(FuzzyEqualsMultiplicative(1.0e-30, 1.0e-30 + (lessThanEpsilon * 1.0e-30)));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsMultiplicative(1.0e-30, 1.0e-30 + (moreThanEpsilon * 1.0e-30)));
  // ... straddling 0
  MOZ_RELEASE_ASSERT(!FuzzyEqualsMultiplicative(1.0e-6, -1.0e-6));
  MOZ_RELEASE_ASSERT(FuzzyEqualsMultiplicative(1.0e-6, -1.0e-6, 1.0e2));
  // Using a small epsilon
  MOZ_RELEASE_ASSERT(FuzzyEqualsMultiplicative(1.0e-15, 1.0e-15 + 1.0e-30, 1.0e-15));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsMultiplicative(1.0e-15, 1.0e-15 + 1.0e-30, 1.0e-16));
  // Using a big epsilon
  MOZ_RELEASE_ASSERT(FuzzyEqualsMultiplicative(1.0e40, 2.0e40, 1.0));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsMultiplicative(1.0e40, 2.0e40, 0.1));

  // "real world case"
  double oneThird = 10.0 / 3.0;
  MOZ_RELEASE_ASSERT(FuzzyEqualsAdditive(10.0, 3.0 * oneThird));
  MOZ_RELEASE_ASSERT(FuzzyEqualsMultiplicative(10.0, 3.0 * oneThird));
  // NaN check
  MOZ_RELEASE_ASSERT(!FuzzyEqualsAdditive(SpecificNaN<double>(1, 1), SpecificNaN<double>(1, 1)));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsAdditive(SpecificNaN<double>(1, 2), SpecificNaN<double>(0, 8)));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsMultiplicative(SpecificNaN<double>(1, 1), SpecificNaN<double>(1, 1)));
  MOZ_RELEASE_ASSERT(!FuzzyEqualsMultiplicative(SpecificNaN<double>(1, 2), SpecificNaN<double>(0, 200)));
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
  TestAreIdentical();
  TestExponentComponent();
  TestPredicates();
  TestAreApproximatelyEqual();
}
