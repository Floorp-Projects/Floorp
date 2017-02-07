/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Tokenizer_h__
#define Tokenizer_h__

#include "nsString.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/UniquePtr.h"
#include "nsTArray.h"

namespace mozilla {

class TokenizerBase
{
public:
  /**
   * The analyzer works with elements in the input cut to a sequence of token
   * where each token has an elementary type
   */
  enum TokenType : uint32_t
  {
    TOKEN_UNKNOWN,
    TOKEN_RAW,
    TOKEN_ERROR,
    TOKEN_INTEGER,
    TOKEN_WORD,
    TOKEN_CHAR,
    TOKEN_WS,
    TOKEN_EOL,
    TOKEN_EOF,
    TOKEN_CUSTOM0 = 1000
  };

  enum ECaseSensitivity
  {
    CASE_SENSITIVE,
    CASE_INSENSITIVE
  };

  /**
   * Class holding the type and the value of a token.  It can be manually created
   * to allow checks against it via methods of Tokenizer or are results of some of
   * the Tokenizer's methods.
   */
  class Token
  {
    TokenType mType;
    nsDependentCSubstring mWord;
    nsCString mCustom;
    char mChar;
    uint64_t mInteger;
    ECaseSensitivity mCustomCaseInsensitivity;
    bool mCustomEnabled;

    // If this token is a result of the parsing process, this member is referencing
    // a sub-string in the input buffer.  If this is externally created Token this
    // member is left an empty string.
    nsDependentCSubstring mFragment;

    friend class TokenizerBase;
    void AssignFragment(nsACString::const_char_iterator begin,
                        nsACString::const_char_iterator end);

    static Token Raw();

  public:
    Token();
    Token(const Token& aOther);
    Token& operator=(const Token& aOther);

    // Static constructors of tokens by type and value
    static Token Word(const nsACString& aWord);
    static Token Char(const char aChar);
    static Token Number(const uint64_t aNumber);
    static Token Whitespace();
    static Token NewLine();
    static Token EndOfFile();
    static Token Error();

    // Compares the two tokens, type must be identical and value
    // of one of the tokens must be 'any' or equal.
    bool Equals(const Token& aOther) const;

    TokenType Type() const { return mType; }
    char AsChar() const;
    nsDependentCSubstring AsString() const;
    uint64_t AsInteger() const;

    nsDependentCSubstring Fragment() const { return mFragment; }
  };

  /**
   * Consumers may register a custom string that, when found in the input, is considered
   * a token and returned by Next*() and accepted by Check*() methods.
   * AddCustomToken() returns a reference to a token that can then be comapred using
   * Token::Equals() againts the output from Next*() or be passed to Check*().
   */
  Token AddCustomToken(const nsACString& aValue, ECaseSensitivity aCaseInsensitivity, bool aEnabled = true);
  template <uint32_t N>
  Token AddCustomToken(const char(&aValue)[N], ECaseSensitivity aCaseInsensitivity, bool aEnabled = true)
  {
    return AddCustomToken(nsDependentCSubstring(aValue, N - 1), aCaseInsensitivity, aEnabled);
  }
  void RemoveCustomToken(Token& aToken);
  /**
   * Only applies to a custom type of a Token (see AddCustomToken above.)
   * This turns on and off token recognition.  When a custom token is disabled,
   * it's ignored as never added as a custom token.
   */
  void EnableCustomToken(Token const& aToken, bool aEnable);

  /**
   * Mode of tokenization.
   * FULL tokenization, the default, recognizes built-in tokens and any custom tokens,
   * if added.
   * CUSTOM_ONLY will only recognize custom tokens, the rest is seen as 'raw'.
   * This mode can be understood as a 'binary' mode.
   */
  enum class Mode
  {
    FULL,
    CUSTOM_ONLY
  };
  void SetTokenizingMode(Mode aMode);

