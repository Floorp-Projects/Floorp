/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Tokenizer.h"

#include "nsUnicharUtils.h"
#include <algorithm>

namespace mozilla {

static const char sWhitespaces[] = " \t";

Tokenizer::Tokenizer(const nsACString& aSource,
                     const char* aWhitespaces,
                     const char* aAdditionalWordChars)
  : TokenizerBase(aWhitespaces, aAdditionalWordChars)
{
  mInputFinished = true;
  aSource.BeginReading(mCursor);
  mRecord = mRollback = mCursor;
  aSource.EndReading(mEnd);
}

Tokenizer::Tokenizer(const char* aSource,
                     const char* aWhitespaces,
                     const char* aAdditionalWordChars)
  : Tokenizer(nsDependentCString(aSource), aWhitespaces, aAdditionalWordChars)
{
}

bool
Tokenizer::Next(Token& aToken)
{
  if (!HasInput()) {
    mHasFailed = true;
    return false;
  }

  mRollback = mCursor;
  mCursor = Parse(aToken);

  AssignFragment(aToken, mRollback, mCursor);

  mPastEof = aToken.Type() == TOKEN_EOF;
  mHasFailed = false;
  return true;
}

bool
Tokenizer::Check(const TokenType aTokenType, Token& aResult)
{
  if (!HasInput()) {
    mHasFailed = true;
    return false;
  }

  nsACString::const_char_iterator next = Parse(aResult);
  if (aTokenType != aResult.Type()) {
    mHasFailed = true;
    return false;
  }

  mRollback = mCursor;
  mCursor = next;

  AssignFragment(aResult, mRollback, mCursor);

  mPastEof = aResult.Type() == TOKEN_EOF;
  mHasFailed = false;
  return true;
}

bool
Tokenizer::Check(const Token& aToken)
{
  if (!HasInput()) {
    mHasFailed = true;
    return false;
  }

  Token parsed;
  nsACString::const_char_iterator next = Parse(parsed);
  if (!aToken.Equals(parsed)) {
    mHasFailed = true;
    return false;
  }

  mRollback = mCursor;
  mCursor = next;
  mPastEof = parsed.Type() == TOKEN_EOF;
  mHasFailed = false;
  return true;
}

void
Tokenizer::SkipWhites(WhiteSkipping aIncludeNewLines)
{
  if (!CheckWhite() && (aIncludeNewLines == DONT_INCLUDE_NEW_LINE || !CheckEOL())) {
    return;
  }

  nsACString::const_char_iterator rollback = mRollback;
  while (CheckWhite() || (aIncludeNewLines == INCLUDE_NEW_LINE && CheckEOL())) {
  }

  mHasFailed = false;
  mRollback = rollback;
}

void
Tokenizer::SkipUntil(Token const& aToken)
{
  nsACString::const_char_iterator rollback = mCursor;
  const Token eof = Token::EndOfFile();

  Token t;
  while (Next(t)) {
    if (aToken.Equals(t) || eof.Equals(t)) {
      Rollback();
      break;
    }
  }

  mRollback = rollback;
}

bool
Tokenizer::CheckChar(bool (*aClassifier)(const char aChar))
{
  if (!aClassifier) {
    MOZ_ASSERT(false);
    return false;
  }

  if (!HasInput() || mCursor == mEnd) {
    mHasFailed = true;
    return false;
  }

  if (!aClassifier(*mCursor)) {
    mHasFailed = true;
    return false;
  }

  mRollback = mCursor;
  ++mCursor;
  mHasFailed = false;
  return true;
}

bool
Tokenizer::ReadChar(char* aValue)
{
  MOZ_RELEASE_ASSERT(aValue);

  Token t;
  if (!Check(TOKEN_CHAR, t)) {
    return false;
  }

  *aValue = t.AsChar();
  return true;
}

bool
Tokenizer::ReadChar(bool (*aClassifier)(const char aChar), char* aValue)
{
  MOZ_RELEASE_ASSERT(aValue);

  if (!CheckChar(aClassifier)) {
    return false;
  }

  *aValue = *mRollback;
  return true;
}

bool
Tokenizer::ReadWord(nsACString& aValue)
{
  Token t;
  if (!Check(TOKEN_WORD, t)) {
    return false;
  }

  aValue.Assign(t.AsString());
  return true;
}

bool
Tokenizer::ReadWord(nsDependentCSubstring& aValue)
{
  Token t;
  if (!Check(TOKEN_WORD, t)) {
    return false;
  }

  aValue.Rebind(t.AsString().BeginReading(), t.AsString().Length());
  return true;
}

bool
Tokenizer::ReadUntil(Token const& aToken, nsACString& aResult, ClaimInclusion aInclude)
{
  nsDependentCSubstring substring;
  bool rv = ReadUntil(aToken, substring, aInclude);
  aResult.Assign(substring);
  return rv;
}

bool
Tokenizer::ReadUntil(Token const& aToken, nsDependentCSubstring& aResult, ClaimInclusion aInclude)
{
  nsACString::const_char_iterator record = mRecord;
  Record();
  nsACString::const_char_iterator rollback = mRollback = mCursor;

  bool found = false;
  Token t;
  while (Next(t)) {
    if (aToken.Equals(t)) {
      found = true;
      break;
    }
    if (t.Equals(Token::EndOfFile())) {
      // We don't want to eat it.
      Rollback();
      break;
    }
  }

  Claim(aResult, aInclude);
  mRollback = rollback;
  mRecord = record;
  return found;
}

void
Tokenizer::Rollback()
{
  MOZ_ASSERT(mCursor > mRollback || mPastEof,
             "Tokenizer::Rollback() cannot use twice or before any parsing");

  mPastEof = false;
  mHasFailed = false;
  mCursor = mRollback;
}

void
Tokenizer::Record(ClaimInclusion aInclude)
{
  mRecord = aInclude == INCLUDE_LAST
    ? mRollback
    : mCursor;
}

void
Tokenizer::Claim(nsACString& aResult, ClaimInclusion aInclusion)
{
  nsACString::const_char_iterator close = aInclusion == EXCLUDE_LAST
    ? mRollback
    : mCursor;
  aResult.Assign(Substring(mRecord, close));
}

void
Tokenizer::Claim(nsDependentCSubstring& aResult, ClaimInclusion aInclusion)
{
  nsACString::const_char_iterator close = aInclusion == EXCLUDE_LAST
    ? mRollback
    : mCursor;

  MOZ_RELEASE_ASSERT(close >= mRecord, "Overflow!");
  aResult.Rebind(mRecord, close - mRecord);
}

// TokenizerBase

TokenizerBase::TokenizerBase(const char* aWhitespaces,
                             const char* aAdditionalWordChars)
  : mPastEof(false)
  , mHasFailed(false)
  , mInputFinished(true)
  , mMode(Mode::FULL)
  , mMinRawDelivery(1024)
  , mWhitespaces(aWhitespaces ? aWhitespaces : sWhitespaces)
  , mAdditionalWordChars(aAdditionalWordChars)
  , mCursor(nullptr)
  , mEnd(nullptr)
  , mNextCustomTokenID(TOKEN_CUSTOM0)
{
}

TokenizerBase::Token
TokenizerBase::AddCustomToken(const nsACString & aValue,
                              ECaseSensitivity aCaseInsensitivity, bool aEnabled)
{
  MOZ_ASSERT(!aValue.IsEmpty());

  UniquePtr<Token>& t = *mCustomTokens.AppendElement();
  t = MakeUnique<Token>();

  t->mType = static_cast<TokenType>(++mNextCustomTokenID);
  t->mCustomCaseInsensitivity = aCaseInsensitivity;
  t->mCustomEnabled = aEnabled;
  t->mCustom.Assign(aValue);
  return *t;
}

void
TokenizerBase::RemoveCustomToken(Token& aToken)
{
  if (aToken.mType == TOKEN_UNKNOWN) {
    // Already removed
    return;
  }

  for (UniquePtr<Token> const& custom : mCustomTokens) {
    if (custom->mType == aToken.mType) {
      mCustomTokens.RemoveElement(custom);
      aToken.mType = TOKEN_UNKNOWN;
      return;
    }
  }

  MOZ_ASSERT(false, "Token to remove not found");
}

void
TokenizerBase::EnableCustomToken(Token const& aToken, bool aEnabled)
{
  if (aToken.mType == TOKEN_UNKNOWN) {
    // Already removed
    return;
  }

  for (UniquePtr<Token> const& custom : mCustomTokens) {
    if (custom->Type() == aToken.Type()) {
      // This effectively destroys the token instance.
      custom->mCustomEnabled = aEnabled;
      return;
    }
  }

  MOZ_ASSERT(false, "Token to change not found");
}

void
TokenizerBase::SetTokenizingMode(Mode aMode)
{
  mMode = aMode;
}

bool
TokenizerBase::HasFailed() const
{
  return mHasFailed;
}

bool
TokenizerBase::HasInput() const
{
  return !mPastEof;
}

nsACString::const_char_iterator
TokenizerBase::Parse(Token& aToken) const
{
  if (mCursor == mEnd) {
    if (!mInputFinished) {
      return mCursor;
    }

    aToken = Token::EndOfFile();
    return mEnd;
  }

  MOZ_RELEASE_ASSERT(mEnd >= mCursor, "Overflow!");
  nsACString::size_type available = mEnd - mCursor;

  uint32_t longestCustom = 0;
  for (UniquePtr<Token> const& custom : mCustomTokens) {
    if (IsCustom(mCursor, *custom, &longestCustom)) {
      aToken = *custom;
      return mCursor + custom->mCustom.Length();
    }
  }

  if (!mInputFinished && available < longestCustom) {
    // Not enough data to deterministically decide.
    return mCursor;
  }

  nsACString::const_char_iterator next = mCursor;

  if (mMode == Mode::CUSTOM_ONLY) {
    // We have to do a brute-force search for all of the enabled custom
    // tokens.
    while (next < mEnd) {
      ++next;
      for (UniquePtr<Token> const& custom : mCustomTokens) {
        if (IsCustom(next, *custom)) {
          aToken = Token::Raw();
          return next;
        }
      }
    }

    if (mInputFinished) {
      // End of the data reached.
      aToken = Token::Raw();
      return next;
    }

    if (longestCustom < available && available > mMinRawDelivery) {
      // We can return some data w/o waiting for either a custom token
      // or call to FinishData() when we leave the tail where all the
      // custom tokens potentially fit, so we can't lose only partially
      // delivered tokens.  This preserves reasonable granularity.
      aToken = Token::Raw();
      return mEnd - longestCustom + 1;
    }

    // Not enough data to deterministically decide.
    return mCursor;
  }

  enum State {
    PARSE_INTEGER,
    PARSE_WORD,
    PARSE_CRLF,
    PARSE_LF,
    PARSE_WS,
    PARSE_CHAR,
  } state;

  if (IsWordFirst(*next)) {
    state = PARSE_WORD;
  } else if (IsNumber(*next)) {
    state = PARSE_INTEGER;
  } else if (strchr(mWhitespaces, *next)) { // not UTF-8 friendly?
    state = PARSE_WS;
  } else if (*next == '\r') {
    state = PARSE_CRLF;
  } else if (*next == '\n') {
    state = PARSE_LF;
  } else {
    state = PARSE_CHAR;
  }

  mozilla::CheckedUint64 resultingNumber = 0;

  while (next < mEnd) {
    switch (state) {
    case PARSE_INTEGER:
      // Keep it simple for now
      resultingNumber *= 10;
      resultingNumber += static_cast<uint64_t>(*next - '0');

      ++next;
      if (IsPending(next)) {
        break;
      }
      if (IsEnd(next) || !IsNumber(*next)) {
        if (!resultingNumber.isValid()) {
          aToken = Token::Error();
        } else {
          aToken = Token::Number(resultingNumber.value());
        }
        return next;
      }
      break;

    case PARSE_WORD:
      ++next;
      if (IsPending(next)) {
        break;
      }
      if (IsEnd(next) || !IsWord(*next)) {
        aToken = Token::Word(Substring(mCursor, next));
        return next;
      }
      break;

    case PARSE_CRLF:
      ++next;
      if (IsPending(next)) {
        break;
      }
      if (!IsEnd(next) && *next == '\n') { // LF is optional
        ++next;
      }
      aToken = Token::NewLine();
      return next;

    case PARSE_LF:
      ++next;
      aToken = Token::NewLine();
      return next;

    case PARSE_WS:
      ++next;
      aToken = Token::Whitespace();
      return next;

    case PARSE_CHAR:
      ++next;
      aToken = Token::Char(*mCursor);
      return next;
    } // switch (state)
  } // while (next < end)

  MOZ_ASSERT(!mInputFinished);
  return mCursor;
}

bool
TokenizerBase::IsEnd(const nsACString::const_char_iterator& caret) const
{
  return caret == mEnd;
}

bool
TokenizerBase::IsPending(const nsACString::const_char_iterator& caret) const
{
  return IsEnd(caret) && !mInputFinished;
}

bool
TokenizerBase::IsWordFirst(const char aInput) const
{
  // TODO: make this fully work with unicode
  return (ToLowerCase(static_cast<uint32_t>(aInput)) !=
          ToUpperCase(static_cast<uint32_t>(aInput))) ||
          '_' == aInput ||
          (mAdditionalWordChars ? !!strchr(mAdditionalWordChars, aInput) : false);
}

bool
TokenizerBase::IsWord(const char aInput) const
{
  return IsWordFirst(aInput) || IsNumber(aInput);
}

bool
TokenizerBase::IsNumber(const char aInput) const
{
  // TODO: are there unicode numbers?
  return aInput >= '0' && aInput <= '9';
}

bool
TokenizerBase::IsCustom(const nsACString::const_char_iterator & caret,
                        const Token & aCustomToken,
                        uint32_t * aLongest) const
{
  MOZ_ASSERT(aCustomToken.mType > TOKEN_CUSTOM0);
  if (!aCustomToken.mCustomEnabled) {
    return false;
  }

  if (aLongest) {
    *aLongest = std::max(*aLongest, aCustomToken.mCustom.Length());
  }

  // This is not very likely to happen according to how we call this method
  // and since it's on a hot path, it's just a diagnostic assert,
  // not a release assert.
  MOZ_DIAGNOSTIC_ASSERT(mEnd >= caret, "Overflow?");
  uint32_t inputLength = mEnd - caret;
  if (aCustomToken.mCustom.Length() > inputLength) {
    return false;
  }

  nsDependentCSubstring inputFragment(caret, aCustomToken.mCustom.Length());
  if (aCustomToken.mCustomCaseInsensitivity == CASE_INSENSITIVE) {
    return inputFragment.Equals(aCustomToken.mCustom, nsCaseInsensitiveUTF8StringComparator());
  }
  return inputFragment.Equals(aCustomToken.mCustom);
}

void TokenizerBase::AssignFragment(Token& aToken,
                                   nsACString::const_char_iterator begin,
                                   nsACString::const_char_iterator end)
{
  aToken.AssignFragment(begin, end);
}

// TokenizerBase::Token

TokenizerBase::Token::Token()
  : mType(TOKEN_UNKNOWN)
  , mChar(0)
  , mInteger(0)
  , mCustomCaseInsensitivity(CASE_SENSITIVE)
  , mCustomEnabled(false)
{
}

TokenizerBase::Token::Token(const Token& aOther)
  : mType(aOther.mType)
  , mCustom(aOther.mCustom)
  , mChar(aOther.mChar)
  , mInteger(aOther.mInteger)
  , mCustomCaseInsensitivity(aOther.mCustomCaseInsensitivity)
  , mCustomEnabled(aOther.mCustomEnabled)
{
  if (mType == TOKEN_WORD || mType > TOKEN_CUSTOM0) {
    mWord.Rebind(aOther.mWord.BeginReading(), aOther.mWord.Length());
  }
}

TokenizerBase::Token&
TokenizerBase::Token::operator=(const Token& aOther)
{
  mType = aOther.mType;
  mCustom = aOther.mCustom;
  mChar = aOther.mChar;
  mWord.Rebind(aOther.mWord.BeginReading(), aOther.mWord.Length());
  mInteger = aOther.mInteger;
  mCustomCaseInsensitivity = aOther.mCustomCaseInsensitivity;
  mCustomEnabled = aOther.mCustomEnabled;
  return *this;
}

void
TokenizerBase::Token::AssignFragment(nsACString::const_char_iterator begin,
                                     nsACString::const_char_iterator end)
{
  MOZ_RELEASE_ASSERT(end >= begin, "Overflow!");
  mFragment.Rebind(begin, end - begin);
}

// static
TokenizerBase::Token
TokenizerBase::Token::Raw()
{
  Token t;
  t.mType = TOKEN_RAW;
  return t;
}

// static
TokenizerBase::Token
TokenizerBase::Token::Word(const nsACString& aValue)
{
  Token t;
  t.mType = TOKEN_WORD;
  t.mWord.Rebind(aValue.BeginReading(), aValue.Length());
  return t;
}

// static
TokenizerBase::Token
TokenizerBase::Token::Char(const char aValue)
{
  Token t;
  t.mType = TOKEN_CHAR;
  t.mChar = aValue;
  return t;
}

// static
TokenizerBase::Token
TokenizerBase::Token::Number(const uint64_t aValue)
{
  Token t;
  t.mType = TOKEN_INTEGER;
  t.mInteger = aValue;
  return t;
}

// static
TokenizerBase::Token
TokenizerBase::Token::Whitespace()
{
  Token t;
  t.mType = TOKEN_WS;
  t.mChar = '\0';
  return t;
}

// static
TokenizerBase::Token
TokenizerBase::Token::NewLine()
{
  Token t;
  t.mType = TOKEN_EOL;
  return t;
}

// static
TokenizerBase::Token
TokenizerBase::Token::EndOfFile()
{
  Token t;
  t.mType = TOKEN_EOF;
  return t;
}

// static
TokenizerBase::Token
TokenizerBase::Token::Error()
{
  Token t;
  t.mType = TOKEN_ERROR;
  return t;
}

bool
TokenizerBase::Token::Equals(const Token& aOther) const
{
  if (mType != aOther.mType) {
    return false;
  }

  switch (mType) {
  case TOKEN_INTEGER:
    return AsInteger() == aOther.AsInteger();
  case TOKEN_WORD:
    return AsString() == aOther.AsString();
  case TOKEN_CHAR:
    return AsChar() == aOther.AsChar();
  default:
    return true;
  }
}

char
TokenizerBase::Token::AsChar() const
{
  MOZ_ASSERT(mType == TOKEN_CHAR || mType == TOKEN_WS);
  return mChar;
}

nsDependentCSubstring
TokenizerBase::Token::AsString() const
{
  MOZ_ASSERT(mType == TOKEN_WORD);
  return mWord;
}

uint64_t
TokenizerBase::Token::AsInteger() const
{
  MOZ_ASSERT(mType == TOKEN_INTEGER);
  return mInteger;
}

} // mozilla
