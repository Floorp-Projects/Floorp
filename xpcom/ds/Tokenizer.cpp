/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Tokenizer.h"

#include "nsUnicharUtils.h"
#include "mozilla/CheckedInt.h"

namespace mozilla {

static const char sWhitespaces[] = " \t";

Tokenizer::Tokenizer(const nsACString& aSource)
  : mPastEof(false)
  , mHasFailed(false)
  , mWhitespaces(sWhitespaces)
{
  aSource.BeginReading(mCursor);
  mRecord = mRollback = mCursor;
  aSource.EndReading(mEnd);
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
  mPastEof = aResult.Type() == TOKEN_EOF;
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
  return true;
}

bool
Tokenizer::HasFailed() const
{
  return mHasFailed;
}

void
Tokenizer::SkipWhites()
{
  if (!CheckWhite()) {
    return;
  }

  nsACString::const_char_iterator rollback = mRollback;
  while (CheckWhite()) {
  }

  mHasFailed = false;
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
  } else if (*next == '\r') {
    state = PARSE_CRLF;
  } else if (*next == '\n') {
    state = PARSE_LF;
  } else if (strchr(mWhitespaces, *next)) { // not UTF-8 friendly?
    state = PARSE_WS;
  } else {
    state = PARSE_CHAR;
  }

  mozilla::CheckedInt64 resultingNumber = 0;

  while (next < mEnd) {
    switch (state) {
    case PARSE_INTEGER:
      // Keep it simple for now
      resultingNumber *= 10;
      resultingNumber += static_cast<int64_t>(*next - '0');

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
          '_' == aInput;
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

// static
Tokenizer::Token
Tokenizer::Token::Word(const nsACString& aValue)
{
  Token t;
  t.mType = TOKEN_WORD;
  t.mWord = aValue;
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
Tokenizer::Token::Number(const int64_t aValue)
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

nsCString
Tokenizer::Token::AsString() const
{
  MOZ_ASSERT(mType == TOKEN_WORD);
  return mWord;
}

int64_t
Tokenizer::Token::AsInteger() const
{
  MOZ_ASSERT(mType == TOKEN_INTEGER);
  return mInteger;
}

} // mozilla
