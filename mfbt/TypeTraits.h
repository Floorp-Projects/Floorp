/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Template-based metaprogramming and type-testing facilities. */

#ifndef mozilla_TypeTraits_h
#define mozilla_TypeTraits_h

#include "mozilla/Types.h"

/*
 * These traits are approximate copies of the traits and semantics from C++11's
 * <type_traits> header.  Don't add traits not in that header!  When all
 * platforms provide that header, we can convert all users and remove this one.
 */

namespace mozilla {

/* Forward declarations. */

template <typename>
struct RemoveCV;
template <typename>
struct AddRvalueReference;

/* 20.2.4 Function template declval [declval] */

/**
 * DeclVal simplifies the definition of expressions which occur as unevaluated
 * operands. It converts T to a reference type, making it possible to use in
 * decltype expressions even if T does not have a default constructor, e.g.:
 * decltype(DeclVal<TWithNoDefaultConstructor>().foo())
 */
template <typename T>
typename AddRvalueReference<T>::Type DeclVal();

/* 20.9.3 Helper classes [meta.help] */

/**
 * Helper class used as a base for various type traits, exposed publicly
 * because <type_traits> exposes it as well.
 */
template <typename T, T Value>
struct IntegralConstant {
  static constexpr T value = Value;
  typedef T ValueType;
  typedef IntegralConstant<T, Value> Type;
};

/** Convenient aliases. */
typedef IntegralConstant<bool, true> TrueType;
typedef IntegralConstant<bool, false> FalseType;

/* 20.9.4 Unary type traits [meta.unary] */

/* 20.9.4.1 Primary type categories [meta.unary.cat] */

namespace detail {

template <typename T>
struct IsVoidHelper : FalseType {};

template <>
struct IsVoidHelper<void> : TrueType {};

}  // namespace detail

/**
 * IsVoid determines whether a type is void.
 *
 * mozilla::IsVoid<int>::value is false;
 * mozilla::IsVoid<void>::value is true;
 * mozilla::IsVoid<void*>::value is false;
 * mozilla::IsVoid<volatile void>::value is true.
 */
template <typename T>
struct IsVoid : detail::IsVoidHelper<typename RemoveCV<T>::Type> {};

namespace detail {

template <typename T>
struct IsIntegralHelper : FalseType {};

template <>
struct IsIntegralHelper<char> : TrueType {};
template <>
struct IsIntegralHelper<signed char> : TrueType {};
template <>
struct IsIntegralHelper<unsigned char> : TrueType {};
template <>
struct IsIntegralHelper<short> : TrueType {};
template <>
struct IsIntegralHelper<unsigned short> : TrueType {};
template <>
struct IsIntegralHelper<int> : TrueType {};
template <>
struct IsIntegralHelper<unsigned int> : TrueType {};
template <>
struct IsIntegralHelper<long> : TrueType {};
template <>
struct IsIntegralHelper<unsigned long> : TrueType {};
template <>
struct IsIntegralHelper<long long> : TrueType {};
template <>
struct IsIntegralHelper<unsigned long long> : TrueType {};
template <>
struct IsIntegralHelper<bool> : TrueType {};
template <>
struct IsIntegralHelper<wchar_t> : TrueType {};
template <>
struct IsIntegralHelper<char16_t> : TrueType {};
template <>
struct IsIntegralHelper<char32_t> : TrueType {};

} /* namespace detail */

/**
 * IsIntegral determines whether a type is an integral type.
 *
 * mozilla::IsIntegral<int>::value is true;
 * mozilla::IsIntegral<unsigned short>::value is true;
 * mozilla::IsIntegral<const long>::value is true;
 * mozilla::IsIntegral<int*>::value is false;
 * mozilla::IsIntegral<double>::value is false;
 */
template <typename T>
struct IsIntegral : detail::IsIntegralHelper<typename RemoveCV<T>::Type> {};

template <typename T, typename U>
struct IsSame;

namespace detail {

template <typename T>
struct IsFloatingPointHelper
    : IntegralConstant<bool, IsSame<T, float>::value ||
                                 IsSame<T, double>::value ||
                                 IsSame<T, long double>::value> {};

}  // namespace detail

/**
 * IsFloatingPoint determines whether a type is a floating point type (float,
 * double, long double).
 *
 * mozilla::IsFloatingPoint<int>::value is false;
 * mozilla::IsFloatingPoint<const float>::value is true;
 * mozilla::IsFloatingPoint<long double>::value is true;
 * mozilla::IsFloatingPoint<double*>::value is false.
 */
template <typename T>
struct IsFloatingPoint
    : detail::IsFloatingPointHelper<typename RemoveCV<T>::Type> {};

namespace detail {

template <typename T>
struct IsArrayHelper : FalseType {};

template <typename T, decltype(sizeof(1)) N>
struct IsArrayHelper<T[N]> : TrueType {};

template <typename T>
struct IsArrayHelper<T[]> : TrueType {};

}  // namespace detail

/**
 * IsArray determines whether a type is an array type, of known or unknown
 * length.
 *
 * mozilla::IsArray<int>::value is false;
 * mozilla::IsArray<int[]>::value is true;
 * mozilla::IsArray<int[5]>::value is true.
 */
template <typename T>
struct IsArray : detail::IsArrayHelper<typename RemoveCV<T>::Type> {};

namespace detail {

template <typename T>
struct IsFunPtr;

template <typename>
struct IsFunPtr : public FalseType {};

template <typename Result, typename... ArgTypes>
struct IsFunPtr<Result (*)(ArgTypes...)> : public TrueType {};

};  // namespace detail

/**
 * IsFunction determines whether a type is a function type. Function pointers
 * don't qualify here--only the type of an actual function symbol. We do not
 * correctly handle varags function types because of a bug in MSVC.
 *
 * Given the function:
 *   void f(int) {}
 *
 * mozilla::IsFunction<void(int)> is true;
 * mozilla::IsFunction<void(*)(int)> is false;
 * mozilla::IsFunction<decltype(f)> is true.
 */
template <typename T>
struct IsFunction : public detail::IsFunPtr<typename RemoveCV<T>::Type*> {};

namespace detail {

template <typename T>
struct IsPointerHelper : FalseType {};

template <typename T>
struct IsPointerHelper<T*> : TrueType {};

}  // namespace detail

/**
 * IsPointer determines whether a type is a possibly-CV-qualified pointer type
 * (but not a pointer-to-member type).
 *
 * mozilla::IsPointer<struct S*>::value is true;
 * mozilla::IsPointer<int*>::value is true;
 * mozilla::IsPointer<int**>::value is true;
 * mozilla::IsPointer<const int*>::value is true;
 * mozilla::IsPointer<int* const>::value is true;
 * mozilla::IsPointer<int* volatile>::value is true;
 * mozilla::IsPointer<void (*)(void)>::value is true;
 * mozilla::IsPointer<int>::value is false;
 * mozilla::IsPointer<struct S>::value is false.
 * mozilla::IsPointer<int(struct S::*)>::value is false
 */
template <typename T>
struct IsPointer : detail::IsPointerHelper<typename RemoveCV<T>::Type> {};

namespace detail {

// __is_enum is a supported extension across all of our supported compilers.
template <typename T>
struct IsEnumHelper : IntegralConstant<bool, __is_enum(T)> {};

}  // namespace detail

/**
 * IsEnum determines whether a type is an enum type.
 *
 * mozilla::IsEnum<enum S>::value is true;
 * mozilla::IsEnum<enum S*>::value is false;
 * mozilla::IsEnum<int>::value is false;
 */
template <typename T>
struct IsEnum : detail::IsEnumHelper<typename RemoveCV<T>::Type> {};

namespace detail {

// __is_class is a supported extension across all of our supported compilers:
// http://llvm.org/releases/3.0/docs/ClangReleaseNotes.html
// http://gcc.gnu.org/onlinedocs/gcc-4.4.7/gcc/Type-Traits.html#Type-Traits
// http://msdn.microsoft.com/en-us/library/ms177194%28v=vs.100%29.aspx
template <typename T>
struct IsClassHelper : IntegralConstant<bool, __is_class(T)> {};

}  // namespace detail

/**
 * IsClass determines whether a type is a class type (but not a union).
 *
 * struct S {};
 * union U {};
 * mozilla::IsClass<int>::value is false;
 * mozilla::IsClass<const S>::value is true;
 * mozilla::IsClass<U>::value is false;
 */
template <typename T>
struct IsClass : detail::IsClassHelper<typename RemoveCV<T>::Type> {};

/* 20.9.4.2 Composite type traits [meta.unary.comp] */

/**
 * IsArithmetic determines whether a type is arithmetic.  A type is arithmetic
 * iff it is an integral type or a floating point type.
 *
 * mozilla::IsArithmetic<int>::value is true;
 * mozilla::IsArithmetic<double>::value is true;
 * mozilla::IsArithmetic<long double*>::value is false.
 */
template <typename T>
struct IsArithmetic : IntegralConstant<bool, IsIntegral<T>::value ||
                                                 IsFloatingPoint<T>::value> {};

namespace detail {

template <typename T>
struct IsMemberPointerHelper : FalseType {};

template <typename T, typename U>
struct IsMemberPointerHelper<T U::*> : TrueType {};

}  // namespace detail

/**
 * IsMemberPointer determines whether a type is pointer to non-static member
 * object or a pointer to non-static member function.
 *
 * mozilla::IsMemberPointer<int(cls::*)>::value is true
 * mozilla::IsMemberPointer<int*>::value is false
 */
template <typename T>
struct IsMemberPointer
    : detail::IsMemberPointerHelper<typename RemoveCV<T>::Type> {};

/**
 * IsScalar determines whether a type is a scalar type.
 *
 * mozilla::IsScalar<int>::value is true
 * mozilla::IsScalar<int*>::value is true
 * mozilla::IsScalar<cls>::value is false
 */
template <typename T>
struct IsScalar
    : IntegralConstant<bool, IsArithmetic<T>::value || IsEnum<T>::value ||
                                 IsPointer<T>::value ||
                                 IsMemberPointer<T>::value> {};

/* 20.9.4.3 Type properties [meta.unary.prop] */

/**
 * IsConst determines whether a type is const or not.
 *
 * mozilla::IsConst<int>::value is false;
 * mozilla::IsConst<void* const>::value is true;
 * mozilla::IsConst<const char*>::value is false.
 */
template <typename T>
struct IsConst : FalseType {};

template <typename T>
struct IsConst<const T> : TrueType {};

/**
 * IsVolatile determines whether a type is volatile or not.
 *
 * mozilla::IsVolatile<int>::value is false;
 * mozilla::IsVolatile<void* volatile>::value is true;
 * mozilla::IsVolatile<volatile char*>::value is false.
 */
template <typename T>
struct IsVolatile : FalseType {};

template <typename T>
struct IsVolatile<volatile T> : TrueType {};

/**
 * Traits class for identifying POD types.  Until C++11 there's no automatic
 * way to detect PODs, so for the moment this is done manually.  Users may
 * define specializations of this class that inherit from mozilla::TrueType and
 * mozilla::FalseType (or equivalently mozilla::IntegralConstant<bool, true or
 * false>, or conveniently from mozilla::IsPod for composite types) as needed to
 * ensure correct IsPod behavior.
 */
template <typename T>
struct IsPod : public FalseType {};

template <>
struct IsPod<char> : TrueType {};
template <>
struct IsPod<signed char> : TrueType {};
template <>
struct IsPod<unsigned char> : TrueType {};
template <>
struct IsPod<short> : TrueType {};
template <>
struct IsPod<unsigned short> : TrueType {};
template <>
struct IsPod<int> : TrueType {};
template <>
struct IsPod<unsigned int> : TrueType {};
template <>
struct IsPod<long> : TrueType {};
template <>
struct IsPod<unsigned long> : TrueType {};
template <>
struct IsPod<long long> : TrueType {};
template <>
struct IsPod<unsigned long long> : TrueType {};
template <>
struct IsPod<bool> : TrueType {};
template <>
struct IsPod<float> : TrueType {};
template <>
struct IsPod<double> : TrueType {};
template <>
struct IsPod<wchar_t> : TrueType {};
template <>
struct IsPod<char16_t> : TrueType {};
template <typename T>
struct IsPod<T*> : TrueType {};

namespace detail {

template <typename T, bool = IsFloatingPoint<T>::value,
          bool = IsIntegral<T>::value,
          typename NoCV = typename RemoveCV<T>::Type>
struct IsSignedHelper;

// Floating point is signed.
template <typename T, typename NoCV>
struct IsSignedHelper<T, true, false, NoCV> : TrueType {};

// Integral is conditionally signed.
template <typename T, typename NoCV>
struct IsSignedHelper<T, false, true, NoCV>
    : IntegralConstant<bool, bool(NoCV(-1) < NoCV(1))> {};

// Non-floating point, non-integral is not signed.
template <typename T, typename NoCV>
struct IsSignedHelper<T, false, false, NoCV> : FalseType {};

}  // namespace detail

/**
 * IsSigned determines whether a type is a signed arithmetic type.  |char| is
 * considered a signed type if it has the same representation as |signed char|.
 *
 * mozilla::IsSigned<int>::value is true;
 * mozilla::IsSigned<const unsigned int>::value is false;
 * mozilla::IsSigned<unsigned char>::value is false;
 * mozilla::IsSigned<float>::value is true.
 */
template <typename T>
struct IsSigned : detail::IsSignedHelper<T> {};

namespace detail {

template <typename T, bool = IsFloatingPoint<T>::value,
          bool = IsIntegral<T>::value,
          typename NoCV = typename RemoveCV<T>::Type>
struct IsUnsignedHelper;

// Floating point is not unsigned.
template <typename T, typename NoCV>
struct IsUnsignedHelper<T, true, false, NoCV> : FalseType {};

// Integral is conditionally unsigned.
template <typename T, typename NoCV>
struct IsUnsignedHelper<T, false, true, NoCV>
    : IntegralConstant<bool, (IsSame<NoCV, bool>::value ||
                              bool(NoCV(1) < NoCV(-1)))> {};

// Non-floating point, non-integral is not unsigned.
template <typename T, typename NoCV>
struct IsUnsignedHelper<T, false, false, NoCV> : FalseType {};

}  // namespace detail

/**
 * IsUnsigned determines whether a type is an unsigned arithmetic type.
 *
 * mozilla::IsUnsigned<int>::value is false;
 * mozilla::IsUnsigned<const unsigned int>::value is true;
 * mozilla::IsUnsigned<unsigned char>::value is true;
 * mozilla::IsUnsigned<float>::value is false.
 */
template <typename T>
struct IsUnsigned : detail::IsUnsignedHelper<T> {};

namespace detail {

struct DoIsDestructibleImpl {
  template <typename T, typename = decltype(DeclVal<T&>().~T())>
  static TrueType test(int);
  template <typename T>
  static FalseType test(...);
};

template <typename T>
struct IsDestructibleImpl : public DoIsDestructibleImpl {
  typedef decltype(test<T>(0)) Type;
};

}  // namespace detail

/**
 * IsDestructible determines whether a type has a public destructor.
 *
 * struct S0 {};                    // Implicit default destructor.
 * struct S1 { ~S1(); };
 * class C2 { ~C2(); };             // private destructor.
 *
 * mozilla::IsDestructible<S0>::value is true;
 * mozilla::IsDestructible<S1>::value is true;
 * mozilla::IsDestructible<C2>::value is false.
 */
template <typename T>
struct IsDestructible : public detail::IsDestructibleImpl<T>::Type {};

/* 20.9.5 Type property queries [meta.unary.prop.query] */

/* 20.9.6 Relationships between types [meta.rel] */

/**
 * IsSame tests whether two types are the same type.
 *
 * mozilla::IsSame<int, int>::value is true;
 * mozilla::IsSame<int*, int*>::value is true;
 * mozilla::IsSame<int, unsigned int>::value is false;
 * mozilla::IsSame<void, void>::value is true;
 * mozilla::IsSame<const int, int>::value is false;
 * mozilla::IsSame<struct S, struct S>::value is true.
 */
template <typename T, typename U>
struct IsSame : FalseType {};

template <typename T>
struct IsSame<T, T> : TrueType {};

namespace detail {

template <typename From, typename To>
struct ConvertibleTester {
 private:
  template <typename To1>
  static char test_helper(To1);

