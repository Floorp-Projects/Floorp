/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stddef.h>

#include <type_traits>
#include <utility>

#include "mozilla/Assertions.h"
#include "mozilla/CompactPair.h"
#include "mozilla/Tuple.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"

using mozilla::CompactPair;
using mozilla::Get;
using mozilla::MakeTuple;
using mozilla::MakeUnique;
using mozilla::Tie;
using mozilla::Tuple;
using mozilla::UniquePtr;
using mozilla::Unused;
using std::pair;

#define CHECK(c)                                       \
  do {                                                 \
    bool cond = !!(c);                                 \
    MOZ_RELEASE_ASSERT(cond, "Failed assertion: " #c); \
  } while (false)

// The second argument is the expected type. It's variadic to allow the
// type to contain commas.
#define CHECK_TYPE(expression, ...)                                \
  static_assert(std::is_same_v<decltype(expression), __VA_ARGS__>, \
                "Type mismatch!")

struct ConvertibleToInt {
  operator int() const { return 42; }
};

static void TestConstruction() {
  // Default construction
  Tuple<> a;
  Unused << a;
  Tuple<int> b;
  Unused << b;

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
  Tuple<UniquePtr<int>> h{std::move(g)};
  CHECK(Get<0>(g) == nullptr);
  CHECK(*Get<0>(h) == 42);
}

static void TestConstructionFromMozPair() {
  // Construction from elements
  int x = 1, y = 1;
  CompactPair<int, int> a{x, y};
  CompactPair<int&, const int&> b{x, y};
  Tuple<int, int> c(a);
  Tuple<int&, const int&> d(b);
  x = 42;
  y = 42;
  CHECK(Get<0>(c) == 1);
  CHECK(Get<1>(c) == 1);
  CHECK(Get<0>(d) == 42);
  CHECK(Get<1>(d) == 42);
}

static void TestConstructionFromStdPair() {
  // Construction from elements
  int x = 1, y = 1;
  pair<int, int> a{x, y};
  pair<int&, const int&> b{x, y};
  Tuple<int, int> c(a);
  Tuple<int&, const int&> d(b);
  x = 42;
  y = 42;
  CHECK(Get<0>(c) == 1);
  CHECK(Get<1>(c) == 1);
  CHECK(Get<0>(d) == 42);
  CHECK(Get<1>(d) == 42);
}

static void TestAssignment() {
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
  e = std::move(f);
  CHECK(*Get<0>(e) == 42);
  CHECK(Get<0>(f) == nullptr);
}

static void TestAssignmentFromMozPair() {
  // Copy assignment
  Tuple<int, int> a{0, 0};
  CompactPair<int, int> b{42, 42};
  a = b;
  CHECK(Get<0>(a) == 42);
  CHECK(Get<1>(a) == 42);

  // Assignment to reference member
  int i = 0;
  int j = 0;
  int k = 42;
  Tuple<int&, int&> c{i, j};
  CompactPair<int&, int&> d{k, k};
  c = d;
  CHECK(i == 42);
  CHECK(j == 42);

  // Move assignment
  Tuple<UniquePtr<int>, UniquePtr<int>> e{MakeUnique<int>(0),
                                          MakeUnique<int>(0)};
  CompactPair<UniquePtr<int>, UniquePtr<int>> f{MakeUnique<int>(42),
                                                MakeUnique<int>(42)};
  e = std::move(f);
  CHECK(*Get<0>(e) == 42);
  CHECK(*Get<1>(e) == 42);
  CHECK(f.first() == nullptr);
  CHECK(f.second() == nullptr);
}

static void TestAssignmentFromStdPair() {
  // Copy assignment
  Tuple<int, int> a{0, 0};
  pair<int, int> b{42, 42};
  a = b;
  CHECK(Get<0>(a) == 42);
  CHECK(Get<1>(a) == 42);

  // Assignment to reference member
  int i = 0;
  int j = 0;
  int k = 42;
  Tuple<int&, int&> c{i, j};
  pair<int&, int&> d{k, k};
  c = d;
  CHECK(i == 42);
  CHECK(j == 42);

  // Move assignment.
  Tuple<UniquePtr<int>, UniquePtr<int>> e{MakeUnique<int>(0),
                                          MakeUnique<int>(0)};
  // XXX: On some platforms std::pair doesn't support move constructor.
  pair<UniquePtr<int>, UniquePtr<int>> f;
  f.first = MakeUnique<int>(42);
  f.second = MakeUnique<int>(42);

  e = std::move(f);
  CHECK(*Get<0>(e) == 42);
  CHECK(*Get<1>(e) == 42);
  CHECK(f.first == nullptr);
  CHECK(f.second == nullptr);
}

static void TestGet() {
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

static void TestMakeTuple() {
  auto tuple = MakeTuple(42, 0.5f, 'c');
  CHECK_TYPE(tuple, Tuple<int, float, char>);
  CHECK(Get<0>(tuple) == 42);
  CHECK(Get<1>(tuple) == 0.5f);
  CHECK(Get<2>(tuple) == 'c');

  // Make sure we don't infer the type to be Tuple<int&>.
  int x = 1;
  auto tuple2 = MakeTuple(x);
  CHECK_TYPE(tuple2, Tuple<int>);
  x = 2;
  CHECK(Get<0>(tuple2) == 1);
}

static bool TestTieMozPair() {
  int i;
  float f;
  char c;
  Tuple<int, float, char> rhs1(42, 0.5f, 'c');
  Tie(i, f, c) = rhs1;
  CHECK(i == Get<0>(rhs1));
  CHECK(f == Get<1>(rhs1));
  CHECK(c == Get<2>(rhs1));
  // Test conversions
  Tuple<ConvertibleToInt, double, unsigned char> rhs2(ConvertibleToInt(), 0.7f,
                                                      'd');
  Tie(i, f, c) = rhs2;
  CHECK(i == Get<0>(rhs2));
  CHECK(f == Get<1>(rhs2));
  CHECK(c == Get<2>(rhs2));

  // Test Pair
  CompactPair<int, float> rhs3(42, 1.5f);
  Tie(i, f) = rhs3;
  CHECK(i == rhs3.first());
  CHECK(f == rhs3.second());

  return true;
}

static bool TestTie() {
  int i;
  float f;
  char c;
  Tuple<int, float, char> rhs1(42, 0.5f, 'c');
  Tie(i, f, c) = rhs1;
  CHECK(i == Get<0>(rhs1));
  CHECK(f == Get<1>(rhs1));
  CHECK(c == Get<2>(rhs1));
  // Test conversions
  Tuple<ConvertibleToInt, double, unsigned char> rhs2(ConvertibleToInt(), 0.7f,
                                                      'd');
  Tie(i, f, c) = rhs2;
  CHECK(i == Get<0>(rhs2));
  CHECK(f == Get<1>(rhs2));
  CHECK(c == Get<2>(rhs2));

  // Test Pair
  pair<int, float> rhs3(42, 1.5f);
  Tie(i, f) = rhs3;
  CHECK(i == rhs3.first);
  CHECK(f == rhs3.second);

  return true;
}

static bool TestTieIgnore() {
  int i;
  char c;
  Tuple<int, float, char> rhs1(42, 0.5f, 'c');

  Tie(i, mozilla::Ignore, c) = rhs1;

  CHECK(i == Get<0>(rhs1));
  CHECK(c == Get<2>(rhs1));

  return true;
}

int main() {
  TestConstruction();
  TestConstructionFromMozPair();
  TestConstructionFromStdPair();
  TestAssignment();
  TestAssignmentFromMozPair();
  TestAssignmentFromStdPair();
  TestGet();
  TestMakeTuple();
  TestTie();
  TestTieIgnore();
  TestTieMozPair();
  return 0;
}
