/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTemporaryFileInputStream.h"
#include "nsStreamUtils.h"

NS_IMPL_ISUPPORTS1(nsTemporaryFileInputStream, nsIInputStream)

nsTemporaryFileInputStream::nsTemporaryFileInputStream(FileDescOwner* aFileDescOwner, uint64_t aStartPos, uint64_t aEndPos)
  : mFileDescOwner(aFileDescOwner),
    mStartPos(aStartPos),
    mEndPos(aEndPos),
    mClosed(false)
{ 
  NS_ASSERTION(aStartPos <= aEndPos, "StartPos should less equal than EndPos!");
}

NS_IMETHODIMP
nsTemporaryFileInputStream::Close()
{
  mClosed = true;
  return NS_OK;
}

NS_IMETHODIMP
nsTemporaryFileInputStream::Available(uint64_t * bytesAvailable)
{
  if (mClosed)
    return NS_BASE_STREAM_CLOSED;

  NS_ASSERTION(mStartPos <= mEndPos, "StartPos should less equal than EndPos!");

  *bytesAvailable = mEndPos - mStartPos;
  return NS_OK;
}

NS_IMETHODIMP
nsTemporaryFileInputStream::Read(char* buffer, uint32_t count, uint32_t* bytesRead)
{
  return ReadSegments(NS_CopySegmentToBuffer, buffer, count, bytesRead);
}

NS_IMETHODIMP
nsTemporaryFileInputStream::ReadSegments(nsWriteSegmentFun writer,
                                         void *            closure,
                                         uint32_t          count,
                                         uint32_t *        result)
{
  NS_ASSERTION(result, "null ptr");
  NS_ASSERTION(mStartPos <= mEndPos, "bad stream state");
  *result = 0;

  if (mClosed) {
    return NS_BASE_STREAM_CLOSED;
  }

  mozilla::MutexAutoLock lock(mFileDescOwner->FileMutex());
  PR_Seek64(mFileDescOwner->mFD, mStartPos, PR_SEEK_SET);

  count = std::min(count, uint32_t(mEndPos - mStartPos));
  uint32_t remainBufCount = count;

  char buf[4096];
  while (remainBufCount > 0) {
    uint32_t bufCount = std::min(remainBufCount, (uint32_t)sizeof(buf));
    int32_t read_result = PR_Read(mFileDescOwner->mFD, buf, bufCount);
    if (read_result < 0) {
      return NS_ErrorAccordingToNSPR();
    }
    uint32_t write_result = 0;
    nsresult rv = writer(this, closure, buf,
                         count - remainBufCount, bufCount, &write_result);
    remainBufCount -= bufCount;
    if (NS_SUCCEEDED(rv)) {
      NS_ASSERTION(write_result <= bufCount,
                   "writer should not write more than we asked it to write");
    }
    mStartPos += bufCount;
  }
  *result = count;
  return NS_OK;
}

NS_IMETHODIMP
nsTemporaryFileInputStream::IsNonBlocking(bool * nonBlocking)
{
  *nonBlocking = false;
  return NS_OK;
}

