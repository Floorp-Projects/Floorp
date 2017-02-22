/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SlicedInputStream.h"
#include "nsISeekableStream.h"
#include "nsStreamUtils.h"

NS_IMPL_ISUPPORTS(SlicedInputStream, nsIInputStream,
                  nsICloneableInputStream, nsIAsyncInputStream)

SlicedInputStream::SlicedInputStream(nsIInputStream* aInputStream,
                                     uint64_t aStart, uint64_t aLength)
  : mInputStream(aInputStream)
  , mStart(aStart)
  , mLength(aLength)
  , mCurPos(0)
  , mClosed(false)
{
  MOZ_ASSERT(aInputStream);
}

SlicedInputStream::~SlicedInputStream()
{}

NS_IMETHODIMP
SlicedInputStream::Close()
{
  mClosed = true;
  return NS_OK;
}

// nsIInputStream interface

NS_IMETHODIMP
SlicedInputStream::Available(uint64_t* aLength)
{
  if (mClosed) {
    return NS_BASE_STREAM_CLOSED;
  }

  nsresult rv = mInputStream->Available(aLength);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Let's remove extra length from the end.
  if (*aLength + mCurPos > mStart + mLength) {
    *aLength -= XPCOM_MIN(*aLength, (*aLength + mCurPos) - (mStart + mLength));
  }

  // Let's remove extra length from the begin.
  if (mCurPos < mStart) {
    *aLength -= XPCOM_MIN(*aLength, mStart - mCurPos);
  }

  return NS_OK;
}

NS_IMETHODIMP
SlicedInputStream::Read(char* aBuffer, uint32_t aCount, uint32_t* aReadCount)
{
  return ReadSegments(NS_CopySegmentToBuffer, aBuffer, aCount, aReadCount);
}

NS_IMETHODIMP
SlicedInputStream::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                                uint32_t aCount, uint32_t *aResult)
{
  uint32_t result;

  if (!aResult) {
    aResult = &result;
  }

  *aResult = 0;

  if (mClosed) {
    return NS_BASE_STREAM_CLOSED;
  }

  if (mCurPos < mStart) {
    nsCOMPtr<nsISeekableStream> seekableStream =
      do_QueryInterface(mInputStream);
    if (seekableStream) {
      nsresult rv = seekableStream->Seek(nsISeekableStream::NS_SEEK_SET,
                                         mStart);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      mCurPos = mStart;
    } else {
      char buf[4096];
      while (mCurPos < mStart) {
	uint32_t bytesRead;
	uint64_t bufCount = XPCOM_MIN(mStart - mCurPos, (uint64_t)sizeof(buf));
	nsresult rv = mInputStream->Read(buf, bufCount, &bytesRead);
	if (NS_WARN_IF(NS_FAILED(rv)) || bytesRead == 0) {
	  return rv;
	}

         mCurPos += bytesRead;
      }
    }
  }

  // Let's reduce aCount in case it's too big.
  if (mCurPos + aCount > mStart + mLength) {
    aCount = mStart + mLength - mCurPos;
  }

  char buf[4096];
  while (mCurPos < mStart + mLength && *aResult < aCount) {
    uint32_t bytesRead;
    uint64_t bufCount = XPCOM_MIN(aCount - *aResult, (uint32_t)sizeof(buf));
    nsresult rv = mInputStream->Read(buf, bufCount, &bytesRead);
    if (NS_WARN_IF(NS_FAILED(rv)) || bytesRead == 0) {
      return rv;
    }

    mCurPos += bytesRead;

    uint32_t bytesWritten = 0;
    while (bytesWritten < bytesRead) {
      uint32_t writerCount = 0;
      rv = aWriter(this, aClosure, buf + bytesWritten, *aResult,
                   bytesRead - bytesWritten, &writerCount);
      if (NS_FAILED(rv) || writerCount == 0) {
	return NS_OK;
      }

      MOZ_ASSERT(writerCount <= bytesRead - bytesWritten);
      bytesWritten += writerCount;
      *aResult += writerCount;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
SlicedInputStream::IsNonBlocking(bool* aNonBlocking)
{
  return mInputStream->IsNonBlocking(aNonBlocking);
}

// nsICloneableInputStream interface

NS_IMETHODIMP
SlicedInputStream::GetCloneable(bool* aCloneable)
{
  *aCloneable = true;
  return NS_OK;
}

NS_IMETHODIMP
SlicedInputStream::Clone(nsIInputStream** aResult)
{
  nsCOMPtr<nsIInputStream> clonedStream;
  nsCOMPtr<nsIInputStream> replacementStream;

  nsresult rv = NS_CloneInputStream(mInputStream, getter_AddRefs(clonedStream),
                                    getter_AddRefs(replacementStream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (replacementStream) {
    mInputStream = replacementStream.forget();
  }

  nsCOMPtr<nsIInputStream> sis =
    new SlicedInputStream(clonedStream, mStart, mLength);

  sis.forget(aResult);
  return NS_OK;
}

// nsIAsyncInputStream interface

NS_IMETHODIMP
SlicedInputStream::CloseWithStatus(nsresult aStatus)
{
  nsCOMPtr<nsIAsyncInputStream> asyncStream =
    do_QueryInterface(mInputStream);
  if (!asyncStream) {
    return NS_ERROR_FAILURE;
  }

  return asyncStream->CloseWithStatus(aStatus);
}

NS_IMETHODIMP
SlicedInputStream::AsyncWait(nsIInputStreamCallback* aCallback,
                             uint32_t aFlags,
                             uint32_t aRequestedCount,
                             nsIEventTarget* aEventTarget)
{
  nsCOMPtr<nsIAsyncInputStream> asyncStream =
    do_QueryInterface(mInputStream);
  if (!asyncStream) {
    return NS_ERROR_FAILURE;
  }

  return asyncStream->AsyncWait(aCallback, aFlags, aRequestedCount,
                                aEventTarget);
}
