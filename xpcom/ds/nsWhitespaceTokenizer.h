/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsWhitespaceTokenizer_h
#define __nsWhitespaceTokenizer_h

#include "mozilla/RangedPtr.h"
#include "nsDependentSubstring.h"

class nsWhitespaceTokenizer
{
public:
    nsWhitespaceTokenizer(const nsSubstring& aSource)
        : mIter(aSource.Data(), aSource.Length()),
          mEnd(aSource.Data() + aSource.Length(), aSource.Data(),
               aSource.Length())
    {
        while (mIter < mEnd && isWhitespace(*mIter)) {
            ++mIter;
        }
    }

    /**
     * Checks if any more tokens are available.
     */
    bool hasMoreTokens()
    {
        return mIter < mEnd;
    }

    /**
     * Returns the next token.
     */
    const nsDependentSubstring nextToken()
    {
        const mozilla::RangedPtr<const PRUnichar> tokenStart = mIter;
        while (mIter < mEnd && !isWhitespace(*mIter)) {
            ++mIter;
        }
        const mozilla::RangedPtr<const PRUnichar> tokenEnd = mIter;
        while (mIter < mEnd && isWhitespace(*mIter)) {
            ++mIter;
        }
        return Substring(tokenStart.get(), tokenEnd.get());
    }

private:
    mozilla::RangedPtr<const PRUnichar> mIter;
    const mozilla::RangedPtr<const PRUnichar> mEnd;

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
        : mIter(aSource.Data(), aSource.Length()),
          mEnd(aSource.Data() + aSource.Length(), aSource.Data(),
               aSource.Length())
    {
        while (mIter < mEnd && isWhitespace(*mIter)) {
            ++mIter;
        }
    }

    /**
     * Checks if any more tokens are available.
     */
    bool hasMoreTokens()
    {
        return mIter < mEnd;
    }

    /**
     * Returns the next token.
     */
    const nsDependentCSubstring nextToken()
    {
        const mozilla::RangedPtr<const char> tokenStart = mIter;
        while (mIter < mEnd && !isWhitespace(*mIter)) {
            ++mIter;
        }
        const mozilla::RangedPtr<const char> tokenEnd = mIter;
        while (mIter < mEnd && isWhitespace(*mIter)) {
            ++mIter;
        }
        return Substring(tokenStart.get(), tokenEnd.get());
    }

private:
    mozilla::RangedPtr<const char> mIter;
    const mozilla::RangedPtr<const char> mEnd;

    bool isWhitespace(char aChar)
    {
        return aChar <= ' ' &&
               (aChar == ' ' || aChar == '\n' ||
                aChar == '\r'|| aChar == '\t');
    }
};

#endif /* __nsWhitespaceTokenizer_h */
