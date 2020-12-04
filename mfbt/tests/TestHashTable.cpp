/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HashTable.h"
#include "mozilla/PairHash.h"

#include <utility>

void TestMoveConstructor() {
  using namespace mozilla;

  HashMap<int, int> map;
  MOZ_RELEASE_ASSERT(map.putNew(3, 32));
  MOZ_RELEASE_ASSERT(map.putNew(4, 42));
  MOZ_RELEASE_ASSERT(map.count() == 2);
  MOZ_RELEASE_ASSERT(!map.empty());
  MOZ_RELEASE_ASSERT(!map.lookup(2));
  MOZ_RELEASE_ASSERT(map.lookup(3)->value() == 32);
  MOZ_RELEASE_ASSERT(map.lookup(4)->value() == 42);

  HashMap<int, int> moved = std::move(map);
  MOZ_RELEASE_ASSERT(moved.count() == 2);
  MOZ_RELEASE_ASSERT(!moved.empty());
  MOZ_RELEASE_ASSERT(!moved.lookup(2));
  MOZ_RELEASE_ASSERT(moved.lookup(3)->value() == 32);
  MOZ_RELEASE_ASSERT(moved.lookup(4)->value() == 42);

  MOZ_RELEASE_ASSERT(map.empty());
  MOZ_RELEASE_ASSERT(!map.count());
}

enum SimpleEnum { SIMPLE_1, SIMPLE_2 };

enum class ClassEnum : int {
  CLASS_ENUM_1,
  CLASS_ENUM_2,
};

void TestEnumHash() {
  using namespace mozilla;

  HashMap<SimpleEnum, int> map;
  MOZ_RELEASE_ASSERT(map.put(SIMPLE_1, 1));
  MOZ_RELEASE_ASSERT(map.put(SIMPLE_2, 2));

  MOZ_RELEASE_ASSERT(map.lookup(SIMPLE_1)->value() == 1);
  MOZ_RELEASE_ASSERT(map.lookup(SIMPLE_2)->value() == 2);

  HashMap<ClassEnum, int> map2;
  MOZ_RELEASE_ASSERT(map2.put(ClassEnum::CLASS_ENUM_1, 1));
  MOZ_RELEASE_ASSERT(map2.put(ClassEnum::CLASS_ENUM_2, 2));

  MOZ_RELEASE_ASSERT(map2.lookup(ClassEnum::CLASS_ENUM_1)->value() == 1);
  MOZ_RELEASE_ASSERT(map2.lookup(ClassEnum::CLASS_ENUM_2)->value() == 2);
}

void TestHashPair() {
  using namespace mozilla;

  // Test with std::pair
  {
    HashMap<std::pair<int, bool>, int, PairHasher<int, bool>> map;
    std::pair<int, bool> key1 = std::make_pair(1, true);
    MOZ_RELEASE_ASSERT(map.putNew(key1, 1));
    MOZ_RELEASE_ASSERT(map.has(key1));
    std::pair<int, bool> key2 = std::make_pair(1, false);
    MOZ_RELEASE_ASSERT(map.putNew(key2, 1));
    std::pair<int, bool> key3 = std::make_pair(2, false);
    MOZ_RELEASE_ASSERT(map.putNew(key3, 2));
    MOZ_RELEASE_ASSERT(map.has(key3));

    MOZ_RELEASE_ASSERT(map.lookup(key1)->value() == 1);
    MOZ_RELEASE_ASSERT(map.lookup(key2)->value() == 1);
    MOZ_RELEASE_ASSERT(map.lookup(key3)->value() == 2);
  }
  // Test wtih compact pair
  {
    HashMap<mozilla::CompactPair<int, bool>, int, CompactPairHasher<int, bool>>
        map;
    mozilla::CompactPair<int, bool> key1 = mozilla::MakeCompactPair(1, true);
    MOZ_RELEASE_ASSERT(map.putNew(key1, 1));
    MOZ_RELEASE_ASSERT(map.has(key1));
    mozilla::CompactPair<int, bool> key2 = mozilla::MakeCompactPair(1, false);
    MOZ_RELEASE_ASSERT(map.putNew(key2, 1));
    mozilla::CompactPair<int, bool> key3 = mozilla::MakeCompactPair(2, false);
    MOZ_RELEASE_ASSERT(map.putNew(key3, 2));
    MOZ_RELEASE_ASSERT(map.has(key3));

    MOZ_RELEASE_ASSERT(map.lookup(key1)->value() == 1);
    MOZ_RELEASE_ASSERT(map.lookup(key2)->value() == 1);
    MOZ_RELEASE_ASSERT(map.lookup(key3)->value() == 2);
  }
}

int main() {
  TestMoveConstructor();
  TestEnumHash();
  TestHashPair();
  return 0;
}
