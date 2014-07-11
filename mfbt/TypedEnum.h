/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Macros to emulate C++11 typed enums and enum classes. */

#ifndef mozilla_TypedEnum_h
#define mozilla_TypedEnum_h

#include "mozilla/TypedEnumInternal.h"
#include "mozilla/MacroArgs.h"

#if defined(__cplusplus)

/**
 * MOZ_ENUM_TYPE specifies the underlying numeric type for an enum.  It's
 * specified by placing MOZ_ENUM_TYPE(type) immediately after the enum name in
 * its declaration, and before the opening curly brace, like
 *
 *   enum MyEnum MOZ_ENUM_TYPE(uint16_t)
 *   {
 *     A,
 *     B = 7,
 *     C
 *   };
 *
 * In supporting compilers, the macro will expand to ": uint16_t".  The
 * compiler will allocate exactly two bytes for MyEnum and will require all
 * enumerators to have values between 0 and 65535.  (Thus specifying "B =
 * 100000" instead of "B = 7" would fail to compile.)  In old compilers the
 * macro expands to the empty string, and the underlying type is generally
 * undefined.
 */
#ifdef MOZ_HAVE_CXX11_ENUM_TYPE
#  define MOZ_ENUM_TYPE(type)   : type
#else
#  define MOZ_ENUM_TYPE(type)   /* no support */
#endif

/**
 * MOZ_BEGIN_ENUM_CLASS and MOZ_END_ENUM_CLASS provide access to the
 * strongly-typed enumeration feature of C++11 ("enum class").  If supported
 * by the compiler, an enum defined using these macros will not be implicitly
 * converted to any other type, and its enumerators will be scoped using the
 * enumeration name.  Place MOZ_BEGIN_ENUM_CLASS(EnumName [, type]) in place of
 * "enum EnumName {", and MOZ_END_ENUM_CLASS(EnumName) in place of the closing
 * "};".  For example,
 *
 *   MOZ_BEGIN_ENUM_CLASS(Enum, int32_t)
 *     A,
 *     B = 6
 *   MOZ_END_ENUM_CLASS(Enum)
 *
 * This will make "Enum::A" and "Enum::B" appear in the global scope, but "A"
 * and "B" will not.  In compilers that support C++11 strongly-typed
 * enumerations, implicit conversions of Enum values to numeric types will
 * fail.  In other compilers, Enum itself will actually be defined as a class,
 * and some implicit conversions will fail while others will succeed.
 *
 * The optional type argument specifies the underlying type for the enum where
 * supported, as with MOZ_ENUM_TYPE().  As with MOZ_ENUM_TYPE(), it will do
 * nothing on compilers that do not support it.
 *
 * MOZ_{BEGIN,END}_ENUM_CLASS doesn't work for defining enum classes nested
 * inside classes.  To define an enum class nested inside another class, use
 * MOZ_{BEGIN,END}_NESTED_ENUM_CLASS, and place a MOZ_FINISH_NESTED_ENUM_CLASS
 * in namespace scope to handle bits that can only be implemented with
 * namespace-scoped code.  For example:
 *
 *   class FooBar
 *   {
 *     MOZ_BEGIN_NESTED_ENUM_CLASS(Enum, int32_t)
 *       A,
 *       B = 6
 *     MOZ_END_NESTED_ENUM_CLASS(Enum)
 *   };
 *
 *   MOZ_FINISH_NESTED_ENUM_CLASS(FooBar::Enum)
 */
#if defined(MOZ_HAVE_CXX11_STRONG_ENUMS)
  /*
   * All compilers that support strong enums also support an explicit
   * underlying type, so no extra check is needed.
   */

   /* Single-argument form. */
