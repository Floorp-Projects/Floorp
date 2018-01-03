/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/TypeTraits.h"

#define TEST_CV_QUALIFIERS(test, type, ...) \
  test(type, __VA_ARGS__) \
  test(const type, __VA_ARGS__) \
  test(volatile type, __VA_ARGS__) \
  test(const volatile type, __VA_ARGS__)

using mozilla::AddLvalueReference;
using mozilla::AddPointer;
using mozilla::AddRvalueReference;
using mozilla::Decay;
using mozilla::DeclVal;
using mozilla::IsFunction;
using mozilla::IsArray;
using mozilla::IsBaseOf;
using mozilla::IsClass;
using mozilla::IsConvertible;
using mozilla::IsDefaultConstructible;
using mozilla::IsDestructible;
using mozilla::IsEmpty;
using mozilla::IsLvalueReference;
using mozilla::IsPointer;
using mozilla::IsReference;
using mozilla::IsRvalueReference;
using mozilla::IsSame;
using mozilla::IsSigned;
using mozilla::IsUnsigned;
using mozilla::MakeSigned;
using mozilla::MakeUnsigned;
using mozilla::RemoveExtent;
using mozilla::RemovePointer;

static_assert(!IsFunction<int>::value,
              "int is not a function type");
static_assert(IsFunction<void(int)>::value,
              "void(int) is a function type");
static_assert(!IsFunction<void(*)(int)>::value,
              "void(*)(int) is not a function type");

static_assert(!IsArray<bool>::value,
              "bool not an array");
static_assert(IsArray<bool[]>::value,
              "bool[] is an array");
static_assert(IsArray<bool[5]>::value,
              "bool[5] is an array");

static_assert(!IsPointer<bool>::value,
              "bool not a pointer");
static_assert(IsPointer<bool*>::value,
              "bool* is a pointer");
static_assert(IsPointer<bool* const>::value,
              "bool* const is a pointer");
static_assert(IsPointer<bool* volatile>::value,
              "bool* volatile is a pointer");
static_assert(IsPointer<bool* const volatile>::value,
              "bool* const volatile is a pointer");
static_assert(IsPointer<bool**>::value,
              "bool** is a pointer");
static_assert(IsPointer<void (*)(void)>::value,
              "void (*)(void) is a pointer");
struct IsPointerTest { bool m; void f(); };
static_assert(!IsPointer<IsPointerTest>::value,
              "IsPointerTest not a pointer");
static_assert(IsPointer<IsPointerTest*>::value,
              "IsPointerTest* is a pointer");
static_assert(!IsPointer<bool(IsPointerTest::*)()>::value,
              "bool(IsPointerTest::*) not a pointer");
static_assert(!IsPointer<void(IsPointerTest::*)(void)>::value,
              "void(IsPointerTest::*)(void) not a pointer");

static_assert(!IsLvalueReference<bool>::value,
              "bool not an lvalue reference");
static_assert(!IsLvalueReference<bool*>::value,
              "bool* not an lvalue reference");
static_assert(IsLvalueReference<bool&>::value,
              "bool& is an lvalue reference");
static_assert(!IsLvalueReference<bool&&>::value,
              "bool&& not an lvalue reference");

static_assert(!IsLvalueReference<void>::value,
              "void not an lvalue reference");
static_assert(!IsLvalueReference<void*>::value,
              "void* not an lvalue reference");

static_assert(!IsLvalueReference<int>::value,
              "int not an lvalue reference");
static_assert(!IsLvalueReference<int*>::value,
              "int* not an lvalue reference");
static_assert(IsLvalueReference<int&>::value,
              "int& is an lvalue reference");
static_assert(!IsLvalueReference<int&&>::value,
              "int&& not an lvalue reference");

static_assert(!IsRvalueReference<bool>::value,
              "bool not an rvalue reference");
static_assert(!IsRvalueReference<bool*>::value,
              "bool* not an rvalue reference");
static_assert(!IsRvalueReference<bool&>::value,
              "bool& not an rvalue reference");
static_assert(IsRvalueReference<bool&&>::value,
              "bool&& is an rvalue reference");

static_assert(!IsRvalueReference<void>::value,
              "void not an rvalue reference");
static_assert(!IsRvalueReference<void*>::value,
              "void* not an rvalue reference");

