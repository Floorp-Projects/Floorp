/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBaseContentStream.h"
#include "nsStreamUtils.h"

//-----------------------------------------------------------------------------

void
nsBaseContentStream::DispatchCallback(bool async)
{
  if (!mCallback)
    return;

  // It's important to clear mCallback and mCallbackTarget up-front because the
  // OnInputStreamReady implementation may call our AsyncWait method.

  nsCOMPtr<nsIInputStreamCallback> callback;
  if (async) {
    callback = NS_NewInputStreamReadyEvent(mCallback, mCallbackTarget);
    mCallback = nullptr;
  } else {
    callback.swap(mCallback);
  }
  mCallbackTarget = nullptr;

  callback->OnInputStreamReady(this);
}

//-----------------------------------------------------------------------------
// nsBaseContentStream::nsISupports

NS_IMPL_ADDREF(nsBaseContentStream)
NS_IMPL_RELEASE(nsBaseContentStream)

// We only support nsIAsyncInputStream when we are in non-blocking mode.
NS_INTERFACE_MAP_BEGIN(nsBaseContentStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAsyncInputStream, mNonBlocking)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
NS_INTERFACE_MAP_END_THREADSAFE

//-----------------------------------------------------------------------------
// nsBaseContentStream::nsIInputStream

NS_IMETHODIMP
nsBaseContentStream::Close()
{
  return IsClosed() ? NS_OK : CloseWithStatus(NS_BASE_STREAM_CLOSED);
}

NS_IMETHODIMP
nsBaseContentStream::Available(uint64_t *result)
{
  *result = 0;
  return mStatus;
}

NS_IMETHODIMP
nsBaseContentStream::Read(char *buf, uint32_t count, uint32_t *result)
{
  return ReadSegments(NS_CopySegmentToBuffer, buf, count, result); 
}

NS_IMETHODIMP
nsBaseContentStream::ReadSegments(nsWriteSegmentFun fun, void *closure,
                                  uint32_t count, uint32_t *result)
{
  *result = 0;

  if (mStatus == NS_BASE_STREAM_CLOSED)
    return NS_OK;

  // No data yet
  if (!IsClosed() && IsNonBlocking())
    return NS_BASE_STREAM_WOULD_BLOCK;

  return mStatus;
}

NS_IMETHODIMP
nsBaseContentStream::IsNonBlocking(bool *result)
{
  *result = mNonBlocking;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsBaseContentStream::nsIAsyncInputStream

NS_IMETHODIMP
nsBaseContentStream::CloseWithStatus(nsresult status)
{
  if (IsClosed())
    return NS_OK;

  NS_ENSURE_ARG(NS_FAILED(status));
  mStatus = status;

  DispatchCallback();
  return NS_OK;
}

NS_IMETHODIMP
nsBaseContentStream::AsyncWait(nsIInputStreamCallback *callback,
                               uint32_t flags, uint32_t requestedCount,
                               nsIEventTarget *target)
{
  // Our _only_ consumer is nsInputStreamPump, so we simplify things here by
  // making assumptions about how we will be called.
  NS_ASSERTION(target, "unexpected parameter");
  NS_ASSERTION(flags == 0, "unexpected parameter");
  NS_ASSERTION(requestedCount == 0, "unexpected parameter");

#ifdef DEBUG
  bool correctThread;
  target->IsOnCurrentThread(&correctThread);
  NS_ASSERTION(correctThread, "event target must be on the current thread");
#endif

  mCallback = callback;
  mCallbackTarget = target;

  if (!mCallback)
    return NS_OK;

  // If we're already closed, then dispatch this callback immediately.
  if (IsClosed()) {
    DispatchCallback();
    return NS_OK;
  }

  OnCallbackPending();
  return NS_OK;
}
