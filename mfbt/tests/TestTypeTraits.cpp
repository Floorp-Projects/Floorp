/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/TypeTraits.h"

using mozilla::IsBaseOf;
using mozilla::IsConvertible;
using mozilla::IsSame;
using mozilla::IsSigned;
using mozilla::IsUnsigned;
using mozilla::MakeSigned;
using mozilla::MakeUnsigned;

static_assert(!IsSigned<bool>::value, "bool shouldn't be signed");
static_assert(IsUnsigned<bool>::value, "bool should be unsigned");

static_assert(!IsSigned<const bool>::value, "const bool shouldn't be signed");
static_assert(IsUnsigned<const bool>::value, "const bool should be unsigned");

static_assert(!IsSigned<volatile bool>::value, "volatile bool shouldn't be signed");
static_assert(IsUnsigned<volatile bool>::value, "volatile bool should be unsigned");

static_assert(!IsSigned<unsigned char>::value, "unsigned char shouldn't be signed");
static_assert(IsUnsigned<unsigned char>::value, "unsigned char should be unsigned");
static_assert(IsSigned<signed char>::value, "signed char should be signed");
static_assert(!IsUnsigned<signed char>::value, "signed char shouldn't be unsigned");

static_assert(!IsSigned<unsigned short>::value, "unsigned short shouldn't be signed");
static_assert(IsUnsigned<unsigned short>::value, "unsigned short should be unsigned");
static_assert(IsSigned<short>::value, "short should be signed");
static_assert(!IsUnsigned<short>::value, "short shouldn't be unsigned");

static_assert(!IsSigned<unsigned int>::value, "unsigned int shouldn't be signed");
static_assert(IsUnsigned<unsigned int>::value, "unsigned int should be unsigned");
static_assert(IsSigned<int>::value, "int should be signed");
static_assert(!IsUnsigned<int>::value, "int shouldn't be unsigned");

static_assert(!IsSigned<unsigned long>::value, "unsigned long shouldn't be signed");
static_assert(IsUnsigned<unsigned long>::value, "unsigned long should be unsigned");
static_assert(IsSigned<long>::value, "long should be signed");
static_assert(!IsUnsigned<long>::value, "long shouldn't be unsigned");

static_assert(IsSigned<float>::value, "float should be signed");
static_assert(!IsUnsigned<float>::value, "float shouldn't be unsigned");

static_assert(IsSigned<const float>::value, "const float should be signed");
static_assert(!IsUnsigned<const float>::value, "const float shouldn't be unsigned");

static_assert(IsSigned<double>::value, "double should be signed");
static_assert(!IsUnsigned<double>::value, "double shouldn't be unsigned");

static_assert(IsSigned<volatile double>::value, "volatile double should be signed");
static_assert(!IsUnsigned<volatile double>::value, "volatile double shouldn't be unsigned");

static_assert(IsSigned<long double>::value, "long double should be signed");
static_assert(!IsUnsigned<long double>::value, "long double shouldn't be unsigned");

static_assert(IsSigned<const volatile long double>::value,
              "const volatile long double should be signed");
static_assert(!IsUnsigned<const volatile long double>::value,
              "const volatile long double shouldn't be unsigned");

namespace CPlusPlus11IsBaseOf {

// Adapted from C++11 § 20.9.6.
struct B {};
struct B1 : B {};
struct B2 : B {};
struct D : private B1, private B2 {};

static void
StandardIsBaseOfTests()
{
  MOZ_ASSERT((IsBaseOf<B, D>::value) == true);
  MOZ_ASSERT((IsBaseOf<const B, D>::value) == true);
  MOZ_ASSERT((IsBaseOf<B, const D>::value) == true);
  MOZ_ASSERT((IsBaseOf<B, const B>::value) == true);
  MOZ_ASSERT((IsBaseOf<D, B>::value) == false);
  MOZ_ASSERT((IsBaseOf<B&, D&>::value) == false);
  MOZ_ASSERT((IsBaseOf<B[3], D[3]>::value) == false);
  // We fail at the following test.  To fix it, we need to specialize IsBaseOf
  // for all built-in types.
  // MOZ_ASSERT((IsBaseOf<int, int>::value) == false);
}

} /* namespace CPlusPlus11IsBaseOf */

class A { };
class B : public A { };
class C : private A { };
class D { };
class E : public A { };
class F : public B, public E { };

static void
TestIsBaseOf()
{
  MOZ_ASSERT((IsBaseOf<A, B>::value),
             "A is a base of B");
  MOZ_ASSERT((!IsBaseOf<B, A>::value),
             "B is not a base of A");
  MOZ_ASSERT((IsBaseOf<A, C>::value),
             "A is a base of C");
  MOZ_ASSERT((!IsBaseOf<C, A>::value),
             "C is not a base of A");
  MOZ_ASSERT((IsBaseOf<A, F>::value),
             "A is a base of F");
  MOZ_ASSERT((!IsBaseOf<F, A>::value),
             "F is not a base of A");
  MOZ_ASSERT((!IsBaseOf<A, D>::value),
             "A is not a base of D");
  MOZ_ASSERT((!IsBaseOf<D, A>::value),
             "D is not a base of A");
  MOZ_ASSERT((IsBaseOf<B, B>::value),
             "B is the same as B (and therefore, a base of B)");
}

