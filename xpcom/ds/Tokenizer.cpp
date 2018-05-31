/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Tokenizer.h"

#include "nsUnicharUtils.h"
#include <algorithm>

namespace mozilla {

template<>
char const TokenizerBase<char>::sWhitespaces[] = { ' ', '\t', 0 };
template<>
char16_t const TokenizerBase<char16_t>::sWhitespaces[3] = { ' ', '\t', 0 };

template<typename TChar>
static bool
contains(TChar const* const list, TChar const needle)
{
  for (TChar const *c = list; *c; ++c) {
    if (needle == *c) {
      return true;
    }
  }
  return false;
}

template<typename TChar>
TTokenizer<TChar>::TTokenizer(const typename base::TAString& aSource,
                              const TChar* aWhitespaces,
                              const TChar* aAdditionalWordChars)
  : TokenizerBase<TChar>(aWhitespaces, aAdditionalWordChars)
{
  base::mInputFinished = true;
  aSource.BeginReading(base::mCursor);
  mRecord = mRollback = base::mCursor;
  aSource.EndReading(base::mEnd);
}

template<typename TChar>
TTokenizer<TChar>::TTokenizer(const TChar* aSource,
                              const TChar* aWhitespaces,
                              const TChar* aAdditionalWordChars)
  : TTokenizer(typename base::TDependentString(aSource), aWhitespaces, aAdditionalWordChars)
{
}

template<typename TChar>
bool
TTokenizer<TChar>::Next(typename base::Token& aToken)
{
  if (!base::HasInput()) {
    base::mHasFailed = true;
    return false;
  }

  mRollback = base::mCursor;
  base::mCursor = base::Parse(aToken);

  base::AssignFragment(aToken, mRollback, base::mCursor);

  base::mPastEof = aToken.Type() == base::TOKEN_EOF;
  base::mHasFailed = false;
  return true;
}

template<typename TChar>
bool
TTokenizer<TChar>::Check(const typename base::TokenType aTokenType, typename base::Token& aResult)
{
  if (!base::HasInput()) {
    base::mHasFailed = true;
    return false;
  }

  typename base::TAString::const_char_iterator next = base::Parse(aResult);
  if (aTokenType != aResult.Type()) {
    base::mHasFailed = true;
    return false;
  }

  mRollback = base::mCursor;
  base::mCursor = next;

  base::AssignFragment(aResult, mRollback, base::mCursor);

  base::mPastEof = aResult.Type() == base::TOKEN_EOF;
  base::mHasFailed = false;
  return true;
}

template<typename TChar>
bool
TTokenizer<TChar>::Check(const typename base::Token& aToken)
{
  if (!base::HasInput()) {
    base::mHasFailed = true;
    return false;
  }

  typename base::Token parsed;
  typename base::TAString::const_char_iterator next = base::Parse(parsed);
  if (!aToken.Equals(parsed)) {
    base::mHasFailed = true;
    return false;
  }

  mRollback = base::mCursor;
  base::mCursor = next;
  base::mPastEof = parsed.Type() == base::TOKEN_EOF;
  base::mHasFailed = false;
  return true;
}

template<typename TChar>
void
TTokenizer<TChar>::SkipWhites(WhiteSkipping aIncludeNewLines)
{
  if (!CheckWhite() && (aIncludeNewLines == DONT_INCLUDE_NEW_LINE || !CheckEOL())) {
    return;
  }

  typename base::TAString::const_char_iterator rollback = mRollback;
  while (CheckWhite() || (aIncludeNewLines == INCLUDE_NEW_LINE && CheckEOL())) {
  }

  base::mHasFailed = false;
  mRollback = rollback;
}

template<typename TChar>
void
TTokenizer<TChar>::SkipUntil(typename base::Token const& aToken)
{
  typename base::TAString::const_char_iterator rollback = base::mCursor;
  const typename base::Token eof = base::Token::EndOfFile();

  typename base::Token t;
  while (Next(t)) {
    if (aToken.Equals(t) || eof.Equals(t)) {
      Rollback();
      break;
    }
  }

  mRollback = rollback;
}

template<typename TChar>
bool
TTokenizer<TChar>::CheckChar(bool (*aClassifier)(const TChar aChar))
{
  if (!aClassifier) {
    MOZ_ASSERT(false);
    return false;
  }

  if (!base::HasInput() || base::mCursor == base::mEnd) {
    base::mHasFailed = true;
    return false;
  }

  if (!aClassifier(*base::mCursor)) {
    base::mHasFailed = true;
    return false;
  }

  mRollback = base::mCursor;
  ++base::mCursor;
  base::mHasFailed = false;
  return true;
}

template<typename TChar>
bool
TTokenizer<TChar>::ReadChar(TChar* aValue)
{
  MOZ_RELEASE_ASSERT(aValue);

  typename base::Token t;
  if (!Check(base::TOKEN_CHAR, t)) {
    return false;
  }

  *aValue = t.AsChar();
  return true;
}

template<typename TChar>
bool
TTokenizer<TChar>::ReadChar(bool (*aClassifier)(const TChar aChar), TChar* aValue)
{
  MOZ_RELEASE_ASSERT(aValue);

  if (!CheckChar(aClassifier)) {
    return false;
  }

  *aValue = *mRollback;
  return true;
}

template<typename TChar>
bool
TTokenizer<TChar>::ReadWord(typename base::TAString& aValue)
{
  typename base::Token t;
  if (!Check(base::TOKEN_WORD, t)) {
    return false;
  }

  aValue.Assign(t.AsString());
  return true;
}

template<typename TChar>
bool
TTokenizer<TChar>::ReadWord(typename base::TDependentSubstring& aValue)
{
  typename base::Token t;
  if (!Check(base::TOKEN_WORD, t)) {
    return false;
  }

  aValue.Rebind(t.AsString().BeginReading(), t.AsString().Length());
  return true;
}

template<typename TChar>
bool
TTokenizer<TChar>::ReadUntil(typename base::Token const& aToken, typename base::TAString& aResult, ClaimInclusion aInclude)
{
  typename base::TDependentSubstring substring;
  bool rv = ReadUntil(aToken, substring, aInclude);
  aResult.Assign(substring);
  return rv;
}

template<typename TChar>
bool
TTokenizer<TChar>::ReadUntil(typename base::Token const& aToken, typename base::TDependentSubstring& aResult, ClaimInclusion aInclude)
{
  typename base::TAString::const_char_iterator record = mRecord;
  Record();
  typename base::TAString::const_char_iterator rollback = mRollback = base::mCursor;

  bool found = false;
  typename base::Token t;
  while (Next(t)) {
    if (aToken.Equals(t)) {
      found = true;
      break;
    }
    if (t.Equals(base::Token::EndOfFile())) {
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

template<typename TChar>
void
TTokenizer<TChar>::Rollback()
{
  MOZ_ASSERT(base::mCursor > mRollback || base::mPastEof, "TODO!!!");

  base::mPastEof = false;
  base::mHasFailed = false;
  base::mCursor = mRollback;
}

template<typename TChar>
void
TTokenizer<TChar>::Record(ClaimInclusion aInclude)
{
  mRecord = aInclude == INCLUDE_LAST
    ? mRollback
    : base::mCursor;
}

template<typename TChar>
void
TTokenizer<TChar>::Claim(typename base::TAString& aResult, ClaimInclusion aInclusion)
{
  typename base::TAString::const_char_iterator close = aInclusion == EXCLUDE_LAST
    ? mRollback
    : base::mCursor;
  aResult.Assign(Substring(mRecord, close));
}

template<typename TChar>
void
TTokenizer<TChar>::Claim(typename base::TDependentSubstring& aResult, ClaimInclusion aInclusion)
{
  typename base::TAString::const_char_iterator close = aInclusion == EXCLUDE_LAST
    ? mRollback
    : base::mCursor;

  MOZ_RELEASE_ASSERT(close >= mRecord, "Overflow!");
  aResult.Rebind(mRecord, close - mRecord);
}

// TokenizerBase

template<typename TChar>
TokenizerBase<TChar>::TokenizerBase(const TChar* aWhitespaces,
                                    const TChar* aAdditionalWordChars)
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

template<typename TChar>
auto
TokenizerBase<TChar>::AddCustomToken(const TAString & aValue,
                                     ECaseSensitivity aCaseInsensitivity, bool aEnabled)
  -> Token
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

template<typename TChar>
void
TokenizerBase<TChar>::RemoveCustomToken(Token& aToken)
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

template<typename TChar>
void
TokenizerBase<TChar>::EnableCustomToken(Token const& aToken, bool aEnabled)
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

template<typename TChar>
void
TokenizerBase<TChar>::SetTokenizingMode(Mode aMode)
{
  mMode = aMode;
}

template<typename TChar>
bool
TokenizerBase<TChar>::HasFailed() const
{
  return mHasFailed;
}

template<typename TChar>
bool
TokenizerBase<TChar>::HasInput() const
{
  return !mPastEof;
}

template<typename TChar>
auto
TokenizerBase<TChar>::Parse(Token& aToken) const
  -> typename TAString::const_char_iterator
{
  if (mCursor == mEnd) {
    if (!mInputFinished) {
      return mCursor;
    }

    aToken = Token::EndOfFile();
    return mEnd;
  }

  MOZ_RELEASE_ASSERT(mEnd >= mCursor, "Overflow!");
  typename TAString::size_type available = mEnd - mCursor;

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

  typename TAString::const_char_iterator next = mCursor;

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
  } else if (contains(mWhitespaces, *next)) { // not UTF-8 friendly?
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

template<typename TChar>
bool
TokenizerBase<TChar>::IsEnd(const typename TAString::const_char_iterator& caret) const
{
  return caret == mEnd;
}

template<typename TChar>
bool
TokenizerBase<TChar>::IsPending(const typename TAString::const_char_iterator& caret) const
{
  return IsEnd(caret) && !mInputFinished;
}

template<typename TChar>
bool
TokenizerBase<TChar>::IsWordFirst(const TChar aInput) const
{
  // TODO: make this fully work with unicode
  return (ToLowerCase(static_cast<uint32_t>(aInput)) !=
          ToUpperCase(static_cast<uint32_t>(aInput))) ||
          '_' == aInput ||
          (mAdditionalWordChars ? contains(mAdditionalWordChars, aInput) : false);
}

template<typename TChar>
bool
TokenizerBase<TChar>::IsWord(const TChar aInput) const
{
  return IsWordFirst(aInput) || IsNumber(aInput);
}

template<typename TChar>
bool
TokenizerBase<TChar>::IsNumber(const TChar aInput) const
{
  // TODO: are there unicode numbers?
  return aInput >= '0' && aInput <= '9';
}

namespace {

template<typename TChar> class TCharComparator;
template<> class TCharComparator<char> final : public nsCaseInsensitiveUTF8StringComparator {};
template<> class TCharComparator<char16_t> final : public nsCaseInsensitiveStringComparator {};

}

template<typename TChar>
bool
TokenizerBase<TChar>::IsCustom(const typename TAString::const_char_iterator & caret,
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

  TDependentSubstring inputFragment(caret, aCustomToken.mCustom.Length());
  if (aCustomToken.mCustomCaseInsensitivity == CASE_INSENSITIVE) {
    return inputFragment.Equals(aCustomToken.mCustom, TCharComparator<TChar>());
  }
  return inputFragment.Equals(aCustomToken.mCustom);
}

template<typename TChar>
void
TokenizerBase<TChar>::AssignFragment(Token& aToken,
                                     typename TAString::const_char_iterator begin,
                                     typename TAString::const_char_iterator end)
{
  aToken.AssignFragment(begin, end);
}

// TokenizerBase::Token

template<typename TChar>
TokenizerBase<TChar>::Token::Token()
  : mType(TOKEN_UNKNOWN)
  , mChar(0)
  , mInteger(0)
  , mCustomCaseInsensitivity(CASE_SENSITIVE)
  , mCustomEnabled(false)
{
}

template<typename TChar>
TokenizerBase<TChar>::Token::Token(const Token& aOther)
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

template<typename TChar>
auto
TokenizerBase<TChar>::Token::operator=(const Token& aOther)
  -> Token&
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

template<typename TChar>
void
TokenizerBase<TChar>::Token::AssignFragment(typename TAString::const_char_iterator begin,
                                     typename TAString::const_char_iterator end)
{
  MOZ_RELEASE_ASSERT(end >= begin, "Overflow!");
  mFragment.Rebind(begin, end - begin);
}

// static
template<typename TChar>
auto
TokenizerBase<TChar>::Token::Raw() -> Token
{
  Token t;
  t.mType = TOKEN_RAW;
  return t;
}

// static
template<typename TChar>
auto
TokenizerBase<TChar>::Token::Word(TAString const& aValue) -> Token
{
  Token t;
  t.mType = TOKEN_WORD;
  t.mWord.Rebind(aValue.BeginReading(), aValue.Length());
  return t;
}

// static
template<typename TChar>
auto
TokenizerBase<TChar>::Token::Char(TChar const aValue) -> Token
{
  Token t;
  t.mType = TOKEN_CHAR;
  t.mChar = aValue;
  return t;
}

// static
template<typename TChar>
auto
TokenizerBase<TChar>::Token::Number(uint64_t const aValue) -> Token
{
  Token t;
  t.mType = TOKEN_INTEGER;
  t.mInteger = aValue;
  return t;
}

// static
template<typename TChar>
auto
TokenizerBase<TChar>::Token::Whitespace() -> Token
{
  Token t;
  t.mType = TOKEN_WS;
  t.mChar = '\0';
  return t;
}

// static
template<typename TChar>
auto
TokenizerBase<TChar>::Token::NewLine() -> Token
{
  Token t;
  t.mType = TOKEN_EOL;
  return t;
}

// static
template<typename TChar>
auto
TokenizerBase<TChar>::Token::EndOfFile() -> Token
{
  Token t;
  t.mType = TOKEN_EOF;
  return t;
}

// static
template<typename TChar>
auto
TokenizerBase<TChar>::Token::Error() -> Token
{
  Token t;
  t.mType = TOKEN_ERROR;
  return t;
}

template<typename TChar>
bool
TokenizerBase<TChar>::Token::Equals(const Token& aOther) const
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

template<typename TChar>
TChar
TokenizerBase<TChar>::Token::AsChar() const
{
  MOZ_ASSERT(mType == TOKEN_CHAR || mType == TOKEN_WS);
  return mChar;
}

template<typename TChar>
auto
TokenizerBase<TChar>::Token::AsString() const -> TDependentSubstring
{
  MOZ_ASSERT(mType == TOKEN_WORD);
  return mWord;
}

template<typename TChar>
uint64_t
TokenizerBase<TChar>::Token::AsInteger() const
{
  MOZ_ASSERT(mType == TOKEN_INTEGER);
  return mInteger;
}

template class TokenizerBase<char>;
template class TokenizerBase<char16_t>;

template class TTokenizer<char>;
template class TTokenizer<char16_t>;

} // mozilla
