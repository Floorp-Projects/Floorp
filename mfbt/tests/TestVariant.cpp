/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/UniquePtr.h"
#include "mozilla/Variant.h"

using mozilla::IsSame;
using mozilla::MakeUnique;
using mozilla::UniquePtr;
using mozilla::Variant;

struct Destroyer {
  static int destroyedCount;
  ~Destroyer() { destroyedCount++; }
};

int Destroyer::destroyedCount = 0;

static void testDetails() {
  printf("testDetails\n");

  using mozilla::detail::Nth;

  // Test Nth with a list of 1 item.
  static_assert(IsSame<typename Nth<0, int>::Type, int>::value,
                "Nth<0, int>::Type should be int");

  // Test Nth with a list of more than 1 item.
  static_assert(IsSame<typename Nth<0, int, char>::Type, int>::value,
                "Nth<0, int, char>::Type should be int");
  static_assert(IsSame<typename Nth<1, int, char>::Type, char>::value,
                "Nth<1, int, char>::Type should be char");

  using mozilla::detail::SelectVariantType;

  // SelectVariantType for zero items (shouldn't happen, but `count` should
  // still work ok.)
  static_assert(SelectVariantType<int, char>::count == 0,
                "SelectVariantType<int, char>::count should be 0");

  // SelectVariantType for 1 type, for all combinations from/to T, const T,
  // const T&, T&&
  // - type to type
  static_assert(IsSame<typename SelectVariantType<int, int>::Type, int>::value,
                "SelectVariantType<int, int>::Type should be int");
  static_assert(SelectVariantType<int, int>::count == 1,
                "SelectVariantType<int, int>::count should be 1");

  // - type to const type
  static_assert(IsSame<typename SelectVariantType<int, const int>::Type,
                       const int>::value,
                "SelectVariantType<int, const int>::Type should be const int");
  static_assert(SelectVariantType<int, const int>::count == 1,
                "SelectVariantType<int, const int>::count should be 1");

  // - type to const type&
  static_assert(
      IsSame<typename SelectVariantType<int, const int&>::Type,
             const int&>::value,
      "SelectVariantType<int, const int&>::Type should be const int&");
  static_assert(SelectVariantType<int, const int&>::count == 1,
                "SelectVariantType<int, const int&>::count should be 1");

  // - type to type&&
  static_assert(
      IsSame<typename SelectVariantType<int, int&&>::Type, int&&>::value,
      "SelectVariantType<int, int&&>::Type should be int&&");
  static_assert(SelectVariantType<int, int&&>::count == 1,
                "SelectVariantType<int, int&&>::count should be 1");

  // - const type to type
  static_assert(
      IsSame<typename SelectVariantType<const int, int>::Type, int>::value,
      "SelectVariantType<const int, int>::Type should be int");
  static_assert(SelectVariantType<const int, int>::count == 1,
                "SelectVariantType<const int, int>::count should be 1");

  // - const type to const type
  static_assert(
      IsSame<typename SelectVariantType<const int, const int>::Type,
             const int>::value,
      "SelectVariantType<const int, const int>::Type should be const int");
  static_assert(SelectVariantType<const int, const int>::count == 1,
                "SelectVariantType<const int, const int>::count should be 1");

  // - const type to const type&
  static_assert(
      IsSame<typename SelectVariantType<const int, const int&>::Type,
             const int&>::value,
      "SelectVariantType<const int, const int&>::Type should be const int&");
  static_assert(SelectVariantType<const int, const int&>::count == 1,
                "SelectVariantType<const int, const int&>::count should be 1");

  // - const type to type&&
  static_assert(
      IsSame<typename SelectVariantType<const int, int&&>::Type, int&&>::value,
      "SelectVariantType<const int, int&&>::Type should be int&&");
  static_assert(SelectVariantType<const int, int&&>::count == 1,
                "SelectVariantType<const int, int&&>::count should be 1");

  // - const type& to type
  static_assert(
      IsSame<typename SelectVariantType<const int&, int>::Type, int>::value,
      "SelectVariantType<const int&, int>::Type should be int");
  static_assert(SelectVariantType<const int&, int>::count == 1,
                "SelectVariantType<const int&, int>::count should be 1");

  // - const type& to const type
  static_assert(
      IsSame<typename SelectVariantType<const int&, const int>::Type,
             const int>::value,
      "SelectVariantType<const int&, const int>::Type should be const int");
  static_assert(SelectVariantType<const int&, const int>::count == 1,
                "SelectVariantType<const int&, const int>::count should be 1");

  // - const type& to const type&
  static_assert(
      IsSame<typename SelectVariantType<const int&, const int&>::Type,
             const int&>::value,
      "SelectVariantType<const int&, const int&>::Type should be const int&");
  static_assert(SelectVariantType<const int&, const int&>::count == 1,
                "SelectVariantType<const int&, const int&>::count should be 1");

  // - const type& to type&&
  static_assert(
      IsSame<typename SelectVariantType<const int&, int&&>::Type, int&&>::value,
      "SelectVariantType<const int&, int&&>::Type should be int&&");
  static_assert(SelectVariantType<const int&, int&&>::count == 1,
                "SelectVariantType<const int&, int&&>::count should be 1");

  // - type&& to type
  static_assert(
      IsSame<typename SelectVariantType<int&&, int>::Type, int>::value,
      "SelectVariantType<int&&, int>::Type should be int");
  static_assert(SelectVariantType<int&&, int>::count == 1,
                "SelectVariantType<int&&, int>::count should be 1");

  // - type&& to const type
  static_assert(
      IsSame<typename SelectVariantType<int&&, const int>::Type,
             const int>::value,
      "SelectVariantType<int&&, const int>::Type should be const int");
  static_assert(SelectVariantType<int&&, const int>::count == 1,
                "SelectVariantType<int&&, const int>::count should be 1");

  // - type&& to const type&
  static_assert(
      IsSame<typename SelectVariantType<int&&, const int&>::Type,
             const int&>::value,
      "SelectVariantType<int&&, const int&>::Type should be const int&");
  static_assert(SelectVariantType<int&&, const int&>::count == 1,
                "SelectVariantType<int&&, const int&>::count should be 1");

  // - type&& to type&&
  static_assert(
      IsSame<typename SelectVariantType<int&&, int&&>::Type, int&&>::value,
      "SelectVariantType<int&&, int&&>::Type should be int&&");
  static_assert(SelectVariantType<int&&, int&&>::count == 1,
                "SelectVariantType<int&&, int&&>::count should be 1");

  // SelectVariantType for two different types.
  // (Don't test all combinations, trust that the above tests are sufficient.)
  static_assert(
      IsSame<typename SelectVariantType<int, int, char>::Type, int>::value,
      "SelectVariantType<int, int, char>::Type should be int");
  static_assert(SelectVariantType<int, int, char>::count == 1,
                "SelectVariantType<int, int, char>::count should be 1");
  static_assert(
      IsSame<typename SelectVariantType<char, int, char>::Type, char>::value,
      "SelectVariantType<char, int, char>::Type should be char");
  static_assert(SelectVariantType<char, int, char>::count == 1,
                "SelectVariantType<char, int, char>::count should be 1");

  // SelectVariantType for two identical types.
  static_assert(
      IsSame<typename SelectVariantType<int, int, int>::Type, int>::value,
      "SelectVariantType<int, int, int>::Type should be int");
  static_assert(SelectVariantType<int, int, int>::count == 2,
                "SelectVariantType<int, int, int>::count should be 2");

  // SelectVariantType for two identical types, with others around.
  static_assert(
      IsSame<typename SelectVariantType<int, char, int, int>::Type, int>::value,
      "SelectVariantType<int, char, int, int>::Type should be int");
  static_assert(SelectVariantType<int, char, int, int>::count == 2,
                "SelectVariantType<int, char, int, int>::count should be 2");

  static_assert(
      IsSame<typename SelectVariantType<int, int, char, int>::Type, int>::value,
      "SelectVariantType<int, int, char, int>::Type should be int");
  static_assert(SelectVariantType<int, int, char, int>::count == 2,
                "SelectVariantType<int, int, char, int>::count should be 2");

  static_assert(
      IsSame<typename SelectVariantType<int, int, int, char>::Type, int>::value,
      "SelectVariantType<int, int, int, char>::Type should be int");
  static_assert(SelectVariantType<int, int, int, char>::count == 2,
                "SelectVariantType<int, int, int, char>::count should be 2");

  static_assert(
      IsSame<typename SelectVariantType<int, char, int, char, int, char>::Type,
             int>::value,
      "SelectVariantType<int, char, int, char, int, char>::Type should be int");
  static_assert(
      SelectVariantType<int, char, int, char, int, char>::count == 2,
      "SelectVariantType<int, char, int, char, int, char>::count should be 2");

  // SelectVariantType for two identically-selectable types (first one wins!).
  static_assert(
      IsSame<typename SelectVariantType<int, int, const int>::Type, int>::value,
      "SelectVariantType<int, int, const int>::Type should be int");
  static_assert(SelectVariantType<int, int, const int>::count == 2,
                "SelectVariantType<int, int, const int>::count should be 2");
  static_assert(
      IsSame<typename SelectVariantType<int, const int, int>::Type,
             const int>::value,
      "SelectVariantType<int, const int, int>::Type should be const int");
  static_assert(SelectVariantType<int, const int, int>::count == 2,
                "SelectVariantType<int, const int, int>::count should be 2");
  static_assert(
      IsSame<typename SelectVariantType<int, const int, int&&>::Type,
             const int>::value,
      "SelectVariantType<int, const int, int&&>::Type should be const int");
  static_assert(SelectVariantType<int, const int, int&&>::count == 2,
                "SelectVariantType<int, const int, int&&>::count should be 2");
}

