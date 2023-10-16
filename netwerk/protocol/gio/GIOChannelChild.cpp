/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=4 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/NeckoChild.h"
#include "GIOChannelChild.h"
#include "nsGIOProtocolHandler.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/BrowserChild.h"
#include "nsContentUtils.h"
#include "nsIBrowserChild.h"
#include "nsStringStream.h"
#include "nsNetUtil.h"
#include "base/compiler_specific.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "SerializedLoadContext.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "nsIURIMutator.h"
#include "nsContentSecurityManager.h"
#include "SerializedLoadContext.h"
#include "mozilla/Logging.h"

using mozilla::dom::ContentChild;

namespace mozilla {
#undef LOG
#define LOG(args) MOZ_LOG(gGIOLog, mozilla::LogLevel::Debug, args)
namespace net {

GIOChannelChild::GIOChannelChild(nsIURI* aUri)
    : mEventQ(new ChannelEventQueue(static_cast<nsIChildChannel*>(this))) {
  SetURI(aUri);
  // We could support thread retargeting, but as long as we're being driven by
  // IPDL on the main thread it doesn't buy us anything.
  DisallowThreadRetargeting();
}

void GIOChannelChild::AddIPDLReference() {
  MOZ_ASSERT(!mIPCOpen, "Attempt to retain more than one IPDL reference");
  mIPCOpen = true;
  AddRef();
}

void GIOChannelChild::ReleaseIPDLReference() {
  MOZ_ASSERT(mIPCOpen, "Attempt to release nonexistent IPDL reference");
  mIPCOpen = false;
  Release();
}

//-----------------------------------------------------------------------------
// GIOChannelChild::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS_INHERITED(GIOChannelChild, nsBaseChannel, nsIChildChannel)

//-----------------------------------------------------------------------------

NS_IMETHODIMP
GIOChannelChild::AsyncOpen(nsIStreamListener* aListener) {
  nsCOMPtr<nsIStreamListener> listener = aListener;
  nsresult rv =
      nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("GIOChannelChild::AsyncOpen [this=%p]\n", this));

