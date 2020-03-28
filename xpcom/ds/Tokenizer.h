/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Tokenizer_h__
#define Tokenizer_h__

#include "nsString.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/UniquePtr.h"
#include "nsTArray.h"

namespace mozilla {

template <typename TChar>
class TokenizerBase {
 public:
  typedef nsTSubstring<TChar> TAString;
  typedef nsTString<TChar> TString;
  typedef nsTDependentString<TChar> TDependentString;
  typedef nsTDependentSubstring<TChar> TDependentSubstring;

  static TChar const sWhitespaces[];

  /**
   * The analyzer works with elements in the input cut to a sequence of token
   * where each token has an elementary type
   */
  enum TokenType : uint32_t {
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

  enum ECaseSensitivity { CASE_SENSITIVE, CASE_INSENSITIVE };

  /**
   * Class holding the type and the value of a token.  It can be manually
   * created to allow checks against it via methods of TTokenizer or are results
   * of some of the TTokenizer's methods.
   */
  class Token {
    TokenType mType;
    TDependentSubstring mWord;
    TString mCustom;
    TChar mChar;
    uint64_t mInteger;
    ECaseSensitivity mCustomCaseInsensitivity;
    bool mCustomEnabled;

    // If this token is a result of the parsing process, this member is
    // referencing a sub-string in the input buffer.  If this is externally
    // created Token this member is left an empty string.
    TDependentSubstring mFragment;

    friend class TokenizerBase<TChar>;
    void AssignFragment(typename TAString::const_char_iterator begin,
                        typename TAString::const_char_iterator end);

    static Token Raw();

   public:
    Token();
    Token(const Token& aOther);
    Token& operator=(const Token& aOther);

    // Static constructors of tokens by type and value
    static Token Word(TAString const& aWord);
    static Token Char(TChar const aChar);
    static Token Number(uint64_t const aNumber);
    static Token Whitespace();
    static Token NewLine();
    static Token EndOfFile();
    static Token Error();

    // Compares the two tokens, type must be identical and value
    // of one of the tokens must be 'any' or equal.
    bool Equals(const Token& aOther) const;

    TokenType Type() const { return mType; }
    TChar AsChar() const;
    TDependentSubstring AsString() const;
    uint64_t AsInteger() const;

    TDependentSubstring Fragment() const { return mFragment; }
  };