  template <typename From1, typename To1>
  static decltype(test_helper<To1>(DeclVal<From1>())) test(int);

  template <typename From1, typename To1>
  static int test(...);

 public:
  static const bool value = sizeof(test<From, To>(0)) == sizeof(char);
};

}  // namespace detail

/**
 * IsConvertible determines whether a value of type From will implicitly convert
 * to a value of type To.  For example:
 *
 *   struct A {};
 *   struct B : public A {};
 *   struct C {};
 *
 * mozilla::IsConvertible<A, A>::value is true;
 * mozilla::IsConvertible<A*, A*>::value is true;
 * mozilla::IsConvertible<B, A>::value is true;
 * mozilla::IsConvertible<B*, A*>::value is true;
 * mozilla::IsConvertible<C, A>::value is false;
 * mozilla::IsConvertible<A, C>::value is false;
 * mozilla::IsConvertible<A*, C*>::value is false;
 * mozilla::IsConvertible<C*, A*>::value is false.
 *
 * For obscure reasons, you can't use IsConvertible when the types being tested
 * are related through private inheritance, and you'll get a compile error if
 * you try.  Just don't do it!
 *
 * Note - we need special handling for void, which ConvertibleTester doesn't
 * handle. The void handling here doesn't handle const/volatile void correctly,
 * which could be easily fixed if the need arises.
 */
template <typename From, typename To>
struct IsConvertible
    : IntegralConstant<bool, detail::ConvertibleTester<From, To>::value> {};

template <typename B>
struct IsConvertible<void, B> : IntegralConstant<bool, IsVoid<B>::value> {};

template <typename A>
struct IsConvertible<A, void> : IntegralConstant<bool, IsVoid<A>::value> {};

template <>
struct IsConvertible<void, void> : TrueType {};

/* 20.9.7 Transformations between types [meta.trans] */

/* 20.9.7.1 Const-volatile modifications [meta.trans.cv] */

/**
 * RemoveConst removes top-level const qualifications on a type.
 *
 * mozilla::RemoveConst<int>::Type is int;
 * mozilla::RemoveConst<const int>::Type is int;
 * mozilla::RemoveConst<const int*>::Type is const int*;
 * mozilla::RemoveConst<int* const>::Type is int*.
 */
template <typename T>
struct RemoveConst {
  typedef T Type;
};

template <typename T>
struct RemoveConst<const T> {
  typedef T Type;
};

/**
 * RemoveVolatile removes top-level volatile qualifications on a type.
 *
 * mozilla::RemoveVolatile<int>::Type is int;
 * mozilla::RemoveVolatile<volatile int>::Type is int;
 * mozilla::RemoveVolatile<volatile int*>::Type is volatile int*;
 * mozilla::RemoveVolatile<int* volatile>::Type is int*.
 */
template <typename T>
struct RemoveVolatile {
  typedef T Type;
};

template <typename T>
struct RemoveVolatile<volatile T> {
  typedef T Type;
};

/**
 * RemoveCV removes top-level const and volatile qualifications on a type.
 *
 * mozilla::RemoveCV<int>::Type is int;
 * mozilla::RemoveCV<const int>::Type is int;
 * mozilla::RemoveCV<volatile int>::Type is int;
 * mozilla::RemoveCV<int* const volatile>::Type is int*.
 */
template <typename T>
struct RemoveCV {
  typedef typename RemoveConst<typename RemoveVolatile<T>::Type>::Type Type;
};

/* 20.9.7.2 Reference modifications [meta.trans.ref] */

/**
 * Converts reference types to the underlying types.
 *
 * mozilla::RemoveReference<T>::Type is T;
 * mozilla::RemoveReference<T&>::Type is T;
 * mozilla::RemoveReference<T&&>::Type is T;
 */

template <typename T>
struct RemoveReference {
  typedef T Type;
};

template <typename T>
struct RemoveReference<T&> {
  typedef T Type;
};

template <typename T>
struct RemoveReference<T&&> {
  typedef T Type;
};

namespace detail {

enum Voidness { TIsVoid, TIsNotVoid };

template <typename T, Voidness V = IsVoid<T>::value ? TIsVoid : TIsNotVoid>
struct AddRvalueReferenceHelper;

template <typename T>
struct AddRvalueReferenceHelper<T, TIsVoid> {
  typedef void Type;
};

template <typename T>
struct AddRvalueReferenceHelper<T, TIsNotVoid> {
  typedef T&& Type;
};

}  // namespace detail

/**
 * AddRvalueReference adds an rvalue && reference to T if one isn't already
 * present. (Note: adding an rvalue reference to an lvalue & reference in
 * essence keeps the &, per C+11 reference collapsing rules. For example,
 * int& would remain int&.)
 *
 * The final computed type will only *not* be a reference if T is void.
 *
 * mozilla::AddRvalueReference<int>::Type is int&&;
 * mozilla::AddRvalueRference<volatile int&>::Type is volatile int&;
 * mozilla::AddRvalueRference<const int&&>::Type is const int&&;
 * mozilla::AddRvalueReference<void*>::Type is void*&&;
 * mozilla::AddRvalueReference<void>::Type is void;
 * mozilla::AddRvalueReference<struct S&>::Type is struct S&.
 */
template <typename T>
struct AddRvalueReference : detail::AddRvalueReferenceHelper<T> {};

/* 20.9.7.4 Array modifications [meta.trans.arr] */

/**
 * RemoveExtent produces either the type of the elements of the array T, or T
 * itself.
 *
 * mozilla::RemoveExtent<int>::Type is int;
 * mozilla::RemoveExtent<const int[]>::Type is const int;
 * mozilla::RemoveExtent<volatile int[5]>::Type is volatile int;
 * mozilla::RemoveExtent<long[][17]>::Type is long[17].
 */
template <typename T>
struct RemoveExtent {
  typedef T Type;
};

template <typename T>
struct RemoveExtent<T[]> {
  typedef T Type;
};

template <typename T, decltype(sizeof(1)) N>
struct RemoveExtent<T[N]> {
  typedef T Type;
};

/* 20.9.7.5 Pointer modifications [meta.trans.ptr] */

namespace detail {

template <typename T, typename CVRemoved>
struct RemovePointerHelper {
  typedef T Type;
};

template <typename T, typename Pointee>
struct RemovePointerHelper<T, Pointee*> {
  typedef Pointee Type;
};

}  // namespace detail

/**
 * Produces the pointed-to type if a pointer is provided, else returns the input
 * type.  Note that this does not dereference pointer-to-member pointers.
 *
 * struct S { bool m; void f(); };
 * mozilla::RemovePointer<int>::Type is int;
 * mozilla::RemovePointer<int*>::Type is int;
 * mozilla::RemovePointer<int* const>::Type is int;
 * mozilla::RemovePointer<int* volatile>::Type is int;
 * mozilla::RemovePointer<const long*>::Type is const long;
 * mozilla::RemovePointer<void* const>::Type is void;
 * mozilla::RemovePointer<void (S::*)()>::Type is void (S::*)();
 * mozilla::RemovePointer<void (*)()>::Type is void();
 * mozilla::RemovePointer<bool S::*>::Type is bool S::*.
 */
template <typename T>
struct RemovePointer
    : detail::RemovePointerHelper<T, typename RemoveCV<T>::Type> {};

/**
 * Converts T& to T*. Otherwise returns T* given T. Note that C++17 wants
 * std::add_pointer to work differently for function types. We don't implement
 * that behavior here.
 *
 * mozilla::AddPointer<int> is int*;
 * mozilla::AddPointer<int*> is int**;
 * mozilla::AddPointer<int&> is int*;
 * mozilla::AddPointer<int* const> is int** const.
 */
template <typename T>
struct AddPointer {
  typedef typename RemoveReference<T>::Type* Type;
};

/* 20.9.7.6 Other transformations [meta.trans.other] */

/**
 * EnableIf is a struct containing a typedef of T if and only if B is true.
 *
 * mozilla::EnableIf<true, int>::Type is int;
 * mozilla::EnableIf<false, int>::Type is a compile-time error.
 *
 * Use this template to implement SFINAE-style (Substitution Failure Is not An
 * Error) requirements.  For example, you might use it to impose a restriction
 * on a template parameter:
 *
 *   template<typename T>
 *   class PodVector // vector optimized to store POD (memcpy-able) types
 *   {
 *      EnableIf<IsPod<T>::value, T>::Type* vector;
 *      size_t length;
 *      ...
 *   };
 */
template <bool B, typename T = void>
struct EnableIf {};

template <typename T>
struct EnableIf<true, T> {
  typedef T Type;
};

/**
 * Conditional selects a class between two, depending on a given boolean value.
 *
 * mozilla::Conditional<true, A, B>::Type is A;
 * mozilla::Conditional<false, A, B>::Type is B;
 */
template <bool Condition, typename A, typename B>
struct Conditional {
  typedef A Type;
};

template <class A, class B>
struct Conditional<false, A, B> {
  typedef B Type;
};

namespace detail {

template <typename U, bool IsArray = IsArray<U>::value,
          bool IsFunction = IsFunction<U>::value>
struct DecaySelector;

template <typename U>
struct DecaySelector<U, false, false> {
  typedef typename RemoveCV<U>::Type Type;
};

template <typename U>
struct DecaySelector<U, true, false> {
  typedef typename RemoveExtent<U>::Type* Type;
};

template <typename U>
struct DecaySelector<U, false, true> {
  typedef typename AddPointer<U>::Type Type;
};

};  // namespace detail

/**
 * Strips const/volatile off a type and decays it from an lvalue to an
 * rvalue. So function types are converted to function pointers, arrays to
 * pointers, and references are removed.
 *
 * mozilla::Decay<int>::Type is int
 * mozilla::Decay<int&>::Type is int
 * mozilla::Decay<int&&>::Type is int
 * mozilla::Decay<const int&>::Type is int
 * mozilla::Decay<int[2]>::Type is int*
 * mozilla::Decay<int(int)>::Type is int(*)(int)
 */
template <typename T>
class Decay : public detail::DecaySelector<typename RemoveReference<T>::Type> {
};

} /* namespace mozilla */

#endif /* mozilla_TypeTraits_h */
