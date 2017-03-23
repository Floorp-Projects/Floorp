// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Use std::tuple as tuple type. This file contains helper functions for
// working with std::tuples.
// The functions DispatchToMethod and DispatchToFunction take a function pointer
// or instance and method pointer, and unpack a tuple into arguments to the
// call.
//
// Example usage:
//   // These two methods of creating a Tuple are identical.
//   std::tuple<int, const char*> tuple_a(1, "wee");
//   std::tuple<int, const char*> tuple_b = std::make_tuple(1, "wee");
//
//   void SomeFunc(int a, const char* b) { }
//   DispatchToFunction(&SomeFunc, tuple_a);  // SomeFunc(1, "wee")
//   DispatchToFunction(
//       &SomeFunc, std::make_tuple(10, "foo"));    // SomeFunc(10, "foo")
//
//   struct { void SomeMeth(int a, int b, int c) { } } foo;
//   DispatchToMethod(&foo, &Foo::SomeMeth, std::make_tuple(1, 2, 3));
//   // foo->SomeMeth(1, 2, 3);

#ifndef BASE_TUPLE_H_
#define BASE_TUPLE_H_

#include <stddef.h>
#include <tuple>

#include "base/bind_helpers.h"
#include "build/build_config.h"

namespace base {

// Index sequences
//
// Minimal clone of the similarly-named C++14 functionality.

template <size_t...>
struct IndexSequence {};

template <size_t... Ns>
struct MakeIndexSequenceImpl;

#if defined(_PREFAST_) && defined(OS_WIN)

// Work around VC++ 2013 /analyze internal compiler error:
// https://connect.microsoft.com/VisualStudio/feedback/details/1053626

template <> struct MakeIndexSequenceImpl<0> {
  using Type = IndexSequence<>;
};
template <> struct MakeIndexSequenceImpl<1> {
  using Type = IndexSequence<0>;
};
template <> struct MakeIndexSequenceImpl<2> {
  using Type = IndexSequence<0,1>;
};
template <> struct MakeIndexSequenceImpl<3> {
  using Type = IndexSequence<0,1,2>;
};
template <> struct MakeIndexSequenceImpl<4> {
  using Type = IndexSequence<0,1,2,3>;
};
template <> struct MakeIndexSequenceImpl<5> {
  using Type = IndexSequence<0,1,2,3,4>;
};
template <> struct MakeIndexSequenceImpl<6> {
  using Type = IndexSequence<0,1,2,3,4,5>;
};
template <> struct MakeIndexSequenceImpl<7> {
  using Type = IndexSequence<0,1,2,3,4,5,6>;
};
template <> struct MakeIndexSequenceImpl<8> {
  using Type = IndexSequence<0,1,2,3,4,5,6,7>;
};
template <> struct MakeIndexSequenceImpl<9> {
  using Type = IndexSequence<0,1,2,3,4,5,6,7,8>;
};
template <> struct MakeIndexSequenceImpl<10> {
  using Type = IndexSequence<0,1,2,3,4,5,6,7,8,9>;
};
template <> struct MakeIndexSequenceImpl<11> {
  using Type = IndexSequence<0,1,2,3,4,5,6,7,8,9,10>;
};
template <> struct MakeIndexSequenceImpl<12> {
  using Type = IndexSequence<0,1,2,3,4,5,6,7,8,9,10,11>;
};
template <> struct MakeIndexSequenceImpl<13> {
  using Type = IndexSequence<0,1,2,3,4,5,6,7,8,9,10,11,12>;
};

#else  // defined(OS_WIN) && defined(_PREFAST_)

template <size_t... Ns>
struct MakeIndexSequenceImpl<0, Ns...> {
  using Type = IndexSequence<Ns...>;
};

template <size_t N, size_t... Ns>
struct MakeIndexSequenceImpl<N, Ns...>
    : MakeIndexSequenceImpl<N - 1, N - 1, Ns...> {};

#endif  // defined(OS_WIN) && defined(_PREFAST_)

// std::get() in <=libstdc++-4.6 returns an lvalue-reference for
// rvalue-reference of a tuple, where an rvalue-reference is expected.
template <size_t I, typename... Ts>
typename std::tuple_element<I, std::tuple<Ts...>>::type&& get(
    std::tuple<Ts...>&& t) {
  using ElemType = typename std::tuple_element<I, std::tuple<Ts...>>::type;
  return std::forward<ElemType>(std::get<I>(t));
}

template <size_t I, typename T>
auto get(T& t) -> decltype(std::get<I>(t)) {
  return std::get<I>(t);
}

template <size_t N>
using MakeIndexSequence = typename MakeIndexSequenceImpl<N>::Type;

template <typename T>
using MakeIndexSequenceForTuple =
    MakeIndexSequence<std::tuple_size<typename std::decay<T>::type>::value>;

// Dispatchers ----------------------------------------------------------------
//
// Helper functions that call the given method on an object, with the unpacked
// tuple arguments.  Notice that they all have the same number of arguments,
// so you need only write:
//   DispatchToMethod(object, &Object::method, args);
// This is very useful for templated dispatchers, since they don't need to know
// what type |args| is.

// Non-Static Dispatchers with no out params.

template <typename ObjT, typename Method, typename Tuple, size_t... Ns>
inline void DispatchToMethodImpl(const ObjT& obj,
                                 Method method,
                                 Tuple&& args,
                                 IndexSequence<Ns...>) {
  (obj->*method)(base::get<Ns>(std::forward<Tuple>(args))...);
}

template <typename ObjT, typename Method, typename Tuple>
inline void DispatchToMethod(const ObjT& obj,
                             Method method,
                             Tuple&& args) {
  DispatchToMethodImpl(obj, method, std::forward<Tuple>(args),
                       MakeIndexSequenceForTuple<Tuple>());
}

// Static Dispatchers with no out params.

template <typename Function, typename Tuple, size_t... Ns>
inline void DispatchToFunctionImpl(Function function,
                                   Tuple&& args,
                                   IndexSequence<Ns...>) {
  (*function)(base::get<Ns>(std::forward<Tuple>(args))...);
}

template <typename Function, typename Tuple>
inline void DispatchToFunction(Function function, Tuple&& args) {
  DispatchToFunctionImpl(function, std::forward<Tuple>(args),
                         MakeIndexSequenceForTuple<Tuple>());
}

// Dispatchers with out parameters.

template <typename ObjT,
          typename Method,
          typename InTuple,
          typename OutTuple,
          size_t... InNs,
          size_t... OutNs>
inline void DispatchToMethodImpl(const ObjT& obj,
                                 Method method,
                                 InTuple&& in,
                                 OutTuple* out,
                                 IndexSequence<InNs...>,
                                 IndexSequence<OutNs...>) {
  (obj->*method)(base::get<InNs>(std::forward<InTuple>(in))...,
                 &std::get<OutNs>(*out)...);
}

template <typename ObjT, typename Method, typename InTuple, typename OutTuple>
inline void DispatchToMethod(const ObjT& obj,
                             Method method,
                             InTuple&& in,
                             OutTuple* out) {
  DispatchToMethodImpl(obj, method, std::forward<InTuple>(in), out,
                       MakeIndexSequenceForTuple<InTuple>(),
                       MakeIndexSequenceForTuple<OutTuple>());
}

}  // namespace base

#endif  // BASE_TUPLE_H_