#  define MOZ_BEGIN_NESTED_ENUM_CLASS_HELPER1(Name) \
     enum class Name {
   /* Two-argument form. */
#  define MOZ_BEGIN_NESTED_ENUM_CLASS_HELPER2(Name, type) \
     enum class Name : type {
#  define MOZ_END_NESTED_ENUM_CLASS(Name) \
     };
#  define MOZ_FINISH_NESTED_ENUM_CLASS(Name) /* nothing */

  /*
   * MOZ_ENUM_CLASS_ENUM_TYPE allows using enum classes
   * as template parameter types. For that, we need integer types.
   * In the present case where the compiler supports strong enums,
   * these are already integer types so there is nothing more to do.
   */
#  define MOZ_ENUM_CLASS_ENUM_TYPE(Name) Name
  /*
   * See the comment below about MOZ_TEMPLATE_ENUM_CLASS_ENUM_TYPE.
   */
#  define MOZ_TEMPLATE_ENUM_CLASS_ENUM_TYPE(Name) Name
#else
   /**
    * We need Name to both name a type, and scope the provided enumerator
    * names.  Namespaces and classes both provide scoping, but namespaces
    * aren't types, so we need to use a class that wraps the enum values.  We
    * have an implicit conversion from the inner enum type to the class, so
    * statements like
    *
    *   Enum x = Enum::A;
    *
    * will still work.  We need to define an implicit conversion from the class
    * to the inner enum as well, so that (for instance) switch statements will
    * work.  This means that the class can be implicitly converted to a numeric
    * value as well via the enum type, since C++ allows an implicit
    * user-defined conversion followed by a standard conversion to still be
    * implicit.
    *
    * We have an explicit constructor from int defined, so that casts like
    * (Enum)7 will still work.  We also have a zero-argument constructor with
    * no arguments, so declaration without initialization (like "Enum foo;")
    * will work.
    *
    * Additionally, we'll delete as many operators as possible for the inner
    * enum type, so statements like this will still fail:
    *
    *   f(5 + Enum::B); // deleted operator+
    *
    * But we can't prevent things like this, because C++ doesn't allow
    * overriding conversions or assignment operators for enums:
    *
    *   int x = Enum::A;
    *   int f()
    *   {
    *     return Enum::A;
    *   }
    */

   /* Single-argument form. */
#  define MOZ_BEGIN_NESTED_ENUM_CLASS_HELPER1(Name) \
     class Name \
     { \
     public: \
       enum Enum \
       {
   /* Two-argument form. */
#  define MOZ_BEGIN_NESTED_ENUM_CLASS_HELPER2(Name, type) \
     class Name \
     { \
     public: \
       enum Enum MOZ_ENUM_TYPE(type) \
       {
#  define MOZ_END_NESTED_ENUM_CLASS(Name) \
       }; \
       Name() {} \
       MOZ_CONSTEXPR Name(Enum aEnum) : mEnum(aEnum) {} \
       template<typename Other> \
       explicit MOZ_CONSTEXPR Name(Other num) : mEnum((Enum)num) {} \
       MOZ_CONSTEXPR operator Enum() const { return mEnum; } \
       explicit MOZ_CONSTEXPR Name(const mozilla::CastableTypedEnumResult<Name>& aOther) \
         : mEnum(aOther.get()) \
       {} \
     private: \
       Enum mEnum; \
     };
#  define MOZ_FINISH_NESTED_ENUM_CLASS(Name) \
     inline int operator+(const int&, const Name::Enum&) MOZ_DELETE; \
     inline int operator+(const Name::Enum&, const int&) MOZ_DELETE; \
     inline int operator-(const int&, const Name::Enum&) MOZ_DELETE; \
     inline int operator-(const Name::Enum&, const int&) MOZ_DELETE; \
     inline int operator*(const int&, const Name::Enum&) MOZ_DELETE; \
     inline int operator*(const Name::Enum&, const int&) MOZ_DELETE; \
     inline int operator/(const int&, const Name::Enum&) MOZ_DELETE; \
     inline int operator/(const Name::Enum&, const int&) MOZ_DELETE; \
     inline int operator%(const int&, const Name::Enum&) MOZ_DELETE; \
     inline int operator%(const Name::Enum&, const int&) MOZ_DELETE; \
     inline int operator+(const Name::Enum&) MOZ_DELETE; \
     inline int operator-(const Name::Enum&) MOZ_DELETE; \
     inline int& operator++(Name::Enum&) MOZ_DELETE; \
     inline int operator++(Name::Enum&, int) MOZ_DELETE; \
     inline int& operator--(Name::Enum&) MOZ_DELETE; \
     inline int operator--(Name::Enum&, int) MOZ_DELETE; \
     inline bool operator==(const int&, const Name::Enum&) MOZ_DELETE; \
     inline bool operator==(const Name::Enum&, const int&) MOZ_DELETE; \
     inline bool operator!=(const int&, const Name::Enum&) MOZ_DELETE; \
     inline bool operator!=(const Name::Enum&, const int&) MOZ_DELETE; \
     inline bool operator>(const int&, const Name::Enum&) MOZ_DELETE; \
     inline bool operator>(const Name::Enum&, const int&) MOZ_DELETE; \
     inline bool operator<(const int&, const Name::Enum&) MOZ_DELETE; \
     inline bool operator<(const Name::Enum&, const int&) MOZ_DELETE; \
     inline bool operator>=(const int&, const Name::Enum&) MOZ_DELETE; \
     inline bool operator>=(const Name::Enum&, const int&) MOZ_DELETE; \
     inline bool operator<=(const int&, const Name::Enum&) MOZ_DELETE; \
     inline bool operator<=(const Name::Enum&, const int&) MOZ_DELETE; \
     inline bool operator!(const Name::Enum&) MOZ_DELETE; \
     inline bool operator&&(const bool&, const Name::Enum&) MOZ_DELETE; \
     inline bool operator&&(const Name::Enum&, const bool&) MOZ_DELETE; \
     inline bool operator||(const bool&, const Name::Enum&) MOZ_DELETE; \
     inline bool operator||(const Name::Enum&, const bool&) MOZ_DELETE; \
     inline int operator&(const int&, const Name::Enum&) MOZ_DELETE; \
     inline int operator&(const Name::Enum&, const int&) MOZ_DELETE; \
     inline int operator|(const int&, const Name::Enum&) MOZ_DELETE; \
     inline int operator|(const Name::Enum&, const int&) MOZ_DELETE; \
     inline int operator^(const int&, const Name::Enum&) MOZ_DELETE; \
     inline int operator^(const Name::Enum&, const int&) MOZ_DELETE; \
     inline int operator<<(const int&, const Name::Enum&) MOZ_DELETE; \
     inline int operator<<(const Name::Enum&, const int&) MOZ_DELETE; \
     inline int operator>>(const int&, const Name::Enum&) MOZ_DELETE; \
     inline int operator>>(const Name::Enum&, const int&) MOZ_DELETE; \
     inline int& operator+=(int&, const Name::Enum&) MOZ_DELETE; \
     inline int& operator-=(int&, const Name::Enum&) MOZ_DELETE; \
     inline int& operator*=(int&, const Name::Enum&) MOZ_DELETE; \
     inline int& operator/=(int&, const Name::Enum&) MOZ_DELETE; \
     inline int& operator%=(int&, const Name::Enum&) MOZ_DELETE; \
     inline int& operator&=(int&, const Name::Enum&) MOZ_DELETE; \
     inline int& operator|=(int&, const Name::Enum&) MOZ_DELETE; \
     inline int& operator^=(int&, const Name::Enum&) MOZ_DELETE; \
     inline int& operator<<=(int&, const Name::Enum&) MOZ_DELETE; \
     inline int& operator>>=(int&, const Name::Enum&) MOZ_DELETE;

  /*
   * MOZ_ENUM_CLASS_ENUM_TYPE allows using enum classes
   * as template parameter types. For that, we need integer types.
   * In the present case, the integer type is the Enum nested type.
   */
#  define MOZ_ENUM_CLASS_ENUM_TYPE(Name) Name::Enum
  /*
   * MOZ_TEMPLATE_ENUM_CLASS_ENUM_TYPE is a variant of MOZ_ENUM_CLASS_ENUM_TYPE
   * to be used when the enum class at hand depends on template parameters.
   *
   * Indeed, if T depends on template parameters, in order to name a nested type
   * in T, C++ does not allow to just write "T::NestedType". Instead, we have
   * to write "typename T::NestedType". The role of this macro is to add
   * this "typename" keywords where needed.
   *
   * Example:
   *
   *    template<typename T, MOZ_TEMPLATE_ENUM_CLASS_ENUM_TYPE(T) Value>
   *    struct S {};
   *
   *    MOZ_BEGIN_ENUM_CLASS(E)
   *      Foo,
   *      Bar
   *    MOZ_END_ENUM_CLASS(E)
   *
   *    S<E, E::Bar> s;
   *
   * In this example, the second template parameter to S is meant to be of type
   * T, but on non-C++11 compilers, type T is a class type, not an integer
   * type, so it is not accepted as the type of a constant template parameter.
   * One would then want to use MOZ_ENUM_CLASS_ENUM_TYPE(T), but that doesn't
   * work either as T depends on template parameters (more specifically here, T
   * _is_ a template parameter) so as MOZ_ENUM_CLASS_ENUM_TYPE(T) expands to
   * T::Enum, we are missing the required "typename" keyword. So here,
   * MOZ_TEMPLATE_ENUM_CLASS_ENUM_TYPE is needed.
   */
#  define MOZ_TEMPLATE_ENUM_CLASS_ENUM_TYPE(Name) typename Name::Enum
#endif

#  define MOZ_BEGIN_NESTED_ENUM_CLASS_GLUE(a, b) a b
#  define MOZ_BEGIN_NESTED_ENUM_CLASS(...) \
     MOZ_BEGIN_NESTED_ENUM_CLASS_GLUE( \
       MOZ_PASTE_PREFIX_AND_ARG_COUNT(MOZ_BEGIN_NESTED_ENUM_CLASS_HELPER, \
                                      __VA_ARGS__), \
       (__VA_ARGS__))

#  define MOZ_BEGIN_ENUM_CLASS(...) MOZ_BEGIN_NESTED_ENUM_CLASS(__VA_ARGS__)
#  define MOZ_END_ENUM_CLASS(Name) \
     MOZ_END_NESTED_ENUM_CLASS(Name) \
     MOZ_FINISH_NESTED_ENUM_CLASS(Name)

#endif /* __cplusplus */

#endif /* mozilla_TypedEnum_h */
