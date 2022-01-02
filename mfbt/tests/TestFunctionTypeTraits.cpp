/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/FunctionTypeTraits.h"

#include <functional>

using mozilla::FunctionTypeTraits;

void f0() {}

int f1(char) { return 0; }

#ifdef NS_HAVE_STDCALL
void NS_STDCALL f0s() {}

int NS_STDCALL f1s(char) { return 0; }
#endif  // NS_HAVE_STDCALL

struct S {
  void f0() {}
  void f0c() const {}
  int f1(char) { return 0; }
  int f1c(char) const { return 0; }
#ifdef NS_HAVE_STDCALL
  void NS_STDCALL f0s() {}
  void NS_STDCALL f0cs() const {}
  int NS_STDCALL f1s(char) { return 0; }
  int NS_STDCALL f1cs(char) const { return 0; }
#endif  // NS_HAVE_STDCALL
};

static_assert(
    std::is_same<typename FunctionTypeTraits<decltype(f0)>::ReturnType,
                 void>::value,
    "f0 returns void");
static_assert(FunctionTypeTraits<decltype(f0)>::arity == 0,
              "f0 takes no parameters");
static_assert(
    std::is_same<
        typename FunctionTypeTraits<decltype(f0)>::template ParameterType<0>,
        void>::value,
    "f0 has no first parameter");

static_assert(
    std::is_same<typename FunctionTypeTraits<decltype(&S::f0)>::ReturnType,
                 void>::value,
    "S::f0 returns void");
static_assert(FunctionTypeTraits<decltype(&S::f0)>::arity == 0,
              "S::f0 takes no parameters");
static_assert(std::is_same<typename FunctionTypeTraits<
                               decltype(&S::f0)>::template ParameterType<0>,
                           void>::value,
              "S::f0 has no first parameter");

static_assert(
    std::is_same<typename FunctionTypeTraits<decltype(&S::f0c)>::ReturnType,
                 void>::value,
    "S::f0c returns void");
static_assert(FunctionTypeTraits<decltype(&S::f0c)>::arity == 0,
              "S::f0c takes no parameters");
static_assert(std::is_same<typename FunctionTypeTraits<
                               decltype(&S::f0c)>::template ParameterType<0>,
                           void>::value,
              "S::f0c has no first parameter");

static_assert(
    std::is_same<typename FunctionTypeTraits<decltype(f1)>::ReturnType,
                 int>::value,
    "f1 returns int");
static_assert(FunctionTypeTraits<decltype(f1)>::arity == 1,
              "f1 takes one parameter");
static_assert(
    std::is_same<
        typename FunctionTypeTraits<decltype(f1)>::template ParameterType<0>,
        char>::value,
    "f1 takes a char");

static_assert(
    std::is_same<typename FunctionTypeTraits<decltype(&S::f1)>::ReturnType,
                 int>::value,
    "S::f1 returns int");
static_assert(FunctionTypeTraits<decltype(&S::f1)>::arity == 1,
              "S::f1 takes one parameter");
static_assert(std::is_same<typename FunctionTypeTraits<
                               decltype(&S::f1)>::template ParameterType<0>,
                           char>::value,
              "S::f1 takes a char");

static_assert(
    std::is_same<typename FunctionTypeTraits<decltype(&S::f1c)>::ReturnType,
                 int>::value,
    "S::f1c returns int");
static_assert(FunctionTypeTraits<decltype(&S::f1c)>::arity == 1,
              "S::f1c takes one parameter");
static_assert(std::is_same<typename FunctionTypeTraits<
                               decltype(&S::f1c)>::template ParameterType<0>,
                           char>::value,
              "S::f1c takes a char");

#ifdef NS_HAVE_STDCALL
static_assert(
    std::is_same<typename FunctionTypeTraits<decltype(f0s)>::ReturnType,
                 void>::value,
    "f0s returns void");
static_assert(FunctionTypeTraits<decltype(f0s)>::arity == 0,
              "f0s takes no parameters");
static_assert(
    std::is_same<
        typename FunctionTypeTraits<decltype(f0s)>::template ParameterType<0>,
        void>::value,
    "f0s has no first parameter");

static_assert(
    std::is_same<typename FunctionTypeTraits<decltype(&S::f0s)>::ReturnType,
                 void>::value,
    "S::f0s returns void");