  NS_ENSURE_TRUE((gNeckoChild), NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(
      !static_cast<ContentChild*>(gNeckoChild->Manager())->IsShuttingDown(),
      NS_ERROR_FAILURE);
  NS_ENSURE_ARG_POINTER(listener);
  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

  // Port checked in parent, but duplicate here so we can return with error
  // immediately, as we've done since before e10s.
  rv = NS_CheckPortSafety(nsBaseChannel::URI());  // Need to disambiguate,
                                                  // because in the child ipdl,
                                                  // a typedef URI is defined...
  if (NS_FAILED(rv)) {
    return rv;
  }

  mozilla::dom::BrowserChild* browserChild = nullptr;
  nsCOMPtr<nsIBrowserChild> iBrowserChild;
  NS_QueryNotificationCallbacks(mCallbacks, mLoadGroup,
                                NS_GET_IID(nsIBrowserChild),
                                getter_AddRefs(iBrowserChild));
  GetCallback(iBrowserChild);
  if (iBrowserChild) {
    browserChild =
        static_cast<mozilla::dom::BrowserChild*>(iBrowserChild.get());
  }

  mListener = listener;

  // add ourselves to the load group.
  if (mLoadGroup) {
    mLoadGroup->AddRequest(this, nullptr);
  }

  Maybe<mozilla::ipc::IPCStream> ipcStream;
  mozilla::ipc::SerializeIPCStream(do_AddRef(mUploadStream), ipcStream,
                                   /* aAllowLazy */ false);

  uint32_t loadFlags = 0;
  GetLoadFlags(&loadFlags);

  GIOChannelOpenArgs openArgs;
  SerializeURI(nsBaseChannel::URI(), openArgs.uri());
  openArgs.startPos() = mStartPos;
  openArgs.entityID() = mEntityID;
  openArgs.uploadStream() = ipcStream;
  openArgs.loadFlags() = loadFlags;

  nsCOMPtr<nsILoadInfo> loadInfo = LoadInfo();
  rv = mozilla::ipc::LoadInfoToLoadInfoArgs(loadInfo, &openArgs.loadInfo());
  NS_ENSURE_SUCCESS(rv, rv);

  // This must happen before the constructor message is sent.
  SetupNeckoTarget();

  gNeckoChild->SendPGIOChannelConstructor(
      this, browserChild, IPC::SerializedLoadContext(this), openArgs);

  // The socket transport layer in the chrome process now has a logical ref to
  // us until OnStopRequest is called.
  AddIPDLReference();

  mIsPending = true;
  mWasOpened = true;

  return rv;
}

NS_IMETHODIMP
GIOChannelChild::IsPending(bool* aResult) {
  *aResult = mIsPending;
  return NS_OK;
}

nsresult GIOChannelChild::OpenContentStream(bool aAsync,
                                            nsIInputStream** aStream,
                                            nsIChannel** aChannel) {
  MOZ_CRASH("GIOChannel*Child* should never have OpenContentStream called!");
  return NS_OK;
}

mozilla::ipc::IPCResult GIOChannelChild::RecvOnStartRequest(
    const nsresult& aChannelStatus, const int64_t& aContentLength,
    const nsACString& aContentType, const nsACString& aEntityID,
    const URIParams& aURI) {
  LOG(("GIOChannelChild::RecvOnStartRequest [this=%p]\n", this));
  mEventQ->RunOrEnqueue(new NeckoTargetChannelFunctionEvent(
      this, [self = UnsafePtr<GIOChannelChild>(this), aChannelStatus,
             aContentLength, aContentType = nsCString(aContentType),
             aEntityID = nsCString(aEntityID), aURI]() {
        self->DoOnStartRequest(aChannelStatus, aContentLength, aContentType,
                               aEntityID, aURI);
      }));
  return IPC_OK();
}

void GIOChannelChild::DoOnStartRequest(const nsresult& aChannelStatus,
                                       const int64_t& aContentLength,
                                       const nsACString& aContentType,
                                       const nsACString& aEntityID,
                                       const URIParams& aURI) {
  LOG(("GIOChannelChild::DoOnStartRequest [this=%p]\n", this));
  if (!mCanceled && NS_SUCCEEDED(mStatus)) {
    mStatus = aChannelStatus;
  }

  mContentLength = aContentLength;
  SetContentType(aContentType);
  mEntityID = aEntityID;

  nsCString spec;
  nsCOMPtr<nsIURI> uri = DeserializeURI(aURI);
  nsresult rv = uri->GetSpec(spec);
  if (NS_SUCCEEDED(rv)) {
    // Changes nsBaseChannel::URI()
    rv = NS_MutateURI(mURI).SetSpec(spec).Finalize(mURI);
  }

  if (NS_FAILED(rv)) {
    Cancel(rv);
  }

  AutoEventEnqueuer ensureSerialDispatch(mEventQ);
  rv = mListener->OnStartRequest(this);
  if (NS_FAILED(rv)) {
    Cancel(rv);
  }
}

mozilla::ipc::IPCResult GIOChannelChild::RecvOnDataAvailable(
    const nsresult& aChannelStatus, const nsACString& aData,
    const uint64_t& aOffset, const uint32_t& aCount) {
  LOG(("GIOChannelChild::RecvOnDataAvailable [this=%p]\n", this));
  mEventQ->RunOrEnqueue(new NeckoTargetChannelFunctionEvent(
      this, [self = UnsafePtr<GIOChannelChild>(this), aChannelStatus,
             aData = nsCString(aData), aOffset, aCount]() {
        self->DoOnDataAvailable(aChannelStatus, aData, aOffset, aCount);
      }));

  return IPC_OK();
}

void GIOChannelChild::DoOnDataAvailable(const nsresult& aChannelStatus,
                                        const nsACString& aData,
                                        const uint64_t& aOffset,
                                        const uint32_t& aCount) {
  LOG(("GIOChannelChild::DoOnDataAvailable [this=%p]\n", this));

  if (!mCanceled && NS_SUCCEEDED(mStatus)) {
    mStatus = aChannelStatus;
  }

  if (mCanceled) {
    return;
  }

  // NOTE: the OnDataAvailable contract requires the client to read all the data
  // in the inputstream.  This code relies on that ('data' will go away after
  // this function).  Apparently the previous, non-e10s behavior was to actually
  // support only reading part of the data, allowing later calls to read the
  // rest.
  nsCOMPtr<nsIInputStream> stringStream;
  nsresult rv =
      NS_NewByteInputStream(getter_AddRefs(stringStream),
                            Span(aData).To(aCount), NS_ASSIGNMENT_DEPEND);
  if (NS_FAILED(rv)) {
    Cancel(rv);
    return;
  }

  AutoEventEnqueuer ensureSerialDispatch(mEventQ);
  rv = mListener->OnDataAvailable(this, stringStream, aOffset, aCount);
  if (NS_FAILED(rv)) {
    Cancel(rv);
  }
  stringStream->Close();
}

mozilla::ipc::IPCResult GIOChannelChild::RecvOnStopRequest(
    const nsresult& aChannelStatus) {
  LOG(("GIOChannelChild::RecvOnStopRequest [this=%p status=%" PRIx32 "]\n",
       this, static_cast<uint32_t>(aChannelStatus)));

  mEventQ->RunOrEnqueue(new NeckoTargetChannelFunctionEvent(
      this, [self = UnsafePtr<GIOChannelChild>(this), aChannelStatus]() {
        self->DoOnStopRequest(aChannelStatus);
      }));
  return IPC_OK();
}

void GIOChannelChild::DoOnStopRequest(const nsresult& aChannelStatus) {
  LOG(("GIOChannelChild::DoOnStopRequest [this=%p status=%" PRIx32 "]\n", this,
       static_cast<uint32_t>(aChannelStatus)));

  if (!mCanceled) {
    mStatus = aChannelStatus;
  }

  {  // Ensure that all queued ipdl events are dispatched before
    // we initiate protocol deletion below.
    mIsPending = false;
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);
    (void)mListener->OnStopRequest(this, aChannelStatus);

    mListener = nullptr;

    if (mLoadGroup) {
      mLoadGroup->RemoveRequest(this, nullptr, aChannelStatus);
    }
  }

