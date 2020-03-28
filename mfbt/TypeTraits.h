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

/* 20.9.4.3 Type properties [meta.unary.prop] */

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

} /* namespace mozilla */

#endif /* mozilla_TypeTraits_h */
