/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef INCREMENTAL_TOKENIZER_H__
#define INCREMENTAL_TOKENIZER_H__

#include "mozilla/Tokenizer.h"

#include "nsError.h"
#include <functional>

class nsIInputStream;

namespace mozilla {

class IncrementalTokenizer : public TokenizerBase<char> {
 public:
  /**
   * The consumer callback.  The function is called for every single token
   * as found in the input.  Failure result returned by this callback stops
   * the tokenization immediately and bubbles to result of Feed/FinishInput.
   *
   * Fragment()s of consumed tokens are ensured to remain valid until next call
   * to Feed/FinishInput and are pointing to a single linear buffer.  Hence,
   * those can be safely used to accumulate the data for processing after
   * Feed/FinishInput returned.
   */
  typedef std::function<nsresult(Token const&, IncrementalTokenizer& i)>
      Consumer;

  /**
   * For aWhitespaces and aAdditionalWordChars arguments see TokenizerBase.
   *
   * @param aConsumer
   *    A mandatory non-null argument, a function that consumes the tokens as
   * they come when the tokenizer is fed.
   * @param aRawMinBuffered
   *    When we have buffered at least aRawMinBuffered data, but there was no
   * custom token found so far because of too small incremental feed chunks,
   * deliver the raw data to preserve streaming and to save memory.  This only
   * has effect in OnlyCustomTokenizing mode.
   */
  explicit IncrementalTokenizer(Consumer&& aConsumer,
                                const char* aWhitespaces = nullptr,
                                const char* aAdditionalWordChars = nullptr,
                                uint32_t aRawMinBuffered = 1024);

  /**
   * Pushes the input to be tokenized.  These directly call the Consumer
   * callback on every found token.  Result of the Consumer callback is returned
   * here.
   *
   * The tokenizer must be initialized with a valid consumer prior call to these
   * methods.  It's not allowed to call Feed/FinishInput from inside the
   * Consumer callback.
   */
  nsresult FeedInput(const nsACString& aInput);
  nsresult FeedInput(nsIInputStream* aInput, uint32_t aCount);
  nsresult FinishInput();

  /**
   * Can only be called from inside the consumer callback.
   *
   * When there is still anything to read from the input, tokenize it, store
   * the token type and value to aToken result and shift the cursor past this
   * just parsed token.  Each call to Next() reads another token from
   * the input and shifts the cursor.
   *
   * Returns false if there is not enough data to deterministically recognize
   * tokens or when the last returned token was EOF.
   */
  [[nodiscard]] bool Next(Token& aToken);

  /**
   * Can only be called from inside the consumer callback.
   *
   * Tells the tokenizer to revert the cursor and stop the async parsing until
   * next feed of the input.  This is useful when more than one token is needed
   * to decide on the syntax but there is not enough input to get a next token
   * (Next() returned false.)
   */
  void NeedMoreInput();

  /**
   * Can only be called from inside the consumer callback.
   *
   * This makes the consumer callback be called again while parsing
   * the input at the previous cursor position again.  This is useful when
   * the tokenizer state (custom tokens, tokenization mode) has changed and
   * we want to re-parse the input again.
   */
  void Rollback();

 private:
  // Loops over the input with TokenizerBase::Parse and calls the Consumer
  // callback.
  nsresult Process();

#ifdef DEBUG
  // True when inside the consumer callback, used only for assertions.
  bool mConsuming;
#endif  // DEBUG
  // Modifyable only from the Consumer callback, tells the parser to break,
  // rollback and wait for more input.
  bool mNeedMoreInput;
  // Modifyable only from the Consumer callback, tells the parser to rollback
  // and parse the input again, with (if modified) new settings of the
  // tokenizer.
  bool mRollback;
  // The input buffer.  Updated with each call to Feed/FinishInput.
  nsCString mInput;
  // Numerical index pointing at the current cursor position.  We don't keep
  // direct reference to the string buffer since the buffer gets often
  // reallocated.
  nsCString::index_type mInputCursor;
  // Refernce to the consumer function.
  Consumer mConsumer;
};

}  // namespace mozilla

#endif
