/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TypeTraits_h_
#define mozilla_TypeTraits_h_

/* Template-based metaprogramming and type-testing facilities. */

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
template <class Base, class Derived>
class IsBaseOf
{
  private:
    static char Test(Base *);
    static int Test(...);
  public:
    static const bool value = (sizeof(Test(static_cast<Derived *>(0))) == sizeof(char));
};

/*
 * Conditional selects a class between two, depending on a given boolean value.
 *
 * mozilla::Conditional<true, A, B>::Type is A;
 * mozilla::Conditional<false, A, B>::Type is B;
 */
template <bool condition, class A, class B>
struct Conditional
{
    typedef A Type;
};

template <class A, class B>
struct Conditional<false, A, B>
{
    typedef B Type;
};

} /* namespace mozilla */

#endif  /* mozilla_TypeTraits_h_ */
