/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SimpleBuffer.h"
#include <algorithm>

namespace mozilla {
namespace net {

SimpleBuffer::SimpleBuffer()
  : mStatus(NS_OK)
  , mAvailable(0)
{
  mOwningThread = PR_GetCurrentThread();
}

nsresult SimpleBuffer::Write(char *src, size_t len)
{
  MOZ_ASSERT(PR_GetCurrentThread() == mOwningThread);
  if (NS_FAILED(mStatus)) {
    return mStatus;
  }

  while (len > 0) {
    SimpleBufferPage *p = mBufferList.getLast();
    if (p && (p->mWriteOffset == SimpleBufferPage::kSimpleBufferPageSize)) {
      // no room.. make a new page
      p = nullptr;
    }
    if (!p) {
      p = new (fallible) SimpleBufferPage();
      if (!p) {
        mStatus = NS_ERROR_OUT_OF_MEMORY;
        return mStatus;
      }
      mBufferList.insertBack(p);
    }
    size_t roomOnPage = SimpleBufferPage::kSimpleBufferPageSize - p->mWriteOffset;
    size_t toWrite = std::min(roomOnPage, len);
    memcpy(p->mBuffer + p->mWriteOffset, src, toWrite);
    src += toWrite;
    len -= toWrite;
    p->mWriteOffset += toWrite;
    mAvailable += toWrite;
  }
  return NS_OK;
}

size_t SimpleBuffer::Read(char *dest, size_t maxLen)
{
  MOZ_ASSERT(PR_GetCurrentThread() == mOwningThread);
  if (NS_FAILED(mStatus)) {
    return 0;
  }

  size_t rv = 0;
  for (SimpleBufferPage *p = mBufferList.getFirst();
       p && (rv < maxLen); p = mBufferList.getFirst()) {
    size_t avail = p->mWriteOffset - p->mReadOffset;
    size_t toRead = std::min(avail, (maxLen - rv));
    memcpy(dest + rv, p->mBuffer + p->mReadOffset, toRead);
    rv += toRead;
    p->mReadOffset += toRead;
    if (p->mReadOffset == p->mWriteOffset) {
      p->remove();
      delete p;
    }
  }

  MOZ_ASSERT(mAvailable >= rv);
  mAvailable -= rv;
  return rv;
}

size_t SimpleBuffer::Available()
{
  MOZ_ASSERT(PR_GetCurrentThread() == mOwningThread);
  return NS_SUCCEEDED(mStatus) ? mAvailable : 0;
}

void SimpleBuffer::Clear()
{
  MOZ_ASSERT(PR_GetCurrentThread() == mOwningThread);
  SimpleBufferPage *p;
  while ((p = mBufferList.popFirst())) {
    delete p;
  }
  mAvailable = 0;
}

} // namespace net
} // namespace mozilla
