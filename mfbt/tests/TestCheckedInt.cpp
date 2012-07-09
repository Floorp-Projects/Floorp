/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/CheckedInt.h"

#include <iostream>
#include <climits>

#ifndef MOZ_CHECKEDINT_ENABLE_MOZ_ASSERTS
#  error MOZ_CHECKEDINT_ENABLE_MOZ_ASSERTS should be defined by CheckedInt.h
#endif

using namespace mozilla;

int gIntegerTypesTested = 0;
int gTestsPassed = 0;
int gTestsFailed = 0;

void verifyImplFunction(bool x, bool expected,
                        const char* file, int line,
                        int size, bool isTSigned)
{
  if (x == expected) {
    gTestsPassed++;
  } else {
    gTestsFailed++;
    std::cerr << "Test failed at " << file << ":" << line;
    std::cerr << " with T a ";
    if (isTSigned)
      std::cerr << "signed";
    else
      std::cerr << "unsigned";
    std::cerr << " " << CHAR_BIT*size << "-bit integer type" << std::endl;
  }
}

#define VERIFY_IMPL(x, expected) \
    verifyImplFunction((x), \
    (expected), \
    __FILE__, \
    __LINE__, \
    sizeof(T), \
    detail::IsSigned<T>::value)

#define VERIFY(x)            VERIFY_IMPL(x, true)
#define VERIFY_IS_FALSE(x)   VERIFY_IMPL(x, false)
#define VERIFY_IS_VALID(x)   VERIFY_IMPL((x).isValid(), true)
#define VERIFY_IS_INVALID(x) VERIFY_IMPL((x).isValid(), false)
#define VERIFY_IS_VALID_IF(x,condition) VERIFY_IMPL((x).isValid(), (condition))

template<typename T, size_t Size = sizeof(T)>
struct testTwiceBiggerType
{
    static void run()
    {
      VERIFY(detail::IsSupported<typename detail::TwiceBiggerType<T>::Type>::value);
      VERIFY(sizeof(typename detail::TwiceBiggerType<T>::Type)
               == 2 * sizeof(T));
      VERIFY(bool(detail::IsSigned<typename detail::TwiceBiggerType<T>::Type>::value)
               == bool(detail::IsSigned<T>::value));
    }
};

template<typename T>
struct testTwiceBiggerType<T, 8>
{
    static void run()
    {
      VERIFY_IS_FALSE(detail::IsSupported<
                        typename detail::TwiceBiggerType<T>::Type
                      >::value);
    }
};


