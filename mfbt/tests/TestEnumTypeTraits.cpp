/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/EnumTypeTraits.h"

#include <cstdint>

using namespace mozilla;

/* Feature check for EnumTypeFitsWithin. */

#define MAKE_FIXED_EMUM_FOR_TYPE(IntType) \
  enum FixedEnumFor_##IntType : IntType{  \
      A_##IntType,                        \
      B_##IntType,                        \
      C_##IntType,                        \
  };

template <typename EnumType, typename IntType>
static void TestShouldFit() {
  static_assert(EnumTypeFitsWithin<EnumType, IntType>::value,
                "Should fit within exact/promoted integral type");
}

template <typename EnumType, typename IntType>
static void TestShouldNotFit() {
  static_assert(!EnumTypeFitsWithin<EnumType, IntType>::value,
                "Should not fit within");
}

void TestFitForTypes() {
  // check for int8_t
  MAKE_FIXED_EMUM_FOR_TYPE(int8_t);
  TestShouldFit<FixedEnumFor_int8_t, int8_t>();
  TestShouldFit<FixedEnumFor_int8_t, int16_t>();
  TestShouldFit<FixedEnumFor_int8_t, int32_t>();
  TestShouldFit<FixedEnumFor_int8_t, int64_t>();

  TestShouldNotFit<FixedEnumFor_int8_t, uint8_t>();
  TestShouldNotFit<FixedEnumFor_int8_t, uint16_t>();
  TestShouldNotFit<FixedEnumFor_int8_t, uint32_t>();
  TestShouldNotFit<FixedEnumFor_int8_t, uint64_t>();

  // check for uint8_t
  MAKE_FIXED_EMUM_FOR_TYPE(uint8_t);
  TestShouldFit<FixedEnumFor_uint8_t, uint8_t>();
  TestShouldFit<FixedEnumFor_uint8_t, uint16_t>();
  TestShouldFit<FixedEnumFor_uint8_t, uint32_t>();
  TestShouldFit<FixedEnumFor_uint8_t, uint64_t>();

  TestShouldNotFit<FixedEnumFor_uint8_t, int8_t>();
  TestShouldFit<FixedEnumFor_uint8_t, int16_t>();
  TestShouldFit<FixedEnumFor_uint8_t, int32_t>();
  TestShouldFit<FixedEnumFor_uint8_t, int64_t>();

  // check for int16_t
  MAKE_FIXED_EMUM_FOR_TYPE(int16_t);
  TestShouldNotFit<FixedEnumFor_int16_t, int8_t>();
  TestShouldFit<FixedEnumFor_int16_t, int16_t>();
  TestShouldFit<FixedEnumFor_int16_t, int32_t>();
  TestShouldFit<FixedEnumFor_int16_t, int64_t>();

  TestShouldNotFit<FixedEnumFor_int16_t, uint8_t>();
  TestShouldNotFit<FixedEnumFor_int16_t, uint16_t>();
  TestShouldNotFit<FixedEnumFor_int16_t, uint32_t>();
  TestShouldNotFit<FixedEnumFor_int16_t, uint64_t>();

  // check for uint16_t
  MAKE_FIXED_EMUM_FOR_TYPE(uint16_t);
  TestShouldNotFit<FixedEnumFor_uint16_t, uint8_t>();
  TestShouldFit<FixedEnumFor_uint16_t, uint16_t>();
  TestShouldFit<FixedEnumFor_uint16_t, uint32_t>();
  TestShouldFit<FixedEnumFor_uint16_t, uint64_t>();

  TestShouldNotFit<FixedEnumFor_uint16_t, int8_t>();
  TestShouldNotFit<FixedEnumFor_uint16_t, int16_t>();
  TestShouldFit<FixedEnumFor_uint16_t, int32_t>();
  TestShouldFit<FixedEnumFor_uint16_t, int64_t>();

  // check for int32_t
  MAKE_FIXED_EMUM_FOR_TYPE(int32_t);
  TestShouldNotFit<FixedEnumFor_int32_t, int8_t>();
  TestShouldNotFit<FixedEnumFor_int32_t, int16_t>();
  TestShouldFit<FixedEnumFor_int32_t, int32_t>();
  TestShouldFit<FixedEnumFor_int32_t, int64_t>();

  TestShouldNotFit<FixedEnumFor_int32_t, uint8_t>();
  TestShouldNotFit<FixedEnumFor_int32_t, uint16_t>();
  TestShouldNotFit<FixedEnumFor_int32_t, uint32_t>();
  TestShouldNotFit<FixedEnumFor_int32_t, uint64_t>();

  // check for uint32_t
  MAKE_FIXED_EMUM_FOR_TYPE(uint32_t);
  TestShouldNotFit<FixedEnumFor_uint32_t, uint8_t>();
  TestShouldNotFit<FixedEnumFor_uint32_t, uint16_t>();
  TestShouldFit<FixedEnumFor_uint32_t, uint32_t>();
  TestShouldFit<FixedEnumFor_uint32_t, uint64_t>();

  TestShouldNotFit<FixedEnumFor_uint32_t, int8_t>();
  TestShouldNotFit<FixedEnumFor_uint32_t, int16_t>();
  TestShouldNotFit<FixedEnumFor_uint32_t, int32_t>();
  TestShouldFit<FixedEnumFor_uint32_t, int64_t>();

  // check for int64_t
  MAKE_FIXED_EMUM_FOR_TYPE(int64_t);
  TestShouldNotFit<FixedEnumFor_int64_t, int8_t>();
  TestShouldNotFit<FixedEnumFor_int64_t, int16_t>();
  TestShouldNotFit<FixedEnumFor_int64_t, int32_t>();
  TestShouldFit<FixedEnumFor_int64_t, int64_t>();

  TestShouldNotFit<FixedEnumFor_int64_t, uint8_t>();
  TestShouldNotFit<FixedEnumFor_int64_t, uint16_t>();
  TestShouldNotFit<FixedEnumFor_int64_t, uint32_t>();
  TestShouldNotFit<FixedEnumFor_int64_t, uint64_t>();

  // check for uint64_t
  MAKE_FIXED_EMUM_FOR_TYPE(uint64_t);
  TestShouldNotFit<FixedEnumFor_uint64_t, uint8_t>();
  TestShouldNotFit<FixedEnumFor_uint64_t, uint16_t>();
  TestShouldNotFit<FixedEnumFor_uint64_t, uint32_t>();
  TestShouldFit<FixedEnumFor_uint64_t, uint64_t>();

  TestShouldNotFit<FixedEnumFor_uint64_t, int8_t>();
  TestShouldNotFit<FixedEnumFor_uint64_t, int16_t>();
  TestShouldNotFit<FixedEnumFor_uint64_t, int32_t>();
  TestShouldNotFit<FixedEnumFor_uint64_t, int64_t>();
}

// -

template <typename T, typename U>
static constexpr void AssertSameTypeAndValue(T a, U b) {
  static_assert(std::is_same_v<T, U>);
  MOZ_ASSERT(a == b);
}

void TestUnderlyingValue() {
  enum class Pet : int16_t { Cat, Dog, Fish };
  enum class Plant { Flower, Tree, Vine };

  AssertSameTypeAndValue(UnderlyingValue(Pet::Cat), int16_t(0));
  AssertSameTypeAndValue(UnderlyingValue(Pet::Dog), int16_t(1));
  AssertSameTypeAndValue(UnderlyingValue(Pet::Fish), int16_t(2));

  AssertSameTypeAndValue(UnderlyingValue(Plant::Flower), int(0));
  AssertSameTypeAndValue(UnderlyingValue(Plant::Tree), int(1));
  AssertSameTypeAndValue(UnderlyingValue(Plant::Vine), int(2));
}

// -

int main() {
  TestFitForTypes();
  TestUnderlyingValue();
  return 0;
}
