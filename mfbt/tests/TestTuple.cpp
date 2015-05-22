/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
   /* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/Move.h"
#include "mozilla/Tuple.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/unused.h"

#include <stddef.h>

using mozilla::Get;
using mozilla::IsSame;
using mozilla::MakeTuple;
using mozilla::MakeUnique;
using mozilla::Move;
using mozilla::Tuple;
using mozilla::UniquePtr;
using mozilla::unused;

#define CHECK(c) \
  do { \
    bool cond = !!(c); \
    MOZ_RELEASE_ASSERT(cond, "Failed assertion: " #c); \
  } while (false)

// The second argument is the expected type. It's variadic to allow the
// type to contain commas.
#define CHECK_TYPE(expression, ...)  \
  static_assert(IsSame<decltype(expression), __VA_ARGS__>::value, \
      "Type mismatch!")

struct ConvertibleToInt
{
  operator int() { return 42; }
};

static void
TestConstruction()
{
  // Default construction
  Tuple<> a;
  unused << a;
  Tuple<int> b;
  unused << b;

  // Construction from elements
  int x = 1, y = 1;
  Tuple<int, int> c{x, y};
  Tuple<int&, const int&> d{x, y};
  x = 42;
  y = 42;
  CHECK(Get<0>(c) == 1);
  CHECK(Get<1>(c) == 1);
  CHECK(Get<0>(d) == 42);
  CHECK(Get<1>(d) == 42);

  // Construction from objects convertible to the element types
  Tuple<int, int> e{1.0, ConvertibleToInt{}};

  // Copy construction
  Tuple<int> x1;
  Tuple<int> x2{x1};

  Tuple<int, int> f(c);
  CHECK(Get<0>(f) == 1);
  CHECK(Get<0>(f) == 1);

  // Move construction
  Tuple<UniquePtr<int>> g{MakeUnique<int>(42)};
  Tuple<UniquePtr<int>> h{Move(g)};
  CHECK(Get<0>(g) == nullptr);
  CHECK(*Get<0>(h) == 42);
}

static void
TestAssignment()
{
  // Copy assignment
  Tuple<int> a{0};
  Tuple<int> b{42};
  a = b;
  CHECK(Get<0>(a) == 42);

  // Assignment to reference member
  int i = 0;
  int j = 42;
  Tuple<int&> c{i};
  Tuple<int&> d{j};
  c = d;
  CHECK(i == 42);

  // Move assignment
  Tuple<UniquePtr<int>> e{MakeUnique<int>(0)};
  Tuple<UniquePtr<int>> f{MakeUnique<int>(42)};
  e = Move(f);
  CHECK(*Get<0>(e) == 42);
  CHECK(Get<0>(f) == nullptr);
}

static void
TestGet()
{
  int x = 1;
  int y = 2;
  int z = 3;
  Tuple<int, int&, const int&> tuple(x, y, z);

  // Using Get<>() to read elements
  CHECK(Get<0>(tuple) == 1);
  CHECK(Get<1>(tuple) == 2);
  CHECK(Get<2>(tuple) == 3);

  // Using Get<>() to write to elements
  Get<0>(tuple) = 41;
  CHECK(Get<0>(tuple) == 41);

  // Writing through reference elements
  Get<1>(tuple) = 42;
  CHECK(Get<1>(tuple) == 42);
  CHECK(y == 42);
}

static bool
TestMakeTuple()
{
  auto tuple = MakeTuple(42, 0.5f, 'c');
  CHECK_TYPE(tuple, Tuple<int, float, char>);
  CHECK(Get<0>(tuple) == 42);
  CHECK(Get<1>(tuple) == 1.0f);
  CHECK(Get<2>(tuple) == 'c');
  return true;
}

int
main()
{
  TestConstruction();
  TestAssignment();
  TestGet();
  TestMakeTuple();
  return 0;
}
