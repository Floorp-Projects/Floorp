/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Tokenizer.h"

#include "nsUnicharUtils.h"

namespace mozilla {

static const char sWhitespaces[] = " \t";

Tokenizer::Tokenizer(const nsACString& aSource,
                     const char* aWhitespaces,
                     const char* aAdditionalWordChars)
  : mPastEof(false)
  , mHasFailed(false)
  , mWhitespaces(aWhitespaces ? aWhitespaces : sWhitespaces)
  , mAdditionalWordChars(aAdditionalWordChars)
{
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

  aToken.AssignFragment(mRollback, mCursor);

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

  aResult.AssignFragment(mRollback, mCursor);

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

bool
Tokenizer::HasFailed() const
{
  return mHasFailed;
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
  Record();
  nsACString::const_char_iterator rollback = mCursor;

  bool found = false;
  Token t;
  while (Next(t)) {
    if (aToken.Equals(t)) {
      found = true;
      break;
    }
  }

  Claim(aResult, aInclude);
  mRollback = rollback;
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
  aResult.Rebind(mRecord, close - mRecord);
}

// protected

bool
Tokenizer::HasInput() const
{
  return !mPastEof;
}

nsACString::const_char_iterator
Tokenizer::Parse(Token& aToken) const
{
  if (mCursor == mEnd) {
    aToken = Token::EndOfFile();
    return mEnd;
  }

  nsACString::const_char_iterator next = mCursor;

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
      if (IsEnd(next) || !IsWord(*next)) {
        aToken = Token::Word(Substring(mCursor, next));
        return next;
      }
      break;

    case PARSE_CRLF:
      ++next;
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

  return next;
}

bool
Tokenizer::IsEnd(const nsACString::const_char_iterator& caret) const
{
  return caret == mEnd;
}

bool
Tokenizer::IsWordFirst(const char aInput) const
{
  // TODO: make this fully work with unicode
  return (ToLowerCase(static_cast<uint32_t>(aInput)) !=
          ToUpperCase(static_cast<uint32_t>(aInput))) ||
          '_' == aInput ||
          (mAdditionalWordChars ? !!strchr(mAdditionalWordChars, aInput) : false);
}

bool
Tokenizer::IsWord(const char aInput) const
{
  return IsWordFirst(aInput) || IsNumber(aInput);
}

bool
Tokenizer::IsNumber(const char aInput) const
{
  // TODO: are there unicode numbers?
  return aInput >= '0' && aInput <= '9';
}

// Tokenizer::Token

Tokenizer::Token::Token(const Token& aOther)
  : mType(aOther.mType)
  , mChar(aOther.mChar)
  , mInteger(aOther.mInteger)
{
  if (mType == TOKEN_WORD) {
    mWord.Rebind(aOther.mWord.BeginReading(), aOther.mWord.Length());
  }
}

Tokenizer::Token&
Tokenizer::Token::operator=(const Token& aOther)
{
  mType = aOther.mType;
  mChar = aOther.mChar;
  mWord.Rebind(aOther.mWord.BeginReading(), aOther.mWord.Length());
  mInteger = aOther.mInteger;
  return *this;
}

void
Tokenizer::Token::AssignFragment(nsACString::const_char_iterator begin,
                                 nsACString::const_char_iterator end)
{
  mFragment.Rebind(begin, end - begin);
}

// static
Tokenizer::Token
Tokenizer::Token::Word(const nsACString& aValue)
{
  Token t;
  t.mType = TOKEN_WORD;
  t.mWord.Rebind(aValue.BeginReading(), aValue.Length());
  return t;
}

// static
Tokenizer::Token
Tokenizer::Token::Char(const char aValue)
{
  Token t;
  t.mType = TOKEN_CHAR;
  t.mChar = aValue;
  return t;
}

// static
Tokenizer::Token
Tokenizer::Token::Number(const uint64_t aValue)
{
  Token t;
  t.mType = TOKEN_INTEGER;
  t.mInteger = aValue;
  return t;
}

// static
Tokenizer::Token
Tokenizer::Token::Whitespace()
{
  Token t;
  t.mType = TOKEN_WS;
  t.mChar = '\0';
  return t;
}

// static
Tokenizer::Token
Tokenizer::Token::NewLine()
{
  Token t;
  t.mType = TOKEN_EOL;
  return t;
}

// static
Tokenizer::Token
Tokenizer::Token::EndOfFile()
{
  Token t;
  t.mType = TOKEN_EOF;
  return t;
}

// static
Tokenizer::Token
Tokenizer::Token::Error()
{
  Token t;
  t.mType = TOKEN_ERROR;
  return t;
}

bool
Tokenizer::Token::Equals(const Token& aOther) const
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
Tokenizer::Token::AsChar() const
{
  MOZ_ASSERT(mType == TOKEN_CHAR || mType == TOKEN_WS);
  return mChar;
}

nsDependentCSubstring
Tokenizer::Token::AsString() const
{
  MOZ_ASSERT(mType == TOKEN_WORD);
  return mWord;
}

uint64_t
Tokenizer::Token::AsInteger() const
{
  MOZ_ASSERT(mType == TOKEN_INTEGER);
  return mInteger;
}

} // mozilla
