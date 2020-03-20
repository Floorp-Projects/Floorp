/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/ChannelDiverterChild.h"
#include "mozilla/net/FTPChannelChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/BrowserChild.h"
#include "nsContentUtils.h"
#include "nsFtpProtocolHandler.h"
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

using mozilla::dom::ContentChild;
using namespace mozilla::ipc;

#undef LOG
#define LOG(args) MOZ_LOG(gFTPLog, mozilla::LogLevel::Debug, args)

namespace mozilla {
namespace net {

FTPChannelChild::FTPChannelChild(nsIURI* aUri)
    : mIPCOpen(false),
      mEventQ(new ChannelEventQueue(static_cast<nsIFTPChannel*>(this))),
      mUnknownDecoderInvolved(false),
      mCanceled(false),
      mSuspendCount(0),
      mIsPending(false),
      mLastModifiedTime(0),
      mStartPos(0),
      mDivertingToParent(false),
      mFlushedForDiversion(false),
      mSuspendSent(false) {
  LOG(("Creating FTPChannelChild @%p\n", this));
  // grab a reference to the handler to ensure that it doesn't go away.
  NS_ADDREF(gFtpHandler);
  SetURI(aUri);

  // We could support thread retargeting, but as long as we're being driven by
  // IPDL on the main thread it doesn't buy us anything.
  DisallowThreadRetargeting();
}

FTPChannelChild::~FTPChannelChild() {
  LOG(("Destroying FTPChannelChild @%p\n", this));
  gFtpHandler->Release();
}

void FTPChannelChild::AddIPDLReference() {
  MOZ_ASSERT(!mIPCOpen, "Attempt to retain more than one IPDL reference");
  mIPCOpen = true;
  AddRef();
}

void FTPChannelChild::ReleaseIPDLReference() {
  MOZ_ASSERT(mIPCOpen, "Attempt to release nonexistent IPDL reference");
  mIPCOpen = false;
  Release();
}

//-----------------------------------------------------------------------------
// FTPChannelChild::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS_INHERITED(FTPChannelChild, nsBaseChannel, nsIFTPChannel,
                            nsIUploadChannel, nsIResumableChannel,
                            nsIProxiedChannel, nsIChildChannel,
                            nsIDivertableChannel)

//-----------------------------------------------------------------------------

NS_IMETHODIMP
FTPChannelChild::GetLastModifiedTime(PRTime* aLastModifiedTime) {
  *aLastModifiedTime = mLastModifiedTime;
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::SetLastModifiedTime(PRTime aLastModifiedTime) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
FTPChannelChild::ResumeAt(uint64_t aStartPos, const nsACString& aEntityID) {
  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  mStartPos = aStartPos;
  mEntityID = aEntityID;
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::GetEntityID(nsACString& aEntityID) {
  aEntityID = mEntityID;
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::GetProxyInfo(nsIProxyInfo** aProxyInfo) { DROP_DEAD(); }

NS_IMETHODIMP FTPChannelChild::GetHttpProxyConnectResponseCode(
    int32_t* aResponseCode) {
  DROP_DEAD();
}

NS_IMETHODIMP
FTPChannelChild::SetUploadStream(nsIInputStream* aStream,
                                 const nsACString& aContentType,
                                 int64_t aContentLength) {
  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  mUploadStream = aStream;
  // NOTE: contentLength is intentionally ignored here.
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::GetUploadStream(nsIInputStream** aStream) {
  NS_ENSURE_ARG_POINTER(aStream);
  *aStream = mUploadStream;
  NS_IF_ADDREF(*aStream);
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::AsyncOpen(nsIStreamListener* aListener) {
  nsCOMPtr<nsIStreamListener> listener = aListener;
  nsresult rv =
      nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("FTPChannelChild::AsyncOpen [this=%p]\n", this));

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
  if (NS_FAILED(rv)) return rv;

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
  if (MissingRequiredBrowserChild(browserChild, "ftp")) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  mListener = listener;

  // add ourselves to the load group.
  if (mLoadGroup) mLoadGroup->AddRequest(this, nullptr);

  mozilla::ipc::AutoIPCStream autoStream;
  autoStream.Serialize(mUploadStream,
                       static_cast<ContentChild*>(gNeckoChild->Manager()));

  uint32_t loadFlags = 0;
  GetLoadFlags(&loadFlags);

  FTPChannelOpenArgs openArgs;
  SerializeURI(nsBaseChannel::URI(), openArgs.uri());
  openArgs.startPos() = mStartPos;
  openArgs.entityID() = mEntityID;
  openArgs.uploadStream() = autoStream.TakeOptionalValue();
  openArgs.loadFlags() = loadFlags;

  nsCOMPtr<nsILoadInfo> loadInfo = LoadInfo();
  rv = mozilla::ipc::LoadInfoToLoadInfoArgs(loadInfo, &openArgs.loadInfo());
  NS_ENSURE_SUCCESS(rv, rv);

  // This must happen before the constructor message is sent.
  SetupNeckoTarget();

  gNeckoChild->SendPFTPChannelConstructor(
      this, browserChild, IPC::SerializedLoadContext(this), openArgs);

  // The socket transport layer in the chrome process now has a logical ref to
  // us until OnStopRequest is called.
  AddIPDLReference();

  mIsPending = true;
  mWasOpened = true;

  return rv;
}

NS_IMETHODIMP
FTPChannelChild::IsPending(bool* aResult) {
  *aResult = mIsPending;
  return NS_OK;
}

nsresult FTPChannelChild::OpenContentStream(bool aAsync,
                                            nsIInputStream** aStream,
                                            nsIChannel** aChannel) {
  MOZ_CRASH("FTPChannel*Child* should never have OpenContentStream called!");
  return NS_OK;
}

//-----------------------------------------------------------------------------
// FTPChannelChild::PFTPChannelChild
//-----------------------------------------------------------------------------

mozilla::ipc::IPCResult FTPChannelChild::RecvOnStartRequest(
    const nsresult& aChannelStatus, const int64_t& aContentLength,
    const nsCString& aContentType, const PRTime& aLastModified,
    const nsCString& aEntityID, nsIURI* aURI) {
  // mFlushedForDiversion and mDivertingToParent should NEVER be set at this
  // stage, as they are set in the listener's OnStartRequest.
  MOZ_RELEASE_ASSERT(
      !mFlushedForDiversion,
      "mFlushedForDiversion should be unset before OnStartRequest!");
  MOZ_RELEASE_ASSERT(
      !mDivertingToParent,
      "mDivertingToParent should be unset before OnStartRequest!");

  LOG(("FTPChannelChild::RecvOnStartRequest [this=%p]\n", this));

  mEventQ->RunOrEnqueue(new NeckoTargetChannelFunctionEvent(
      this, [self = UnsafePtr<FTPChannelChild>(this), aChannelStatus,
             aContentLength, aContentType, aLastModified, aEntityID, aURI]() {
        self->DoOnStartRequest(aChannelStatus, aContentLength, aContentType,
                               aLastModified, aEntityID, aURI);
      }));
  return IPC_OK();
}

void FTPChannelChild::DoOnStartRequest(const nsresult& aChannelStatus,
                                       const int64_t& aContentLength,
                                       const nsCString& aContentType,
                                       const PRTime& aLastModified,
                                       const nsCString& aEntityID,
                                       nsIURI* aURI) {
  mDuringOnStart = true;
  RefPtr<FTPChannelChild> self = this;
  auto clearDuringFlag =
      mozilla::MakeScopeExit([self] { self->mDuringOnStart = false; });

  LOG(("FTPChannelChild::DoOnStartRequest [this=%p]\n", this));

  // mFlushedForDiversion and mDivertingToParent should NEVER be set at this
  // stage, as they are set in the listener's OnStartRequest.
  MOZ_RELEASE_ASSERT(
      !mFlushedForDiversion,
      "mFlushedForDiversion should be unset before OnStartRequest!");
  MOZ_RELEASE_ASSERT(
      !mDivertingToParent,
      "mDivertingToParent should be unset before OnStartRequest!");

  if (!mCanceled && NS_SUCCEEDED(mStatus)) {
    mStatus = aChannelStatus;
  }

  mContentLength = aContentLength;
  SetContentType(aContentType);
  mLastModifiedTime = aLastModified;
  mEntityID = aEntityID;

  nsCString spec;
  nsresult rv = aURI->GetSpec(spec);
  if (NS_SUCCEEDED(rv)) {
    // Changes nsBaseChannel::URI()
    rv = NS_MutateURI(mURI).SetSpec(spec).Finalize(mURI);
    if (NS_FAILED(rv)) {
      Cancel(rv);
    }
  } else {
    Cancel(rv);
  }

  AutoEventEnqueuer ensureSerialDispatch(mEventQ);
  rv = mListener->OnStartRequest(this);
  if (NS_FAILED(rv)) Cancel(rv);

  if (mDivertingToParent) {
    mListener = nullptr;
    if (mLoadGroup) {
      mLoadGroup->RemoveRequest(this, nullptr, mStatus);
    }
  }
}

mozilla::ipc::IPCResult FTPChannelChild::RecvOnDataAvailable(
    const nsresult& aChannelStatus, const nsCString& aData,
    const uint64_t& aOffset, const uint32_t& aCount) {
  MOZ_RELEASE_ASSERT(!mFlushedForDiversion,
                     "Should not be receiving any more callbacks from parent!");

  LOG(("FTPChannelChild::RecvOnDataAvailable [this=%p]\n", this));

  mEventQ->RunOrEnqueue(new NeckoTargetChannelFunctionEvent(
                            this,
                            [self = UnsafePtr<FTPChannelChild>(this),
                             aChannelStatus, aData, aOffset, aCount]() {
                              self->DoOnDataAvailable(aChannelStatus, aData,
                                                      aOffset, aCount);
                            }),
                        mDivertingToParent);

  return IPC_OK();
}

void FTPChannelChild::DoOnDataAvailable(const nsresult& aChannelStatus,
                                        const nsCString& aData,
                                        const uint64_t& aOffset,
                                        const uint32_t& aCount) {
  LOG(("FTPChannelChild::DoOnDataAvailable [this=%p]\n", this));

  if (!mCanceled && NS_SUCCEEDED(mStatus)) {
    mStatus = aChannelStatus;
  }

  if (mDivertingToParent) {
    MOZ_RELEASE_ASSERT(
        !mFlushedForDiversion,
        "Should not be processing any more callbacks from parent!");

    SendDivertOnDataAvailable(aData, aOffset, aCount);
    return;
  }

  if (mCanceled) {
    return;
  }

  if (mUnknownDecoderInvolved) {
    mUnknownDecoderEventQ.AppendElement(
        MakeUnique<NeckoTargetChannelFunctionEvent>(
            this, [self = UnsafePtr<FTPChannelChild>(this), aData, aOffset,
                   aCount]() {
              if (self->mDivertingToParent) {
                self->SendDivertOnDataAvailable(aData, aOffset, aCount);
              }
            }));
  }

  // NOTE: the OnDataAvailable contract requires the client to read all the data
  // in the inputstream.  This code relies on that ('data' will go away after
  // this function).  Apparently the previous, non-e10s behavior was to actually
  // support only reading part of the data, allowing later calls to read the
  // rest.
  nsCOMPtr<nsIInputStream> stringStream;
  nsresult rv =
      NS_NewByteInputStream(getter_AddRefs(stringStream),
                            MakeSpan(aData).To(aCount), NS_ASSIGNMENT_DEPEND);
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

mozilla::ipc::IPCResult FTPChannelChild::RecvOnStopRequest(
    const nsresult& aChannelStatus, const nsCString& aErrorMsg,
    const bool& aUseUTF8) {
  MOZ_RELEASE_ASSERT(!mFlushedForDiversion,
                     "Should not be receiving any more callbacks from parent!");

  LOG(("FTPChannelChild::RecvOnStopRequest [this=%p status=%" PRIx32 "]\n",
       this, static_cast<uint32_t>(aChannelStatus)));

  mEventQ->RunOrEnqueue(new NeckoTargetChannelFunctionEvent(
      this, [self = UnsafePtr<FTPChannelChild>(this), aChannelStatus, aErrorMsg,
             aUseUTF8]() {
        self->DoOnStopRequest(aChannelStatus, aErrorMsg, aUseUTF8);
      }));
  return IPC_OK();
}

void FTPChannelChild::DoOnStopRequest(const nsresult& aChannelStatus,
                                      const nsCString& aErrorMsg,
                                      bool aUseUTF8) {
  LOG(("FTPChannelChild::DoOnStopRequest [this=%p status=%" PRIx32 "]\n", this,
       static_cast<uint32_t>(aChannelStatus)));

  if (mDivertingToParent) {
    MOZ_RELEASE_ASSERT(
        !mFlushedForDiversion,
        "Should not be processing any more callbacks from parent!");

    SendDivertOnStopRequest(aChannelStatus);
    return;
  }

  if (!mCanceled) mStatus = aChannelStatus;

  if (mUnknownDecoderInvolved) {
    mUnknownDecoderEventQ.AppendElement(
        MakeUnique<NeckoTargetChannelFunctionEvent>(
            this, [self = UnsafePtr<FTPChannelChild>(this), aChannelStatus]() {
              if (self->mDivertingToParent) {
                self->SendDivertOnStopRequest(aChannelStatus);
              }
            }));
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

  // This calls NeckoChild::DeallocPFTPChannelChild(), which deletes |this| if
  // IPDL holds the last reference.  Don't rely on |this| existing after here!
  Send__delete__(this);
}

mozilla::ipc::IPCResult FTPChannelChild::RecvFailedAsyncOpen(
    const nsresult& aStatusCode) {
  LOG(("FTPChannelChild::RecvFailedAsyncOpen [this=%p status=%" PRIx32 "]\n",
       this, static_cast<uint32_t>(aStatusCode)));
  mEventQ->RunOrEnqueue(new NeckoTargetChannelFunctionEvent(
      this, [self = UnsafePtr<FTPChannelChild>(this), aStatusCode]() {
        self->DoFailedAsyncOpen(aStatusCode);
      }));
  return IPC_OK();
}

void FTPChannelChild::DoFailedAsyncOpen(const nsresult& aStatusCode) {
  LOG(("FTPChannelChild::DoFailedAsyncOpen [this=%p status=%" PRIx32 "]\n",
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

mozilla::ipc::IPCResult FTPChannelChild::RecvFlushedForDiversion() {
  LOG(("FTPChannelChild::RecvFlushedForDiversion [this=%p]\n", this));
  MOZ_ASSERT(mDivertingToParent);

  mEventQ->RunOrEnqueue(new NeckoTargetChannelFunctionEvent(
      this, [self = UnsafePtr<FTPChannelChild>(this)]() {
        self->FlushedForDiversion();
      }));
  return IPC_OK();
}

void FTPChannelChild::FlushedForDiversion() {
  LOG(("FTPChannelChild::FlushedForDiversion [this=%p]\n", this));
  MOZ_RELEASE_ASSERT(mDivertingToParent);

  // Once this is set, it should not be unset before FTPChannelChild is taken
  // down. After it is set, no OnStart/OnData/OnStop callbacks should be
  // received from the parent channel, nor dequeued from the ChannelEventQueue.
  mFlushedForDiversion = true;

  SendDivertComplete();
}

mozilla::ipc::IPCResult FTPChannelChild::RecvDivertMessages() {
  LOG(("FTPChannelChild::RecvDivertMessages [this=%p]\n", this));
  MOZ_RELEASE_ASSERT(mDivertingToParent);
  MOZ_RELEASE_ASSERT(mSuspendCount > 0);

  // DivertTo() has been called on parent, so we can now start sending queued
  // IPDL messages back to parent listener.
  if (NS_WARN_IF(NS_FAILED(Resume()))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult FTPChannelChild::RecvDeleteSelf() {
  mEventQ->RunOrEnqueue(new NeckoTargetChannelFunctionEvent(
      this,
      [self = UnsafePtr<FTPChannelChild>(this)]() { self->DoDeleteSelf(); }));
  return IPC_OK();
}

void FTPChannelChild::DoDeleteSelf() {
  if (mIPCOpen) {
    Send__delete__(this);
  }
}

NS_IMETHODIMP
FTPChannelChild::Cancel(nsresult aStatus) {
  LOG(("FTPChannelChild::Cancel [this=%p]\n", this));
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
FTPChannelChild::Suspend() {
  NS_ENSURE_TRUE(mIPCOpen, NS_ERROR_NOT_AVAILABLE);

  LOG(("FTPChannelChild::Suspend [this=%p]\n", this));

  // SendSuspend only once, when suspend goes from 0 to 1.
  // Don't SendSuspend at all if we're diverting callbacks to the parent;
  // suspend will be called at the correct time in the parent itself.
  if (!mSuspendCount++ && !mDivertingToParent) {
    SendSuspend();
    mSuspendSent = true;
  }
  mEventQ->Suspend();

  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::Resume() {
  NS_ENSURE_TRUE(mIPCOpen, NS_ERROR_NOT_AVAILABLE);

  LOG(("FTPChannelChild::Resume [this=%p]\n", this));

  // SendResume only once, when suspend count drops to 0.
  // Don't SendResume at all if we're diverting callbacks to the parent (unless
  // suspend was sent earlier); otherwise, resume will be called at the correct
  // time in the parent itself.
  if (!--mSuspendCount && (!mDivertingToParent || mSuspendSent)) {
    SendResume();
  }
  mEventQ->Resume();

  return NS_OK;
}

//-----------------------------------------------------------------------------
// FTPChannelChild::nsIChildChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
FTPChannelChild::ConnectParent(uint32_t aId) {
  NS_ENSURE_TRUE((gNeckoChild), NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(
      !static_cast<ContentChild*>(gNeckoChild->Manager())->IsShuttingDown(),
      NS_ERROR_FAILURE);

  LOG(("FTPChannelChild::ConnectParent [this=%p]\n", this));

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

  FTPChannelConnectArgs connectArgs(aId);

  if (!gNeckoChild->SendPFTPChannelConstructor(
          this, browserChild, IPC::SerializedLoadContext(this), connectArgs)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::CompleteRedirectSetup(nsIStreamListener* aListener,
                                       nsISupports* aContext) {
  LOG(("FTPChannelChild::CompleteRedirectSetup [this=%p]\n", this));

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

//-----------------------------------------------------------------------------
// FTPChannelChild::nsIDivertableChannel
//-----------------------------------------------------------------------------
NS_IMETHODIMP
FTPChannelChild::DivertToParent(ChannelDiverterChild** aChild) {
  MOZ_RELEASE_ASSERT(aChild);
  MOZ_RELEASE_ASSERT(gNeckoChild);
  MOZ_RELEASE_ASSERT(!mDivertingToParent);
  NS_ENSURE_TRUE(
      !static_cast<ContentChild*>(gNeckoChild->Manager())->IsShuttingDown(),
      NS_ERROR_FAILURE);

  LOG(("FTPChannelChild::DivertToParent [this=%p]\n", this));

  // This method should only be called during OnStartRequest.
  if (!mDuringOnStart) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // We must fail DivertToParent() if there's no parent end of the channel (and
  // won't be!) due to early failure.
  if (NS_FAILED(mStatus) && !mIPCOpen) {
    return mStatus;
  }

  // Once this is set, it should not be unset before the child is taken down.
  mDivertingToParent = true;

  nsresult rv = Suspend();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  PChannelDiverterChild* diverter =
      gNeckoChild->SendPChannelDiverterConstructor(this);
  MOZ_RELEASE_ASSERT(diverter);

  *aChild = static_cast<ChannelDiverterChild*>(diverter);

  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::UnknownDecoderInvolvedKeepData() {
  mUnknownDecoderInvolved = true;
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::UnknownDecoderInvolvedOnStartRequestCalled() {
  mUnknownDecoderInvolved = false;

  if (mDivertingToParent) {
    mEventQ->PrependEvents(mUnknownDecoderEventQ);
  }
  mUnknownDecoderEventQ.Clear();

  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::GetDivertingToParent(bool* aDiverting) {
  NS_ENSURE_ARG_POINTER(aDiverting);
  *aDiverting = mDivertingToParent;
  return NS_OK;
}

void FTPChannelChild::SetupNeckoTarget() {
  if (mNeckoTarget) {
    return;
  }
  nsCOMPtr<nsILoadInfo> loadInfo = LoadInfo();
  mNeckoTarget =
      nsContentUtils::GetEventTargetByLoadInfo(loadInfo, TaskCategory::Network);
  if (!mNeckoTarget) {
    return;
  }

  gNeckoChild->SetEventTargetForActor(this, mNeckoTarget);
}

}  // namespace net
}  // namespace mozilla
