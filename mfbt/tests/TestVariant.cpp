/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/UniquePtr.h"
#include "mozilla/Variant.h"

using mozilla::MakeUnique;
using mozilla::UniquePtr;
using mozilla::Variant;

struct Destroyer {
  static int destroyedCount;
  ~Destroyer() {
    destroyedCount++;
  }
};

int Destroyer::destroyedCount = 0;

static void
testSimple()
{
  printf("testSimple\n");
  Variant<uint32_t, uint64_t> v(uint64_t(1));
  MOZ_RELEASE_ASSERT(v.is<uint64_t>());
  MOZ_RELEASE_ASSERT(!v.is<uint32_t>());
  MOZ_RELEASE_ASSERT(v.as<uint64_t>() == 1);
}

static void
testCopy()
{
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

static void
testMove()
{
  printf("testMove\n");
  Variant<UniquePtr<int>, char> v1(MakeUnique<int>(5));
  Variant<UniquePtr<int>, char> v2(Move(v1));

  MOZ_RELEASE_ASSERT(v2.is<UniquePtr<int>>());
  MOZ_RELEASE_ASSERT(*v2.as<UniquePtr<int>>() == 5);

  MOZ_RELEASE_ASSERT(v1.is<UniquePtr<int>>());
  MOZ_RELEASE_ASSERT(v1.as<UniquePtr<int>>() == nullptr);

  Destroyer::destroyedCount = 0;
  {
    Variant<char, UniquePtr<Destroyer>> v3(MakeUnique<Destroyer>());
    Variant<char, UniquePtr<Destroyer>> v4(Move(v3));

    Variant<char, UniquePtr<Destroyer>> v5('a');
    v5 = Move(v4);

    auto ptr = v5.extract<UniquePtr<Destroyer>>();
    MOZ_RELEASE_ASSERT(Destroyer::destroyedCount == 0);
  }
  MOZ_RELEASE_ASSERT(Destroyer::destroyedCount == 1);
}

static void
testDestructor()
{
  printf("testDestructor\n");
  Destroyer::destroyedCount = 0;

  {
    Destroyer d;

    {
      Variant<char, UniquePtr<char[]>, Destroyer> v(d);
      MOZ_RELEASE_ASSERT(Destroyer::destroyedCount == 0); // None detroyed yet.
    }

    MOZ_RELEASE_ASSERT(Destroyer::destroyedCount == 1); // v's copy of d is destroyed.
  }

  MOZ_RELEASE_ASSERT(Destroyer::destroyedCount == 2); // d is destroyed.
}

int
main()
{
  testSimple();
  testCopy();
  testMove();
  testDestructor();

  printf("TestVariant OK!\n");
  return 0;
}
