/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/TypeTraits.h"

#define TEST_CV_QUALIFIERS(test, type, ...)             \
  test(type, __VA_ARGS__) test(const type, __VA_ARGS__) \
      test(volatile type, __VA_ARGS__) test(const volatile type, __VA_ARGS__)

using mozilla::AddPointer;
using mozilla::AddRvalueReference;
using mozilla::Decay;
using mozilla::DeclVal;
using mozilla::IsArray;
using mozilla::IsClass;
using mozilla::IsConvertible;
using mozilla::IsDestructible;
using mozilla::IsFunction;
using mozilla::IsPointer;
using mozilla::IsSame;
using mozilla::IsSigned;
using mozilla::IsUnsigned;
using mozilla::RemoveExtent;
using mozilla::RemovePointer;

static_assert(!IsFunction<int>::value, "int is not a function type");
static_assert(IsFunction<void(int)>::value, "void(int) is a function type");
static_assert(!IsFunction<void (*)(int)>::value,
              "void(*)(int) is not a function type");

static_assert(!IsArray<bool>::value, "bool not an array");
static_assert(IsArray<bool[]>::value, "bool[] is an array");
static_assert(IsArray<bool[5]>::value, "bool[5] is an array");

static_assert(!IsPointer<bool>::value, "bool not a pointer");
static_assert(IsPointer<bool*>::value, "bool* is a pointer");
static_assert(IsPointer<bool* const>::value, "bool* const is a pointer");
static_assert(IsPointer<bool* volatile>::value, "bool* volatile is a pointer");
static_assert(IsPointer<bool* const volatile>::value,
              "bool* const volatile is a pointer");
static_assert(IsPointer<bool**>::value, "bool** is a pointer");
static_assert(IsPointer<void (*)(void)>::value, "void (*)(void) is a pointer");
struct IsPointerTest {
  bool m;
  void f();
};
static_assert(!IsPointer<IsPointerTest>::value, "IsPointerTest not a pointer");
static_assert(IsPointer<IsPointerTest*>::value, "IsPointerTest* is a pointer");
static_assert(!IsPointer<bool (IsPointerTest::*)()>::value,
              "bool(IsPointerTest::*)() not a pointer");
static_assert(!IsPointer<void (IsPointerTest::*)(void)>::value,
              "void(IsPointerTest::*)(void) not a pointer");

