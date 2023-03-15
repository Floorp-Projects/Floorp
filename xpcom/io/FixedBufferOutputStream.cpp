/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FixedBufferOutputStream.h"

#include "mozilla/RefPtr.h"
#include "mozilla/StreamBufferSinkImpl.h"
#include "nsStreamUtils.h"
#include "nsString.h"

namespace mozilla {

FixedBufferOutputStream::FixedBufferOutputStream(
    UniquePtr<StreamBufferSink>&& aSink)
    : mSink(std::move(aSink)),
      mMutex("FixedBufferOutputStream::mMutex"),
      mOffset(0),
      mWriting(false),
      mClosed(false) {}

// static
RefPtr<FixedBufferOutputStream> FixedBufferOutputStream::Create(
    size_t aLength) {
  MOZ_ASSERT(aLength);

  return MakeRefPtr<FixedBufferOutputStream>(MakeUnique<BufferSink>(aLength));
}

// static
RefPtr<FixedBufferOutputStream> FixedBufferOutputStream::Create(
    size_t aLength, const mozilla::fallible_t&) {
  MOZ_ASSERT(aLength);

  auto sink = BufferSink::Alloc(aLength);

  if (NS_WARN_IF(!sink)) {
    return nullptr;
  }

  return MakeRefPtr<FixedBufferOutputStream>(std::move(sink));
}

// static
RefPtr<FixedBufferOutputStream> FixedBufferOutputStream::Create(
    mozilla::Span<char> aBuffer) {
  return MakeRefPtr<FixedBufferOutputStream>(
      MakeUnique<nsBorrowedSink>(aBuffer));
}

// static
RefPtr<FixedBufferOutputStream> FixedBufferOutputStream::Create(
    UniquePtr<StreamBufferSink>&& aSink) {
  return MakeRefPtr<FixedBufferOutputStream>(std::move(aSink));
}

nsDependentCSubstring FixedBufferOutputStream::WrittenData() {
  MutexAutoLock autoLock(mMutex);

  return mSink->Slice(mOffset);
}

NS_IMPL_ISUPPORTS(FixedBufferOutputStream, nsIOutputStream)

NS_IMETHODIMP
FixedBufferOutputStream::Close() {
  MutexAutoLock autoLock(mMutex);

  if (mWriting) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mClosed = true;

  return NS_OK;
}

NS_IMETHODIMP
FixedBufferOutputStream::Flush() { return NS_OK; }

NS_IMETHODIMP
FixedBufferOutputStream::StreamStatus() {
  MutexAutoLock autoLock(mMutex);
  return mClosed ? NS_BASE_STREAM_CLOSED : NS_OK;
}

NS_IMETHODIMP
FixedBufferOutputStream::Write(const char* aBuf, uint32_t aCount,
                               uint32_t* _retval) {
  return WriteSegments(NS_CopyBufferToSegment, const_cast<char*>(aBuf), aCount,
                       _retval);
}

NS_IMETHODIMP
FixedBufferOutputStream::WriteFrom(nsIInputStream* aFromStream, uint32_t aCount,
                                   uint32_t* _retval) {
  return WriteSegments(NS_CopyStreamToSegment, aFromStream, aCount, _retval);
}

NS_IMETHODIMP
FixedBufferOutputStream::WriteSegments(nsReadSegmentFun aReader, void* aClosure,
                                       uint32_t aCount, uint32_t* _retval) {
  MOZ_ASSERT(_retval);

  MutexAutoLock autoLock(mMutex);

  if (mWriting) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (mClosed) {
    return NS_BASE_STREAM_CLOSED;
  }

  size_t length = mSink->Data().Length();
  size_t offset = mOffset;

  MOZ_ASSERT(length >= offset, "Bad stream state!");

  size_t maxCount = length - offset;
  if (maxCount == 0) {
    *_retval = 0;
    return NS_OK;
  }

  if (aCount > maxCount) {
    aCount = maxCount;
  }

  mWriting = true;

  nsresult rv;

  {
    MutexAutoUnlock autoUnlock(mMutex);

    rv = aReader(this, aClosure, mSink->Data().Elements() + offset, 0, aCount,
                 _retval);
  }

  mWriting = false;

  if (NS_SUCCEEDED(rv)) {
    MOZ_ASSERT(*_retval <= aCount,
               "Reader should not read more than we asked it to read!");
    mOffset += *_retval;
  }

  // As defined in nsIOutputStream.idl, do not pass reader func errors.
  return NS_OK;
}

NS_IMETHODIMP
FixedBufferOutputStream::IsNonBlocking(bool* _retval) {
  *_retval = true;
  return NS_OK;
}

}  // namespace mozilla
