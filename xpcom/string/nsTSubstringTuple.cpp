/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/CheckedInt.h"

/**
 * computes the aggregate string length
 */

nsTSubstringTuple_CharT::size_type
nsTSubstringTuple_CharT::Length() const
{
  mozilla::CheckedInt<size_type> len;
  if (mHead) {
    len = mHead->Length();
  } else {
    len = TO_SUBSTRING(mFragA).Length();
  }

  len += TO_SUBSTRING(mFragB).Length();
  MOZ_RELEASE_ASSERT(len.isValid(), "Substring tuple length is invalid");
  return len.value();
}


/**
 * writes the aggregate string to the given buffer. aBufLen is assumed
 * to be equal to or greater than the value returned by the Length()
 * method.  the string written to |aBuf| is not null-terminated.
 */

void
nsTSubstringTuple_CharT::WriteTo(char_type* aBuf, uint32_t aBufLen) const
{
  const substring_type& b = TO_SUBSTRING(mFragB);

  MOZ_RELEASE_ASSERT(aBufLen >= b.Length(), "buffer too small");
  uint32_t headLen = aBufLen - b.Length();
  if (mHead) {
    mHead->WriteTo(aBuf, headLen);
  } else {
    const substring_type& a = TO_SUBSTRING(mFragA);

    MOZ_RELEASE_ASSERT(a.Length() == headLen, "buffer incorrectly sized");
    char_traits::copy(aBuf, a.Data(), a.Length());
  }

  char_traits::copy(aBuf + headLen, b.Data(), b.Length());

#if 0
  // we need to write out data into |aBuf|, ending at |aBuf + aBufLen|. So our
  // data needs to precede |aBuf + aBufLen| exactly. We trust that the buffer
  // was properly sized!

  const substring_type& b = TO_SUBSTRING(mFragB);

  NS_ASSERTION(aBufLen >= b.Length(), "buffer is too small");
  char_traits::copy(aBuf + aBufLen - b.Length(), b.Data(), b.Length());

  aBufLen -= b.Length();

  if (mHead) {
    mHead->WriteTo(aBuf, aBufLen);
  } else {
    const substring_type& a = TO_SUBSTRING(mFragA);
    NS_ASSERTION(aBufLen == a.Length(), "buffer is too small");
    char_traits::copy(aBuf, a.Data(), a.Length());
  }
#endif
}


/**
 * returns true if this tuple is dependent on (i.e., overlapping with)
 * the given char sequence.
 */

bool
nsTSubstringTuple_CharT::IsDependentOn(const char_type* aStart,
                                       const char_type* aEnd) const
{
  // we aStart with the right-most fragment since it is faster to check.

  if (TO_SUBSTRING(mFragB).IsDependentOn(aStart, aEnd)) {
    return true;
  }

  if (mHead) {
    return mHead->IsDependentOn(aStart, aEnd);
  }

  return TO_SUBSTRING(mFragA).IsDependentOn(aStart, aEnd);
}
