/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsBaseContentStream.h"
#include "nsStreamUtils.h"

//-----------------------------------------------------------------------------

void
nsBaseContentStream::DispatchCallback(PRBool async)
{
  if (!mCallback)
    return;

  // It's important to clear mCallback and mCallbackTarget up-front because the
  // OnInputStreamReady implementation may call our AsyncWait method.

  nsCOMPtr<nsIInputStreamCallback> callback;
  if (async) {
    NS_NewInputStreamReadyEvent(getter_AddRefs(callback), mCallback,
                                mCallbackTarget);
    if (!callback)
      return;  // out of memory!
    mCallback = nsnull;
  } else {
    callback.swap(mCallback);
  }
  mCallbackTarget = nsnull;

  callback->OnInputStreamReady(this);
}

//-----------------------------------------------------------------------------
// nsBaseContentStream::nsISupports

NS_IMPL_THREADSAFE_ADDREF(nsBaseContentStream)
NS_IMPL_THREADSAFE_RELEASE(nsBaseContentStream)

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
nsBaseContentStream::Available(PRUint32 *result)
{
  *result = 0;
  return mStatus;
}

NS_IMETHODIMP
nsBaseContentStream::Read(char *buf, PRUint32 count, PRUint32 *result)
{
  return ReadSegments(NS_CopySegmentToBuffer, buf, count, result); 
}

NS_IMETHODIMP
nsBaseContentStream::ReadSegments(nsWriteSegmentFun fun, void *closure,
                                  PRUint32 count, PRUint32 *result)
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
nsBaseContentStream::IsNonBlocking(PRBool *result)
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
                               PRUint32 flags, PRUint32 requestedCount,
                               nsIEventTarget *target)
{
  // Our _only_ consumer is nsInputStreamPump, so we simplify things here by
  // making assumptions about how we will be called.
  NS_ASSERTION(target, "unexpected parameter");
  NS_ASSERTION(flags == 0, "unexpected parameter");
  NS_ASSERTION(requestedCount == 0, "unexpected parameter");

#ifdef DEBUG
  PRBool correctThread;
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
