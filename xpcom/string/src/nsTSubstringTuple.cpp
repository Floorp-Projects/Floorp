/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


  /**
   * computes the aggregate string length
   */

nsTSubstringTuple_CharT::size_type
nsTSubstringTuple_CharT::Length() const
  {
    uint32_t len;
    if (mHead)
      len = mHead->Length();
    else
      len = TO_SUBSTRING(mFragA).Length();

    return len + TO_SUBSTRING(mFragB).Length();
  }


  /**
   * writes the aggregate string to the given buffer.  bufLen is assumed
   * to be equal to or greater than the value returned by the Length()
   * method.  the string written to |buf| is not null-terminated.
   */

void
nsTSubstringTuple_CharT::WriteTo( char_type *buf, uint32_t bufLen ) const
  {
    const substring_type& b = TO_SUBSTRING(mFragB);

    NS_ASSERTION(bufLen >= b.Length(), "buffer too small");
    uint32_t headLen = bufLen - b.Length();
    if (mHead)
      {
        mHead->WriteTo(buf, headLen);
      }
    else
      {
        const substring_type& a = TO_SUBSTRING(mFragA);

        NS_ASSERTION(a.Length() == headLen, "buffer incorrectly sized");
        char_traits::copy(buf, a.Data(), a.Length());
      }

    char_traits::copy(buf + headLen, b.Data(), b.Length());

#if 0
    // we need to write out data into |buf|, ending at |buf+bufLen|.  so our
    // data needs to precede |buf+bufLen| exactly.  we trust that the buffer
    // was properly sized!

    const substring_type& b = TO_SUBSTRING(mFragB);

    NS_ASSERTION(bufLen >= b.Length(), "buffer is too small");
    char_traits::copy(buf + bufLen - b.Length(), b.Data(), b.Length());

    bufLen -= b.Length();

    if (mHead)
      {
        mHead->WriteTo(buf, bufLen);
      }
    else
      {
        const substring_type& a = TO_SUBSTRING(mFragA);
        NS_ASSERTION(bufLen == a.Length(), "buffer is too small");
        char_traits::copy(buf, a.Data(), a.Length());
      }
#endif
  }


  /**
   * returns true if this tuple is dependent on (i.e., overlapping with)
   * the given char sequence.
   */

bool
nsTSubstringTuple_CharT::IsDependentOn( const char_type *start, const char_type *end ) const
  {
    // we start with the right-most fragment since it is faster to check.

    if (TO_SUBSTRING(mFragB).IsDependentOn(start, end))
      return true;

    if (mHead)
      return mHead->IsDependentOn(start, end);

    return TO_SUBSTRING(mFragA).IsDependentOn(start, end);
  }
