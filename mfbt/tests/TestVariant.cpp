/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <type_traits>

#include "mozilla/UniquePtr.h"
#include "mozilla/Variant.h"

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
  static_assert(std::is_same_v<typename Nth<0, int>::Type, int>,
                "Nth<0, int>::Type should be int");

  // Test Nth with a list of more than 1 item.
  static_assert(std::is_same_v<typename Nth<0, int, char>::Type, int>,
                "Nth<0, int, char>::Type should be int");
  static_assert(std::is_same_v<typename Nth<1, int, char>::Type, char>,
                "Nth<1, int, char>::Type should be char");

  using mozilla::detail::SelectVariantType;

  // SelectVariantType for zero items (shouldn't happen, but `count` should
  // still work ok.)
  static_assert(SelectVariantType<int, char>::count == 0,
                "SelectVariantType<int, char>::count should be 0");

  // SelectVariantType for 1 type, for all combinations from/to T, const T,
  // const T&, T&&
  // - type to type
  static_assert(std::is_same_v<typename SelectVariantType<int, int>::Type, int>,
                "SelectVariantType<int, int>::Type should be int");
  static_assert(SelectVariantType<int, int>::count == 1,
                "SelectVariantType<int, int>::count should be 1");

  // - type to const type
  static_assert(std::is_same_v<typename SelectVariantType<int, const int>::Type,
                               const int>,
                "SelectVariantType<int, const int>::Type should be const int");
  static_assert(SelectVariantType<int, const int>::count == 1,
                "SelectVariantType<int, const int>::count should be 1");

  // - type to const type&
  static_assert(
      std::is_same_v<typename SelectVariantType<int, const int&>::Type,
                     const int&>,
      "SelectVariantType<int, const int&>::Type should be const int&");
  static_assert(SelectVariantType<int, const int&>::count == 1,
                "SelectVariantType<int, const int&>::count should be 1");

  // - type to type&&
  static_assert(
      std::is_same_v<typename SelectVariantType<int, int&&>::Type, int&&>,
      "SelectVariantType<int, int&&>::Type should be int&&");
  static_assert(SelectVariantType<int, int&&>::count == 1,
                "SelectVariantType<int, int&&>::count should be 1");

  // - const type to type
  static_assert(
      std::is_same_v<typename SelectVariantType<const int, int>::Type, int>,
      "SelectVariantType<const int, int>::Type should be int");
  static_assert(SelectVariantType<const int, int>::count == 1,
                "SelectVariantType<const int, int>::count should be 1");

  // - const type to const type
  static_assert(
      std::is_same_v<typename SelectVariantType<const int, const int>::Type,
                     const int>,
      "SelectVariantType<const int, const int>::Type should be const int");
  static_assert(SelectVariantType<const int, const int>::count == 1,
                "SelectVariantType<const int, const int>::count should be 1");

  // - const type to const type&
  static_assert(
      std::is_same_v<typename SelectVariantType<const int, const int&>::Type,
                     const int&>,
      "SelectVariantType<const int, const int&>::Type should be const int&");
  static_assert(SelectVariantType<const int, const int&>::count == 1,
                "SelectVariantType<const int, const int&>::count should be 1");

  // - const type to type&&
  static_assert(
      std::is_same_v<typename SelectVariantType<const int, int&&>::Type, int&&>,
      "SelectVariantType<const int, int&&>::Type should be int&&");
  static_assert(SelectVariantType<const int, int&&>::count == 1,
                "SelectVariantType<const int, int&&>::count should be 1");

  // - const type& to type
  static_assert(
      std::is_same_v<typename SelectVariantType<const int&, int>::Type, int>,
      "SelectVariantType<const int&, int>::Type should be int");
  static_assert(SelectVariantType<const int&, int>::count == 1,
                "SelectVariantType<const int&, int>::count should be 1");

  // - const type& to const type
  static_assert(
      std::is_same_v<typename SelectVariantType<const int&, const int>::Type,
                     const int>,
      "SelectVariantType<const int&, const int>::Type should be const int");
  static_assert(SelectVariantType<const int&, const int>::count == 1,
                "SelectVariantType<const int&, const int>::count should be 1");

  // - const type& to const type&
  static_assert(
      std::is_same_v<typename SelectVariantType<const int&, const int&>::Type,
                     const int&>,
      "SelectVariantType<const int&, const int&>::Type should be const int&");
  static_assert(SelectVariantType<const int&, const int&>::count == 1,
                "SelectVariantType<const int&, const int&>::count should be 1");

  // - const type& to type&&
  static_assert(
      std::is_same_v<typename SelectVariantType<const int&, int&&>::Type,
                     int&&>,
      "SelectVariantType<const int&, int&&>::Type should be int&&");
  static_assert(SelectVariantType<const int&, int&&>::count == 1,
                "SelectVariantType<const int&, int&&>::count should be 1");

  // - type&& to type
  static_assert(
      std::is_same_v<typename SelectVariantType<int&&, int>::Type, int>,
      "SelectVariantType<int&&, int>::Type should be int");
  static_assert(SelectVariantType<int&&, int>::count == 1,
                "SelectVariantType<int&&, int>::count should be 1");

  // - type&& to const type
  static_assert(
      std::is_same_v<typename SelectVariantType<int&&, const int>::Type,
                     const int>,
      "SelectVariantType<int&&, const int>::Type should be const int");
  static_assert(SelectVariantType<int&&, const int>::count == 1,
                "SelectVariantType<int&&, const int>::count should be 1");

  // - type&& to const type&
  static_assert(
      std::is_same_v<typename SelectVariantType<int&&, const int&>::Type,
                     const int&>,
      "SelectVariantType<int&&, const int&>::Type should be const int&");
  static_assert(SelectVariantType<int&&, const int&>::count == 1,
                "SelectVariantType<int&&, const int&>::count should be 1");

  // - type&& to type&&
  static_assert(
      std::is_same_v<typename SelectVariantType<int&&, int&&>::Type, int&&>,
      "SelectVariantType<int&&, int&&>::Type should be int&&");
  static_assert(SelectVariantType<int&&, int&&>::count == 1,
                "SelectVariantType<int&&, int&&>::count should be 1");

  // SelectVariantType for two different types.
  // (Don't test all combinations, trust that the above tests are sufficient.)
  static_assert(
      std::is_same_v<typename SelectVariantType<int, int, char>::Type, int>,
      "SelectVariantType<int, int, char>::Type should be int");
  static_assert(SelectVariantType<int, int, char>::count == 1,
                "SelectVariantType<int, int, char>::count should be 1");
  static_assert(
      std::is_same_v<typename SelectVariantType<char, int, char>::Type, char>,
      "SelectVariantType<char, int, char>::Type should be char");
  static_assert(SelectVariantType<char, int, char>::count == 1,
                "SelectVariantType<char, int, char>::count should be 1");

  // SelectVariantType for two identical types.
  static_assert(
      std::is_same_v<typename SelectVariantType<int, int, int>::Type, int>,
      "SelectVariantType<int, int, int>::Type should be int");
  static_assert(SelectVariantType<int, int, int>::count == 2,
                "SelectVariantType<int, int, int>::count should be 2");

  // SelectVariantType for two identical types, with others around.
  static_assert(
      std::is_same_v<typename SelectVariantType<int, char, int, int>::Type,
                     int>,
      "SelectVariantType<int, char, int, int>::Type should be int");
  static_assert(SelectVariantType<int, char, int, int>::count == 2,
                "SelectVariantType<int, char, int, int>::count should be 2");

  static_assert(
      std::is_same_v<typename SelectVariantType<int, int, char, int>::Type,
                     int>,
      "SelectVariantType<int, int, char, int>::Type should be int");
  static_assert(SelectVariantType<int, int, char, int>::count == 2,
                "SelectVariantType<int, int, char, int>::count should be 2");

  static_assert(
      std::is_same_v<typename SelectVariantType<int, int, int, char>::Type,
                     int>,
      "SelectVariantType<int, int, int, char>::Type should be int");
  static_assert(SelectVariantType<int, int, int, char>::count == 2,
                "SelectVariantType<int, int, int, char>::count should be 2");

  static_assert(
      std::is_same_v<
          typename SelectVariantType<int, char, int, char, int, char>::Type,
          int>,
      "SelectVariantType<int, char, int, char, int, char>::Type should be int");
  static_assert(
      SelectVariantType<int, char, int, char, int, char>::count == 2,
      "SelectVariantType<int, char, int, char, int, char>::count should be 2");

  // SelectVariantType for two identically-selectable types (first one wins!).
  static_assert(
      std::is_same_v<typename SelectVariantType<int, int, const int>::Type,
                     int>,
      "SelectVariantType<int, int, const int>::Type should be int");
  static_assert(SelectVariantType<int, int, const int>::count == 2,
                "SelectVariantType<int, int, const int>::count should be 2");
  static_assert(
      std::is_same_v<typename SelectVariantType<int, const int, int>::Type,
                     const int>,
      "SelectVariantType<int, const int, int>::Type should be const int");
  static_assert(SelectVariantType<int, const int, int>::count == 2,
                "SelectVariantType<int, const int, int>::count should be 2");
  static_assert(
      std::is_same_v<typename SelectVariantType<int, const int, int&&>::Type,
                     const int>,
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
  static_assert(std::is_same_v<decltype(v.as<1>()), uint64_t&>,
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
  static_assert(std::is_same_v<decltype(v.as<0>()), uint32_t&>,
                "as<0>() should return a uint64_t");
  static_assert(std::is_same_v<decltype(v.as<1>()), uint64_t&>,
                "as<1>() should return a uint64_t");
  static_assert(std::is_same_v<decltype(v.as<2>()), uint32_t&>,
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

static void testEmplaceWithType() {
  printf("testEmplaceWithType\n");
  Variant<uint32_t, uint64_t, uint32_t> v1(mozilla::VariantIndex<0>{}, 0);
  v1.emplace<uint64_t>(3);
  MOZ_RELEASE_ASSERT(v1.is<uint64_t>());
  MOZ_RELEASE_ASSERT(v1.as<uint64_t>() == 3);

  Variant<UniquePtr<int>, char> v2('a');
  v2.emplace<UniquePtr<int>>();
  MOZ_RELEASE_ASSERT(v2.is<UniquePtr<int>>());
  MOZ_RELEASE_ASSERT(!v2.as<UniquePtr<int>>().get());

  Variant<UniquePtr<int>, char> v3('a');
  v3.emplace<UniquePtr<int>>(MakeUnique<int>(4));
  MOZ_RELEASE_ASSERT(v3.is<UniquePtr<int>>());
  MOZ_RELEASE_ASSERT(*v3.as<UniquePtr<int>>().get() == 4);
}

static void testEmplaceWithIndex() {
  printf("testEmplaceWithIndex\n");
  Variant<uint32_t, uint64_t, uint32_t> v1(mozilla::VariantIndex<1>{}, 0);
  v1.emplace<2>(2);
  MOZ_RELEASE_ASSERT(!v1.is<uint64_t>());
  MOZ_RELEASE_ASSERT(!v1.is<1>());
  MOZ_RELEASE_ASSERT(!v1.is<0>());
  MOZ_RELEASE_ASSERT(v1.is<2>());
  MOZ_RELEASE_ASSERT(v1.as<2>() == 2);
  MOZ_RELEASE_ASSERT(v1.extract<2>() == 2);

  Variant<UniquePtr<int>, char> v2('a');
  v2.emplace<0>();
  MOZ_RELEASE_ASSERT(v2.is<UniquePtr<int>>());
  MOZ_RELEASE_ASSERT(!v2.is<1>());
  MOZ_RELEASE_ASSERT(v2.is<0>());
  MOZ_RELEASE_ASSERT(!v2.as<0>().get());
  MOZ_RELEASE_ASSERT(!v2.extract<0>().get());

  Variant<UniquePtr<int>, char> v3('a');
  v3.emplace<0>(MakeUnique<int>(4));
  MOZ_RELEASE_ASSERT(v3.is<UniquePtr<int>>());
  MOZ_RELEASE_ASSERT(!v3.is<1>());
  MOZ_RELEASE_ASSERT(v3.is<0>());
  MOZ_RELEASE_ASSERT(*v3.as<0>().get() == 4);
  MOZ_RELEASE_ASSERT(*v3.extract<0>().get() == 4);
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
      Variant<char, UniquePtr<char[]>, Destroyer> v1(d);
      MOZ_RELEASE_ASSERT(Destroyer::destroyedCount == 0);  // None detroyed yet.
    }

    MOZ_RELEASE_ASSERT(Destroyer::destroyedCount ==
                       1);  // v1's copy of d is destroyed.

    {
      Variant<char, UniquePtr<char[]>, Destroyer> v2(
          mozilla::VariantIndex<2>{});
      v2.emplace<Destroyer>(d);
      MOZ_RELEASE_ASSERT(Destroyer::destroyedCount ==
                         2);  // v2's initial value is destroyed.
    }

    MOZ_RELEASE_ASSERT(Destroyer::destroyedCount ==
                       3);  // v2's second value is destroyed.
  }

  MOZ_RELEASE_ASSERT(Destroyer::destroyedCount == 4);  // d is destroyed.
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
  static const char* littleNonConst;
  static const char* mediumNonConst;
  static const char* bigNonConst;

  static const char* littleConst;
  static const char* mediumConst;
  static const char* bigConst;

  static const char* littleRRef;
  static const char* mediumRRef;
  static const char* bigRRef;

  const char* operator()(const uint8_t&) & { return littleNonConst; }
  const char* operator()(const uint32_t&) & { return mediumNonConst; }
  const char* operator()(const uint64_t&) & { return bigNonConst; }

  const char* operator()(const uint8_t&) const& { return littleConst; }
  const char* operator()(const uint32_t&) const& { return mediumConst; }
  const char* operator()(const uint64_t&) const& { return bigConst; }

  const char* operator()(const uint8_t&) && { return littleRRef; }
  const char* operator()(const uint32_t&) && { return mediumRRef; }
  const char* operator()(const uint64_t&) && { return bigRRef; }

  // Catch-all, to verify that there is no call with any type other than the
  // expected ones above.
  template <typename Other>
  const char* operator()(const Other&) {
    MOZ_RELEASE_ASSERT(false);
    return "uh?";
  }
};

const char* Describer::littleNonConst = "little non-const";
const char* Describer::mediumNonConst = "medium non-const";
const char* Describer::bigNonConst = "big non-const";

const char* Describer::littleConst = "little const";
const char* Describer::mediumConst = "medium const";
const char* Describer::bigConst = "big const";

const char* Describer::littleRRef = "little rvalue-ref";
const char* Describer::mediumRRef = "medium rvalue-ref";
const char* Describer::bigRRef = "big rvalue-ref";

static void testMatching() {
  printf("testMatching\n");
  using V = Variant<uint8_t, uint32_t, uint64_t>;

  Describer desc;
  const Describer descConst;

  V v1(uint8_t(1));
  V v2(uint32_t(2));
  V v3(uint64_t(3));

  MOZ_RELEASE_ASSERT(v1.match(desc) == Describer::littleNonConst);
  MOZ_RELEASE_ASSERT(v2.match(desc) == Describer::mediumNonConst);
  MOZ_RELEASE_ASSERT(v3.match(desc) == Describer::bigNonConst);

  MOZ_RELEASE_ASSERT(v1.match(descConst) == Describer::littleConst);
  MOZ_RELEASE_ASSERT(v2.match(descConst) == Describer::mediumConst);
  MOZ_RELEASE_ASSERT(v3.match(descConst) == Describer::bigConst);

  MOZ_RELEASE_ASSERT(v1.match(Describer()) == Describer::littleRRef);
  MOZ_RELEASE_ASSERT(v2.match(Describer()) == Describer::mediumRRef);
  MOZ_RELEASE_ASSERT(v3.match(Describer()) == Describer::bigRRef);

  const V& constRef1 = v1;
  const V& constRef2 = v2;
  const V& constRef3 = v3;

  MOZ_RELEASE_ASSERT(constRef1.match(desc) == Describer::littleNonConst);
  MOZ_RELEASE_ASSERT(constRef2.match(desc) == Describer::mediumNonConst);
  MOZ_RELEASE_ASSERT(constRef3.match(desc) == Describer::bigNonConst);

  MOZ_RELEASE_ASSERT(constRef1.match(descConst) == Describer::littleConst);
  MOZ_RELEASE_ASSERT(constRef2.match(descConst) == Describer::mediumConst);
  MOZ_RELEASE_ASSERT(constRef3.match(descConst) == Describer::bigConst);

  MOZ_RELEASE_ASSERT(constRef1.match(Describer()) == Describer::littleRRef);
  MOZ_RELEASE_ASSERT(constRef2.match(Describer()) == Describer::mediumRRef);
  MOZ_RELEASE_ASSERT(constRef3.match(Describer()) == Describer::bigRRef);
}

static void testMatchingLambda() {
  printf("testMatchingLambda\n");
  using V = Variant<uint8_t, uint32_t, uint64_t>;

  // Note: Lambdas' call operators are const by default (unless the lambda is
  // declared `mutable`), hence the use of "...Const" strings below.
  // There is no need to test mutable lambdas, nor rvalue lambda, because there
  // would be no way to distinguish how each lambda is actually invoked because
  // there is only one choice of call operator in each overload set.
  auto desc = [](auto& a) {
    switch (sizeof(a)) {
      case 1:
        return Describer::littleConst;
      case 4:
        return Describer::mediumConst;
      case 8:
        return Describer::bigConst;
      default:
        MOZ_RELEASE_ASSERT(false);
        return "";
    }
  };

  V v1(uint8_t(1));
  V v2(uint32_t(2));
  V v3(uint64_t(3));

  MOZ_RELEASE_ASSERT(v1.match(desc) == Describer::littleConst);
  MOZ_RELEASE_ASSERT(v2.match(desc) == Describer::mediumConst);
  MOZ_RELEASE_ASSERT(v3.match(desc) == Describer::bigConst);

  const V& constRef1 = v1;
  const V& constRef2 = v2;
  const V& constRef3 = v3;

  MOZ_RELEASE_ASSERT(constRef1.match(desc) == Describer::littleConst);
  MOZ_RELEASE_ASSERT(constRef2.match(desc) == Describer::mediumConst);
  MOZ_RELEASE_ASSERT(constRef3.match(desc) == Describer::bigConst);
}

static void testMatchingLambdaWithIndex() {
  printf("testMatchingLambdaWithIndex\n");
  using V = Variant<uint8_t, uint32_t, uint64_t>;

  // Note: Lambdas' call operators are const by default (unless the lambda is
  // declared `mutable`), hence the use of "...Const" strings below.
  // There is no need to test mutable lambdas, nor rvalue lambda, because there
  // would be no way to distinguish how each lambda is actually invoked because
  // there is only one choice of call operator in each overload set.
  auto desc = [](auto aIndex, auto& a) {
    static_assert(sizeof(aIndex) < sizeof(size_t), "Expected small index type");
    switch (sizeof(a)) {
      case 1:
        MOZ_RELEASE_ASSERT(aIndex == 0);
        return Describer::littleConst;
      case 4:
        MOZ_RELEASE_ASSERT(aIndex == 1);
        return Describer::mediumConst;
      case 8:
        MOZ_RELEASE_ASSERT(aIndex == 2);
        return Describer::bigConst;
      default:
        MOZ_RELEASE_ASSERT(false);
        return "";
    }
  };

  V v1(uint8_t(1));
  V v2(uint32_t(2));
  V v3(uint64_t(3));

  MOZ_RELEASE_ASSERT(v1.match(desc) == Describer::littleConst);
  MOZ_RELEASE_ASSERT(v2.match(desc) == Describer::mediumConst);
  MOZ_RELEASE_ASSERT(v3.match(desc) == Describer::bigConst);

  const V& constRef1 = v1;
  const V& constRef2 = v2;
  const V& constRef3 = v3;

  MOZ_RELEASE_ASSERT(constRef1.match(desc) == Describer::littleConst);
  MOZ_RELEASE_ASSERT(constRef2.match(desc) == Describer::mediumConst);
  MOZ_RELEASE_ASSERT(constRef3.match(desc) == Describer::bigConst);
}

static void testMatchingLambdas() {
  printf("testMatchingLambdas\n");
  using V = Variant<uint8_t, uint32_t, uint64_t>;

  auto desc8 = [](const uint8_t& a) { return Describer::littleConst; };
  auto desc32 = [](const uint32_t& a) { return Describer::mediumConst; };
  auto desc64 = [](const uint64_t& a) { return Describer::bigConst; };

  V v1(uint8_t(1));
  V v2(uint32_t(2));
  V v3(uint64_t(3));

  MOZ_RELEASE_ASSERT(v1.match(desc8, desc32, desc64) == Describer::littleConst);
  MOZ_RELEASE_ASSERT(v2.match(desc8, desc32, desc64) == Describer::mediumConst);
  MOZ_RELEASE_ASSERT(v3.match(desc8, desc32, desc64) == Describer::bigConst);

  const V& constRef1 = v1;
  const V& constRef2 = v2;
  const V& constRef3 = v3;

  MOZ_RELEASE_ASSERT(constRef1.match(desc8, desc32, desc64) ==
                     Describer::littleConst);
  MOZ_RELEASE_ASSERT(constRef2.match(desc8, desc32, desc64) ==
                     Describer::mediumConst);
  MOZ_RELEASE_ASSERT(constRef3.match(desc8, desc32, desc64) ==
                     Describer::bigConst);
}

static void testMatchingLambdasWithIndex() {
  printf("testMatchingLambdasWithIndex\n");
  using V = Variant<uint8_t, uint32_t, uint64_t>;

  auto desc8 = [](size_t aIndex, const uint8_t& a) {
    MOZ_RELEASE_ASSERT(aIndex == 0);
    return Describer::littleConst;
  };
  auto desc32 = [](size_t aIndex, const uint32_t& a) {
    MOZ_RELEASE_ASSERT(aIndex == 1);
    return Describer::mediumConst;
  };
  auto desc64 = [](size_t aIndex, const uint64_t& a) {
    MOZ_RELEASE_ASSERT(aIndex == 2);
    return Describer::bigConst;
  };

  V v1(uint8_t(1));
  V v2(uint32_t(2));
  V v3(uint64_t(3));

  MOZ_RELEASE_ASSERT(v1.match(desc8, desc32, desc64) == Describer::littleConst);
  MOZ_RELEASE_ASSERT(v2.match(desc8, desc32, desc64) == Describer::mediumConst);
  MOZ_RELEASE_ASSERT(v3.match(desc8, desc32, desc64) == Describer::bigConst);

  const V& constRef1 = v1;
  const V& constRef2 = v2;
  const V& constRef3 = v3;

  MOZ_RELEASE_ASSERT(constRef1.match(desc8, desc32, desc64) ==
                     Describer::littleConst);
  MOZ_RELEASE_ASSERT(constRef2.match(desc8, desc32, desc64) ==
                     Describer::mediumConst);
  MOZ_RELEASE_ASSERT(constRef3.match(desc8, desc32, desc64) ==
                     Describer::bigConst);
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
  testEmplaceWithType();
  testEmplaceWithIndex();
  testCopy();
  testMove();
  testDestructor();
  testEquality();
  testMatching();
  testMatchingLambda();
  testMatchingLambdaWithIndex();
  testMatchingLambdas();
  testMatchingLambdasWithIndex();
  testAddTagToHash();

  printf("TestVariant OK!\n");
  return 0;
}
