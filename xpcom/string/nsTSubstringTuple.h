/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// IWYU pragma: private, include "nsString.h"

#ifndef nsTSubstringTuple_h
#define nsTSubstringTuple_h

#include "mozilla/Attributes.h"
#include "nsTStringRepr.h"

/**
 * nsTSubstringTuple
 *
 * Represents a tuple of string fragments.  Built as a recursive binary tree.
 * It is used to implement the concatenation of two or more string objects.
 *
 * NOTE: This class is a private implementation detail and should never be
 * referenced outside the string code.
 */
template <typename T>
class MOZ_TEMPORARY_CLASS nsTSubstringTuple {
 public:
  typedef T char_type;
  typedef nsCharTraits<char_type> char_traits;

  typedef nsTSubstringTuple<T> self_type;
  typedef mozilla::detail::nsTStringRepr<char_type> base_string_type;
  typedef size_t size_type;

 public:
  nsTSubstringTuple(const base_string_type* aStrA,
                    const base_string_type* aStrB)
      : mHead(nullptr), mFragA(aStrA), mFragB(aStrB) {}

  nsTSubstringTuple(const self_type& aHead, const base_string_type* aStrB)
      : mHead(&aHead),
        mFragA(nullptr),  // this fragment is ignored when aHead != nullptr
        mFragB(aStrB) {}

  /**
   * computes the aggregate string length
   */
  size_type Length() const;

  /**
   * writes the aggregate string to the given buffer.  bufLen is assumed
   * to be equal to or greater than the value returned by the Length()
   * method.  the string written to |buf| is not null-terminated.
   */
  void WriteTo(char_type* aBuf, size_type aBufLen) const;

  /**
   * returns true if this tuple is dependent on (i.e., overlapping with)
   * the given char sequence.
   */
  bool IsDependentOn(const char_type* aStart, const char_type* aEnd) const;

  /**
   * returns a pair of the results of IsDependentOn() and Length(). This is more
   * efficient than calling both functions subsequently, as this traverses the
   * tree only once.
   */
  std::pair<bool, size_type> IsDependentOnWithLength(
      const char_type* aStart, const char_type* aEnd) const;

 private:
  const self_type* const mHead;
  const base_string_type* const mFragA;
  const base_string_type* const mFragB;
};

template <typename T>
inline const nsTSubstringTuple<T> operator+(
    const mozilla::detail::nsTStringRepr<T>& aStrA,
    const mozilla::detail::nsTStringRepr<T>& aStrB) {
  return nsTSubstringTuple<T>(&aStrA, &aStrB);
}

template <typename T>
inline const nsTSubstringTuple<T> operator+(
    const nsTSubstringTuple<T>& aHead,
    const mozilla::detail::nsTStringRepr<T>& aStrB) {
  return nsTSubstringTuple<T>(aHead, &aStrB);
}

#endif
