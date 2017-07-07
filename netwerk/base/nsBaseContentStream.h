/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBaseContentStream_h__
#define nsBaseContentStream_h__

#include "nsIAsyncInputStream.h"
#include "nsIEventTarget.h"
#include "nsCOMPtr.h"

//-----------------------------------------------------------------------------
// nsBaseContentStream is designed to be subclassed with the intention of being
// used to satisfy the nsBaseChannel::OpenContentStream method.
//
// The subclass typically overrides the default Available, ReadSegments and
// CloseWithStatus methods.  By default, Read is implemented in terms of
// ReadSegments, and Close is implemented in terms of CloseWithStatus.  If
// CloseWithStatus is overriden, then the subclass will usually want to call
// the base class' CloseWithStatus method before returning.
//
// If the stream is non-blocking, then readSegments may return the exception
// NS_BASE_STREAM_WOULD_BLOCK if there is no data available and the stream is
// not at the "end-of-file" or already closed.  This error code must not be
// returned from the Available implementation.  When the caller receives this
// error code, he may choose to call the stream's AsyncWait method, in which
// case the base stream will have a non-null PendingCallback.  When the stream
// has data or encounters an error, it should be sure to dispatch a pending
// callback if one exists (see DispatchCallback).  The implementation of the
// base stream's CloseWithStatus (and Close) method will ensure that any
// pending callback is dispatched.  It is the responsibility of the subclass
// to ensure that the pending callback is dispatched when it wants to have its
// ReadSegments method called again.

class nsBaseContentStream : public nsIAsyncInputStream
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM

  explicit nsBaseContentStream(bool nonBlocking)
    : mStatus(NS_OK)
    , mNonBlocking(nonBlocking) {
  }

  nsresult Status() { return mStatus; }
  bool IsNonBlocking() { return mNonBlocking; }
  bool IsClosed() { return NS_FAILED(mStatus); }

  // Called to test if the stream has a pending callback.
  bool HasPendingCallback() { return mCallback != nullptr; }

  // The current dispatch target (may be null) for the pending callback if any.
  nsIEventTarget *CallbackTarget() { return mCallbackTarget; }

  // Called to dispatch a pending callback.  If there is no pending callback,
  // then this function does nothing.  Pass true to this function to cause the
  // callback to occur asynchronously; otherwise, the callback will happen
  // before this function returns.
  void DispatchCallback(bool async = true);

  // Helper function to make code more self-documenting.
  void DispatchCallbackSync() { DispatchCallback(false); }

protected:
  virtual ~nsBaseContentStream() {}

private:
  // Called from the base stream's AsyncWait method when a pending callback
  // is installed on the stream.
  virtual void OnCallbackPending() {}

private:
  nsCOMPtr<nsIInputStreamCallback> mCallback;
  nsCOMPtr<nsIEventTarget>         mCallbackTarget;
  nsresult                         mStatus;
  bool                             mNonBlocking;
};

#endif // nsBaseContentStream_h__
