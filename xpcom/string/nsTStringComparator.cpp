/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"

template <typename T>
int NS_FASTCALL Compare(const mozilla::detail::nsTStringRepr<T>& aLhs,
                        const mozilla::detail::nsTStringRepr<T>& aRhs,
                        const nsTStringComparator<T> comp) {
  typedef typename nsTSubstring<T>::size_type size_type;
  typedef typename nsTSubstring<T>::const_iterator const_iterator;

  if (&aLhs == &aRhs) {
    return 0;
  }

  const_iterator leftIter, rightIter;
  aLhs.BeginReading(leftIter);
  aRhs.BeginReading(rightIter);

  size_type lLength = aLhs.Length();
  size_type rLength = aRhs.Length();
  size_type lengthToCompare = XPCOM_MIN(lLength, rLength);

  int result;
  if ((result = comp(leftIter.get(), rightIter.get(), lengthToCompare,
                     lengthToCompare)) == 0) {
    if (lLength < rLength) {
      result = -1;
    } else if (rLength < lLength) {
      result = 1;
    } else {
      result = 0;
    }
  }

  return result;
}

template int NS_FASTCALL Compare<char>(
    mozilla::detail::nsTStringRepr<char> const&,
    mozilla::detail::nsTStringRepr<char> const&, nsTStringComparator<char>);

template int NS_FASTCALL
Compare<char16_t>(mozilla::detail::nsTStringRepr<char16_t> const&,
                  mozilla::detail::nsTStringRepr<char16_t> const&,
                  nsTStringComparator<char16_t>);

template <typename T>
int nsTDefaultStringComparator(const T* aLhs, const T* aRhs, size_t aLLength,
                               size_t aRLength) {
  return aLLength == aRLength ? nsCharTraits<T>::compare(aLhs, aRhs, aLLength)
         : (aLLength > aRLength) ? 1
                                 : -1;
}

template int nsTDefaultStringComparator(const char*, const char*, size_t,
                                        size_t);
template int nsTDefaultStringComparator(const char16_t*, const char16_t*,
                                        size_t, size_t);
