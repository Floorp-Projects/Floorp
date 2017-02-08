/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/IncrementalTokenizer.h"

#include "mozilla/AutoRestore.h"

#include "nsIInputStream.h"
#include "IncrementalTokenizer.h"
#include <algorithm>

namespace mozilla {

IncrementalTokenizer::IncrementalTokenizer(Consumer aConsumer,
                                           const char * aWhitespaces,
                                           const char * aAdditionalWordChars,
                                           uint32_t aRawMinBuffered)
  : TokenizerBase(aWhitespaces, aAdditionalWordChars)
#ifdef DEBUG
  , mConsuming(false)
#endif
  , mNeedMoreInput(false)
  , mRollback(false)
  , mInputCursor(0)
  , mConsumer(aConsumer)
{
  mInputFinished = false;
  mMinRawDelivery = aRawMinBuffered;
}

nsresult IncrementalTokenizer::FeedInput(const nsACString & aInput)
{
  NS_ENSURE_TRUE(mConsumer, NS_ERROR_NOT_INITIALIZED);
  MOZ_ASSERT(!mInputFinished);

  mInput.Cut(0, mInputCursor);
  mInputCursor = 0;

  mInput.Append(aInput);

  return Process();
}

nsresult IncrementalTokenizer::FeedInput(nsIInputStream * aInput, uint32_t aCount)
{
  NS_ENSURE_TRUE(mConsumer, NS_ERROR_NOT_INITIALIZED);
  MOZ_ASSERT(!mInputFinished);
  MOZ_ASSERT(!mConsuming);

  mInput.Cut(0, mInputCursor);
  mInputCursor = 0;

  nsresult rv = NS_OK;
  while (NS_SUCCEEDED(rv) && aCount) {
    nsCString::index_type remainder = mInput.Length();
    nsCString::index_type load =
      std::min<nsCString::index_type>(aCount, PR_UINT32_MAX - remainder);

    if (!load) {
      // To keep the API simple, we fail if the input data buffer if filled.
      // It's highly unlikely there will ever be such amout of data cumulated
      // unless a logic fault in the consumer code.
      NS_ERROR("IncrementalTokenizer consumer not reading data?");
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!mInput.SetLength(remainder + load, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCString::char_iterator buffer = mInput.BeginWriting() + remainder;

    uint32_t read;
    rv = aInput->Read(buffer, load, &read);
    if (NS_SUCCEEDED(rv)) {
      // remainder + load fits the uint32_t size, so must remainder + read.
      mInput.SetLength(remainder + read);
      aCount -= read;

      rv = Process();
    }
  }

  return rv;
}

nsresult IncrementalTokenizer::FinishInput()
{
  NS_ENSURE_TRUE(mConsumer, NS_ERROR_NOT_INITIALIZED);
  MOZ_ASSERT(!mInputFinished);
  MOZ_ASSERT(!mConsuming);

  mInput.Cut(0, mInputCursor);
  mInputCursor = 0;

  mInputFinished = true;
  nsresult rv = Process();
  mConsumer = nullptr;
  return rv;
}

bool IncrementalTokenizer::Next(Token & aToken)
{
  // Assert we are called only from the consumer callback
  MOZ_ASSERT(mConsuming);

  if (mPastEof) {
    return false;
  }

  nsACString::const_char_iterator next = Parse(aToken);
  mPastEof = aToken.Type() == TOKEN_EOF;
  if (next == mCursor && !mPastEof) {
    // Not enough input to make a deterministic decision.
    return false;
  }

  AssignFragment(aToken, mCursor, next);
  mCursor = next;
  return true;
}

void IncrementalTokenizer::NeedMoreInput()
{
  // Assert we are called only from the consumer callback
  MOZ_ASSERT(mConsuming);

  // When the input has been finished, we can't set the flag to prevent
  // indefinite wait for more input (that will never come)
  mNeedMoreInput = !mInputFinished;
}

void IncrementalTokenizer::Rollback()
{
  // Assert we are called only from the consumer callback
  MOZ_ASSERT(mConsuming);

  mRollback = true;
}

nsresult IncrementalTokenizer::Process()
{
#ifdef DEBUG
  // Assert we are not re-entered
  MOZ_ASSERT(!mConsuming);

  AutoRestore<bool> consuming(mConsuming);
  mConsuming = true;
#endif

  MOZ_ASSERT(!mPastEof);

  nsresult rv = NS_OK;

  mInput.BeginReading(mCursor);
  mCursor += mInputCursor;
  mInput.EndReading(mEnd);

  while (NS_SUCCEEDED(rv) && !mPastEof) {
    Token token;
    nsACString::const_char_iterator next = Parse(token);
    mPastEof = token.Type() == TOKEN_EOF;
    if (next == mCursor && !mPastEof) {
      // Not enough input to make a deterministic decision.
      break;
    }

    AssignFragment(token, mCursor, next);

    nsACString::const_char_iterator rollback = mCursor;
    mCursor = next;

    mNeedMoreInput = mRollback = false;

    rv = mConsumer(token, *this);
    if (NS_FAILED(rv)) {
      break;
    }
    if (mNeedMoreInput || mRollback) {
      mCursor = rollback;
      mPastEof = false;
      if (mNeedMoreInput) {
        break;
      }
    }
  }

  mInputCursor = mCursor - mInput.BeginReading();
  return rv;
}

} // mozilla