  /**
   * Consumers may register a custom string that, when found in the input, is
   * considered a token and returned by Next*() and accepted by Check*()
   * methods. AddCustomToken() returns a reference to a token that can then be
   * comapred using Token::Equals() againts the output from Next*() or be passed
   * to Check*().
   */
  Token AddCustomToken(const TAString& aValue,
                       ECaseSensitivity aCaseInsensitivity,
                       bool aEnabled = true);
  template <uint32_t N>
  Token AddCustomToken(const TChar (&aValue)[N],
                       ECaseSensitivity aCaseInsensitivity,
                       bool aEnabled = true) {
    return AddCustomToken(TDependentSubstring(aValue, N - 1),
                          aCaseInsensitivity, aEnabled);
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
   * FULL tokenization, the default, recognizes built-in tokens and any custom
   * tokens, if added. CUSTOM_ONLY will only recognize custom tokens, the rest
   * is seen as 'raw'. This mode can be understood as a 'binary' mode.
   */
  enum class Mode { FULL, CUSTOM_ONLY };
  void SetTokenizingMode(Mode aMode);

  /**
   * Return false iff the last Check*() call has returned false or when we've
   * read past the end of the input string.
   */
  [[nodiscard]] bool HasFailed() const;

 protected:
  explicit TokenizerBase(const TChar* aWhitespaces = nullptr,
                         const TChar* aAdditionalWordChars = nullptr);

  // false if we have already read the EOF token.
  bool HasInput() const;
  // Main parsing function, it doesn't shift the read cursor, just returns the
  // next token position.
  typename TAString::const_char_iterator Parse(Token& aToken) const;
  // Is read cursor at the end?
  bool IsEnd(const typename TAString::const_char_iterator& caret) const;
  // True, when we are at the end of the input data, but it has not been marked
  // as complete yet.  In that case we cannot proceed with providing a
  // multi-TChar token.
  bool IsPending(const typename TAString::const_char_iterator& caret) const;
  // Is read cursor on a character that is a word start?
  bool IsWordFirst(const TChar aInput) const;
  // Is read cursor on a character that is an in-word letter?
  bool IsWord(const TChar aInput) const;
  // Is read cursor on a character that is a valid number?
  // TODO - support multiple radix
  bool IsNumber(const TChar aInput) const;
  // Is equal to the given custom token?
  bool IsCustom(const typename TAString::const_char_iterator& caret,
                const Token& aCustomToken, uint32_t* aLongest = nullptr) const;

  // Friendly helper to assign a fragment on a Token
  static void AssignFragment(Token& aToken,
                             typename TAString::const_char_iterator begin,
                             typename TAString::const_char_iterator end);

  // true iff we have already read the EOF token
  bool mPastEof;
  // true iff the last Check*() call has returned false, reverts to true on
  // Rollback() call
  bool mHasFailed;
  // true if the input string is final (finished), false when we expect more
  // data yet to be fed to the tokenizer (see IncrementalTokenizer derived
  // class).
  bool mInputFinished;
  // custom only vs full tokenizing mode, see the Parse() method
  Mode mMode;
  // minimal raw data chunked delivery during incremental feed
  uint32_t mMinRawDelivery;

  // Customizable list of whitespaces
  const TChar* mWhitespaces;
  // Additinal custom word characters
  const TChar* mAdditionalWordChars;

  // All these point to the original buffer passed to the constructor or to the
  // incremental buffer after FeedInput.
  typename TAString::const_char_iterator
      mCursor;  // Position of the current (actually next to read) token start
  typename TAString::const_char_iterator mEnd;  // End of the input position

  // This is the list of tokens user has registered with AddCustomToken()
  nsTArray<UniquePtr<Token>> mCustomTokens;
  uint32_t mNextCustomTokenID;

 private:
  TokenizerBase() = delete;
  TokenizerBase(const TokenizerBase&) = delete;
  TokenizerBase(TokenizerBase&&) = delete;
  TokenizerBase(const TokenizerBase&&) = delete;
  TokenizerBase& operator=(const TokenizerBase&) = delete;
};

/**
 * This is a simple implementation of a lexical analyzer or maybe better
 * called a tokenizer.
 *
 * Please use Tokenizer or Tokenizer16 classes, that are specializations
 * of this template class.  Tokenizer is for ASCII input, Tokenizer16 may
 * handle char16_t input, but doesn't recognize whitespaces or numbers
 * other than standard `char` specialized Tokenizer class.
 */
template <typename TChar>
class TTokenizer : public TokenizerBase<TChar> {
 public:
  typedef TokenizerBase<TChar> base;

  /**
   * @param aSource
   *    The string to parse.
   *    IMPORTANT NOTE: TTokenizer doesn't ensure the input string buffer
   * lifetime. It's up to the consumer to make sure the string's buffer outlives
   * the TTokenizer!
   * @param aWhitespaces
   *    If non-null TTokenizer will use this custom set of whitespaces for
   * CheckWhite() and SkipWhites() calls. By default the list consists of space
   * and tab.
   * @param aAdditionalWordChars
   *    If non-null it will be added to the list of characters that consist a
   * word. This is useful when you want to accept e.g. '-' in HTTP headers. By
   * default a word character is consider any character for which upper case
   *    is different from lower case.
   *
   * If there is an overlap between aWhitespaces and aAdditionalWordChars, the
   * check for word characters is made first.
   */
  explicit TTokenizer(const typename base::TAString& aSource,
                      const TChar* aWhitespaces = nullptr,
                      const TChar* aAdditionalWordChars = nullptr);
  explicit TTokenizer(const TChar* aSource, const TChar* aWhitespaces = nullptr,
                      const TChar* aAdditionalWordChars = nullptr);

