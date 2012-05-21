/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsWhitespaceTokenizer_h
#define __nsWhitespaceTokenizer_h

#include "nsDependentSubstring.h"

class nsWhitespaceTokenizer
{
public:
    nsWhitespaceTokenizer(const nsSubstring& aSource)
    {
        aSource.BeginReading(mIter);
        aSource.EndReading(mEnd);

        while (mIter != mEnd && isWhitespace(*mIter)) {
            ++mIter;
        }
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
        while (mIter != mEnd && !isWhitespace(*mIter)) {
            ++mIter;
        }
        nsSubstring::const_char_iterator end = mIter;
        while (mIter != mEnd && isWhitespace(*mIter)) {
            ++mIter;
        }
        return Substring(begin, end);
    }

private:
    nsSubstring::const_char_iterator mIter, mEnd;

    bool isWhitespace(PRUnichar aChar)
    {
        return aChar <= ' ' &&
               (aChar == ' ' || aChar == '\n' ||
                aChar == '\r'|| aChar == '\t');
    }
};

class nsCWhitespaceTokenizer
{
public:
    nsCWhitespaceTokenizer(const nsCSubstring& aSource)
    {
        aSource.BeginReading(mIter);
        aSource.EndReading(mEnd);

        while (mIter != mEnd && isWhitespace(*mIter)) {
            ++mIter;
        }
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
        while (mIter != mEnd && !isWhitespace(*mIter)) {
            ++mIter;
        }
        nsCSubstring::const_char_iterator end = mIter;
        while (mIter != mEnd && isWhitespace(*mIter)) {
            ++mIter;
        }
        return Substring(begin, end);
    }

private:
    nsCSubstring::const_char_iterator mIter, mEnd;

    bool isWhitespace(char aChar)
    {
        return aChar <= ' ' &&
               (aChar == ' ' || aChar == '\n' ||
                aChar == '\r'|| aChar == '\t');
    }
};

#endif /* __nsWhitespaceTokenizer_h */