static void testSimple() {
  printf("testSimple\n");
  Variant<uint32_t, uint64_t> v(uint64_t(1));
  MOZ_RELEASE_ASSERT(v.is<uint64_t>());
  MOZ_RELEASE_ASSERT(!v.is<uint32_t>());
  MOZ_RELEASE_ASSERT(v.as<uint64_t>() == 1);

  MOZ_RELEASE_ASSERT(v.is<1>());
  MOZ_RELEASE_ASSERT(!v.is<0>());
  static_assert(IsSame<decltype(v.as<1>()), uint64_t&>::value,
                "as<1>() should return a uint64_t");
  MOZ_RELEASE_ASSERT(v.as<1>() == 1);
}

static void testDuplicate() {
  printf("testDuplicate\n");
  Variant<uint32_t, uint64_t, uint32_t> v(uint64_t(1));
  MOZ_RELEASE_ASSERT(v.is<uint64_t>());
  MOZ_RELEASE_ASSERT(v.as<uint64_t>() == 1);
  // Note: uint32_t is not unique, so `v.is<uint32_t>()` is not allowed.

  MOZ_RELEASE_ASSERT(v.is<1>());
  MOZ_RELEASE_ASSERT(!v.is<0>());
  MOZ_RELEASE_ASSERT(!v.is<2>());
  static_assert(IsSame<decltype(v.as<0>()), uint32_t&>::value,
                "as<0>() should return a uint64_t");
  static_assert(IsSame<decltype(v.as<1>()), uint64_t&>::value,
                "as<1>() should return a uint64_t");
  static_assert(IsSame<decltype(v.as<2>()), uint32_t&>::value,
                "as<2>() should return a uint64_t");
  MOZ_RELEASE_ASSERT(v.as<1>() == 1);
  MOZ_RELEASE_ASSERT(v.extract<1>() == 1);
}