  /**
   * Return false iff the last Check*() call has returned false or when we've read past
   * the end of the input string.
   */
  MOZ_MUST_USE bool HasFailed() const;

protected:
  explicit TokenizerBase(const char* aWhitespaces = nullptr,
                         const char* aAdditionalWordChars = nullptr);

  // false if we have already read the EOF token.
  bool HasInput() const;
  // Main parsing function, it doesn't shift the read cursor, just returns the next
  // token position.
  nsACString::const_char_iterator Parse(Token& aToken) const;
  // Is read cursor at the end?
  bool IsEnd(const nsACString::const_char_iterator& caret) const;
  // True, when we are at the end of the input data, but it has not been marked
  // as complete yet.  In that case we cannot proceed with providing a multi-char token.
  bool IsPending(const nsACString::const_char_iterator & caret) const;
  // Is read cursor on a character that is a word start?
  bool IsWordFirst(const char aInput) const;
  // Is read cursor on a character that is an in-word letter?
  bool IsWord(const char aInput) const;
  // Is read cursor on a character that is a valid number?
  // TODO - support multiple radix
  bool IsNumber(const char aInput) const;
  // Is equal to the given custom token?
  bool IsCustom(const nsACString::const_char_iterator& caret,
                const Token& aCustomToken, uint32_t* aLongest = nullptr) const;

  // Friendly helper to assign a fragment on a Token
  static void AssignFragment(Token& aToken,
                             nsACString::const_char_iterator begin,
                             nsACString::const_char_iterator end);

  // true iff we have already read the EOF token
  bool mPastEof;
  // true iff the last Check*() call has returned false, reverts to true on Rollback() call
  bool mHasFailed;
  // true if the input string is final (finished), false when we expect more data
  // yet to be fed to the tokenizer (see IncrementalTokenizer derived class).
  bool mInputFinished;
  // custom only vs full tokenizing mode, see the Parse() method
  Mode mMode;
  // minimal raw data chunked delivery during incremental feed
  uint32_t mMinRawDelivery;

  // Customizable list of whitespaces
  const char* mWhitespaces;
  // Additinal custom word characters
  const char* mAdditionalWordChars;

  // All these point to the original buffer passed to the constructor or to the incremental
  // buffer after FeedInput.
  nsACString::const_char_iterator mCursor; // Position of the current (actually next to read) token start
  nsACString::const_char_iterator mEnd; // End of the input position

  // This is the list of tokens user has registered with AddCustomToken()
  nsTArray<UniquePtr<Token>> mCustomTokens;
  uint32_t mNextCustomTokenID;

private:
  TokenizerBase() = delete;
  TokenizerBase(const TokenizerBase&) = delete;
  TokenizerBase(TokenizerBase&&) = delete;
  TokenizerBase(const TokenizerBase&&) = delete;
  TokenizerBase &operator=(const TokenizerBase&) = delete;
};

/**
 * This is a simple implementation of a lexical analyzer or maybe better
 * called a tokenizer.  It doesn't allow any user dictionaries or
 * user define token types.
 *
 * It is limited only to ASCII input for now. UTF-8 or any other input
 * encoding must yet be implemented.
 */
class Tokenizer : public TokenizerBase
{
public:
  /**
   * @param aSource
   *    The string to parse.
   *    IMPORTANT NOTE: Tokenizer doesn't ensure the input string buffer lifetime.
   *    It's up to the consumer to make sure the string's buffer outlives the Tokenizer!
   * @param aWhitespaces
   *    If non-null Tokenizer will use this custom set of whitespaces for CheckWhite()
   *    and SkipWhites() calls.
   *    By default the list consists of space and tab.
   * @param aAdditionalWordChars
   *    If non-null it will be added to the list of characters that consist a word.
   *    This is useful when you want to accept e.g. '-' in HTTP headers.
   *    By default a word character is consider any character for which upper case
   *    is different from lower case.
   *
   * If there is an overlap between aWhitespaces and aAdditionalWordChars, the check for
   * word characters is made first.
   */
  explicit Tokenizer(const nsACString& aSource,
                     const char* aWhitespaces = nullptr,
                     const char* aAdditionalWordChars = nullptr);
  explicit Tokenizer(const char* aSource,
                     const char* aWhitespaces = nullptr,
                     const char* aAdditionalWordChars = nullptr);

