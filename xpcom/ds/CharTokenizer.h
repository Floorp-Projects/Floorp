/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CharTokenizer_h
#define mozilla_CharTokenizer_h

#include "nsDependentSubstring.h"

namespace mozilla {

template<PRUnichar delimiter>
class CharTokenizer
{
public:
    CharTokenizer(const nsSubstring& aSource)
    {
      aSource.BeginReading(mIter);
      aSource.EndReading(mEnd);
    }

    /**
     * Checks if any more tokens are available.
     */
    bool hasMoreTokens()
    {
      return mIter != mEnd;
    }

    /**
     * Returns the next token.
     */
    const nsDependentSubstring nextToken()
    {
      nsSubstring::const_char_iterator begin = mIter;
      while (mIter != mEnd && (*mIter) != delimiter) {
        ++mIter;
      }
      nsSubstring::const_char_iterator end = mIter;
      if (mIter != mEnd) {
        ++mIter;
      }

      return Substring(begin, end);
    }

private:
    nsSubstring::const_char_iterator mIter, mEnd;
};

template<char delimiter>
class CCharTokenizer
{
public:
    CCharTokenizer(const nsCSubstring& aSource)
    {
      aSource.BeginReading(mIter);
      aSource.EndReading(mEnd);
    }

    /**
     * Checks if any more tokens are available.
     */
    bool hasMoreTokens()
    {
      return mIter != mEnd;
    }

    /**
     * Returns the next token.
     */
    const nsDependentCSubstring nextToken()
    {
      nsCSubstring::const_char_iterator begin = mIter;
      while (mIter != mEnd && (*mIter) != delimiter) {
        ++mIter;
      }
      nsCSubstring::const_char_iterator end = mIter;
      if (mIter != mEnd) {
        ++mIter;
      }

      return Substring(begin, end);
    }

private:
    nsCSubstring::const_char_iterator mIter, mEnd;
};

} // namespace mozilla

#endif /* mozilla_CharTokenizer_h */