static void testConstructionWithVariantType() {
  Variant<uint32_t, uint64_t, uint32_t> v(mozilla::VariantType<uint64_t>{}, 3);
  MOZ_RELEASE_ASSERT(v.is<uint64_t>());
  // MOZ_RELEASE_ASSERT(!v.is<uint32_t>()); // uint32_t is not unique!
  MOZ_RELEASE_ASSERT(v.as<uint64_t>() == 3);
}

static void testConstructionWithVariantIndex() {
  Variant<uint32_t, uint64_t, uint32_t> v(mozilla::VariantIndex<2>{}, 2);
  MOZ_RELEASE_ASSERT(!v.is<uint64_t>());
  // Note: uint32_t is not unique, so `v.is<uint32_t>()` is not allowed.

  MOZ_RELEASE_ASSERT(!v.is<1>());
  MOZ_RELEASE_ASSERT(!v.is<0>());
  MOZ_RELEASE_ASSERT(v.is<2>());
  MOZ_RELEASE_ASSERT(v.as<2>() == 2);
  MOZ_RELEASE_ASSERT(v.extract<2>() == 2);
}

static void testCopy() {
  printf("testCopy\n");
  Variant<uint32_t, uint64_t> v1(uint64_t(1));
  Variant<uint32_t, uint64_t> v2(v1);
  MOZ_RELEASE_ASSERT(v2.is<uint64_t>());
  MOZ_RELEASE_ASSERT(!v2.is<uint32_t>());
  MOZ_RELEASE_ASSERT(v2.as<uint64_t>() == 1);

  Variant<uint32_t, uint64_t> v3(uint32_t(10));
  v3 = v2;
  MOZ_RELEASE_ASSERT(v3.is<uint64_t>());
  MOZ_RELEASE_ASSERT(v3.as<uint64_t>() == 1);
}

