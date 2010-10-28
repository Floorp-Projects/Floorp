/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

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
 * The Initial Developer of the Original Code is
 *  The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alon Zakai <azakai@mozilla.com>
 *   Josh Matthews <josh@joshmatthews.net>
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

#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/FTPChannelChild.h"
#include "nsFtpProtocolHandler.h"

#include "nsStringStream.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"
#include "nsIURIFixup.h"
#include "nsCDefaultURIFixup.h"

#undef LOG
#define LOG(args) PR_LOG(gFTPLog, PR_LOG_DEBUG, args)

namespace mozilla {
namespace net {

FTPChannelChild::FTPChannelChild(nsIURI* uri)
: ChannelEventQueue<FTPChannelChild>(this)
, mIPCOpen(false)
, mCanceled(false)
, mSuspendCount(0)
, mIsPending(PR_FALSE)
, mWasOpened(PR_FALSE)
, mLastModifiedTime(0)
, mStartPos(0)
{
  LOG(("Creating FTPChannelChild @%x\n", this));
  // grab a reference to the handler to ensure that it doesn't go away.
  NS_ADDREF(gFtpHandler);
  SetURI(uri);
}

FTPChannelChild::~FTPChannelChild()
{
  LOG(("Destroying FTPChannelChild @%x\n", this));
  gFtpHandler->Release();
}

void
FTPChannelChild::AddIPDLReference()
{
  NS_ABORT_IF_FALSE(!mIPCOpen, "Attempt to retain more than one IPDL reference");
  mIPCOpen = true;
  AddRef();
}

void
FTPChannelChild::ReleaseIPDLReference()
{
  NS_ABORT_IF_FALSE(mIPCOpen, "Attempt to release nonexistent IPDL reference");
  mIPCOpen = false;
  Release();
}

//-----------------------------------------------------------------------------
// FTPChannelChild::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS_INHERITED4(FTPChannelChild,
                             nsBaseChannel,
                             nsIFTPChannel,
                             nsIUploadChannel,
                             nsIResumableChannel,
                             nsIProxiedChannel)

//-----------------------------------------------------------------------------

NS_IMETHODIMP
FTPChannelChild::GetLastModifiedTime(PRTime* lastModifiedTime)
{
  *lastModifiedTime = mLastModifiedTime;
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::SetLastModifiedTime(PRTime lastModifiedTime)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
FTPChannelChild::ResumeAt(PRUint64 aStartPos, const nsACString& aEntityID)
{
  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  mStartPos = aStartPos;
  mEntityID = aEntityID;
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::GetEntityID(nsACString& entityID)
{
  entityID = mEntityID;
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::GetProxyInfo(nsIProxyInfo** aProxyInfo)
{
  DROP_DEAD();
}

NS_IMETHODIMP
FTPChannelChild::SetUploadStream(nsIInputStream* stream,
                                 const nsACString& contentType,
                                 PRInt32 contentLength)
{
  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  mUploadStream = stream;
  // NOTE: contentLength is intentionally ignored here.
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::GetUploadStream(nsIInputStream** stream)
{
  NS_ENSURE_ARG_POINTER(stream);
  *stream = mUploadStream;
  NS_IF_ADDREF(*stream);
  return NS_OK;
}

//-----------------------------------------------------------------------------

NS_IMETHODIMP
FTPChannelChild::AsyncOpen(::nsIStreamListener* listener, nsISupports* aContext)
{
  LOG(("FTPChannelChild::AsyncOpen [this=%x]\n", this));

  NS_ENSURE_TRUE((gNeckoChild), NS_ERROR_FAILURE);
  NS_ENSURE_ARG_POINTER(listener);
  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

  // Port checked in parent, but duplicate here so we can return with error
  // immediately, as we've done since before e10s.
  nsresult rv;
  rv = NS_CheckPortSafety(nsBaseChannel::URI()); // Need to disambiguate,
                                                 // because in the child ipdl,
                                                 // a typedef URI is defined...
  if (NS_FAILED(rv))
    return rv;

  // FIXME: like bug 558623, merge constructor+SendAsyncOpen into 1 IPC msg
  gNeckoChild->SendPFTPChannelConstructor(this);
  mListener = listener;
  mListenerContext = aContext;

  // add ourselves to the load group. 
  if (mLoadGroup)
    mLoadGroup->AddRequest(this, nsnull);

  SendAsyncOpen(nsBaseChannel::URI(), mStartPos, mEntityID,
                IPC::InputStream(mUploadStream));

  // The socket transport layer in the chrome process now has a logical ref to
  // us until OnStopRequest is called.
  AddIPDLReference();

  mIsPending = PR_TRUE;
  mWasOpened = PR_TRUE;

  return rv;
}

NS_IMETHODIMP
FTPChannelChild::IsPending(PRBool* result)
{
  *result = mIsPending;
  return NS_OK;
}

nsresult
FTPChannelChild::OpenContentStream(PRBool async,
                                   nsIInputStream** stream,
                                   nsIChannel** channel)
{
  NS_RUNTIMEABORT("FTPChannel*Child* should never have OpenContentStream called!");
  return NS_OK;
}
  
//-----------------------------------------------------------------------------
// FTPChannelChild::PFTPChannelChild
//-----------------------------------------------------------------------------

class FTPStartRequestEvent : public ChannelEvent
{
 public:
  FTPStartRequestEvent(FTPChannelChild* aChild, const PRInt32& aContentLength,
                       const nsCString& aContentType, const PRTime& aLastModified,
                       const nsCString& aEntityID, const IPC::URI& aURI)
  : mChild(aChild), mContentLength(aContentLength), mContentType(aContentType),
    mLastModified(aLastModified), mEntityID(aEntityID), mURI(aURI) {}
  void Run() { mChild->DoOnStartRequest(mContentLength, mContentType,
                                       mLastModified, mEntityID, mURI); }
 private:
  FTPChannelChild* mChild;
  PRInt32 mContentLength;
  nsCString mContentType;
  PRTime mLastModified;
  nsCString mEntityID;
  IPC::URI mURI;
};

bool
FTPChannelChild::RecvOnStartRequest(const PRInt32& aContentLength,
                                    const nsCString& aContentType,
                                    const PRTime& aLastModified,
                                    const nsCString& aEntityID,
                                    const IPC::URI& aURI)
{
  if (ShouldEnqueue()) {
    EnqueueEvent(new FTPStartRequestEvent(this, aContentLength, aContentType,
                                          aLastModified, aEntityID, aURI));
  } else {
    DoOnStartRequest(aContentLength, aContentType, aLastModified,
                     aEntityID, aURI);
  }
  return true;
}

void
FTPChannelChild::DoOnStartRequest(const PRInt32& aContentLength,
                                  const nsCString& aContentType,
                                  const PRTime& aLastModified,
                                  const nsCString& aEntityID,
                                  const IPC::URI& aURI)
{
  LOG(("FTPChannelChild::RecvOnStartRequest [this=%x]\n", this));

  SetContentLength(aContentLength);
  SetContentType(aContentType);
  mLastModifiedTime = aLastModified;
  mEntityID = aEntityID;

  nsCString spec;
  nsCOMPtr<nsIURI> uri(aURI);
  uri->GetSpec(spec);
  nsBaseChannel::URI()->SetSpec(spec);

  AutoEventEnqueuer ensureSerialDispatch(this);
  nsresult rv = mListener->OnStartRequest(this, mListenerContext);
  if (NS_FAILED(rv))
    Cancel(rv);
}

class FTPDataAvailableEvent : public ChannelEvent
{
 public:
  FTPDataAvailableEvent(FTPChannelChild* aChild, const nsCString& aData,
                        const PRUint32& aOffset, const PRUint32& aCount)
  : mChild(aChild), mData(aData), mOffset(aOffset), mCount(aCount) {}
  void Run() { mChild->DoOnDataAvailable(mData, mOffset, mCount); }
 private:
  FTPChannelChild* mChild;
  nsCString mData;
  PRUint32 mOffset, mCount;
};

bool
FTPChannelChild::RecvOnDataAvailable(const nsCString& data,
                                     const PRUint32& offset,
                                     const PRUint32& count)
{
  if (ShouldEnqueue()) {
    EnqueueEvent(new FTPDataAvailableEvent(this, data, offset, count));
  } else {
    DoOnDataAvailable(data, offset, count);
  }
  return true;
}

void
FTPChannelChild::DoOnDataAvailable(const nsCString& data,
                                   const PRUint32& offset,
                                   const PRUint32& count)
{
  LOG(("FTPChannelChild::RecvOnDataAvailable [this=%x]\n", this));

  if (mCanceled)
    return;

  // NOTE: the OnDataAvailable contract requires the client to read all the data
  // in the inputstream.  This code relies on that ('data' will go away after
  // this function).  Apparently the previous, non-e10s behavior was to actually
  // support only reading part of the data, allowing later calls to read the
  // rest.
  nsCOMPtr<nsIInputStream> stringStream;
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stringStream),
                                      data.get(),
                                      count,
                                      NS_ASSIGNMENT_DEPEND);
  if (NS_FAILED(rv)) {
    Cancel(rv);
    return;
  }

  AutoEventEnqueuer ensureSerialDispatch(this);
  rv = mListener->OnDataAvailable(this, mListenerContext,
                                  stringStream, offset, count);
  if (NS_FAILED(rv))
    Cancel(rv);
  stringStream->Close();
}

class FTPStopRequestEvent : public ChannelEvent
{
 public:
  FTPStopRequestEvent(FTPChannelChild* aChild, const nsresult& aStatusCode)
  : mChild(aChild), mStatusCode(aStatusCode) {}
  void Run() { mChild->DoOnStopRequest(mStatusCode); }
 private:
  FTPChannelChild* mChild;
  nsresult mStatusCode;
};

bool
FTPChannelChild::RecvOnStopRequest(const nsresult& statusCode)
{
  if (ShouldEnqueue()) {
    EnqueueEvent(new FTPStopRequestEvent(this, statusCode));
  } else {
    DoOnStopRequest(statusCode);
  }
  return true;
}

void
FTPChannelChild::DoOnStopRequest(const nsresult& statusCode)
{
  LOG(("FTPChannelChild::RecvOnStopRequest [this=%x status=%u]\n",
           this, statusCode));

  if (!mCanceled)
    mStatus = statusCode;

  { // Ensure that all queued ipdl events are dispatched before
    // we initiate protocol deletion below.
    mIsPending = PR_FALSE;
    AutoEventEnqueuer ensureSerialDispatch(this);
    (void)mListener->OnStopRequest(this, mListenerContext, statusCode);
    mListener = nsnull;
    mListenerContext = nsnull;

    if (mLoadGroup)
      mLoadGroup->RemoveRequest(this, nsnull, statusCode);
  }

  // This calls NeckoChild::DeallocPFTPChannel(), which deletes |this| if IPDL
  // holds the last reference.  Don't rely on |this| existing after here!
  Send__delete__(this);
}

class FTPCancelEarlyEvent : public ChannelEvent
{
 public:
  FTPCancelEarlyEvent(FTPChannelChild* aChild, nsresult aStatus)
  : mChild(aChild), mStatus(aStatus) {}
  void Run() { mChild->DoCancelEarly(mStatus); }
 private:
  FTPChannelChild* mChild;
  nsresult mStatus;
};

bool
FTPChannelChild::RecvCancelEarly(const nsresult& statusCode)
{
  if (ShouldEnqueue()) {
    EnqueueEvent(new FTPCancelEarlyEvent(this, statusCode));
  } else {
    DoCancelEarly(statusCode);
  }
  return true;
}

void
FTPChannelChild::DoCancelEarly(const nsresult& statusCode)
{
  if (mCanceled)
    return;

  mCanceled = true;
  mStatus = statusCode;
  mIsPending = PR_FALSE;
  
  if (mLoadGroup)
    mLoadGroup->RemoveRequest(this, nsnull, statusCode);

  if (mListener) {
    mListener->OnStartRequest(this, mListenerContext);
    mListener->OnStopRequest(this, mListenerContext, statusCode);
  }

  mListener = nsnull;
  mListenerContext = nsnull;

  if (mIPCOpen)
    Send__delete__(this);
}

NS_IMETHODIMP
FTPChannelChild::Cancel(nsresult status)
{
  if (mCanceled)
    return NS_OK;

  mCanceled = true;
  mStatus = status;
  if (mIPCOpen)
    SendCancel(status);
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::Suspend()
{
  NS_ENSURE_TRUE(mIPCOpen, NS_ERROR_NOT_AVAILABLE);
  mSuspendCount++;
  SendSuspend();
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::Resume()
{
  NS_ENSURE_TRUE(mIPCOpen, NS_ERROR_NOT_AVAILABLE);
  SendResume();
  --mSuspendCount;
  if (!mSuspendCount) {
    if (mQueuePhase == PHASE_UNQUEUED)
      mQueuePhase = PHASE_FINISHED_QUEUEING;
    FlushEventQueue();
  }
  return NS_OK;
}

} // namespace net
} // namespace mozilla