  /**
   * When there is still anything to read from the input, tokenize it, store the
   * token type and value to aToken result and shift the cursor past this just
   * parsed token.  Each call to Next() reads another token from the input and
   * shifts the cursor. Returns false if we have passed the end of the input.
   */
  [[nodiscard]] bool Next(typename base::Token& aToken);

  /**
   * Parse the token on the input read cursor position, check its type is equal
   * to aTokenType and if so, put it into aResult, shift the cursor and return
   * true.  Otherwise, leave the input read cursor position intact and return
   * false.
   */
  [[nodiscard]] bool Check(const typename base::TokenType aTokenType,
                           typename base::Token& aResult);
  /**
   * Same as above method, just compares both token type and token value passed
   * in aToken. When both the type and the value equals, shift the cursor and
   * return true.  Otherwise return false.
   */
  [[nodiscard]] bool Check(const typename base::Token& aToken);

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
  void SkipUntil(typename base::Token const& aToken);

  // These are mostly shortcuts for the Check() methods above.

  /**
   * Check whitespace character is present.
   */
  [[nodiscard]] bool CheckWhite() { return Check(base::Token::Whitespace()); }
  /**
   * Check there is a single character on the read cursor position.  If so,
   * shift the read cursor position and return true.  Otherwise false.
   */
  [[nodiscard]] bool CheckChar(const TChar aChar) {
    return Check(base::Token::Char(aChar));
  }
  /**
   * This is a customizable version of CheckChar.  aClassifier is a function
   * called with value of the character on the current input read position.  If
   * this user function returns true, read cursor is shifted and true returned.
   * Otherwise false. The user classifiction function is not called when we are
   * at or past the end and false is immediately returned.
   */
  [[nodiscard]] bool CheckChar(bool (*aClassifier)(const TChar aChar));
  /**
   * Check for a whole expected word.
   */
  [[nodiscard]] bool CheckWord(const typename base::TAString& aWord) {
    return Check(base::Token::Word(aWord));
  }
  /**
   * Shortcut for literal const word check with compile time length calculation.
   */
  template <uint32_t N>
  [[nodiscard]] bool CheckWord(const TChar (&aWord)[N]) {
    return Check(
        base::Token::Word(typename base::TDependentString(aWord, N - 1)));
  }
  /**
   * Checks \r, \n or \r\n.
   */
  [[nodiscard]] bool CheckEOL() { return Check(base::Token::NewLine()); }
  /**
   * Checks we are at the end of the input string reading.  If so, shift past
   * the end and returns true.  Otherwise does nothing and returns false.
   */
  [[nodiscard]] bool CheckEOF() { return Check(base::Token::EndOfFile()); }

  /**
   * These are shortcuts to obtain the value immediately when the token type
   * matches.
   */
  [[nodiscard]] bool ReadChar(TChar* aValue);
  [[nodiscard]] bool ReadChar(bool (*aClassifier)(const TChar aChar),
                              TChar* aValue);
  [[nodiscard]] bool ReadWord(typename base::TAString& aValue);
  [[nodiscard]] bool ReadWord(typename base::TDependentSubstring& aValue);

  /**
   * This is an integer read helper.  It returns false and doesn't move the read
   * cursor when any of the following happens:
   *  - the token at the read cursor is not an integer
   *  - the final number doesn't fit the T type
   * Otherwise true is returned, aValue is filled with the integral number
   * and the cursor is moved forward.
   */
  template <typename T>
  [[nodiscard]] bool ReadInteger(T* aValue) {
    MOZ_RELEASE_ASSERT(aValue);

    typename base::TAString::const_char_iterator rollback = mRollback;
    typename base::TAString::const_char_iterator cursor = base::mCursor;
    typename base::Token t;
    if (!Check(base::TOKEN_INTEGER, t)) {
      return false;
    }

    mozilla::CheckedInt<T> checked(t.AsInteger());
    if (!checked.isValid()) {
      // Move to a state as if Check() call has failed
      mRollback = rollback;
      base::mCursor = cursor;
      base::mHasFailed = true;
      return false;
    }

    *aValue = checked.value();
    return true;
  }

