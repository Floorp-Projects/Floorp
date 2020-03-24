/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/FunctionRef.h"

#define CHECK(c)                                       \
  do {                                                 \
    bool cond = !!(c);                                 \
    MOZ_RELEASE_ASSERT(cond, "Failed assertion: " #c); \
  } while (false)

int addConstRefs(const int& arg1, const int& arg2) { return arg1 + arg2; }

void incrementPointer(int* arg) { (*arg)++; }

int increment(int arg) { return arg + 1; }

static bool helloWorldCalled = false;

void helloWorld() { helloWorldCalled = true; }

struct S {
  static int increment(int arg) { return arg + 1; }
};

struct Incrementor {
  int operator()(int arg) { return arg + 1; }
};

static void TestNonmemberFunction() {
  mozilla::FunctionRef<int(int)> f(&increment);
  CHECK(f(42) == 43);
}

static void TestStaticMemberFunction() {
  mozilla::FunctionRef<int(int)> f(&S::increment);
  CHECK(f(42) == 43);
}

static void TestFunctionObject() {
  auto incrementor = Incrementor();
  mozilla::FunctionRef<int(int)> f(incrementor);
  CHECK(f(42) == 43);
}

static void TestLambda() {
  // Test non-capturing lambda
  auto lambda1 = [](int arg) { return arg + 1; };
  mozilla::FunctionRef<int(int)> f(lambda1);
  CHECK(f(42) == 43);

  // Test capturing lambda
  int one = 1;
  auto lambda2 = [one](int arg) { return arg + one; };
  mozilla::FunctionRef<int(int)> g(lambda2);
  CHECK(g(42) == 43);

  mozilla::FunctionRef<int(int)> h([](int arg) { return arg + 1; });
  CHECK(h(42) == 43);
}

static void TestOperatorBool() {
  mozilla::FunctionRef<int(int)> f1;
  CHECK(!static_cast<bool>(f1));

  mozilla::FunctionRef<int(int)> f2 = increment;
  CHECK(static_cast<bool>(f2));

  mozilla::FunctionRef<int(int)> f3 = nullptr;
  CHECK(!static_cast<bool>(f3));
}

static void TestReferenceParameters() {
  mozilla::FunctionRef<int(const int&, const int&)> f = &addConstRefs;
  int x = 1;
  int y = 2;
  CHECK(f(x, y) == 3);
}

static void TestVoidNoParameters() {
  mozilla::FunctionRef<void()> f = &helloWorld;
  CHECK(!helloWorldCalled);
  f();
  CHECK(helloWorldCalled);
}

static void TestPointerParameters() {
  mozilla::FunctionRef<void(int*)> f = &incrementPointer;
  int x = 1;
  f(&x);
  CHECK(x == 2);
}

static void TestImplicitFunctorTypeConversion() {
  auto incrementor = Incrementor();
  mozilla::FunctionRef<long(short)> f = incrementor;
  short x = 1;
  CHECK(f(x) == 2);
}

static void TestImplicitLambdaTypeConversion() {
  mozilla::FunctionRef<long(short)> f = [](short arg) { return arg + 1; };
  short x = 1;
  CHECK(f(x) == 2);
}

static void TestImplicitFunctionPointerTypeConversion() {
  mozilla::FunctionRef<long(short)> f = &increment;
  short x = 1;
  CHECK(f(x) == 2);
}

int main() {
  TestNonmemberFunction();
  TestStaticMemberFunction();
  TestFunctionObject();
  TestLambda();
  TestOperatorBool();
  TestReferenceParameters();
  TestPointerParameters();
  TestVoidNoParameters();
  TestImplicitFunctorTypeConversion();
  TestImplicitLambdaTypeConversion();
  TestImplicitFunctionPointerTypeConversion();

  printf("TestFunctionRef OK!\n");
  return 0;
}
