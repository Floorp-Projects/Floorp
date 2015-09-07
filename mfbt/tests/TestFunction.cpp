/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
   /* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/Function.h"

using mozilla::Function;

#define CHECK(c) \
  do { \
    bool cond = !!(c); \
    MOZ_RELEASE_ASSERT(cond, "Failed assertion: " #c); \
  } while (false)

struct ConvertibleToInt
{
  operator int() const { return 42; }
};

int increment(int arg) { return arg + 1; }

struct S {
  static int increment(int arg) { return arg + 1; }
};

struct Incrementor {
  int operator()(int arg) { return arg + 1; }
};

static void
TestNonmemberFunction()
{
  Function<int(int)> f = &increment;
  CHECK(f(42) == 43);
}

static void
TestStaticMemberFunction()
{
  Function<int(int)> f = &S::increment;
  CHECK(f(42) == 43);
}

static void
TestFunctionObject()
{
  Function<int(int)> f = Incrementor();
  CHECK(f(42) == 43);
}

static void
TestLambda()
{
  // Test non-capturing lambda
  Function<int(int)> f = [](int arg){ return arg + 1; };
  CHECK(f(42) == 43);

  // Test capturing lambda
  int one = 1;
  Function<int(int)> g = [one](int arg){ return arg + one; };
  CHECK(g(42) == 43);
}

static void
TestDefaultConstructionAndAssignmentLater()
{
  Function<int(int)> f;  // allowed
  // Would get an assertion if we tried calling f now.
  f = &increment;
  CHECK(f(42) == 43);
}

static void
TestReassignment()
{
  Function<int(int)> f = &increment;
  CHECK(f(42) == 43);
  f = [](int arg){ return arg + 2; };
  CHECK(f(42) == 44);
}

int
main()
{
  TestNonmemberFunction();
  TestStaticMemberFunction();
  TestFunctionObject();
  TestLambda();
  TestDefaultConstructionAndAssignmentLater();
  TestReassignment();
  return 0;
}