  /**
   * Same as above, but accepts an integer with an optional minus sign.
   */
  template <typename T, typename V = typename EnableIf<
                            IsSigned<typename RemovePointer<T>::Type>::value,
                            typename RemovePointer<T>::Type>::Type>
  [[nodiscard]] bool ReadSignedInteger(T* aValue) {
    MOZ_RELEASE_ASSERT(aValue);

    typename base::TAString::const_char_iterator rollback = mRollback;
    typename base::TAString::const_char_iterator cursor = base::mCursor;
    auto revert = MakeScopeExit([&] {
      // Move to a state as if Check() call has failed
      mRollback = rollback;
      base::mCursor = cursor;
      base::mHasFailed = true;
    });

    // Using functional raw access because '-' could be part of the word set
    // making CheckChar('-') not work.
    bool minus = CheckChar([](const TChar aChar) { return aChar == '-'; });

    typename base::Token t;
    if (!Check(base::TOKEN_INTEGER, t)) {
      return false;
    }

    mozilla::CheckedInt<T> checked(t.AsInteger());
    if (minus) {
      checked *= -1;
    }

    if (!checked.isValid()) {
      return false;
    }

    *aValue = checked.value();
    revert.release();
    return true;
  }

  /**
   * Returns the read cursor position back as it was before the last call of any
   * parsing method of TTokenizer (Next, Check*, Skip*, Read*) so that the last
   * operation can be repeated. Rollback cannot be used multiple times, it only
   * reverts the last successfull parse operation.  It also cannot be used
   * before any parsing operation has been called on the TTokenizer.
   */
  void Rollback();

  /**
   * Record() and Claim() are collecting the input as it is being parsed to
   * obtain a substring between particular syntax bounderies defined by any
   * recursive descent parser or simple parser the TTokenizer is used to read
   * the input for. Inlucsion of a token that has just been parsed can be
   * controlled using an arguemnt.
   */
  enum ClaimInclusion {
    /**
     * Include resulting (or passed) token of the last lexical analyzer
     * operation in the result.
     */
    INCLUDE_LAST,
    /**
     * Do not include it.
     */
    EXCLUDE_LAST
  };

  /**
   * Start the process of recording.  Based on aInclude value the begining of
   * the recorded sub-string is at the current position (EXCLUDE_LAST) or at the
   * position before the last parsed token (INCLUDE_LAST).
   */
  void Record(ClaimInclusion aInclude = EXCLUDE_LAST);
  /**
   * Claim result of the record started with Record() call before.  Depending on
   * aInclude the ending of the sub-string result includes or excludes the last
   * parsed or checked token.
   */
  void Claim(typename base::TAString& aResult,
             ClaimInclusion aInclude = EXCLUDE_LAST);
  void Claim(typename base::TDependentSubstring& aResult,
             ClaimInclusion aInclude = EXCLUDE_LAST);

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
  [[nodiscard]] bool ReadUntil(typename base::Token const& aToken,
                               typename base::TDependentSubstring& aResult,
                               ClaimInclusion aInclude = EXCLUDE_LAST);
  [[nodiscard]] bool ReadUntil(typename base::Token const& aToken,
                               typename base::TAString& aResult,
                               ClaimInclusion aInclude = EXCLUDE_LAST);

 protected:
  // All these point to the original buffer passed to the TTokenizer's
  // constructor
  typename base::TAString::const_char_iterator
      mRecord;  // Position where the recorded sub-string for Claim() is
  typename base::TAString::const_char_iterator
      mRollback;  // Position of the previous token start

 private:
  TTokenizer() = delete;
  TTokenizer(const TTokenizer&) = delete;
  TTokenizer(TTokenizer&&) = delete;
  TTokenizer(const TTokenizer&&) = delete;
  TTokenizer& operator=(const TTokenizer&) = delete;
};

typedef TTokenizer<char> Tokenizer;
typedef TTokenizer<char16_t> Tokenizer16;

}  // namespace mozilla

#endif  // Tokenizer_h__