namespace CPlusPlus11IsMemberPointer {

using mozilla::IsMemberPointer;

struct S {};
union U {};

#define ASSERT_IS_MEMBER_POINTER(type, msg) \
  static_assert(IsMemberPointer<type>::value, #type msg);
#define TEST_IS_MEMBER_POINTER(type)                 \
  TEST_CV_QUALIFIERS(ASSERT_IS_MEMBER_POINTER, type, \
                     " is a member pointer type")

TEST_IS_MEMBER_POINTER(int S::*)
TEST_IS_MEMBER_POINTER(int U::*)

#undef TEST_IS_MEMBER_POINTER
#undef ASSERT_IS_MEMBER_POINTER

#define ASSERT_IS_NOT_MEMBER_POINTER(type, msg) \
  static_assert(!IsMemberPointer<type>::value, #type msg);
#define TEST_IS_NOT_MEMBER_POINTER(type)                 \
  TEST_CV_QUALIFIERS(ASSERT_IS_NOT_MEMBER_POINTER, type, \
                     " is not a member pointer type")

TEST_IS_NOT_MEMBER_POINTER(int*)

#undef TEST_IS_NOT_MEMBER_POINTER
#undef ASSERT_IS_NOT_MEMBER_POINTER

}  // namespace CPlusPlus11IsMemberPointer

namespace CPlusPlus11IsScalar {

using mozilla::IsScalar;

enum E {};
enum class EC {};
class C {};
struct S {};
union U {};

#define ASSERT_IS_SCALAR(type, msg) \
  static_assert(IsScalar<type>::value, #type msg);
#define TEST_IS_SCALAR(type) \
  TEST_CV_QUALIFIERS(ASSERT_IS_SCALAR, type, " is a scalar type")

TEST_IS_SCALAR(int)
TEST_IS_SCALAR(float)
TEST_IS_SCALAR(E)
TEST_IS_SCALAR(EC)
TEST_IS_SCALAR(S*)
TEST_IS_SCALAR(int S::*)

#undef TEST_IS_SCALAR
#undef ASSERT_IS_SCALAR

#define ASSERT_IS_NOT_SCALAR(type, msg) \
  static_assert(!IsScalar<type>::value, #type msg);
#define TEST_IS_NOT_SCALAR(type) \
  TEST_CV_QUALIFIERS(ASSERT_IS_NOT_SCALAR, type, " is not a scalar type")

TEST_IS_NOT_SCALAR(C)
TEST_IS_NOT_SCALAR(S)
TEST_IS_NOT_SCALAR(U)

#undef TEST_IS_NOT_SCALAR
#undef ASSERT_IS_NOT_SCALAR

}  // namespace CPlusPlus11IsScalar

struct S1 {};
union U1 {
  int mX;
};

static_assert(!IsClass<int>::value, "int isn't a class");
static_assert(IsClass<const S1>::value, "S is a class");
static_assert(!IsClass<U1>::value, "U isn't a class");

static_assert(!IsSigned<bool>::value, "bool shouldn't be signed");
static_assert(IsUnsigned<bool>::value, "bool should be unsigned");

static_assert(!IsSigned<const bool>::value, "const bool shouldn't be signed");
static_assert(IsUnsigned<const bool>::value, "const bool should be unsigned");

static_assert(!IsSigned<volatile bool>::value,
              "volatile bool shouldn't be signed");
static_assert(IsUnsigned<volatile bool>::value,
              "volatile bool should be unsigned");

static_assert(!IsSigned<unsigned char>::value,
              "unsigned char shouldn't be signed");
static_assert(IsUnsigned<unsigned char>::value,
              "unsigned char should be unsigned");
static_assert(IsSigned<signed char>::value, "signed char should be signed");
static_assert(!IsUnsigned<signed char>::value,
              "signed char shouldn't be unsigned");

static_assert(!IsSigned<unsigned short>::value,
              "unsigned short shouldn't be signed");
static_assert(IsUnsigned<unsigned short>::value,
              "unsigned short should be unsigned");
static_assert(IsSigned<short>::value, "short should be signed");
static_assert(!IsUnsigned<short>::value, "short shouldn't be unsigned");

static_assert(!IsSigned<unsigned int>::value,
              "unsigned int shouldn't be signed");
static_assert(IsUnsigned<unsigned int>::value,
              "unsigned int should be unsigned");
static_assert(IsSigned<int>::value, "int should be signed");
static_assert(!IsUnsigned<int>::value, "int shouldn't be unsigned");

static_assert(!IsSigned<unsigned long>::value,
              "unsigned long shouldn't be signed");
static_assert(IsUnsigned<unsigned long>::value,
              "unsigned long should be unsigned");
static_assert(IsSigned<long>::value, "long should be signed");
static_assert(!IsUnsigned<long>::value, "long shouldn't be unsigned");

static_assert(IsSigned<float>::value, "float should be signed");
static_assert(!IsUnsigned<float>::value, "float shouldn't be unsigned");

static_assert(IsSigned<const float>::value, "const float should be signed");
static_assert(!IsUnsigned<const float>::value,
              "const float shouldn't be unsigned");

static_assert(IsSigned<double>::value, "double should be signed");
static_assert(!IsUnsigned<double>::value, "double shouldn't be unsigned");

static_assert(IsSigned<volatile double>::value,
              "volatile double should be signed");
static_assert(!IsUnsigned<volatile double>::value,
              "volatile double shouldn't be unsigned");

static_assert(IsSigned<long double>::value, "long double should be signed");
static_assert(!IsUnsigned<long double>::value,
              "long double shouldn't be unsigned");

static_assert(IsSigned<const volatile long double>::value,
              "const volatile long double should be signed");
static_assert(!IsUnsigned<const volatile long double>::value,
              "const volatile long double shouldn't be unsigned");

class NotIntConstructible {
  NotIntConstructible(int) = delete;
};

static_assert(!IsSigned<NotIntConstructible>::value,
              "non-arithmetic types are not signed");
static_assert(!IsUnsigned<NotIntConstructible>::value,
              "non-arithmetic types are not unsigned");

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

class A {};
class B : public A {};
class C : private A {};
class D {};
class E : public A {};
class F : public B, public E {};

class ExplicitCopyConstructor {
  explicit ExplicitCopyConstructor(const ExplicitCopyConstructor&) = default;
};

static void TestIsConvertible() {
  // Pointer type convertibility
  static_assert((IsConvertible<A*, A*>::value), "A* should convert to A*");
  static_assert((IsConvertible<B*, A*>::value), "B* should convert to A*");
  static_assert((!IsConvertible<A*, B*>::value), "A* shouldn't convert to B*");
  static_assert((!IsConvertible<A*, C*>::value), "A* shouldn't convert to C*");
  static_assert((!IsConvertible<A*, D*>::value),
                "A* shouldn't convert to unrelated D*");
  static_assert((!IsConvertible<D*, A*>::value),
                "D* shouldn't convert to unrelated A*");

  // Instance type convertibility
  static_assert((IsConvertible<A, A>::value), "A is A");
  static_assert((IsConvertible<B, A>::value), "B converts to A");
  static_assert((!IsConvertible<D, A>::value), "D and A are unrelated");
  static_assert((!IsConvertible<A, D>::value), "A and D are unrelated");

  static_assert(IsConvertible<void, void>::value, "void is void");
  static_assert(!IsConvertible<A, void>::value, "A shouldn't convert to void");
  static_assert(!IsConvertible<void, B>::value, "void shouldn't convert to B");

  static_assert(!IsConvertible<const ExplicitCopyConstructor&,
                               ExplicitCopyConstructor>::value,
                "IsConvertible should test for implicit convertibility");

  // These cases seem to require C++11 support to properly implement them, so
  // for now just disable them.
  // static_assert((!IsConvertible<C*, A*>::value),
  //           "C* shouldn't convert to A* (private inheritance)");
  // static_assert((!IsConvertible<C, A>::value),
  //           "C doesn't convert to A (private inheritance)");
}

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

static_assert(IsSame<RemoveExtent<int>::Type, int>::value,
              "removing extent from non-array must return the non-array");
static_assert(
    IsSame<RemoveExtent<const int[]>::Type, const int>::value,
    "removing extent from unknown-bound array must return element type");
static_assert(
    IsSame<RemoveExtent<volatile int[5]>::Type, volatile int>::value,
    "removing extent from known-bound array must return element type");
static_assert(
    IsSame<RemoveExtent<long[][17]>::Type, long[17]>::value,
    "removing extent from multidimensional array must return element type");

struct TestRemovePointer {
  bool m;
  void f();
};
static_assert(IsSame<RemovePointer<int>::Type, int>::value,
              "removing pointer from int must return int");
static_assert(IsSame<RemovePointer<int*>::Type, int>::value,
              "removing pointer from int* must return int");
static_assert(IsSame<RemovePointer<int* const>::Type, int>::value,
              "removing pointer from int* const must return int");
static_assert(IsSame<RemovePointer<int* volatile>::Type, int>::value,
              "removing pointer from int* volatile must return int");
static_assert(IsSame<RemovePointer<const long*>::Type, const long>::value,
              "removing pointer from const long* must return const long");
static_assert(IsSame<RemovePointer<void* const>::Type, void>::value,
              "removing pointer from void* const must return void");
static_assert(IsSame<RemovePointer<void (TestRemovePointer::*)()>::Type,
                     void (TestRemovePointer::*)()>::value,
              "removing pointer from void (S::*)() must return void (S::*)()");
static_assert(IsSame<RemovePointer<void (*)()>::Type, void()>::value,
              "removing pointer from void (*)() must return void()");
static_assert(IsSame<RemovePointer<bool TestRemovePointer::*>::Type,
                     bool TestRemovePointer::*>::value,
              "removing pointer from bool S::* must return bool S::*");

static_assert(IsSame<AddPointer<int>::Type, int*>::value,
              "adding pointer to int must return int*");
static_assert(IsSame<AddPointer<int*>::Type, int**>::value,
              "adding pointer to int* must return int**");
static_assert(IsSame<AddPointer<int&>::Type, int*>::value,
              "adding pointer to int& must return int*");
static_assert(IsSame<AddPointer<int* const>::Type, int* const*>::value,
              "adding pointer to int* const must return int* const*");
static_assert(IsSame<AddPointer<int* volatile>::Type, int* volatile*>::value,
              "adding pointer to int* volatile must return int* volatile*");

static_assert(IsSame<Decay<int>::Type, int>::value,
              "decaying int must return int");
static_assert(IsSame<Decay<int*>::Type, int*>::value,
              "decaying int* must return int*");
static_assert(IsSame<Decay<int* const>::Type, int*>::value,
              "decaying int* const must return int*");
static_assert(IsSame<Decay<int* volatile>::Type, int*>::value,
              "decaying int* volatile must return int*");
static_assert(IsSame<Decay<int&>::Type, int>::value,
              "decaying int& must return int");
static_assert(IsSame<Decay<const int&>::Type, int>::value,
              "decaying const int& must return int");
static_assert(IsSame<Decay<int&&>::Type, int>::value,
              "decaying int&& must return int");
static_assert(IsSame<Decay<int[1]>::Type, int*>::value,
              "decaying int[1] must return int*");
static_assert(IsSame<Decay<void(int)>::Type, void (*)(int)>::value,
              "decaying void(int) must return void(*)(int)");

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

int main() {
  TestIsConvertible();
  return 0;
}
