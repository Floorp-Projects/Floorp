/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HashTable.h"

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

int main() {
  TestMoveConstructor();
  return 0;
}
