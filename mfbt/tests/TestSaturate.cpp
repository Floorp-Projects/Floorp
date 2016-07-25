/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <mozilla/Saturate.h>

#include <mozilla/Assertions.h>

#include <limits>

using mozilla::detail::Saturate;

#define A(a) MOZ_RELEASE_ASSERT(a, "Test \'" #a "\'  failed.")

static const unsigned long sNumOps = 32;

template<typename T>
static T
StartValue()
{
  // Specialize |StartValue| for the given type.
  A(false);
}

template<>
int8_t
StartValue<int8_t>()
{
  return 0;
}

template<>
int16_t
StartValue<int16_t>()
{
  return 0;
}

template<>
int32_t
StartValue<int32_t>()
{
  return 0;
}

template<>
uint8_t
StartValue<uint8_t>()
{
  // Picking a value near middle of uint8_t's range.
  return static_cast<uint8_t>(std::numeric_limits<int8_t>::max());
}

template<>
uint16_t
StartValue<uint16_t>()
{
  // Picking a value near middle of uint16_t's range.
  return static_cast<uint8_t>(std::numeric_limits<int16_t>::max());
}

template<>
uint32_t
StartValue<uint32_t>()
{
  // Picking a value near middle of uint32_t's range.
  return static_cast<uint8_t>(std::numeric_limits<int32_t>::max());
}

// Add
//

template<typename T>
static void
TestPrefixIncr()
{
  T value = StartValue<T>();
  Saturate<T> satValue(value);

  for (T i = 0; i < static_cast<T>(sNumOps); ++i) {
    A(++value == ++satValue);
  }
}

template<typename T>
static void
TestPostfixIncr()
{
  T value = StartValue<T>();
  Saturate<T> satValue(value);

  for (T i = 0; i < static_cast<T>(sNumOps); ++i) {
    A(value++ == satValue++);
  }
}

template<typename T>
static void
TestAdd()
{
  T value = StartValue<T>();
  Saturate<T> satValue(value);

  for (T i = 0; i < static_cast<T>(sNumOps); ++i) {
    A((value + i) == (satValue + i));
  }
}

// Subtract
//

template<typename T>
static void
TestPrefixDecr()
{
  T value = StartValue<T>();
  Saturate<T> satValue(value);

  for (T i = 0; i < static_cast<T>(sNumOps); ++i) {
    A(--value == --satValue);
  }
}

template<typename T>
static void
TestPostfixDecr()
{
  T value = StartValue<T>();
  Saturate<T> satValue(value);

  for (T i = 0; i < static_cast<T>(sNumOps); ++i) {
    A(value-- == satValue--);
  }
}

template<typename T>
static void
TestSub()
{
  T value = StartValue<T>();
  Saturate<T> satValue(value);

  for (T i = 0; i < static_cast<T>(sNumOps); ++i) {
    A((value - i) == (satValue - i));
  }
}

// Corner cases near bounds
//

template<typename T>
static void
TestUpperBound()
{
  Saturate<T> satValue(std::numeric_limits<T>::max());

  A(--satValue == (std::numeric_limits<T>::max() - 1));
  A(++satValue == (std::numeric_limits<T>::max()));
  A(++satValue == (std::numeric_limits<T>::max())); // don't overflow here
  A(++satValue == (std::numeric_limits<T>::max())); // don't overflow here
  A(--satValue == (std::numeric_limits<T>::max() - 1)); // back at (max - 1)
  A(--satValue == (std::numeric_limits<T>::max() - 2));
}

template<typename T>
static void
TestLowerBound()
{
  Saturate<T> satValue(std::numeric_limits<T>::min());

  A(++satValue == (std::numeric_limits<T>::min() + 1));
  A(--satValue == (std::numeric_limits<T>::min()));
  A(--satValue == (std::numeric_limits<T>::min())); // don't overflow here
  A(--satValue == (std::numeric_limits<T>::min())); // don't overflow here
  A(++satValue == (std::numeric_limits<T>::min() + 1)); // back at (max + 1)
  A(++satValue == (std::numeric_limits<T>::min() + 2));
}

// Framework
//

template<typename T>
static void
TestAll()
{
  // Assert that we don't accidently hit type's range limits in tests.
  const T value = StartValue<T>();
  A(std::numeric_limits<T>::min() + static_cast<T>(sNumOps) <= value);
  A(std::numeric_limits<T>::max() - static_cast<T>(sNumOps) >= value);

  TestPrefixIncr<T>();
  TestPostfixIncr<T>();
  TestAdd<T>();

  TestPrefixDecr<T>();
  TestPostfixDecr<T>();
  TestSub<T>();

  TestUpperBound<T>();
  TestLowerBound<T>();
}

int
main()
{
  TestAll<int8_t>();
  TestAll<int16_t>();
  TestAll<int32_t>();
  TestAll<uint8_t>();
  TestAll<uint16_t>();
  TestAll<uint32_t>();
  return 0;
}
