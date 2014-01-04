/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsWhitespaceTokenizer_h
#define __nsWhitespaceTokenizer_h

#include "mozilla/RangedPtr.h"
#include "nsDependentSubstring.h"
#include "nsCRT.h"

template<bool IsWhitespace(char16_t) = NS_IsAsciiWhitespace>
class nsWhitespaceTokenizerTemplate
{
public:
    nsWhitespaceTokenizerTemplate(const nsSubstring& aSource)
        : mIter(aSource.Data(), aSource.Length()),
          mEnd(aSource.Data() + aSource.Length(), aSource.Data(),
               aSource.Length()),
          mWhitespaceBeforeFirstToken(false),
          mWhitespaceAfterCurrentToken(false)
    {
        while (mIter < mEnd && IsWhitespace(*mIter)) {
            mWhitespaceBeforeFirstToken = true;
            ++mIter;
        }
    }

    /**
     * Checks if any more tokens are available.
     */
    bool hasMoreTokens() const
    {
        return mIter < mEnd;
    }

    /*
     * Returns true if there is whitespace prior to the first token.
     */
    bool whitespaceBeforeFirstToken() const
    {
        return mWhitespaceBeforeFirstToken;
    }

    /*
     * Returns true if there is any whitespace after the current token.
     * This is always true unless we're reading the last token.
     */
    bool whitespaceAfterCurrentToken() const
    {
        return mWhitespaceAfterCurrentToken;
    }

    /**
     * Returns the next token.
     */
    const nsDependentSubstring nextToken()
    {
        const mozilla::RangedPtr<const char16_t> tokenStart = mIter;
        while (mIter < mEnd && !IsWhitespace(*mIter)) {
            ++mIter;
        }
        const mozilla::RangedPtr<const char16_t> tokenEnd = mIter;
        mWhitespaceAfterCurrentToken = false;
        while (mIter < mEnd && IsWhitespace(*mIter)) {
            mWhitespaceAfterCurrentToken = true;
            ++mIter;
        }
        return Substring(tokenStart.get(), tokenEnd.get());
    }

private:
    mozilla::RangedPtr<const char16_t> mIter;
    const mozilla::RangedPtr<const char16_t> mEnd;
    bool mWhitespaceBeforeFirstToken;
    bool mWhitespaceAfterCurrentToken;
};

class nsWhitespaceTokenizer: public nsWhitespaceTokenizerTemplate<>
{
public:
    nsWhitespaceTokenizer(const nsSubstring& aSource)
      : nsWhitespaceTokenizerTemplate<>(aSource)
    {
    }
};

template<bool IsWhitespace(char16_t) = NS_IsAsciiWhitespace>
class nsCWhitespaceTokenizerTemplate
{
public:
    nsCWhitespaceTokenizerTemplate(const nsCSubstring& aSource)
        : mIter(aSource.Data(), aSource.Length()),
          mEnd(aSource.Data() + aSource.Length(), aSource.Data(),
               aSource.Length()),
          mWhitespaceBeforeFirstToken(false),
          mWhitespaceAfterCurrentToken(false)
    {
        while (mIter < mEnd && IsWhitespace(*mIter)) {
            mWhitespaceBeforeFirstToken = true;
            ++mIter;
        }
    }

    /**
     * Checks if any more tokens are available.
     */
    bool hasMoreTokens() const
    {
        return mIter < mEnd;
    }

    /*
     * Returns true if there is whitespace prior to the first token.
     */
    bool whitespaceBeforeFirstToken() const
    {
        return mWhitespaceBeforeFirstToken;
    }

    /*
     * Returns true if there is any whitespace after the current token.
     * This is always true unless we're reading the last token.
     */
    bool whitespaceAfterCurrentToken() const
    {
        return mWhitespaceAfterCurrentToken;
    }

    /**
     * Returns the next token.
     */
    const nsDependentCSubstring nextToken()
    {
        const mozilla::RangedPtr<const char> tokenStart = mIter;
        while (mIter < mEnd && !IsWhitespace(*mIter)) {
            ++mIter;
        }
        const mozilla::RangedPtr<const char> tokenEnd = mIter;
        mWhitespaceAfterCurrentToken = false;
        while (mIter < mEnd && IsWhitespace(*mIter)) {
            mWhitespaceAfterCurrentToken = true;
            ++mIter;
        }
        return Substring(tokenStart.get(), tokenEnd.get());
    }

private:
    mozilla::RangedPtr<const char> mIter;
    const mozilla::RangedPtr<const char> mEnd;
    bool mWhitespaceBeforeFirstToken;
    bool mWhitespaceAfterCurrentToken;
};

class nsCWhitespaceTokenizer: public nsCWhitespaceTokenizerTemplate<>
{
public:
    nsCWhitespaceTokenizer(const nsCSubstring& aSource)
      : nsCWhitespaceTokenizerTemplate<>(aSource)
    {
    }
};

#endif /* __nsWhitespaceTokenizer_h */