static void testMove() {
  printf("testMove\n");
  Variant<UniquePtr<int>, char> v1(MakeUnique<int>(5));
  Variant<UniquePtr<int>, char> v2(std::move(v1));

  MOZ_RELEASE_ASSERT(v2.is<UniquePtr<int>>());
  MOZ_RELEASE_ASSERT(*v2.as<UniquePtr<int>>() == 5);

  MOZ_RELEASE_ASSERT(v1.is<UniquePtr<int>>());
  MOZ_RELEASE_ASSERT(v1.as<UniquePtr<int>>() == nullptr);

  Destroyer::destroyedCount = 0;
  {
    Variant<char, UniquePtr<Destroyer>> v3(MakeUnique<Destroyer>());
    Variant<char, UniquePtr<Destroyer>> v4(std::move(v3));

    Variant<char, UniquePtr<Destroyer>> v5('a');
    v5 = std::move(v4);

    auto ptr = v5.extract<UniquePtr<Destroyer>>();
    MOZ_RELEASE_ASSERT(Destroyer::destroyedCount == 0);
  }
  MOZ_RELEASE_ASSERT(Destroyer::destroyedCount == 1);
}

static void testDestructor() {
  printf("testDestructor\n");
  Destroyer::destroyedCount = 0;

  {
    Destroyer d;

    {
      Variant<char, UniquePtr<char[]>, Destroyer> v(d);
      MOZ_RELEASE_ASSERT(Destroyer::destroyedCount == 0);  // None detroyed yet.
    }

    MOZ_RELEASE_ASSERT(Destroyer::destroyedCount ==
                       1);  // v's copy of d is destroyed.
  }

  MOZ_RELEASE_ASSERT(Destroyer::destroyedCount == 2);  // d is destroyed.
}

static void testEquality() {
  printf("testEquality\n");
  using V = Variant<char, int>;

  V v0('a');
  V v1('b');
  V v2('b');
  V v3(42);
  V v4(27);
  V v5(27);
  V v6(int('b'));

  MOZ_RELEASE_ASSERT(v0 != v1);
  MOZ_RELEASE_ASSERT(v1 == v2);
  MOZ_RELEASE_ASSERT(v2 != v3);
  MOZ_RELEASE_ASSERT(v3 != v4);
  MOZ_RELEASE_ASSERT(v4 == v5);
  MOZ_RELEASE_ASSERT(v1 != v6);

  MOZ_RELEASE_ASSERT(v0 == v0);
  MOZ_RELEASE_ASSERT(v1 == v1);
  MOZ_RELEASE_ASSERT(v2 == v2);
  MOZ_RELEASE_ASSERT(v3 == v3);
  MOZ_RELEASE_ASSERT(v4 == v4);
  MOZ_RELEASE_ASSERT(v5 == v5);
  MOZ_RELEASE_ASSERT(v6 == v6);
}

struct Describer {
  static const char* little;
  static const char* medium;
  static const char* big;

  const char* operator()(const uint8_t&) { return little; }
  const char* operator()(const uint32_t&) { return medium; }
  const char* operator()(const uint64_t&) { return big; }
};

const char* Describer::little = "little";
const char* Describer::medium = "medium";
const char* Describer::big = "big";

static void testMatching() {
  printf("testMatching\n");
  using V = Variant<uint8_t, uint32_t, uint64_t>;

  Describer desc;

  V v1(uint8_t(1));
  V v2(uint32_t(2));
  V v3(uint64_t(3));

  MOZ_RELEASE_ASSERT(v1.match(desc) == Describer::little);
  MOZ_RELEASE_ASSERT(v2.match(desc) == Describer::medium);
  MOZ_RELEASE_ASSERT(v3.match(desc) == Describer::big);

  const V& constRef1 = v1;
  const V& constRef2 = v2;
  const V& constRef3 = v3;

  MOZ_RELEASE_ASSERT(constRef1.match(desc) == Describer::little);
  MOZ_RELEASE_ASSERT(constRef2.match(desc) == Describer::medium);
  MOZ_RELEASE_ASSERT(constRef3.match(desc) == Describer::big);
}