static void
TestIsConvertible()
{
  // Pointer type convertibility
  MOZ_ASSERT((IsConvertible<A*, A*>::value),
             "A* should convert to A*");
  MOZ_ASSERT((IsConvertible<B*, A*>::value),
             "B* should convert to A*");
  MOZ_ASSERT((!IsConvertible<A*, B*>::value),
             "A* shouldn't convert to B*");
  MOZ_ASSERT((!IsConvertible<A*, C*>::value),
             "A* shouldn't convert to C*");
  MOZ_ASSERT((!IsConvertible<A*, D*>::value),
             "A* shouldn't convert to unrelated D*");
  MOZ_ASSERT((!IsConvertible<D*, A*>::value),
             "D* shouldn't convert to unrelated A*");

  // Instance type convertibility
  MOZ_ASSERT((IsConvertible<A, A>::value),
             "A is A");
  MOZ_ASSERT((IsConvertible<B, A>::value),
             "B converts to A");
  MOZ_ASSERT((!IsConvertible<D, A>::value),
             "D and A are unrelated");
  MOZ_ASSERT((!IsConvertible<A, D>::value),
             "A and D are unrelated");

  // These cases seem to require C++11 support to properly implement them, so
  // for now just disable them.
  //MOZ_ASSERT((!IsConvertible<C*, A*>::value),
  //           "C* shouldn't convert to A* (private inheritance)");
  //MOZ_ASSERT((!IsConvertible<C, A>::value),
  //           "C doesn't convert to A (private inheritance)");
}

static_assert(IsSame<MakeSigned<const unsigned char>::Type, const signed char>::value,
              "const unsigned char won't signify correctly");
static_assert(IsSame<MakeSigned<volatile unsigned short>::Type, volatile signed short>::value,
              "volatile unsigned short won't signify correctly");
static_assert(IsSame<MakeSigned<const volatile unsigned int>::Type, const volatile signed int>::value,
              "const volatile unsigned int won't signify correctly");
static_assert(IsSame<MakeSigned<unsigned long>::Type, signed long>::value,
              "unsigned long won't signify correctly");
static_assert(IsSame<MakeSigned<const signed char>::Type, const signed char>::value,
              "const signed char won't signify correctly");

static_assert(IsSame<MakeSigned<volatile signed short>::Type, volatile signed short>::value,
              "volatile signed short won't signify correctly");
static_assert(IsSame<MakeSigned<const volatile signed int>::Type, const volatile signed int>::value,
              "const volatile signed int won't signify correctly");
static_assert(IsSame<MakeSigned<signed long>::Type, signed long>::value,
              "signed long won't signify correctly");

static_assert(IsSame<MakeSigned<char>::Type, signed char>::value,
              "char won't signify correctly");
static_assert(IsSame<MakeSigned<volatile char>::Type, volatile signed char>::value,
              "volatile char won't signify correctly");
static_assert(IsSame<MakeSigned<const char>::Type, const signed char>::value,
              "const char won't signify correctly");

static_assert(IsSame<MakeUnsigned<const signed char>::Type, const unsigned char>::value,
              "const signed char won't unsignify correctly");
static_assert(IsSame<MakeUnsigned<volatile signed short>::Type, volatile unsigned short>::value,
              "volatile signed short won't unsignify correctly");
static_assert(IsSame<MakeUnsigned<const volatile signed int>::Type, const volatile unsigned int>::value,
              "const volatile signed int won't unsignify correctly");
static_assert(IsSame<MakeUnsigned<signed long>::Type, unsigned long>::value,
              "signed long won't unsignify correctly");

static_assert(IsSame<MakeUnsigned<const unsigned char>::Type, const unsigned char>::value,
              "const unsigned char won't unsignify correctly");

static_assert(IsSame<MakeUnsigned<volatile unsigned short>::Type, volatile unsigned short>::value,
              "volatile unsigned short won't unsignify correctly");
static_assert(IsSame<MakeUnsigned<const volatile unsigned int>::Type, const volatile unsigned int>::value,
              "const volatile unsigned int won't unsignify correctly");
static_assert(IsSame<MakeUnsigned<unsigned long>::Type, unsigned long>::value,
              "signed long won't unsignify correctly");

static_assert(IsSame<MakeUnsigned<char>::Type, unsigned char>::value,
              "char won't unsignify correctly");
static_assert(IsSame<MakeUnsigned<volatile char>::Type, volatile unsigned char>::value,
              "volatile char won't unsignify correctly");
static_assert(IsSame<MakeUnsigned<const char>::Type, const unsigned char>::value,
              "const char won't unsignify correctly");

int
main()
{
  CPlusPlus11IsBaseOf::StandardIsBaseOfTests();
  TestIsBaseOf();
  TestIsConvertible();
}