static_assert(!IsRvalueReference<int>::value,
              "int not an rvalue reference");
static_assert(!IsRvalueReference<int*>::value,
              "int* not an rvalue reference");
static_assert(!IsRvalueReference<int&>::value,
              "int& not an rvalue reference");
static_assert(IsRvalueReference<int&&>::value,
              "int&& is an rvalue reference");

static_assert(!IsReference<bool>::value,
              "bool not a reference");
static_assert(!IsReference<bool*>::value,
              "bool* not a reference");
static_assert(IsReference<bool&>::value,
              "bool& is a reference");
static_assert(IsReference<bool&&>::value,
              "bool&& is a reference");

static_assert(!IsReference<void>::value,
              "void not a reference");
static_assert(!IsReference<void*>::value,
              "void* not a reference");

static_assert(!IsReference<int>::value,
              "int not a reference");
static_assert(!IsReference<int*>::value,
              "int* not a reference");
static_assert(IsReference<int&>::value,
              "int& is a reference");
static_assert(IsReference<int&&>::value,
              "int&& is a reference");

namespace CPlusPlus11IsMemberPointer {

using mozilla::IsMemberPointer;

struct S {};
union U {};

#define ASSERT_IS_MEMBER_POINTER(type, msg) \
  static_assert(IsMemberPointer<type>::value, #type msg);
#define TEST_IS_MEMBER_POINTER(type) \
  TEST_CV_QUALIFIERS(ASSERT_IS_MEMBER_POINTER, type, \
                     " is a member pointer type")

TEST_IS_MEMBER_POINTER(int S::*)
TEST_IS_MEMBER_POINTER(int U::*)

#undef TEST_IS_MEMBER_POINTER
#undef ASSERT_IS_MEMBER_POINTER

#define ASSERT_IS_NOT_MEMBER_POINTER(type, msg) \
  static_assert(!IsMemberPointer<type>::value, #type msg);
#define TEST_IS_NOT_MEMBER_POINTER(type) \
  TEST_CV_QUALIFIERS(ASSERT_IS_NOT_MEMBER_POINTER, type, \
                     " is not a member pointer type")

TEST_IS_NOT_MEMBER_POINTER(int*)

#undef TEST_IS_NOT_MEMBER_POINTER
#undef ASSERT_IS_NOT_MEMBER_POINTER

} // CPlusPlus11IsMemberPointer

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

} // CPlusPlus11IsScalar

struct S1 {};
union U1 { int mX; };

static_assert(!IsClass<int>::value,
              "int isn't a class");
static_assert(IsClass<const S1>::value,
              "S is a class");
static_assert(!IsClass<U1>::value,
              "U isn't a class");

static_assert(!mozilla::IsEmpty<int>::value,
              "not a class => not empty");
static_assert(!mozilla::IsEmpty<bool[5]>::value,
              "not a class => not empty");

static_assert(!mozilla::IsEmpty<U1>::value,
              "not a class => not empty");

struct E1 {};
struct E2 { int : 0; };
struct E3 : E1 {};
struct E4 : E2 {};

static_assert(IsEmpty<const volatile S1>::value,
              "S should be empty");

static_assert(mozilla::IsEmpty<E1>::value &&
              mozilla::IsEmpty<E2>::value &&
              mozilla::IsEmpty<E3>::value &&
              mozilla::IsEmpty<E4>::value,
              "all empty");

union U2 { E1 e1; };
static_assert(!mozilla::IsEmpty<U2>::value,
              "not a class => not empty");

struct NE1 { int mX; };
struct NE2 : virtual E1 {};
struct NE3 : E2 { virtual ~NE3() {} };
struct NE4 { virtual void f() {} };

static_assert(!mozilla::IsEmpty<NE1>::value &&
              !mozilla::IsEmpty<NE2>::value &&
              !mozilla::IsEmpty<NE3>::value &&
              !mozilla::IsEmpty<NE4>::value,
              "all empty");

static_assert(!IsSigned<bool>::value,
              "bool shouldn't be signed");
static_assert(IsUnsigned<bool>::value,
              "bool should be unsigned");

static_assert(!IsSigned<const bool>::value,
              "const bool shouldn't be signed");
static_assert(IsUnsigned<const bool>::value,
              "const bool should be unsigned");

static_assert(!IsSigned<volatile bool>::value,
              "volatile bool shouldn't be signed");
static_assert(IsUnsigned<volatile bool>::value,
              "volatile bool should be unsigned");

static_assert(!IsSigned<unsigned char>::value,
              "unsigned char shouldn't be signed");
static_assert(IsUnsigned<unsigned char>::value,
              "unsigned char should be unsigned");
static_assert(IsSigned<signed char>::value,
              "signed char should be signed");
static_assert(!IsUnsigned<signed char>::value,
              "signed char shouldn't be unsigned");

static_assert(!IsSigned<unsigned short>::value,
              "unsigned short shouldn't be signed");
static_assert(IsUnsigned<unsigned short>::value,
              "unsigned short should be unsigned");
static_assert(IsSigned<short>::value,
              "short should be signed");
static_assert(!IsUnsigned<short>::value,
              "short shouldn't be unsigned");

static_assert(!IsSigned<unsigned int>::value,
              "unsigned int shouldn't be signed");
static_assert(IsUnsigned<unsigned int>::value,
              "unsigned int should be unsigned");
static_assert(IsSigned<int>::value,
              "int should be signed");
static_assert(!IsUnsigned<int>::value,
              "int shouldn't be unsigned");

static_assert(!IsSigned<unsigned long>::value,
              "unsigned long shouldn't be signed");
static_assert(IsUnsigned<unsigned long>::value,
              "unsigned long should be unsigned");
static_assert(IsSigned<long>::value,
              "long should be signed");
static_assert(!IsUnsigned<long>::value,
              "long shouldn't be unsigned");

static_assert(IsSigned<float>::value,
              "float should be signed");
static_assert(!IsUnsigned<float>::value,
              "float shouldn't be unsigned");

static_assert(IsSigned<const float>::value,
              "const float should be signed");
static_assert(!IsUnsigned<const float>::value,
              "const float shouldn't be unsigned");

static_assert(IsSigned<double>::value,
              "double should be signed");
static_assert(!IsUnsigned<double>::value,
              "double shouldn't be unsigned");

static_assert(IsSigned<volatile double>::value,
              "volatile double should be signed");
static_assert(!IsUnsigned<volatile double>::value,
              "volatile double shouldn't be unsigned");

static_assert(IsSigned<long double>::value,
              "long double should be signed");
static_assert(!IsUnsigned<long double>::value,
              "long double shouldn't be unsigned");

static_assert(IsSigned<const volatile long double>::value,
              "const volatile long double should be signed");
static_assert(!IsUnsigned<const volatile long double>::value,
              "const volatile long double shouldn't be unsigned");

class NotIntConstructible
{
  NotIntConstructible(int) = delete;
};

static_assert(!IsSigned<NotIntConstructible>::value,
              "non-arithmetic types are not signed");
static_assert(!IsUnsigned<NotIntConstructible>::value,
              "non-arithmetic types are not unsigned");

struct TrivialCtor0 {};
struct TrivialCtor1 { int mX; };

struct DefaultCtor0 { DefaultCtor0() {} };
struct DefaultCtor1 { DefaultCtor1() = default; };
struct DefaultCtor2 { DefaultCtor2() {} explicit DefaultCtor2(int) {} };

struct NoDefaultCtor0 { explicit NoDefaultCtor0(int) {} };
struct NoDefaultCtor1 { NoDefaultCtor1() = delete; };

class PrivateCtor0 { PrivateCtor0() {} };
class PrivateCtor1 { PrivateCtor1() = default; };

enum EnumCtor0 {};
enum EnumCtor1 : int {};

enum class EnumClassCtor0 {};
enum class EnumClassCtor1 : int {};

union UnionCtor0 {};
union UnionCtor1 { int mX; };

union UnionCustomCtor0 { explicit UnionCustomCtor0(int) {} };
union UnionCustomCtor1
{
  int mX;
  explicit UnionCustomCtor1(int aX) : mX(aX) {}
};

static_assert(IsDefaultConstructible<int>::value,
              "integral type is default-constructible");

static_assert(IsDefaultConstructible<TrivialCtor0>::value,
              "trivial constructor class 0 is default-constructible");
static_assert(IsDefaultConstructible<TrivialCtor1>::value,
              "trivial constructor class 1 is default-constructible");

static_assert(IsDefaultConstructible<DefaultCtor0>::value,
              "default constructor class 0 is default-constructible");
static_assert(IsDefaultConstructible<DefaultCtor1>::value,
              "default constructor class 1 is default-constructible");
static_assert(IsDefaultConstructible<DefaultCtor2>::value,
              "default constructor class 2 is default-constructible");

static_assert(!IsDefaultConstructible<NoDefaultCtor0>::value,
              "no default constructor class is not default-constructible");
static_assert(!IsDefaultConstructible<NoDefaultCtor1>::value,
              "deleted default constructor class is not default-constructible");

static_assert(!IsDefaultConstructible<PrivateCtor0>::value,
              "private default constructor class 0 is not default-constructible");
static_assert(!IsDefaultConstructible<PrivateCtor1>::value,
              "private default constructor class 1 is not default-constructible");

static_assert(IsDefaultConstructible<EnumCtor0>::value,
              "enum constructor 0 is default-constructible");
static_assert(IsDefaultConstructible<EnumCtor1>::value,
              "enum constructor 1 is default-constructible");

static_assert(IsDefaultConstructible<EnumClassCtor0>::value,
              "enum class constructor 0 is default-constructible");
static_assert(IsDefaultConstructible<EnumClassCtor1>::value,
              "enum class constructor 1 is default-constructible");

static_assert(IsDefaultConstructible<UnionCtor0>::value,
              "union constructor 0 is default-constructible");
static_assert(IsDefaultConstructible<UnionCtor1>::value,
              "union constructor 1 is default-constructible");

static_assert(!IsDefaultConstructible<UnionCustomCtor0>::value,
              "union with custom 1-arg constructor 0 is not default-constructible");
static_assert(!IsDefaultConstructible<UnionCustomCtor1>::value,
              "union with custom 1-arg constructor 1 is not default-constructible");

class PublicDestructible
{
public:
  ~PublicDestructible();
};
class PrivateDestructible
{
private:
  ~PrivateDestructible();
};
class TrivialDestructible
{
};

static_assert(IsDestructible<PublicDestructible>::value,
              "public destructible class is destructible");
static_assert(!IsDestructible<PrivateDestructible>::value,
              "private destructible class is not destructible");
static_assert(IsDestructible<TrivialDestructible>::value,
              "trivial destructible class is destructible");

namespace CPlusPlus11IsBaseOf {

// Adapted from C++11 § 20.9.6.
struct B {};
struct B1 : B {};
struct B2 : B {};
struct D : private B1, private B2 {};

static void
StandardIsBaseOfTests()
{
  static_assert((IsBaseOf<B, D>::value) == true,
                "IsBaseOf fails on diamond");
  static_assert((IsBaseOf<const B, D>::value) == true,
                "IsBaseOf fails on diamond plus constness change");
  static_assert((IsBaseOf<B, const D>::value) == true,
                "IsBaseOf fails on diamond plus constness change");
  static_assert((IsBaseOf<B, const B>::value) == true,
                "IsBaseOf fails on constness change");
  static_assert((IsBaseOf<D, B>::value) == false,
                "IsBaseOf got the direction of inheritance wrong");
  static_assert((IsBaseOf<B&, D&>::value) == false,
                "IsBaseOf should return false on references");
  static_assert((IsBaseOf<B[3], D[3]>::value) == false,
                "IsBaseOf should return false on arrays");
  // We fail at the following test.  To fix it, we need to specialize IsBaseOf
  // for all built-in types.
  // static_assert((IsBaseOf<int, int>::value) == false);
}

} /* namespace CPlusPlus11IsBaseOf */

class A { };
class B : public A { };
class C : private A { };
class D { };
class E : public A { };
class F : public B, public E { };

static void
TestIsBaseOf()
{
  static_assert((IsBaseOf<A, B>::value),
                "A is a base of B");
  static_assert((!IsBaseOf<B, A>::value),
                "B is not a base of A");
  static_assert((IsBaseOf<A, C>::value),
                "A is a base of C");
  static_assert((!IsBaseOf<C, A>::value),
                "C is not a base of A");
  static_assert((IsBaseOf<A, F>::value),
                "A is a base of F");
  static_assert((!IsBaseOf<F, A>::value),
                "F is not a base of A");
  static_assert((!IsBaseOf<A, D>::value),
                "A is not a base of D");
  static_assert((!IsBaseOf<D, A>::value),
                "D is not a base of A");
  static_assert((IsBaseOf<B, B>::value),
                "B is the same as B (and therefore, a base of B)");
}

class ExplicitCopyConstructor {
  explicit ExplicitCopyConstructor(const ExplicitCopyConstructor&) = default;
};

static void
TestIsConvertible()
{
  // Pointer type convertibility
  static_assert((IsConvertible<A*, A*>::value),
                "A* should convert to A*");
  static_assert((IsConvertible<B*, A*>::value),
                "B* should convert to A*");
  static_assert((!IsConvertible<A*, B*>::value),
                "A* shouldn't convert to B*");
  static_assert((!IsConvertible<A*, C*>::value),
                "A* shouldn't convert to C*");
  static_assert((!IsConvertible<A*, D*>::value),
                "A* shouldn't convert to unrelated D*");
  static_assert((!IsConvertible<D*, A*>::value),
                "D* shouldn't convert to unrelated A*");

  // Instance type convertibility
  static_assert((IsConvertible<A, A>::value),
                "A is A");
  static_assert((IsConvertible<B, A>::value),
                "B converts to A");
  static_assert((!IsConvertible<D, A>::value),
                "D and A are unrelated");
  static_assert((!IsConvertible<A, D>::value),
                "A and D are unrelated");

  static_assert(IsConvertible<void, void>::value, "void is void");
  static_assert(!IsConvertible<A, void>::value, "A shouldn't convert to void");
  static_assert(!IsConvertible<void, B>::value, "void shouldn't convert to B");

  static_assert(!IsConvertible<const ExplicitCopyConstructor&,
                               ExplicitCopyConstructor>::value,
                "IsConvertible should test for implicit convertibility");

  // These cases seem to require C++11 support to properly implement them, so
  // for now just disable them.
  //static_assert((!IsConvertible<C*, A*>::value),
  //           "C* shouldn't convert to A* (private inheritance)");
  //static_assert((!IsConvertible<C, A>::value),
  //           "C doesn't convert to A (private inheritance)");
}

static_assert(IsSame<AddLvalueReference<int>::Type, int&>::value,
              "not adding & to int correctly");
static_assert(IsSame<AddLvalueReference<volatile int&>::Type, volatile int&>::value,
              "not adding & to volatile int& correctly");
static_assert(IsSame<AddLvalueReference<void*>::Type, void*&>::value,
              "not adding & to void* correctly");
static_assert(IsSame<AddLvalueReference<void>::Type, void>::value,
              "void shouldn't be transformed by AddLvalueReference");
static_assert(IsSame<AddLvalueReference<struct S1&&>::Type, struct S1&>::value,
              "not reference-collapsing struct S1&& & to struct S1& correctly");

static_assert(IsSame<AddRvalueReference<int>::Type, int&&>::value,
              "not adding && to int correctly");
static_assert(IsSame<AddRvalueReference<volatile int&>::Type, volatile int&>::value,
              "not adding && to volatile int& correctly");
static_assert(IsSame<AddRvalueReference<const int&&>::Type, const int&&>::value,
              "not adding && to volatile int& correctly");
static_assert(IsSame<AddRvalueReference<void*>::Type, void*&&>::value,
              "not adding && to void* correctly");
static_assert(IsSame<AddRvalueReference<void>::Type, void>::value,
              "void shouldn't be transformed by AddRvalueReference");
static_assert(IsSame<AddRvalueReference<struct S1&>::Type, struct S1&>::value,
              "not reference-collapsing struct S1& && to struct S1& correctly");

struct TestWithDefaultConstructor
{
  int foo() const { return 0; }
};
struct TestWithNoDefaultConstructor
{
  explicit TestWithNoDefaultConstructor(int) {}
  int foo() const { return 1; }
};

static_assert(IsSame<decltype(TestWithDefaultConstructor().foo()), int>::value,
              "decltype should work using a struct with a default constructor");
static_assert(IsSame<decltype(DeclVal<TestWithDefaultConstructor>().foo()), int>::value,
              "decltype should work using a DeclVal'd struct with a default constructor");
static_assert(IsSame<decltype(DeclVal<TestWithNoDefaultConstructor>().foo()), int>::value,
              "decltype should work using a DeclVal'd struct without a default constructor");

static_assert(IsSame<MakeSigned<const unsigned char>::Type, const signed char>::value,
              "const unsigned char won't signify correctly");
static_assert(IsSame<MakeSigned<volatile unsigned short>::Type, volatile signed short>::value,
              "volatile unsigned short won't signify correctly");
static_assert(IsSame<MakeSigned<const volatile unsigned int>::Type, const volatile signed int>::value,
              "const volatile unsigned int won't signify correctly");
static_assert(IsSame<MakeSigned<unsigned long>::Type, signed long>::value,
              "unsigned long won't signify correctly");
static_assert(IsSame<MakeSigned<const signed char>::Type, const signed char>::value,
              "const signed char won't signify correctly");

static_assert(IsSame<MakeSigned<volatile signed short>::Type, volatile signed short>::value,
              "volatile signed short won't signify correctly");
static_assert(IsSame<MakeSigned<const volatile signed int>::Type, const volatile signed int>::value,
              "const volatile signed int won't signify correctly");
static_assert(IsSame<MakeSigned<signed long>::Type, signed long>::value,
              "signed long won't signify correctly");

static_assert(IsSame<MakeSigned<char>::Type, signed char>::value,
              "char won't signify correctly");
static_assert(IsSame<MakeSigned<volatile char>::Type, volatile signed char>::value,
              "volatile char won't signify correctly");
static_assert(IsSame<MakeSigned<const char>::Type, const signed char>::value,
              "const char won't signify correctly");

static_assert(IsSame<MakeUnsigned<const signed char>::Type, const unsigned char>::value,
              "const signed char won't unsignify correctly");
static_assert(IsSame<MakeUnsigned<volatile signed short>::Type, volatile unsigned short>::value,
              "volatile signed short won't unsignify correctly");
static_assert(IsSame<MakeUnsigned<const volatile signed int>::Type, const volatile unsigned int>::value,
              "const volatile signed int won't unsignify correctly");
static_assert(IsSame<MakeUnsigned<signed long>::Type, unsigned long>::value,
              "signed long won't unsignify correctly");

static_assert(IsSame<MakeUnsigned<const unsigned char>::Type, const unsigned char>::value,
              "const unsigned char won't unsignify correctly");

static_assert(IsSame<MakeUnsigned<volatile unsigned short>::Type, volatile unsigned short>::value,
              "volatile unsigned short won't unsignify correctly");
static_assert(IsSame<MakeUnsigned<const volatile unsigned int>::Type, const volatile unsigned int>::value,
              "const volatile unsigned int won't unsignify correctly");
static_assert(IsSame<MakeUnsigned<unsigned long>::Type, unsigned long>::value,
              "signed long won't unsignify correctly");

static_assert(IsSame<MakeUnsigned<char>::Type, unsigned char>::value,
              "char won't unsignify correctly");
static_assert(IsSame<MakeUnsigned<volatile char>::Type, volatile unsigned char>::value,
              "volatile char won't unsignify correctly");
static_assert(IsSame<MakeUnsigned<const char>::Type, const unsigned char>::value,
              "const char won't unsignify correctly");

static_assert(IsSame<RemoveExtent<int>::Type, int>::value,
              "removing extent from non-array must return the non-array");
static_assert(IsSame<RemoveExtent<const int[]>::Type, const int>::value,
              "removing extent from unknown-bound array must return element type");
static_assert(IsSame<RemoveExtent<volatile int[5]>::Type, volatile int>::value,
              "removing extent from known-bound array must return element type");
static_assert(IsSame<RemoveExtent<long[][17]>::Type, long[17]>::value,
              "removing extent from multidimensional array must return element type");

struct TestRemovePointer { bool m; void f(); };
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
static_assert(IsSame<Decay<void(int)>::Type, void(*)(int)>::value,
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

int
main()
{
  CPlusPlus11IsBaseOf::StandardIsBaseOfTests();
  TestIsBaseOf();
  TestIsConvertible();
  return 0;
}