template<typename T>
void test()
{
  static bool alreadyRun = false;
  // Integer types from different families may just be typedefs for types from other families.
  // e.g. int32_t might be just a typedef for int. No point re-running the same tests then.
  if (alreadyRun)
      return;
  alreadyRun = true;

  VERIFY(detail::IsSupported<T>::value);
  const bool isTSigned = detail::IsSigned<T>::value;
  VERIFY(bool(isTSigned) == !bool(T(-1) > T(0)));

  testTwiceBiggerType<T>::run();

  typedef typename detail::UnsignedType<T>::Type unsignedT;

  VERIFY(sizeof(unsignedT) == sizeof(T));
  VERIFY(detail::IsSigned<unsignedT>::value == false);

  const CheckedInt<T> max(detail::MaxValue<T>::value);
  const CheckedInt<T> min(detail::MinValue<T>::value);

  // Check MinValue and MaxValue, since they are custom implementations and a mistake there
  // could potentially NOT be caught by any other tests... while making everything wrong!

  unsignedT bit = 1;
  unsignedT unsignedMinValue(min.value());
  unsignedT unsignedMaxValue(max.value());
  for (size_t i = 0; i < sizeof(T) * CHAR_BIT - 1; i++)
  {
    VERIFY((unsignedMinValue & bit) == 0);
    bit <<= 1;
  }
  VERIFY((unsignedMinValue & bit) == (isTSigned ? bit : unsignedT(0)));
  VERIFY(unsignedMaxValue == unsignedT(~unsignedMinValue));

  const CheckedInt<T> zero(0);
  const CheckedInt<T> one(1);
  const CheckedInt<T> two(2);
  const CheckedInt<T> three(3);
  const CheckedInt<T> four(4);

  /* Addition / subtraction checks */

  VERIFY_IS_VALID(zero + zero);
  VERIFY(zero + zero == zero);
  VERIFY_IS_FALSE(zero + zero == one); // Check that == doesn't always return true
  VERIFY_IS_VALID(zero + one);
  VERIFY(zero + one == one);
  VERIFY_IS_VALID(one + one);
  VERIFY(one + one == two);

  const CheckedInt<T> maxMinusOne = max - one;
  const CheckedInt<T> maxMinusTwo = max - two;
  VERIFY_IS_VALID(maxMinusOne);
  VERIFY_IS_VALID(maxMinusTwo);
  VERIFY_IS_VALID(maxMinusOne + one);
  VERIFY_IS_VALID(maxMinusTwo + one);
  VERIFY_IS_VALID(maxMinusTwo + two);
  VERIFY(maxMinusOne + one == max);
  VERIFY(maxMinusTwo + one == maxMinusOne);
  VERIFY(maxMinusTwo + two == max);

  VERIFY_IS_VALID(max + zero);
  VERIFY_IS_VALID(max - zero);
  VERIFY_IS_INVALID(max + one);
  VERIFY_IS_INVALID(max + two);
  VERIFY_IS_INVALID(max + maxMinusOne);
  VERIFY_IS_INVALID(max + max);

  const CheckedInt<T> minPlusOne = min + one;
  const CheckedInt<T> minPlusTwo = min + two;
  VERIFY_IS_VALID(minPlusOne);
  VERIFY_IS_VALID(minPlusTwo);
  VERIFY_IS_VALID(minPlusOne - one);
  VERIFY_IS_VALID(minPlusTwo - one);
  VERIFY_IS_VALID(minPlusTwo - two);
  VERIFY(minPlusOne - one == min);
  VERIFY(minPlusTwo - one == minPlusOne);
  VERIFY(minPlusTwo - two == min);

  const CheckedInt<T> minMinusOne = min - one;
  VERIFY_IS_VALID(min + zero);
  VERIFY_IS_VALID(min - zero);
  VERIFY_IS_INVALID(min - one);
  VERIFY_IS_INVALID(min - two);
  VERIFY_IS_INVALID(min - minMinusOne);
  VERIFY_IS_VALID(min - min);

  const CheckedInt<T> maxOverTwo = max / two;
  VERIFY_IS_VALID(maxOverTwo + maxOverTwo);
  VERIFY_IS_VALID(maxOverTwo + one);
  VERIFY((maxOverTwo + one) - one == maxOverTwo);
  VERIFY_IS_VALID(maxOverTwo - maxOverTwo);
  VERIFY(maxOverTwo - maxOverTwo == zero);

  const CheckedInt<T> minOverTwo = min / two;
  VERIFY_IS_VALID(minOverTwo + minOverTwo);
  VERIFY_IS_VALID(minOverTwo + one);
  VERIFY((minOverTwo + one) - one == minOverTwo);
  VERIFY_IS_VALID(minOverTwo - minOverTwo);
  VERIFY(minOverTwo - minOverTwo == zero);

  VERIFY_IS_INVALID(min - one);
  VERIFY_IS_INVALID(min - two);

  if (isTSigned) {
    VERIFY_IS_INVALID(min + min);
    VERIFY_IS_INVALID(minOverTwo + minOverTwo + minOverTwo);
    VERIFY_IS_INVALID(zero - min + min);
    VERIFY_IS_INVALID(one - min + min);
  }

  /* Unary operator- checks */

  const CheckedInt<T> negOne = -one;
  const CheckedInt<T> negTwo = -two;

  if (isTSigned) {
    VERIFY_IS_VALID(-max);
    VERIFY_IS_VALID(-max - one);
    VERIFY_IS_VALID(negOne);
    VERIFY_IS_VALID(-max + negOne);
    VERIFY_IS_VALID(negOne + one);
    VERIFY(negOne + one == zero);
    VERIFY_IS_VALID(negTwo);
    VERIFY_IS_VALID(negOne + negOne);
    VERIFY(negOne + negOne == negTwo);
  } else {
    VERIFY_IS_INVALID(negOne);
  }

  /* multiplication checks */

  VERIFY_IS_VALID(zero * zero);
  VERIFY(zero * zero == zero);
  VERIFY_IS_VALID(zero * one);
  VERIFY(zero * one == zero);
  VERIFY_IS_VALID(one * zero);
  VERIFY(one * zero == zero);
  VERIFY_IS_VALID(one * one);
  VERIFY(one * one == one);
  VERIFY_IS_VALID(one * three);
  VERIFY(one * three == three);
  VERIFY_IS_VALID(two * two);
  VERIFY(two * two == four);

  VERIFY_IS_INVALID(max * max);
  VERIFY_IS_INVALID(maxOverTwo * max);
  VERIFY_IS_INVALID(maxOverTwo * maxOverTwo);

  const CheckedInt<T> maxApproxSqrt(T(T(1) << (CHAR_BIT*sizeof(T)/2)));

  VERIFY_IS_VALID(maxApproxSqrt);
  VERIFY_IS_VALID(maxApproxSqrt * two);
  VERIFY_IS_INVALID(maxApproxSqrt * maxApproxSqrt);
  VERIFY_IS_INVALID(maxApproxSqrt * maxApproxSqrt * maxApproxSqrt);

  if (isTSigned) {
    VERIFY_IS_INVALID(min * min);
    VERIFY_IS_INVALID(minOverTwo * min);
    VERIFY_IS_INVALID(minOverTwo * minOverTwo);

    const CheckedInt<T> minApproxSqrt = -maxApproxSqrt;

    VERIFY_IS_VALID(minApproxSqrt);
    VERIFY_IS_VALID(minApproxSqrt * two);
    VERIFY_IS_INVALID(minApproxSqrt * maxApproxSqrt);
    VERIFY_IS_INVALID(minApproxSqrt * minApproxSqrt);
  }

  // make sure to check all 4 paths in signed multiplication validity check.
  // test positive * positive
  VERIFY_IS_VALID(max * one);
  VERIFY(max * one == max);
  VERIFY_IS_INVALID(max * two);
  VERIFY_IS_VALID(maxOverTwo * two);
  VERIFY((maxOverTwo + maxOverTwo) == (maxOverTwo * two));

  if (isTSigned) {
    // test positive * negative
    VERIFY_IS_VALID(max * negOne);
    VERIFY_IS_VALID(-max);
    VERIFY(max * negOne == -max);
    VERIFY_IS_VALID(one * min);
    VERIFY_IS_INVALID(max * negTwo);
    VERIFY_IS_VALID(maxOverTwo * negTwo);
    VERIFY_IS_VALID(two * minOverTwo);
    VERIFY_IS_VALID((maxOverTwo + one) * negTwo);
    VERIFY_IS_INVALID((maxOverTwo + two) * negTwo);
    VERIFY_IS_INVALID(two * (minOverTwo - one));

    // test negative * positive
    VERIFY_IS_VALID(min * one);
    VERIFY_IS_VALID(minPlusOne * one);
    VERIFY_IS_INVALID(min * two);
    VERIFY_IS_VALID(minOverTwo * two);
    VERIFY(minOverTwo * two == min);
    VERIFY_IS_INVALID((minOverTwo - one) * negTwo);
    VERIFY_IS_INVALID(negTwo * max);
    VERIFY_IS_VALID(minOverTwo * two);
    VERIFY(minOverTwo * two == min);
    VERIFY_IS_VALID(negTwo * maxOverTwo);
    VERIFY_IS_INVALID((minOverTwo - one) * two);
    VERIFY_IS_VALID(negTwo * (maxOverTwo + one));
    VERIFY_IS_INVALID(negTwo * (maxOverTwo + two));

    // test negative * negative
    VERIFY_IS_INVALID(min * negOne);
    VERIFY_IS_VALID(minPlusOne * negOne);
    VERIFY(minPlusOne * negOne == max);
    VERIFY_IS_INVALID(min * negTwo);
    VERIFY_IS_INVALID(minOverTwo * negTwo);
    VERIFY_IS_INVALID(negOne * min);
    VERIFY_IS_VALID(negOne * minPlusOne);
    VERIFY(negOne * minPlusOne == max);
    VERIFY_IS_INVALID(negTwo * min);
    VERIFY_IS_INVALID(negTwo * minOverTwo);
  }

  /* Division checks */

  VERIFY_IS_VALID(one / one);
  VERIFY(one / one == one);
  VERIFY_IS_VALID(three / three);
  VERIFY(three / three == one);
  VERIFY_IS_VALID(four / two);
  VERIFY(four / two == two);
  VERIFY((four*three)/four == three);

  // Check that div by zero is invalid
  VERIFY_IS_INVALID(zero / zero);
  VERIFY_IS_INVALID(one / zero);
  VERIFY_IS_INVALID(two / zero);
  VERIFY_IS_INVALID(negOne / zero);
  VERIFY_IS_INVALID(max / zero);
  VERIFY_IS_INVALID(min / zero);

  if (isTSigned) {
    // Check that min / -1 is invalid
    VERIFY_IS_INVALID(min / negOne);

    // Check that the test for div by -1 isn't banning other numerators than min
    VERIFY_IS_VALID(one / negOne);
    VERIFY_IS_VALID(zero / negOne);
    VERIFY_IS_VALID(negOne / negOne);
    VERIFY_IS_VALID(max / negOne);
  }

  /* Check that invalidity is correctly preserved by arithmetic ops */

  const CheckedInt<T> someInvalid = max + max;
  VERIFY_IS_INVALID(someInvalid + zero);
  VERIFY_IS_INVALID(someInvalid - zero);
  VERIFY_IS_INVALID(zero + someInvalid);
  VERIFY_IS_INVALID(zero - someInvalid);
  VERIFY_IS_INVALID(-someInvalid);
  VERIFY_IS_INVALID(someInvalid * zero);
  VERIFY_IS_INVALID(someInvalid * one);
  VERIFY_IS_INVALID(zero * someInvalid);
  VERIFY_IS_INVALID(one * someInvalid);
  VERIFY_IS_INVALID(someInvalid / zero);
  VERIFY_IS_INVALID(someInvalid / one);
  VERIFY_IS_INVALID(zero / someInvalid);
  VERIFY_IS_INVALID(one / someInvalid);
  VERIFY_IS_INVALID(someInvalid + someInvalid);
  VERIFY_IS_INVALID(someInvalid - someInvalid);
  VERIFY_IS_INVALID(someInvalid * someInvalid);
  VERIFY_IS_INVALID(someInvalid / someInvalid);

  /* Check that mixing checked integers with plain integers in expressions is allowed */

  VERIFY(one + T(2) == three);
  VERIFY(2 + one == three);
  {
    CheckedInt<T> x = one;
    x += 2;
    VERIFY(x == three);
  }
  VERIFY(two - 1 == one);
  VERIFY(2 - one == one);
  {
    CheckedInt<T> x = two;
    x -= 1;
    VERIFY(x == one);
  }
  VERIFY(one * 2 == two);
  VERIFY(2 * one == two);
  {
    CheckedInt<T> x = one;
    x *= 2;
    VERIFY(x == two);
  }
  VERIFY(four / 2 == two);
  VERIFY(4 / two == two);
  {
    CheckedInt<T> x = four;
    x /= 2;
    VERIFY(x == two);
  }

  VERIFY(one == 1);
  VERIFY(1 == one);
  VERIFY_IS_FALSE(two == 1);
  VERIFY_IS_FALSE(1 == two);
  VERIFY_IS_FALSE(someInvalid == 1);
  VERIFY_IS_FALSE(1 == someInvalid);

  /* Check that construction of CheckedInt from an integer value of a mismatched type is checked */

  #define VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(U) \
  { \
    bool isUSigned = detail::IsSigned<U>::value; \
    VERIFY_IS_VALID(CheckedInt<T>(U(0))); \
    VERIFY_IS_VALID(CheckedInt<T>(U(1))); \
    VERIFY_IS_VALID(CheckedInt<T>(U(100))); \
    if (isUSigned) \
      VERIFY_IS_VALID_IF(CheckedInt<T>(U(-1)), isTSigned); \
    if (sizeof(U) > sizeof(T)) \
      VERIFY_IS_INVALID(CheckedInt<T>(U(detail::MaxValue<T>::value) + one.value())); \
    VERIFY_IS_VALID_IF(CheckedInt<T>(detail::MaxValue<U>::value), \
      (sizeof(T) > sizeof(U) || ((sizeof(T) == sizeof(U)) && (isUSigned || !isTSigned)))); \
    VERIFY_IS_VALID_IF(CheckedInt<T>(detail::MinValue<U>::value), \
      isUSigned == false ? 1 : \
      bool(isTSigned) == false ? 0 : \
      sizeof(T) >= sizeof(U)); \
  }
  VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(int8_t)
  VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(uint8_t)
  VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(int16_t)
  VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(uint16_t)
  VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(int32_t)
  VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(uint32_t)
  VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(int64_t)
  VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(uint64_t)

  typedef unsigned char unsignedChar;
  typedef unsigned short unsignedShort;
  typedef unsigned int  unsignedInt;
  typedef unsigned long unsignedLong;

  VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(char)
  VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(unsignedChar)
  VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(short)
  VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(unsignedShort)
  VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(int)
  VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(unsignedInt)
  VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(long)
  VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(unsignedLong)

  /* Test increment/decrement operators */

  CheckedInt<T> x, y;
  x = one;
  y = x++;
  VERIFY(x == two);
  VERIFY(y == one);
  x = one;
  y = ++x;
  VERIFY(x == two);
  VERIFY(y == two);
  x = one;
  y = x--;
  VERIFY(x == zero);
  VERIFY(y == one);
  x = one;
  y = --x;
  VERIFY(x == zero);
  VERIFY(y == zero);
  x = max;
  VERIFY_IS_VALID(x++);
  x = max;
  VERIFY_IS_INVALID(++x);
  x = min;
  VERIFY_IS_VALID(x--);
  x = min;
  VERIFY_IS_INVALID(--x);

  gIntegerTypesTested++;
}

int main()
{
  test<int8_t>();
  test<uint8_t>();
  test<int16_t>();
  test<uint16_t>();
  test<int32_t>();
  test<uint32_t>();
  test<int64_t>();
  test<uint64_t>();

  test<char>();
  test<unsigned char>();
  test<short>();
  test<unsigned short>();
  test<int>();
  test<unsigned int>();
  test<long>();
  test<unsigned long>();

  if (gIntegerTypesTested < 8) {
    std::cerr << "Only " << gIntegerTypesTested << " have been tested. "
              << "This should not be less than 8." << std::endl;
    gTestsFailed++;
  }

  std::cerr << gTestsFailed << " tests failed, "
            << gTestsPassed << " tests passed out of "
            << gTestsFailed + gTestsPassed
            << " tests, covering " << gIntegerTypesTested
            << " distinct integer types." << std::endl;

  return gTestsFailed > 0;
}