  /**
   * When there is still anything to read from the input, tokenize it, store the token type
   * and value to aToken result and shift the cursor past this just parsed token.  Each call
   * to Next() reads another token from the input and shifts the cursor.
   * Returns false if we have passed the end of the input.
   */
  MOZ_MUST_USE
  bool Next(Token& aToken);

  /**
   * Parse the token on the input read cursor position, check its type is equal to aTokenType
   * and if so, put it into aResult, shift the cursor and return true.  Otherwise, leave
   * the input read cursor position intact and return false.
   */
  MOZ_MUST_USE
  bool Check(const TokenType aTokenType, Token& aResult);
  /**
   * Same as above method, just compares both token type and token value passed in aToken.
   * When both the type and the value equals, shift the cursor and return true.  Otherwise
   * return false.
   */
  MOZ_MUST_USE
  bool Check(const Token& aToken);

  /**
   * SkipWhites method (below) may also skip new line characters automatically.
   */
  enum WhiteSkipping {
    /**
     * SkipWhites will only skip what is defined as a white space (default).
     */
    DONT_INCLUDE_NEW_LINE = 0,
    /**
     * SkipWhites will skip definited white spaces as well as new lines
     * automatically.
     */
    INCLUDE_NEW_LINE = 1
  };

  /**
   * Skips any occurence of whitespaces specified in mWhitespaces member,
   * optionally skip also new lines.
   */
  void SkipWhites(WhiteSkipping aIncludeNewLines = DONT_INCLUDE_NEW_LINE);

  /**
   * Skips all tokens until the given one is found or EOF is hit.  The token
   * or EOF are next to read.
   */
  void SkipUntil(Token const& aToken);

  // These are mostly shortcuts for the Check() methods above.

  /**
   * Check whitespace character is present.
   */
  MOZ_MUST_USE
  bool CheckWhite() { return Check(Token::Whitespace()); }
  /**
   * Check there is a single character on the read cursor position.  If so, shift the read
   * cursor position and return true.  Otherwise false.
   */
  MOZ_MUST_USE
  bool CheckChar(const char aChar) { return Check(Token::Char(aChar)); }
  /**
   * This is a customizable version of CheckChar.  aClassifier is a function called with
   * value of the character on the current input read position.  If this user function
   * returns true, read cursor is shifted and true returned.  Otherwise false.
   * The user classifiction function is not called when we are at or past the end and
   * false is immediately returned.
   */
  MOZ_MUST_USE
  bool CheckChar(bool (*aClassifier)(const char aChar));
  /**
   * Check for a whole expected word.
   */
  MOZ_MUST_USE
  bool CheckWord(const nsACString& aWord) { return Check(Token::Word(aWord)); }
  /**
   * Shortcut for literal const word check with compile time length calculation.
   */
  template <uint32_t N>
  MOZ_MUST_USE
  bool CheckWord(const char (&aWord)[N]) { return Check(Token::Word(nsDependentCString(aWord, N - 1))); }
  /**
   * Checks \r, \n or \r\n.
   */
  MOZ_MUST_USE
  bool CheckEOL() { return Check(Token::NewLine()); }
  /**
   * Checks we are at the end of the input string reading.  If so, shift past the end
   * and returns true.  Otherwise does nothing and returns false.
   */
  MOZ_MUST_USE
  bool CheckEOF() { return Check(Token::EndOfFile()); }

  /**
   * These are shortcuts to obtain the value immediately when the token type matches.
   */
  MOZ_MUST_USE bool ReadChar(char* aValue);
  MOZ_MUST_USE bool ReadChar(bool (*aClassifier)(const char aChar),
                             char* aValue);
  MOZ_MUST_USE bool ReadWord(nsACString& aValue);
  MOZ_MUST_USE bool ReadWord(nsDependentCSubstring& aValue);

