/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/FTPChannelChild.h"
#include "mozilla/dom/TabChild.h"
#include "nsFtpProtocolHandler.h"
#include "nsITabChild.h"
#include "nsStringStream.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"
#include "nsIURIFixup.h"
#include "nsILoadContext.h"
#include "nsCDefaultURIFixup.h"
#include "base/compiler_specific.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/URIUtils.h"

using namespace mozilla::ipc;

#undef LOG
#define LOG(args) PR_LOG(gFTPLog, PR_LOG_DEBUG, args)

namespace mozilla {
namespace net {

FTPChannelChild::FTPChannelChild(nsIURI* uri)
: mIPCOpen(false)
, ALLOW_THIS_IN_INITIALIZER_LIST(mEventQ(static_cast<nsIFTPChannel*>(this)))
, mCanceled(false)
, mSuspendCount(0)
, mIsPending(false)
, mWasOpened(false)
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

NS_IMPL_ISUPPORTS_INHERITED5(FTPChannelChild,
                             nsBaseChannel,
                             nsIFTPChannel,
                             nsIUploadChannel,
                             nsIResumableChannel,
                             nsIProxiedChannel,
                             nsIChildChannel)

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
FTPChannelChild::ResumeAt(uint64_t aStartPos, const nsACString& aEntityID)
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
                                 int64_t contentLength)
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

  mozilla::dom::TabChild* tabChild = nullptr;
  nsCOMPtr<nsITabChild> iTabChild;
  NS_QueryNotificationCallbacks(mCallbacks, mLoadGroup,
                                NS_GET_IID(nsITabChild),
                                getter_AddRefs(iTabChild));
  GetCallback(iTabChild);
  if (iTabChild) {
    tabChild = static_cast<mozilla::dom::TabChild*>(iTabChild.get());
  }

  // FIXME: like bug 558623, merge constructor+SendAsyncOpen into 1 IPC msg
  gNeckoChild->SendPFTPChannelConstructor(this, tabChild, IPC::SerializedLoadContext(this));
  mListener = listener;
  mListenerContext = aContext;

  // add ourselves to the load group. 
  if (mLoadGroup)
    mLoadGroup->AddRequest(this, nullptr);

  URIParams uri;
  SerializeURI(nsBaseChannel::URI(), uri);

  OptionalInputStreamParams uploadStream;
  SerializeInputStream(mUploadStream, uploadStream);

  SendAsyncOpen(uri, mStartPos, mEntityID, uploadStream);

  // The socket transport layer in the chrome process now has a logical ref to
  // us until OnStopRequest is called.
  AddIPDLReference();

  mIsPending = true;
  mWasOpened = true;

  return rv;
}

NS_IMETHODIMP
FTPChannelChild::IsPending(bool* result)
{
  *result = mIsPending;
  return NS_OK;
}

nsresult
FTPChannelChild::OpenContentStream(bool async,
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
  FTPStartRequestEvent(FTPChannelChild* aChild, const int64_t& aContentLength,
                       const nsCString& aContentType, const PRTime& aLastModified,
                       const nsCString& aEntityID, const URIParams& aURI)
  : mChild(aChild), mContentLength(aContentLength), mContentType(aContentType),
    mLastModified(aLastModified), mEntityID(aEntityID), mURI(aURI) {}
  void Run() { mChild->DoOnStartRequest(mContentLength, mContentType,
                                       mLastModified, mEntityID, mURI); }
 private:
  FTPChannelChild* mChild;
  int64_t mContentLength;
  nsCString mContentType;
  PRTime mLastModified;
  nsCString mEntityID;
  URIParams mURI;
};

bool
FTPChannelChild::RecvOnStartRequest(const int64_t& aContentLength,
                                    const nsCString& aContentType,
                                    const PRTime& aLastModified,
                                    const nsCString& aEntityID,
                                    const URIParams& aURI)
{
  if (mEventQ.ShouldEnqueue()) {
    mEventQ.Enqueue(new FTPStartRequestEvent(this, aContentLength, aContentType,
                                             aLastModified, aEntityID, aURI));
  } else {
    DoOnStartRequest(aContentLength, aContentType, aLastModified,
                     aEntityID, aURI);
  }
  return true;
}

void
FTPChannelChild::DoOnStartRequest(const int64_t& aContentLength,
                                  const nsCString& aContentType,
                                  const PRTime& aLastModified,
                                  const nsCString& aEntityID,
                                  const URIParams& aURI)
{
  LOG(("FTPChannelChild::RecvOnStartRequest [this=%x]\n", this));

  mContentLength = aContentLength;
  SetContentType(aContentType);
  mLastModifiedTime = aLastModified;
  mEntityID = aEntityID;

  nsCString spec;
  nsCOMPtr<nsIURI> uri = DeserializeURI(aURI);
  uri->GetSpec(spec);
  nsBaseChannel::URI()->SetSpec(spec);

  AutoEventEnqueuer ensureSerialDispatch(mEventQ);
  nsresult rv = mListener->OnStartRequest(this, mListenerContext);
  if (NS_FAILED(rv))
    Cancel(rv);
}

class FTPDataAvailableEvent : public ChannelEvent
{
 public:
  FTPDataAvailableEvent(FTPChannelChild* aChild, const nsCString& aData,
                        const uint64_t& aOffset, const uint32_t& aCount)
  : mChild(aChild), mData(aData), mOffset(aOffset), mCount(aCount) {}
  void Run() { mChild->DoOnDataAvailable(mData, mOffset, mCount); }
 private:
  FTPChannelChild* mChild;
  nsCString mData;
  uint64_t mOffset;
  uint32_t mCount;
};

