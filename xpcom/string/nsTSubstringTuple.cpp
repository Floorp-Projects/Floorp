/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTSubstringTuple.h"
#include "mozilla/CheckedInt.h"

/**
 * computes the aggregate string length
 */

template <typename T>
typename nsTSubstringTuple<T>::size_type nsTSubstringTuple<T>::Length() const {
  mozilla::CheckedInt<size_type> len;
  if (mHead) {
    len = mHead->Length();
  } else {
    len = mFragA->Length();
  }

  len += mFragB->Length();
  MOZ_RELEASE_ASSERT(len.isValid(), "Substring tuple length is invalid");
  return len.value();
}

/**
 * writes the aggregate string to the given buffer. aBufLen is assumed
 * to be equal to or greater than the value returned by the Length()
 * method.  the string written to |aBuf| is not null-terminated.
 */

template <typename T>
void nsTSubstringTuple<T>::WriteTo(char_type* aBuf, uint32_t aBufLen) const {
  MOZ_RELEASE_ASSERT(aBufLen >= mFragB->Length(), "buffer too small");
  uint32_t headLen = aBufLen - mFragB->Length();
  if (mHead) {
    mHead->WriteTo(aBuf, headLen);
  } else {
    MOZ_RELEASE_ASSERT(mFragA->Length() == headLen, "buffer incorrectly sized");
    char_traits::copy(aBuf, mFragA->Data(), mFragA->Length());
  }

  char_traits::copy(aBuf + headLen, mFragB->Data(), mFragB->Length());
}

/**
 * returns true if this tuple is dependent on (i.e., overlapping with)
 * the given char sequence.
 */

template <typename T>
bool nsTSubstringTuple<T>::IsDependentOn(const char_type* aStart,
                                         const char_type* aEnd) const {
  // we aStart with the right-most fragment since it is faster to check.

  if (mFragB->IsDependentOn(aStart, aEnd)) {
    return true;
  }

  if (mHead) {
    return mHead->IsDependentOn(aStart, aEnd);
  }

  return mFragA->IsDependentOn(aStart, aEnd);
}
