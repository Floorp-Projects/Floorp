/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsCharSeparatedTokenizer_h
#define __nsCharSeparatedTokenizer_h

#include "mozilla/Maybe.h"
#include "mozilla/RangedPtr.h"

#include "nsDependentSubstring.h"
#include "nsCRTGlue.h"

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
template <typename TDependentSubstringType, bool IsWhitespace(char16_t)>
class nsTCharSeparatedTokenizer {
  using CharType = typename TDependentSubstringType::char_type;
  using SubstringType = typename TDependentSubstringType::substring_type;

 public:
  // Flags -- only one for now. If we need more, they should be defined to
  // be 1 << 1, 1 << 2, etc. (They're masks, and aFlags is a bitfield.)
  enum { SEPARATOR_OPTIONAL = 1 };

  using DependentSubstringType = TDependentSubstringType;

  nsTCharSeparatedTokenizer(const SubstringType& aSource,
                            CharType aSeparatorChar, uint32_t aFlags = 0)
      : mIter(aSource.Data(), aSource.Length()),
        mEnd(aSource.Data() + aSource.Length(), aSource.Data(),
             aSource.Length()),
        mSeparatorChar(aSeparatorChar),
        mWhitespaceBeforeFirstToken(false),
        mWhitespaceAfterCurrentToken(false),
        mSeparatorAfterCurrentToken(false),
        mSeparatorOptional(aFlags & SEPARATOR_OPTIONAL) {
    // Skip initial whitespace
    while (mIter < mEnd && IsWhitespace(*mIter)) {
      mWhitespaceBeforeFirstToken = true;
      ++mIter;
    }
  }

  /**
   * Checks if any more tokens are available.
   */
  bool hasMoreTokens() const {
    MOZ_ASSERT(mIter == mEnd || !IsWhitespace(*mIter),
               "Should be at beginning of token if there is one");

    return mIter < mEnd;
  }

  /*
   * Returns true if there is whitespace prior to the first token.
   */
  bool whitespaceBeforeFirstToken() const {
    return mWhitespaceBeforeFirstToken;
  }

  /*
   * Returns true if there is a separator after the current token.
   * Useful if you want to check whether the last token has a separator
   * after it which may not be valid.
   */
  bool separatorAfterCurrentToken() const {
    return mSeparatorAfterCurrentToken;
  }

  /*
   * Returns true if there is any whitespace after the current token.
   */
  bool whitespaceAfterCurrentToken() const {
    return mWhitespaceAfterCurrentToken;
  }

  /**
   * Returns the next token.
   */
  const DependentSubstringType nextToken() {
    mozilla::RangedPtr<const CharType> tokenStart = mIter;
    mozilla::RangedPtr<const CharType> tokenEnd = mIter;

    MOZ_ASSERT(mIter == mEnd || !IsWhitespace(*mIter),
               "Should be at beginning of token if there is one");

    // Search until we hit separator or end (or whitespace, if a separator
    // isn't required -- see clause with 'break' below).
    while (mIter < mEnd && *mIter != mSeparatorChar) {
      // Skip to end of the current word.
      while (mIter < mEnd && !IsWhitespace(*mIter) &&
             *mIter != mSeparatorChar) {
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
      }  // (else, we'll keep looping until we hit mEnd or SeparatorChar)
    }

    mSeparatorAfterCurrentToken = (mIter != mEnd && *mIter == mSeparatorChar);
    MOZ_ASSERT(
        mSeparatorOptional || (mSeparatorAfterCurrentToken == (mIter < mEnd)),
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

  auto ToRange() const;

 private:
  mozilla::RangedPtr<const CharType> mIter;
  const mozilla::RangedPtr<const CharType> mEnd;
  const CharType mSeparatorChar;
  bool mWhitespaceBeforeFirstToken;
  bool mWhitespaceAfterCurrentToken;
  bool mSeparatorAfterCurrentToken;
  bool mSeparatorOptional;
};

constexpr bool NS_TokenizerIgnoreNothing(char16_t) { return false; }

template <bool IsWhitespace(char16_t), typename CharType>
using nsTCharSeparatedTokenizerTemplate =
    nsTCharSeparatedTokenizer<nsTDependentSubstring<CharType>, IsWhitespace>;

template <bool IsWhitespace(char16_t)>
using nsCharSeparatedTokenizerTemplate =
    nsTCharSeparatedTokenizerTemplate<IsWhitespace, char16_t>;

using nsCharSeparatedTokenizer =
    nsCharSeparatedTokenizerTemplate<NS_IsAsciiWhitespace>;

template <bool IsWhitespace(char16_t)>
using nsCCharSeparatedTokenizerTemplate =
    nsTCharSeparatedTokenizerTemplate<IsWhitespace, char>;

using nsCCharSeparatedTokenizer =
    nsCCharSeparatedTokenizerTemplate<NS_IsAsciiWhitespace>;

/**
 * Adapts a char separated tokenizer for use in a range-based for loop.
 *
 * Use this typically only indirectly, e.g. like
 *
 * for (const auto& token : nsCharSeparatedTokenizer(aText, ' ').ToRange()) {
 *    // ...
 * }
 */
template <typename Tokenizer>
class nsTokenizedRange {
 public:
  using DependentSubstringType = typename Tokenizer::DependentSubstringType;

  explicit nsTokenizedRange(Tokenizer&& aTokenizer)
      : mTokenizer(std::move(aTokenizer)) {}

  struct EndSentinel {};
  struct Iterator {
    explicit Iterator(const Tokenizer& aTokenizer) : mTokenizer(aTokenizer) {
      Next();
    }

    const DependentSubstringType& operator*() const { return *mCurrentToken; }

    Iterator& operator++() {
      Next();
      return *this;
    }

    bool operator==(const EndSentinel&) const {
      return mCurrentToken.isNothing();
    }

    bool operator!=(const EndSentinel&) const { return mCurrentToken.isSome(); }

   private:
    void Next() {
      mCurrentToken.reset();

      if (mTokenizer.hasMoreTokens()) {
        mCurrentToken.emplace(mTokenizer.nextToken());
      }
    }

    Tokenizer mTokenizer;
    mozilla::Maybe<DependentSubstringType> mCurrentToken;
  };

  auto begin() const { return Iterator{mTokenizer}; }
  auto end() const { return EndSentinel{}; }

 private:
  const Tokenizer mTokenizer;
};

template <typename TDependentSubstringType, bool IsWhitespace(char16_t)>
auto nsTCharSeparatedTokenizer<TDependentSubstringType, IsWhitespace>::ToRange()
    const {
  return nsTokenizedRange{nsTCharSeparatedTokenizer{*this}};
}

#endif /* __nsCharSeparatedTokenizer_h */
