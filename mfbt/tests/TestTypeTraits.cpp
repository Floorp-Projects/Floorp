/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/TypeTraits.h"

using mozilla::AddRvalueReference;
using mozilla::DeclVal;
using mozilla::IsDestructible;
using mozilla::IsSame;

class PublicDestructible {
 public:
  ~PublicDestructible();
};
class PrivateDestructible {
 private:
  ~PrivateDestructible();
};
class TrivialDestructible {};

static_assert(IsDestructible<PublicDestructible>::value,
              "public destructible class is destructible");
static_assert(!IsDestructible<PrivateDestructible>::value,
              "private destructible class is not destructible");
static_assert(IsDestructible<TrivialDestructible>::value,
              "trivial destructible class is destructible");

static_assert(IsSame<AddRvalueReference<int>::Type, int&&>::value,
              "not adding && to int correctly");
static_assert(
    IsSame<AddRvalueReference<volatile int&>::Type, volatile int&>::value,
    "not adding && to volatile int& correctly");
static_assert(IsSame<AddRvalueReference<const int&&>::Type, const int&&>::value,
              "not adding && to volatile int& correctly");
static_assert(IsSame<AddRvalueReference<void*>::Type, void*&&>::value,
              "not adding && to void* correctly");
static_assert(IsSame<AddRvalueReference<void>::Type, void>::value,
              "void shouldn't be transformed by AddRvalueReference");
static_assert(IsSame<AddRvalueReference<struct S1&>::Type, struct S1&>::value,
              "not reference-collapsing struct S1& && to struct S1& correctly");

struct TestWithDefaultConstructor {
  int foo() const { return 0; }
};
struct TestWithNoDefaultConstructor {
  explicit TestWithNoDefaultConstructor(int) {}
  int foo() const { return 1; }
};

static_assert(IsSame<decltype(TestWithDefaultConstructor().foo()), int>::value,
              "decltype should work using a struct with a default constructor");
static_assert(
    IsSame<decltype(DeclVal<TestWithDefaultConstructor>().foo()), int>::value,
    "decltype should work using a DeclVal'd struct with a default constructor");
static_assert(
    IsSame<decltype(DeclVal<TestWithNoDefaultConstructor>().foo()), int>::value,
    "decltype should work using a DeclVal'd struct without a default "
    "constructor");

/*
 * Android's broken [u]intptr_t inttype macros are broken because its PRI*PTR
 * macros are defined as "ld", but sizeof(long) is 8 and sizeof(intptr_t)
 * is 4 on 32-bit Android. We redefine Android's PRI*PTR macros in
 * IntegerPrintfMacros.h and assert here that our new definitions match the
 * actual type sizes seen at compile time.
 */
#if defined(ANDROID) && !defined(__LP64__)
static_assert(mozilla::IsSame<int, intptr_t>::value,
              "emulated PRI[di]PTR definitions will be wrong");
static_assert(mozilla::IsSame<unsigned int, uintptr_t>::value,
              "emulated PRI[ouxX]PTR definitions will be wrong");
#endif

int main() { return 0; }