static void testMatchingLambda() {
  printf("testMatchingLambda\n");
  using V = Variant<uint8_t, uint32_t, uint64_t>;

  auto desc = [](auto& a) {
    switch (sizeof(a)) {
      case 1:
        return Describer::little;
      case 4:
        return Describer::medium;
      case 8:
        return Describer::big;
      default:
        MOZ_RELEASE_ASSERT(false);
        return "";
    }
  };

  V v1(uint8_t(1));
  V v2(uint32_t(2));
  V v3(uint64_t(3));

  MOZ_RELEASE_ASSERT(v1.match(desc) == Describer::little);
  MOZ_RELEASE_ASSERT(v2.match(desc) == Describer::medium);
  MOZ_RELEASE_ASSERT(v3.match(desc) == Describer::big);

  const V& constRef1 = v1;
  const V& constRef2 = v2;
  const V& constRef3 = v3;

  MOZ_RELEASE_ASSERT(constRef1.match(desc) == Describer::little);
  MOZ_RELEASE_ASSERT(constRef2.match(desc) == Describer::medium);
  MOZ_RELEASE_ASSERT(constRef3.match(desc) == Describer::big);
}

static void testMatchingLambdas() {
  printf("testMatchingLambdas\n");
  using V = Variant<uint8_t, uint32_t, uint64_t>;

  auto desc8 = [](const uint8_t& a) { return Describer::little; };
  auto desc32 = [](const uint32_t& a) { return Describer::medium; };
  auto desc64 = [](const uint64_t& a) { return Describer::big; };

  V v1(uint8_t(1));
  V v2(uint32_t(2));
  V v3(uint64_t(3));

  MOZ_RELEASE_ASSERT(v1.match(desc8, desc32, desc64) == Describer::little);
  MOZ_RELEASE_ASSERT(v2.match(desc8, desc32, desc64) == Describer::medium);
  MOZ_RELEASE_ASSERT(v3.match(desc8, desc32, desc64) == Describer::big);

  const V& constRef1 = v1;
  const V& constRef2 = v2;
  const V& constRef3 = v3;

  MOZ_RELEASE_ASSERT(constRef1.match(desc8, desc32, desc64) ==
                     Describer::little);
  MOZ_RELEASE_ASSERT(constRef2.match(desc8, desc32, desc64) ==
                     Describer::medium);
  MOZ_RELEASE_ASSERT(constRef3.match(desc8, desc32, desc64) == Describer::big);
}

static void testRvalueMatcher() {
  printf("testRvalueMatcher\n");
  using V = Variant<uint8_t, uint32_t, uint64_t>;
  V v(uint8_t(1));
  MOZ_RELEASE_ASSERT(v.match(Describer()) == Describer::little);
}

static void testAddTagToHash() {
  printf("testAddToHash\n");
  using V = Variant<uint8_t, uint16_t, uint32_t, uint64_t>;

  // We don't know what our hash function is, and these are certainly not all
  // true under all hash functions. But they are probably true under almost any
  // decent hash function, and our aim is simply to establish that the tag
  // *does* influence the hash value.
  {
    mozilla::HashNumber h8 = V(uint8_t(1)).addTagToHash(0);
    mozilla::HashNumber h16 = V(uint16_t(1)).addTagToHash(0);
    mozilla::HashNumber h32 = V(uint32_t(1)).addTagToHash(0);
    mozilla::HashNumber h64 = V(uint64_t(1)).addTagToHash(0);

    MOZ_RELEASE_ASSERT(h8 != h16 && h8 != h32 && h8 != h64);
    MOZ_RELEASE_ASSERT(h16 != h32 && h16 != h64);
    MOZ_RELEASE_ASSERT(h32 != h64);
  }

  {
    mozilla::HashNumber h8 = V(uint8_t(1)).addTagToHash(0x124356);
    mozilla::HashNumber h16 = V(uint16_t(1)).addTagToHash(0x124356);
    mozilla::HashNumber h32 = V(uint32_t(1)).addTagToHash(0x124356);
    mozilla::HashNumber h64 = V(uint64_t(1)).addTagToHash(0x124356);

    MOZ_RELEASE_ASSERT(h8 != h16 && h8 != h32 && h8 != h64);
    MOZ_RELEASE_ASSERT(h16 != h32 && h16 != h64);
    MOZ_RELEASE_ASSERT(h32 != h64);
  }
}

int main() {
  testDetails();
  testSimple();
  testDuplicate();
  testConstructionWithVariantType();
  testConstructionWithVariantIndex();
  testCopy();
  testMove();
  testDestructor();
  testEquality();
  testMatching();
  testMatchingLambda();
  testMatchingLambdas();
  testRvalueMatcher();
  testAddTagToHash();

  printf("TestVariant OK!\n");
  return 0;
}
