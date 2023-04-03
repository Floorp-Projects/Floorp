/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// #define __INCREMENTAL 1

#include "nsScanner.h"

#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Encoding.h"
#include "mozilla/UniquePtr.h"
#include "nsDebug.h"
#include "nsReadableUtils.h"
#include "nsUTF8Utils.h"  // for LossyConvertEncoding
#include "nsCRT.h"
#include "nsParser.h"
#include "nsCharsetSource.h"

nsReadEndCondition::nsReadEndCondition(const char16_t* aTerminateChars)
    : mChars(aTerminateChars),
      mFilter(char16_t(~0))  // All bits set
{
  // Build filter that will be used to filter out characters with
  // bits that none of the terminal chars have. This works very well
  // because terminal chars often have only the last 4-6 bits set and
  // normal ascii letters have bit 7 set. Other letters have even higher
  // bits set.

  // Calculate filter
  const char16_t* current = aTerminateChars;
  char16_t terminalChar = *current;
  while (terminalChar) {
    mFilter &= ~terminalChar;
    ++current;
    terminalChar = *current;
  }
}

/**
 *  Use this constructor if you want i/o to be based on
 *  a single string you hand in during construction.
 *  This short cut was added for Javascript.
 *
 *  @update  gess 5/12/98
 *  @param   aMode represents the parser mode (nav, other)
 *  @return
 */
nsScanner::nsScanner(const nsAString& anHTMLString, bool aIncremental)
    : mIncremental(aIncremental) {
  MOZ_COUNT_CTOR(nsScanner);

  AppendToBuffer(anHTMLString);
  MOZ_ASSERT(mMarkPosition == mCurrentPosition);
}

/**
 *  Use this constructor if you want i/o to be based on strings
 *  the scanner receives. If you pass a null filename, you
 *  can still provide data to the scanner via append.
 */
nsScanner::nsScanner(nsIURI* aURI) : mURI(aURI), mIncremental(true) {
  MOZ_COUNT_CTOR(nsScanner);

  // XXX This is a big hack.  We need to initialize the iterators to something.
  // What matters is that mCurrentPosition == mEndPosition, so that our methods
  // believe that we are at EOF (see bug 182067).  We null out mCurrentPosition
  // so that we have some hope of catching null pointer dereferences associated
  // with this hack. --darin
  memset(&mCurrentPosition, 0, sizeof(mCurrentPosition));
  mMarkPosition = mCurrentPosition;
  mEndPosition = mCurrentPosition;

  // XML defaults to UTF-8 and about:blank is UTF-8, too.
  SetDocumentCharset(UTF_8_ENCODING, kCharsetFromDocTypeDefault);
}

nsresult nsScanner::SetDocumentCharset(NotNull<const Encoding*> aEncoding,
                                       int32_t aSource) {
  if (aSource < mCharsetSource)  // priority is lower than the current one
    return NS_OK;

  mCharsetSource = aSource;
  nsCString charsetName;
  aEncoding->Name(charsetName);
  if (!mCharset.IsEmpty() && charsetName.Equals(mCharset)) {
    return NS_OK;  // no difference, don't change it
  }

  // different, need to change it

  mCharset.Assign(charsetName);

  mUnicodeDecoder = aEncoding->NewDecoderWithBOMRemoval();

  return NS_OK;
}

/**
 *  default destructor
 *
 *  @update  gess 3/25/98
 *  @param
 *  @return
 */
nsScanner::~nsScanner() { MOZ_COUNT_DTOR(nsScanner); }

/**
 *  Resets current offset position of input stream to marked position.
 *  This allows us to back up to this point if the need should arise,
 *  such as when tokenization gets interrupted.
 *  NOTE: IT IS REALLY BAD FORM TO CALL RELEASE WITHOUT CALLING MARK FIRST!
 *
 *  @update  gess 5/12/98
 *  @param
 *  @return
 */
void nsScanner::RewindToMark(void) {
  if (mSlidingBuffer) {
    mCurrentPosition = mMarkPosition;
  }
}

/**
 *  Records current offset position in input stream. This allows us
 *  to back up to this point if the need should arise, such as when
 *  tokenization gets interrupted.
 *
 *  @update  gess 7/29/98
 *  @param
 *  @return
 */
int32_t nsScanner::Mark() {
  int32_t distance = 0;
  if (mSlidingBuffer) {
    nsScannerIterator oldStart;
    mSlidingBuffer->BeginReading(oldStart);

    distance = Distance(oldStart, mCurrentPosition);

    mSlidingBuffer->DiscardPrefix(mCurrentPosition);
    mSlidingBuffer->BeginReading(mCurrentPosition);
    mMarkPosition = mCurrentPosition;
  }

  return distance;
}

/**
 * Insert data to our underlying input buffer as
 * if it were read from an input stream.
 *
 * @update  harishd 01/12/99
 * @return  error code
 */
bool nsScanner::UngetReadable(const nsAString& aBuffer) {
  if (!mSlidingBuffer) {
    return false;
  }

  mSlidingBuffer->UngetReadable(aBuffer, mCurrentPosition);
  mSlidingBuffer->BeginReading(
      mCurrentPosition);  // Insertion invalidated our iterators
  mSlidingBuffer->EndReading(mEndPosition);

  return true;
}