static_assert(FunctionTypeTraits<decltype(&S::f0s)>::arity == 0,
              "S::f0s takes no parameters");
static_assert(std::is_same<typename FunctionTypeTraits<
                               decltype(&S::f0s)>::template ParameterType<0>,
                           void>::value,
              "S::f0s has no first parameter");

static_assert(
    std::is_same<typename FunctionTypeTraits<decltype(&S::f0cs)>::ReturnType,
                 void>::value,
    "S::f0cs returns void");
static_assert(FunctionTypeTraits<decltype(&S::f0cs)>::arity == 0,
              "S::f0cs takes no parameters");
static_assert(std::is_same<typename FunctionTypeTraits<
                               decltype(&S::f0cs)>::template ParameterType<0>,
                           void>::value,
              "S::f0cs has no first parameter");

static_assert(
    std::is_same<typename FunctionTypeTraits<decltype(f1s)>::ReturnType,
                 int>::value,
    "f1s returns int");
static_assert(FunctionTypeTraits<decltype(f1s)>::arity == 1,
              "f1s takes one parameter");
static_assert(
    std::is_same<
        typename FunctionTypeTraits<decltype(f1s)>::template ParameterType<0>,
        char>::value,
    "f1s takes a char");

static_assert(
    std::is_same<typename FunctionTypeTraits<decltype(&S::f1s)>::ReturnType,
                 int>::value,
    "S::f1s returns int");
static_assert(FunctionTypeTraits<decltype(&S::f1s)>::arity == 1,
              "S::f1s takes one parameter");
static_assert(std::is_same<typename FunctionTypeTraits<
                               decltype(&S::f1s)>::template ParameterType<0>,
                           char>::value,
              "S::f1s takes a char");

static_assert(
    std::is_same<typename FunctionTypeTraits<decltype(&S::f1cs)>::ReturnType,
                 int>::value,
    "S::f1cs returns int");
static_assert(FunctionTypeTraits<decltype(&S::f1cs)>::arity == 1,
              "S::f1cs takes one parameter");
static_assert(std::is_same<typename FunctionTypeTraits<
                               decltype(&S::f1cs)>::template ParameterType<0>,
                           char>::value,
              "S::f1cs takes a char");
#endif  // NS_HAVE_STDCALL

template <typename F>
void TestVoidVoid(F&&) {
  static_assert(
      std::is_same<typename FunctionTypeTraits<F>::ReturnType, void>::value,
      "Should return void");
  static_assert(FunctionTypeTraits<F>::arity == 0, "Should take no parameters");
  static_assert(
      std::is_same<typename FunctionTypeTraits<F>::template ParameterType<0>,
                   void>::value,
      "Should have no first parameter");
}

template <typename F>
void TestIntChar(F&&) {
  static_assert(
      std::is_same<typename FunctionTypeTraits<F>::ReturnType, int>::value,
      "Should return int");
  static_assert(FunctionTypeTraits<F>::arity == 1, "Should take one parameter");
  static_assert(
      std::is_same<typename FunctionTypeTraits<F>::template ParameterType<0>,
                   char>::value,
      "Should take a char");
}

int main() {
  TestVoidVoid(f0);
  TestVoidVoid(&f0);
  TestVoidVoid(&S::f0);
  TestVoidVoid(&S::f0c);
  TestVoidVoid([]() {});
  std::function<void()> ff0 = f0;
  TestVoidVoid(ff0);

  TestIntChar(f1);
  TestIntChar(&f1);
  TestIntChar(&S::f1);
  TestIntChar(&S::f1c);
  TestIntChar([](char) { return 0; });
  std::function<int(char)> ff1 = f1;
  TestIntChar(ff1);

#ifdef NS_HAVE_STDCALL
  TestVoidVoid(f0s);
  TestVoidVoid(&f0s);
  TestVoidVoid(&S::f0s);
  TestVoidVoid(&S::f0cs);
  std::function<void()> ff0s = f0s;
  TestVoidVoid(ff0s);

  TestIntChar(f1s);
  TestIntChar(&f1s);
  TestIntChar(&S::f1s);
  TestIntChar(&S::f1cs);
  std::function<int(char)> ff1s = f1s;
  TestIntChar(ff1s);
#endif  // NS_HAVE_STDCALL

  return 0;
}
