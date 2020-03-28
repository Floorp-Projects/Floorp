/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A class holding a pair of objects that tries to conserve storage space. */

#ifndef mozilla_CompactPair_h
#define mozilla_CompactPair_h

#include <type_traits>
#include <utility>

#include "mozilla/Attributes.h"
#include "mozilla/TypeTraits.h"

namespace mozilla {

namespace detail {

enum StorageType { AsBase, AsMember };

// Optimize storage using the Empty Base Optimization -- that empty base classes
// don't take up space -- to optimize size when one or the other class is
// stateless and can be used as a base class.
//
// The extra conditions on storage for B are necessary so that CompactPairHelper
// won't ambiguously inherit from either A or B, such that one or the other base
// class would be inaccessible.
template <typename A, typename B,
          detail::StorageType =
              std::is_empty_v<A> ? detail::AsBase : detail::AsMember,
          detail::StorageType = std::is_empty_v<B> &&
                                        !std::is_base_of<A, B>::value &&
                                        !std::is_base_of<B, A>::value
                                    ? detail::AsBase
                                    : detail::AsMember>
struct CompactPairHelper;

template <typename A, typename B>
struct CompactPairHelper<A, B, AsMember, AsMember> {
 protected:
  template <typename AArg, typename BArg>
  CompactPairHelper(AArg&& aA, BArg&& aB)
      : mFirstA(std::forward<AArg>(aA)), mSecondB(std::forward<BArg>(aB)) {}

  A& first() { return mFirstA; }
  const A& first() const { return mFirstA; }
  B& second() { return mSecondB; }
  const B& second() const { return mSecondB; }

  void swap(CompactPairHelper& aOther) {
    std::swap(mFirstA, aOther.mFirstA);
    std::swap(mSecondB, aOther.mSecondB);
  }

 private:
  A mFirstA;
  B mSecondB;
};

template <typename A, typename B>
struct CompactPairHelper<A, B, AsMember, AsBase> : private B {
 protected:
  template <typename AArg, typename BArg>
  CompactPairHelper(AArg&& aA, BArg&& aB)
      : B(std::forward<BArg>(aB)), mFirstA(std::forward<AArg>(aA)) {}

  A& first() { return mFirstA; }
  const A& first() const { return mFirstA; }
  B& second() { return *this; }
  const B& second() const { return *this; }

  void swap(CompactPairHelper& aOther) {
    std::swap(mFirstA, aOther.mFirstA);
    std::swap(static_cast<B&>(*this), static_cast<B&>(aOther));
  }

 private:
  A mFirstA;
};

template <typename A, typename B>
struct CompactPairHelper<A, B, AsBase, AsMember> : private A {
 protected:
  template <typename AArg, typename BArg>
  CompactPairHelper(AArg&& aA, BArg&& aB)
      : A(std::forward<AArg>(aA)), mSecondB(std::forward<BArg>(aB)) {}

  A& first() { return *this; }
  const A& first() const { return *this; }
  B& second() { return mSecondB; }
  const B& second() const { return mSecondB; }

  void swap(CompactPairHelper& aOther) {
    std::swap(static_cast<A&>(*this), static_cast<A&>(aOther));
    std::swap(mSecondB, aOther.mSecondB);
  }

 private:
  B mSecondB;
};

template <typename A, typename B>
struct CompactPairHelper<A, B, AsBase, AsBase> : private A, private B {
 protected:
  template <typename AArg, typename BArg>
  CompactPairHelper(AArg&& aA, BArg&& aB)
      : A(std::forward<AArg>(aA)), B(std::forward<BArg>(aB)) {}

  A& first() { return static_cast<A&>(*this); }
  const A& first() const { return static_cast<A&>(*this); }
  B& second() { return static_cast<B&>(*this); }
  const B& second() const { return static_cast<B&>(*this); }

  void swap(CompactPairHelper& aOther) {
    std::swap(static_cast<A&>(*this), static_cast<A&>(aOther));
    std::swap(static_cast<B&>(*this), static_cast<B&>(aOther));
  }
};

}  // namespace detail

/**
 * CompactPair is the logical concatenation of an instance of A with an instance
 * B. Space is conserved when possible.  Neither A nor B may be a final class.
 *
 * In general if space conservation is not critical is preferred to use
 * std::pair.
 *
 * It's typically clearer to have individual A and B member fields.  Except if
 * you want the space-conserving qualities of CompactPair, you're probably
 * better off not using this!
 *
 * No guarantees are provided about the memory layout of A and B, the order of
 * initialization or destruction of A and B, and so on.  (This is approximately
 * required to optimize space usage.)  The first/second names are merely
 * conceptual!
 */
template <typename A, typename B>
struct CompactPair : private detail::CompactPairHelper<A, B> {
  typedef typename detail::CompactPairHelper<A, B> Base;

 public:
  template <typename AArg, typename BArg>
  CompactPair(AArg&& aA, BArg&& aB)
      : Base(std::forward<AArg>(aA), std::forward<BArg>(aB)) {}

  CompactPair(CompactPair&& aOther) = default;
  CompactPair(const CompactPair& aOther) = default;

  CompactPair& operator=(CompactPair&& aOther) = default;
  CompactPair& operator=(const CompactPair& aOther) = default;

  /** The A instance. */
  using Base::first;
  /** The B instance. */
  using Base::second;

  /** Swap this pair with another pair. */
  void swap(CompactPair& aOther) { Base::swap(aOther); }
};

/**
 * MakeCompactPair allows you to construct a CompactPair instance using type
 * inference. A call like this:
 *
 *   MakeCompactPair(Foo(), Bar())
 *
 * will return a CompactPair<Foo, Bar>.
 */
template <typename A, typename B>
CompactPair<typename RemoveCV<std::remove_reference_t<A>>::Type,
            typename RemoveCV<std::remove_reference_t<B>>::Type>
MakeCompactPair(A&& aA, B&& aB) {
  return CompactPair<typename RemoveCV<std::remove_reference_t<A>>::Type,
                     typename RemoveCV<std::remove_reference_t<B>>::Type>(
      std::forward<A>(aA), std::forward<B>(aB));
}

}  // namespace mozilla

namespace std {

template <typename A, class B>
void swap(mozilla::CompactPair<A, B>& aX, mozilla::CompactPair<A, B>& aY) {
  aX.swap(aY);
}

}  // namespace std

#endif /* mozilla_CompactPair_h */
