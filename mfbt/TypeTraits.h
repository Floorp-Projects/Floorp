/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Template-based metaprogramming and type-testing facilities. */

#ifndef mozilla_TypeTraits_h_
#define mozilla_TypeTraits_h_

namespace mozilla {

/*
 * IsBaseOf allows to know whether a given class is derived from another.
 *
 * Consider the following class definitions:
 *
 *   class A {};
 *   class B : public A {};
 *   class C {};
 *
 * mozilla::IsBaseOf<A, B>::value is true;
 * mozilla::IsBaseOf<A, C>::value is false;
 */
template<class Base, class Derived>
class IsBaseOf
{
  private:
    static char test(Base* b);
    static int test(...);
  public:
    static const bool value = (sizeof(test(static_cast<Derived*>(0))) == sizeof(char));
};

/*
 * Conditional selects a class between two, depending on a given boolean value.
 *
 * mozilla::Conditional<true, A, B>::Type is A;
 * mozilla::Conditional<false, A, B>::Type is B;
 */
template<bool condition, class A, class B>
struct Conditional
{
    typedef A Type;
};

template<class A, class B>
struct Conditional<false, A, B>
{
    typedef B Type;
};

/*
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
 *      EnableIf<IsPodType<T>, T>::Type* vector;
 *      size_t length;
 *      ...
 *   };
 */
template<bool B, typename T = void>
struct EnableIf
{};

template<typename T>
struct EnableIf<true, T>
{
    typedef T Type;
};

} /* namespace mozilla */

#endif  /* mozilla_TypeTraits_h_ */
