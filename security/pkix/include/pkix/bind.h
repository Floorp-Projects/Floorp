/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Copyright 2013 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Work around missing std::bind, std::ref, std::cref in older compilers. This
// implementation isn't intended to be complete; rather, it is the minimal
// implementation needed to make our use of std::bind work.

#ifndef mozilla_pkix__bind_h
#define mozilla_pkix__bind_h

#ifdef _MSC_VER
#pragma warning(disable:4275) //Suppress spurious MSVC warning
#endif
#include <functional>
#ifdef _MSC_VER
#pragma warning(default:4275)
#endif

namespace mozilla { namespace pkix {

#ifdef _MSC_VER

using std::bind;
using std::ref;
using std::cref;
using std::placeholders::_1;

#else

class Placeholder1 { };
extern Placeholder1 _1;

template <typename V>       V&  ref(V& v)       { return v; }
template <typename V> const V& cref(const V& v) { return v; }

namespace internal {

template <typename R, typename P1, typename B1>
class Bind1
{
public:
  typedef R (*F)(P1 &, B1 &);
  Bind1(F f, B1 & b1) : f(f), b1(b1) { }
  R operator()(P1 & p1) const { return f(p1, b1); }
private:
  const F f;
  B1& b1;
};

template <typename R, typename P1, typename B1, typename B2>
class Bind2
{
public:
  typedef R (*F)(P1&, B1&, B2&);
  Bind2(F f, B1& b1, B2& b2) : f(f), b1(b1), b2(b2) { }
  R operator()(P1& p1) const { return f(p1, b1, b2); }
private:
  const F f;
  B1& b1;
  B2& b2;
};

} // namespace internal

template <typename R, typename P1, typename B1>
inline internal::Bind1<R, P1, B1>
bind(R (*f)(P1&, B1&), Placeholder1&, B1& b1)
{
  return internal::Bind1<R, P1, B1>(f, b1);
}

template <typename R, typename P1, typename B1, typename B2>
inline internal::Bind2<R, P1, B1, B2>
bind(R (*f)(P1&, B1&, B2&), Placeholder1 &, B1 & b1, B2 & b2)
{
  return internal::Bind2<R, P1, B1, B2>(f, b1, b2);
}

#endif // _MSC_VER

} } // namespace mozilla::pkix

#endif // mozilla_pkix__bind_h
