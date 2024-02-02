/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/FunctionRef.h"
#include "mozilla/UniquePtr.h"

using mozilla::FunctionRef;

#define CHECK(c)                                       \
  do {                                                 \
    bool cond = !!(c);                                 \
    MOZ_RELEASE_ASSERT(cond, "Failed assertion: " #c); \
  } while (false)

int addConstRefs(const int& arg1, const int& arg2) { return arg1 + arg2; }

void incrementPointer(int* arg) { (*arg)++; }

int increment(int arg) { return arg + 1; }

int incrementUnique(mozilla::UniquePtr<int> ptr) { return *ptr + 1; }

static bool helloWorldCalled = false;

void helloWorld() { helloWorldCalled = true; }

struct S {
  static int increment(int arg) { return arg + 1; }
};

struct Incrementor {
  int operator()(int arg) { return arg + 1; }
};

template <typename Fn>
struct Caller;

template <typename Fn, typename... Params>
std::invoke_result_t<Fn, Params...> CallFunctionRef(FunctionRef<Fn> aRef,
                                                    Params... aParams) {
  return aRef(std::forward<Params>(aParams)...);
}

static void TestNonmemberFunction() {
  CHECK(CallFunctionRef<int(int)>(increment, 42) == 43);
}

static void TestStaticMemberFunction() {
  CHECK(CallFunctionRef<int(int)>(&S::increment, 42) == 43);
}

static void TestFunctionObject() {
  auto incrementor = Incrementor();
  CHECK(CallFunctionRef<int(int)>(incrementor, 42) == 43);
}

static void TestFunctionObjectTemporary() {
  CHECK(CallFunctionRef<int(int)>(Incrementor(), 42) == 43);
}

static void TestLambda() {
  // Test non-capturing lambda
  auto lambda1 = [](int arg) { return arg + 1; };
  CHECK(CallFunctionRef<int(int)>(lambda1, 42) == 43);

  // Test capturing lambda
  int one = 1;
  auto lambda2 = [one](int arg) { return arg + one; };
  CHECK(CallFunctionRef<int(int)>(lambda2, 42) == 43);

  CHECK(CallFunctionRef<int(int)>([](int arg) { return arg + 1; }, 42) == 43);
}

static void TestOperatorBool() {
  auto ToBool = [](FunctionRef<int(int)> aRef) {
    return static_cast<bool>(aRef);
  };
  CHECK(!ToBool({}));
  CHECK(ToBool(increment));
  CHECK(!ToBool(nullptr));
}

static void TestReferenceParameters() {
  int x = 1;
  int y = 2;
  CHECK(CallFunctionRef<int(const int&, const int&)>(addConstRefs, x, y) == 3);
}

static void TestVoidNoParameters() {
  CHECK(!helloWorldCalled);
  CallFunctionRef<void()>(helloWorld);
  CHECK(helloWorldCalled);
}

static void TestPointerParameters() {
  int x = 1;
  CallFunctionRef<void(int*)>(incrementPointer, &x);
  CHECK(x == 2);
}

static void TestImplicitFunctorTypeConversion() {
  auto incrementor = Incrementor();
  short x = 1;
  CHECK(CallFunctionRef<long(short)>(incrementor, x) == 2);
}

static void TestImplicitLambdaTypeConversion() {
  short x = 1;
  CHECK(CallFunctionRef<long(short)>([](short arg) { return arg + 1; }, x) ==
        2);
}

static void TestImplicitFunctionPointerTypeConversion() {
  short x = 1;
  CHECK(CallFunctionRef<long(short)>(&increment, x) == 2);
}

static void TestMoveOnlyArguments() {
  CHECK(CallFunctionRef<int(mozilla::UniquePtr<int>)>(
            &incrementUnique, mozilla::MakeUnique<int>(5)) == 6);
}

int main() {
  TestNonmemberFunction();
  TestStaticMemberFunction();
  TestFunctionObject();
  TestFunctionObjectTemporary();
  TestLambda();
  TestOperatorBool();
  TestReferenceParameters();
  TestPointerParameters();
  TestVoidNoParameters();
  TestImplicitFunctorTypeConversion();
  TestImplicitLambdaTypeConversion();
  TestImplicitFunctionPointerTypeConversion();
  TestMoveOnlyArguments();

  printf("TestFunctionRef OK!\n");
  return 0;
}