/**
 * Append data to our underlying input buffer as
 * if it were read from an input stream.
 *
 * @update  gess4/3/98
 * @return  error code
 */
nsresult nsScanner::Append(const nsAString& aBuffer) {
  if (!AppendToBuffer(aBuffer)) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

/**
 *
 *
 *  @update  gess 5/21/98
 *  @param
 *  @return
 */
nsresult nsScanner::Append(const char* aBuffer, uint32_t aLen) {
  nsresult res = NS_OK;
  if (mUnicodeDecoder) {
    mozilla::CheckedInt<size_t> needed =
        mUnicodeDecoder->MaxUTF16BufferLength(aLen);
    if (!needed.isValid()) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mozilla::CheckedInt<uint32_t> allocLen(
        1);  // null terminator due to legacy sadness
    allocLen += needed.value();
    if (!allocLen.isValid()) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    nsScannerString::Buffer* buffer =
        nsScannerString::AllocBuffer(allocLen.value());
    NS_ENSURE_TRUE(buffer, NS_ERROR_OUT_OF_MEMORY);
    char16_t* unichars = buffer->DataStart();

    uint32_t result;
    size_t read;
    size_t written;
    // Do not use structured binding lest deal with [-Werror=unused-variable]
    std::tie(result, read, written) =
        mUnicodeDecoder->DecodeToUTF16WithoutReplacement(
            AsBytes(mozilla::Span(aBuffer, aLen)),
            mozilla::Span(unichars, needed.value()),
            false);  // Retain bug about failure to handle EOF
    MOZ_ASSERT(result != mozilla::kOutputFull);
    MOZ_ASSERT(read <= aLen);
    MOZ_ASSERT(written <= needed.value());
    if (result != mozilla::kInputEmpty) {
      // Since about:blank is empty, this line runs only for XML. Use a
      // character that's illegal in XML instead of U+FFFD in order to make
      // expat flag the error. There is no need to loop and convert more, since
      // expat will stop here anyway.
      unichars[written++] = 0xFFFF;
    }
    buffer->SetDataLength(written);
    // Don't propagate return code of unicode decoder
    // since it doesn't reflect on our success or failure
    // - Ref. bug 87110
    res = NS_OK;
    AppendToBuffer(buffer);
  } else {
    NS_WARNING("No decoder found.");
    res = NS_ERROR_FAILURE;
  }

  return res;
}

/**
 *  retrieve next char from scanners internal input stream
 *
 *  @update  gess 3/25/98
 *  @param
 *  @return  error code reflecting read status
 */
nsresult nsScanner::GetChar(char16_t& aChar) {
  if (!mSlidingBuffer || mCurrentPosition == mEndPosition) {
    aChar = 0;
    return NS_ERROR_HTMLPARSER_EOF;
  }

  aChar = *mCurrentPosition++;

  return NS_OK;
}

void nsScanner::BindSubstring(nsScannerSubstring& aSubstring,
                              const nsScannerIterator& aStart,
                              const nsScannerIterator& aEnd) {
  aSubstring.Rebind(*mSlidingBuffer, aStart, aEnd);
}

void nsScanner::CurrentPosition(nsScannerIterator& aPosition) {
  aPosition = mCurrentPosition;
}

void nsScanner::EndReading(nsScannerIterator& aPosition) {
  aPosition = mEndPosition;
}

void nsScanner::SetPosition(nsScannerIterator& aPosition, bool aTerminate) {
  if (mSlidingBuffer) {
    mCurrentPosition = aPosition;
    if (aTerminate && (mCurrentPosition == mEndPosition)) {
      mMarkPosition = mCurrentPosition;
      mSlidingBuffer->DiscardPrefix(mCurrentPosition);
    }
  }
}

void nsScanner::AppendToBuffer(nsScannerString::Buffer* aBuf) {
  if (!mSlidingBuffer) {
    mSlidingBuffer = mozilla::MakeUnique<nsScannerString>(aBuf);
    mSlidingBuffer->BeginReading(mCurrentPosition);
    mMarkPosition = mCurrentPosition;
  } else {
    mSlidingBuffer->AppendBuffer(aBuf);
    if (mCurrentPosition == mEndPosition) {
      mSlidingBuffer->BeginReading(mCurrentPosition);
    }
  }
  mSlidingBuffer->EndReading(mEndPosition);
}

/**
 *  call this to copy bytes out of the scanner that have not yet been consumed
 *  by the tokenization process.
 *
 *  @update  gess 5/12/98
 *  @param   aCopyBuffer is where the scanner buffer will be copied to
 *  @return  true if OK or false on OOM
 */
bool nsScanner::CopyUnusedData(nsString& aCopyBuffer) {
  if (!mSlidingBuffer) {
    aCopyBuffer.Truncate();
    return true;
  }

  nsScannerIterator start, end;
  start = mCurrentPosition;
  end = mEndPosition;

  return CopyUnicodeTo(start, end, aCopyBuffer);
}

/**
 *  Conduct self test. Actually, selftesting for this class
 *  occurs in the parser selftest.
 *
 *  @update  gess 3/25/98
 *  @param
 *  @return
 */

void nsScanner::SelfTest(void) {
#ifdef _DEBUG
#endif
}