bool
FTPChannelChild::RecvOnDataAvailable(const nsCString& data,
                                     const uint64_t& offset,
                                     const uint32_t& count)
{
  if (mEventQ.ShouldEnqueue()) {
    mEventQ.Enqueue(new FTPDataAvailableEvent(this, data, offset, count));
  } else {
    DoOnDataAvailable(data, offset, count);
  }
  return true;
}

void
FTPChannelChild::DoOnDataAvailable(const nsCString& data,
                                   const uint64_t& offset,
                                   const uint32_t& count)
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

  AutoEventEnqueuer ensureSerialDispatch(mEventQ);
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
  if (mEventQ.ShouldEnqueue()) {
    mEventQ.Enqueue(new FTPStopRequestEvent(this, statusCode));
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
    mIsPending = false;
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);
    (void)mListener->OnStopRequest(this, mListenerContext, statusCode);
    mListener = nullptr;
    mListenerContext = nullptr;

    if (mLoadGroup)
      mLoadGroup->RemoveRequest(this, nullptr, statusCode);
  }

  // This calls NeckoChild::DeallocPFTPChannel(), which deletes |this| if IPDL
  // holds the last reference.  Don't rely on |this| existing after here!
  Send__delete__(this);
}

class FTPFailedAsyncOpenEvent : public ChannelEvent
{
 public:
  FTPFailedAsyncOpenEvent(FTPChannelChild* aChild, nsresult aStatus)
  : mChild(aChild), mStatus(aStatus) {}
  void Run() { mChild->DoFailedAsyncOpen(mStatus); }
 private:
  FTPChannelChild* mChild;
  nsresult mStatus;
};

bool
FTPChannelChild::RecvFailedAsyncOpen(const nsresult& statusCode)
{
  if (mEventQ.ShouldEnqueue()) {
    mEventQ.Enqueue(new FTPFailedAsyncOpenEvent(this, statusCode));
  } else {
    DoFailedAsyncOpen(statusCode);
  }
  return true;
}

void
FTPChannelChild::DoFailedAsyncOpen(const nsresult& statusCode)
{
  mStatus = statusCode;

  if (mLoadGroup)
    mLoadGroup->RemoveRequest(this, nullptr, statusCode);

  if (mListener) {
    mListener->OnStartRequest(this, mListenerContext);
    mIsPending = false;
    mListener->OnStopRequest(this, mListenerContext, statusCode);
  } else {
    mIsPending = false;
  }

  mListener = nullptr;
  mListenerContext = nullptr;

  if (mIPCOpen)
    Send__delete__(this);
}

class FTPDeleteSelfEvent : public ChannelEvent
{
 public:
  FTPDeleteSelfEvent(FTPChannelChild* aChild)
  : mChild(aChild) {}
  void Run() { mChild->DoDeleteSelf(); }
 private:
  FTPChannelChild* mChild;
};

bool
FTPChannelChild::RecvDeleteSelf()
{
  if (mEventQ.ShouldEnqueue()) {
    mEventQ.Enqueue(new FTPDeleteSelfEvent(this));
  } else {
    DoDeleteSelf();
  }
  return true;
}

void
FTPChannelChild::DoDeleteSelf()
{
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
  if (!mSuspendCount++) {
    SendSuspend();
    mEventQ.Suspend();
  }
  return NS_OK;
}

nsresult
FTPChannelChild::AsyncCall(void (FTPChannelChild::*funcPtr)(),
                           nsRunnableMethod<FTPChannelChild> **retval)
{
  nsresult rv;

  nsRefPtr<nsRunnableMethod<FTPChannelChild> > event = NS_NewRunnableMethod(this, funcPtr);
  rv = NS_DispatchToCurrentThread(event);
  if (NS_SUCCEEDED(rv) && retval) {
    *retval = event;
  }

  return rv;
}

void
FTPChannelChild::CompleteResume()
{
  mEventQ.Resume();
}

NS_IMETHODIMP
FTPChannelChild::Resume()
{
  NS_ENSURE_TRUE(mIPCOpen, NS_ERROR_NOT_AVAILABLE);

  if (!--mSuspendCount) {
    SendResume();
    AsyncCall(&FTPChannelChild::CompleteResume);
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------
// FTPChannelChild::nsIChildChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
FTPChannelChild::ConnectParent(uint32_t id)
{
  mozilla::dom::TabChild* tabChild = nullptr;
  nsCOMPtr<nsITabChild> iTabChild;
  NS_QueryNotificationCallbacks(mCallbacks, mLoadGroup,
                                NS_GET_IID(nsITabChild),
                                getter_AddRefs(iTabChild));
  GetCallback(iTabChild);
  if (iTabChild) {
    tabChild = static_cast<mozilla::dom::TabChild*>(iTabChild.get());
  }

  // The socket transport in the chrome process now holds a logical ref to us
  // until OnStopRequest, or we do a redirect, or we hit an IPDL error.
  AddIPDLReference();

  if (!gNeckoChild->SendPFTPChannelConstructor(this, tabChild,
                                               IPC::SerializedLoadContext(this)))
    return NS_ERROR_FAILURE;

  if (!SendConnectChannel(id))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::CompleteRedirectSetup(nsIStreamListener *listener,
                                       nsISupports *aContext)
{
  LOG(("FTPChannelChild::CompleteRedirectSetup [this=%x]\n", this));

  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

  mIsPending = true;
  mWasOpened = true;
  mListener = listener;
  mListenerContext = aContext;

  // add ourselves to the load group.
  if (mLoadGroup)
    mLoadGroup->AddRequest(this, nullptr);

  // We already have an open IPDL connection to the parent. If on-modify-request
  // listeners or load group observers canceled us, let the parent handle it
  // and send it back to us naturally.
  return NS_OK;
}

} // namespace net
} // namespace mozilla

