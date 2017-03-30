// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEMPLATE_UTIL_H_
#define BASE_TEMPLATE_UTIL_H_

#include <stddef.h>
#include <iosfwd>
#include <type_traits>
#include <utility>

#include "build/build_config.h"

// This hacks around libstdc++ 4.6 missing stuff in type_traits, while we need
// to support it.
#define CR_GLIBCXX_4_7_0 20120322
#define CR_GLIBCXX_4_5_4 20120702
#define CR_GLIBCXX_4_6_4 20121127
#if defined(__GLIBCXX__) &&                                               \
    (__GLIBCXX__ < CR_GLIBCXX_4_7_0 || __GLIBCXX__ == CR_GLIBCXX_4_5_4 || \
     __GLIBCXX__ == CR_GLIBCXX_4_6_4)
#define CR_USE_FALLBACKS_FOR_OLD_GLIBCXX
#endif

namespace base {

template <class T> struct is_non_const_reference : std::false_type {};
template <class T> struct is_non_const_reference<T&> : std::true_type {};
template <class T> struct is_non_const_reference<const T&> : std::false_type {};

// is_assignable

namespace internal {

template <typename First, typename Second>
struct SelectSecond {
  using type = Second;
};

struct Any {
  Any(...);
};

// True case: If |Lvalue| can be assigned to from |Rvalue|, then the return
// value is a true_type.
template <class Lvalue, class Rvalue>
typename internal::SelectSecond<
    decltype((std::declval<Lvalue>() = std::declval<Rvalue>())),
    std::true_type>::type
IsAssignableTest(Lvalue&&, Rvalue&&);

// False case: Otherwise the return value is a false_type.
template <class Rvalue>
std::false_type IsAssignableTest(internal::Any, Rvalue&&);

// Default case: Neither Lvalue nor Rvalue is void. Uses IsAssignableTest to
// determine the type of IsAssignableImpl.
template <class Lvalue,
          class Rvalue,
          bool = std::is_void<Lvalue>::value || std::is_void<Rvalue>::value>
struct IsAssignableImpl
    : public std::common_type<decltype(
          internal::IsAssignableTest(std::declval<Lvalue>(),
                                     std::declval<Rvalue>()))>::type {};

// Void case: Either Lvalue or Rvalue is void. Then the type of IsAssignableTest
// is false_type.
template <class Lvalue, class Rvalue>
struct IsAssignableImpl<Lvalue, Rvalue, true> : public std::false_type {};

// Uses expression SFINAE to detect whether using operator<< would work.
template <typename T, typename = void>
struct SupportsOstreamOperator : std::false_type {};
template <typename T>
struct SupportsOstreamOperator<T,
                               decltype(void(std::declval<std::ostream&>()
                                             << std::declval<T>()))>
    : std::true_type {};

}  // namespace internal

// TODO(crbug.com/554293): Remove this when all platforms have this in the std
// namespace.
template <class Lvalue, class Rvalue>
struct is_assignable : public internal::IsAssignableImpl<Lvalue, Rvalue> {};

// is_copy_assignable is true if a T const& is assignable to a T&.
// TODO(crbug.com/554293): Remove this when all platforms have this in the std
// namespace.
template <class T>
struct is_copy_assignable
    : public is_assignable<typename std::add_lvalue_reference<T>::type,
                           typename std::add_lvalue_reference<
                               typename std::add_const<T>::type>::type> {};

// is_move_assignable is true if a T&& is assignable to a T&.
// TODO(crbug.com/554293): Remove this when all platforms have this in the std
// namespace.
template <class T>
struct is_move_assignable
    : public is_assignable<typename std::add_lvalue_reference<T>::type,
                           const typename std::add_rvalue_reference<T>::type> {
};

// underlying_type produces the integer type backing an enum type.
// TODO(crbug.com/554293): Remove this when all platforms have this in the std
// namespace.
#if defined(CR_USE_FALLBACKS_FOR_OLD_GLIBCXX)
template <typename T>
struct underlying_type {
  using type = __underlying_type(T);
};
#else
template <typename T>
using underlying_type = std::underlying_type<T>;
#endif

// TODO(crbug.com/554293): Remove this when all platforms have this in the std
// namespace.
#if defined(CR_USE_FALLBACKS_FOR_OLD_GLIBCXX)
template <class T>
using is_trivially_destructible = std::has_trivial_destructor<T>;
#else
template <class T>
using is_trivially_destructible = std::is_trivially_destructible<T>;
#endif

}  // namespace base

#undef CR_USE_FALLBACKS_FOR_OLD_GLIBCXX

#endif  // BASE_TEMPLATE_UTIL_H_