  /**
   * This is an integer read helper.  It returns false and doesn't move the read
   * cursor when any of the following happens:
   *  - the token at the read cursor is not an integer
   *  - the final number doesn't fit the T type
   * Otherwise true is returned, aValue is filled with the integral number
   * and the cursor is moved forward.
   */
  template <typename T>
  MOZ_MUST_USE bool ReadInteger(T* aValue)
  {
    MOZ_RELEASE_ASSERT(aValue);

    nsACString::const_char_iterator rollback = mRollback;
    nsACString::const_char_iterator cursor = mCursor;
    Token t;
    if (!Check(TOKEN_INTEGER, t)) {
      return false;
    }

    mozilla::CheckedInt<T> checked(t.AsInteger());
    if (!checked.isValid()) {
      // Move to a state as if Check() call has failed
      mRollback = rollback;
      mCursor = cursor;
      mHasFailed = true;
      return false;
    }

    *aValue = checked.value();
    return true;
  }

  /**
   * Returns the read cursor position back as it was before the last call of any parsing
   * method of Tokenizer (Next, Check*, Skip*, Read*) so that the last operation
   * can be repeated.
   * Rollback cannot be used multiple times, it only reverts the last successfull parse
   * operation.  It also cannot be used before any parsing operation has been called
   * on the Tokenizer.
   */
  void Rollback();

  /**
   * Record() and Claim() are collecting the input as it is being parsed to obtain
   * a substring between particular syntax bounderies defined by any recursive
   * descent parser or simple parser the Tokenizer is used to read the input for.
   * Inlucsion of a token that has just been parsed can be controlled using an arguemnt.
   */
  enum ClaimInclusion {
    /**
     * Include resulting (or passed) token of the last lexical analyzer operation in the result.
     */
    INCLUDE_LAST,
    /**
     * Do not include it.
     */
    EXCLUDE_LAST
  };

  /**
   * Start the process of recording.  Based on aInclude value the begining of the recorded
   * sub-string is at the current position (EXCLUDE_LAST) or at the position before the last
   * parsed token (INCLUDE_LAST).
   */
  void Record(ClaimInclusion aInclude = EXCLUDE_LAST);
  /**
   * Claim result of the record started with Record() call before.  Depending on aInclude
   * the ending of the sub-string result includes or excludes the last parsed or checked
   * token.
   */
  void Claim(nsACString& aResult, ClaimInclusion aInclude = EXCLUDE_LAST);
  void Claim(nsDependentCSubstring& aResult, ClaimInclusion aInclude = EXCLUDE_LAST);

  /**
   * If aToken is found, aResult is set to the substring between the current
   * position and the position of aToken, potentially including aToken depending
   * on aInclude.
   * If aToken isn't found aResult is set to the substring between the current
   * position and the end of the string.
   * If aToken is found, the method returns true. Otherwise it returns false.
   *
   * Calling Rollback() after ReadUntil() will return the read cursor to the
   * position it had before ReadUntil was called.
   */
  MOZ_MUST_USE bool ReadUntil(Token const& aToken, nsDependentCSubstring& aResult,
                              ClaimInclusion aInclude = EXCLUDE_LAST);
  MOZ_MUST_USE bool ReadUntil(Token const& aToken, nsACString& aResult,
                              ClaimInclusion aInclude = EXCLUDE_LAST);

protected:
  // All these point to the original buffer passed to the Tokenizer's constructor
  nsACString::const_char_iterator mRecord; // Position where the recorded sub-string for Claim() is
  nsACString::const_char_iterator mRollback; // Position of the previous token start

private:
  Tokenizer() = delete;
  Tokenizer(const Tokenizer&) = delete;
  Tokenizer(Tokenizer&&) = delete;
  Tokenizer(const Tokenizer&&) = delete;
  Tokenizer &operator=(const Tokenizer&) = delete;
};

} // mozilla

#endif // Tokenizer_h__
