/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsCharSeparatedTokenizer_h
#define __nsCharSeparatedTokenizer_h

#include "mozilla/RangedPtr.h"

#include "nsDependentSubstring.h"
#include "nsCRT.h"

/**
 * This parses a SeparatorChar-separated string into tokens.
 * Whitespace surrounding tokens is not treated as part of tokens, however
 * whitespace inside a token is. If the final token is the empty string, it is
 * not returned.
 *
 * Some examples, with SeparatorChar = ',':
 *
 * "foo, bar, baz" ->      "foo" "bar" "baz"
 * "foo,bar,baz" ->        "foo" "bar" "baz"
 * "foo , bar hi , baz" -> "foo" "bar hi" "baz"
 * "foo, ,bar,baz" ->      "foo" "" "bar" "baz"
 * "foo,,bar,baz" ->       "foo" "" "bar" "baz"
 * "foo,bar,baz," ->       "foo" "bar" "baz"
 *
 * The function used for whitespace detection is a template argument.
 * By default, it is NS_IsAsciiWhitespace.
 */
template<typename DependentSubstringType, bool IsWhitespace(char16_t)>
class nsTCharSeparatedTokenizer
{
  typedef typename DependentSubstringType::char_type CharType;
  typedef typename DependentSubstringType::substring_type SubstringType;

public:
  // Flags -- only one for now. If we need more, they should be defined to
  // be 1 << 1, 1 << 2, etc. (They're masks, and aFlags is a bitfield.)
  enum
  {
    SEPARATOR_OPTIONAL = 1
  };

  nsTCharSeparatedTokenizer(const SubstringType& aSource,
                            CharType aSeparatorChar,
                            uint32_t aFlags = 0)
    : mIter(aSource.Data(), aSource.Length())
    , mEnd(aSource.Data() + aSource.Length(), aSource.Data(),
           aSource.Length())
    , mSeparatorChar(aSeparatorChar)
    , mWhitespaceBeforeFirstToken(false)
    , mWhitespaceAfterCurrentToken(false)
    , mSeparatorAfterCurrentToken(false)
    , mSeparatorOptional(aFlags & SEPARATOR_OPTIONAL)
  {
    // Skip initial whitespace
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
    MOZ_ASSERT(mIter == mEnd || !IsWhitespace(*mIter),
               "Should be at beginning of token if there is one");

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
   * Returns true if there is a separator after the current token.
   * Useful if you want to check whether the last token has a separator
   * after it which may not be valid.
   */
  bool separatorAfterCurrentToken() const
  {
    return mSeparatorAfterCurrentToken;
  }

  /*
   * Returns true if there is any whitespace after the current token.
   */
  bool whitespaceAfterCurrentToken() const
  {
    return mWhitespaceAfterCurrentToken;
  }

  /**
   * Returns the next token.
   */
  const DependentSubstringType nextToken()
  {
    mozilla::RangedPtr<const CharType> tokenStart = mIter;
    mozilla::RangedPtr<const CharType> tokenEnd = mIter;

    MOZ_ASSERT(mIter == mEnd || !IsWhitespace(*mIter),
               "Should be at beginning of token if there is one");

    // Search until we hit separator or end (or whitespace, if a separator
    // isn't required -- see clause with 'break' below).
    while (mIter < mEnd && *mIter != mSeparatorChar) {
      // Skip to end of the current word.
      while (mIter < mEnd &&
             !IsWhitespace(*mIter) && *mIter != mSeparatorChar) {
        ++mIter;
      }
      tokenEnd = mIter;

      // Skip whitespace after the current word.
      mWhitespaceAfterCurrentToken = false;
      while (mIter < mEnd && IsWhitespace(*mIter)) {
        mWhitespaceAfterCurrentToken = true;
        ++mIter;
      }
      if (mSeparatorOptional) {
        // We've hit (and skipped) whitespace, and that's sufficient to end
        // our token, regardless of whether we've reached a SeparatorChar.
        break;
      } // (else, we'll keep looping until we hit mEnd or SeparatorChar)
    }

    mSeparatorAfterCurrentToken = (mIter != mEnd &&
                                   *mIter == mSeparatorChar);
    MOZ_ASSERT(mSeparatorOptional ||
               (mSeparatorAfterCurrentToken == (mIter < mEnd)),
               "If we require a separator and haven't hit the end of "
               "our string, then we shouldn't have left the loop "
               "unless we hit a separator");

    // Skip separator (and any whitespace after it), if we're at one.
    if (mSeparatorAfterCurrentToken) {
      ++mIter;

      while (mIter < mEnd && IsWhitespace(*mIter)) {
        mWhitespaceAfterCurrentToken = true;
        ++mIter;
      }
    }

    return Substring(tokenStart.get(), tokenEnd.get());
  }

private:
  mozilla::RangedPtr<const CharType> mIter;
  const mozilla::RangedPtr<const CharType> mEnd;
  CharType mSeparatorChar;
  bool mWhitespaceBeforeFirstToken;
  bool mWhitespaceAfterCurrentToken;
  bool mSeparatorAfterCurrentToken;
  bool mSeparatorOptional;
};

template<bool IsWhitespace(char16_t) = NS_IsAsciiWhitespace>
class nsCharSeparatedTokenizerTemplate
  : public nsTCharSeparatedTokenizer<nsDependentSubstring, IsWhitespace>
{
public:
  nsCharSeparatedTokenizerTemplate(const nsSubstring& aSource,
                                   char16_t aSeparatorChar,
                                   uint32_t aFlags = 0)
    : nsTCharSeparatedTokenizer<nsDependentSubstring,
                                IsWhitespace>(aSource, aSeparatorChar, aFlags)
  {
  }
};

typedef nsCharSeparatedTokenizerTemplate<> nsCharSeparatedTokenizer;

template<bool IsWhitespace(char16_t) = NS_IsAsciiWhitespace>
class nsCCharSeparatedTokenizerTemplate
  : public nsTCharSeparatedTokenizer<nsDependentCSubstring, IsWhitespace>
{
public:
  nsCCharSeparatedTokenizerTemplate(const nsCSubstring& aSource,
                                    char aSeparatorChar,
                                    uint32_t aFlags = 0)
    : nsTCharSeparatedTokenizer<nsDependentCSubstring,
                                IsWhitespace>(aSource, aSeparatorChar, aFlags)
  {
  }
};

typedef nsCCharSeparatedTokenizerTemplate<> nsCCharSeparatedTokenizer;

#endif /* __nsCharSeparatedTokenizer_h */