  // This calls NeckoChild::DeallocPGIOChannelChild(), which deletes |this| if
  // IPDL holds the last reference.  Don't rely on |this| existing after here!
  Send__delete__(this);
}

mozilla::ipc::IPCResult GIOChannelChild::RecvFailedAsyncOpen(
    const nsresult& aStatusCode) {
  LOG(("GIOChannelChild::RecvFailedAsyncOpen [this=%p status=%" PRIx32 "]\n",
       this, static_cast<uint32_t>(aStatusCode)));
  mEventQ->RunOrEnqueue(new NeckoTargetChannelFunctionEvent(
      this, [self = UnsafePtr<GIOChannelChild>(this), aStatusCode]() {
        self->DoFailedAsyncOpen(aStatusCode);
      }));
  return IPC_OK();
}

void GIOChannelChild::DoFailedAsyncOpen(const nsresult& aStatusCode) {
  LOG(("GIOChannelChild::DoFailedAsyncOpen [this=%p status=%" PRIx32 "]\n",
       this, static_cast<uint32_t>(aStatusCode)));
  mStatus = aStatusCode;

  if (mLoadGroup) {
    mLoadGroup->RemoveRequest(this, nullptr, aStatusCode);
  }

  if (mListener) {
    mListener->OnStartRequest(this);
    mIsPending = false;
    mListener->OnStopRequest(this, aStatusCode);
  } else {
    mIsPending = false;
  }

  mListener = nullptr;

  if (mIPCOpen) {
    Send__delete__(this);
  }
}

mozilla::ipc::IPCResult GIOChannelChild::RecvDeleteSelf() {
  mEventQ->RunOrEnqueue(new NeckoTargetChannelFunctionEvent(
      this,
      [self = UnsafePtr<GIOChannelChild>(this)]() { self->DoDeleteSelf(); }));
  return IPC_OK();
}

void GIOChannelChild::DoDeleteSelf() {
  if (mIPCOpen) {
    Send__delete__(this);
  }
}

//-----------------------------------------------------------------------------
// GIOChannelChild::nsIResumableChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
GIOChannelChild::Cancel(nsresult aStatus) {
  LOG(("GIOChannelChild::Cancel [this=%p]\n", this));

  if (mCanceled) {
    return NS_OK;
  }

  mCanceled = true;
  mStatus = aStatus;
  if (mIPCOpen) {
    SendCancel(aStatus);
  }
  return NS_OK;
}

NS_IMETHODIMP
GIOChannelChild::Suspend() {
  NS_ENSURE_TRUE(mIPCOpen, NS_ERROR_NOT_AVAILABLE);

  LOG(("GIOChannelChild::Suspend [this=%p]\n", this));

  // SendSuspend only once, when suspend goes from 0 to 1.
  if (!mSuspendCount++) {
    SendSuspend();
    mSuspendSent = true;
  }
  mEventQ->Suspend();

  return NS_OK;
}

NS_IMETHODIMP
GIOChannelChild::Resume() {
  NS_ENSURE_TRUE(mIPCOpen, NS_ERROR_NOT_AVAILABLE);

  LOG(("GIOChannelChild::Resume [this=%p]\n", this));

  // SendResume only once, when suspend count drops to 0.
  if (!--mSuspendCount && mSuspendSent) {
    SendResume();
  }
  mEventQ->Resume();

  return NS_OK;
}

//-----------------------------------------------------------------------------
// GIOChannelChild::nsIChildChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
GIOChannelChild::ConnectParent(uint32_t aId) {
  NS_ENSURE_TRUE((gNeckoChild), NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(
      !static_cast<ContentChild*>(gNeckoChild->Manager())->IsShuttingDown(),
      NS_ERROR_FAILURE);

  LOG(("GIOChannelChild::ConnectParent [this=%p]\n", this));

  mozilla::dom::BrowserChild* browserChild = nullptr;
  nsCOMPtr<nsIBrowserChild> iBrowserChild;
  NS_QueryNotificationCallbacks(mCallbacks, mLoadGroup,
                                NS_GET_IID(nsIBrowserChild),
                                getter_AddRefs(iBrowserChild));
  GetCallback(iBrowserChild);
  if (iBrowserChild) {
    browserChild =
        static_cast<mozilla::dom::BrowserChild*>(iBrowserChild.get());
  }

  // This must happen before the constructor message is sent.
  SetupNeckoTarget();

  // The socket transport in the chrome process now holds a logical ref to us
  // until OnStopRequest, or we do a redirect, or we hit an IPDL error.
  AddIPDLReference();

  GIOChannelConnectArgs connectArgs(aId);

  if (!gNeckoChild->SendPGIOChannelConstructor(
          this, browserChild, IPC::SerializedLoadContext(this), connectArgs)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
GIOChannelChild::CompleteRedirectSetup(nsIStreamListener* aListener) {
  LOG(("GIOChannelChild::CompleteRedirectSetup [this=%p]\n", this));
  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

  mIsPending = true;
  mWasOpened = true;
  mListener = aListener;

  // add ourselves to the load group.
  if (mLoadGroup) {
    mLoadGroup->AddRequest(this, nullptr);
  }

  // We already have an open IPDL connection to the parent. If on-modify-request
  // listeners or load group observers canceled us, let the parent handle it
  // and send it back to us naturally.
  return NS_OK;
}

void GIOChannelChild::SetupNeckoTarget() {
  if (mNeckoTarget) {
    return;
  }
  nsCOMPtr<nsILoadInfo> loadInfo = LoadInfo();
  mNeckoTarget =
      nsContentUtils::GetEventTargetByLoadInfo(loadInfo, TaskCategory::Network);
  if (!mNeckoTarget) {
    return;
  }
}

}  // namespace net
}  // namespace mozilla
