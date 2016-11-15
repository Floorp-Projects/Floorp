/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/FTPChannelParent.h"
#include "nsStringStream.h"
#include "mozilla/net/ChannelEventQueue.h"
#include "mozilla/dom/TabParent.h"
#include "nsFTPChannel.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsQueryObject.h"
#include "nsFtpProtocolHandler.h"
#include "nsIAuthPrompt.h"
#include "nsIAuthPromptProvider.h"
#include "nsIEncodedChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIForcePendingChannel.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/Unused.h"
#include "SerializedLoadContext.h"
#include "nsIContentPolicy.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/LoadInfo.h"

using namespace mozilla::dom;
using namespace mozilla::ipc;

#undef LOG
#define LOG(args) MOZ_LOG(gFTPLog, mozilla::LogLevel::Debug, args)

namespace mozilla {
namespace net {

FTPChannelParent::FTPChannelParent(const PBrowserOrId& aIframeEmbedding,
                                   nsILoadContext* aLoadContext,
                                   PBOverrideStatus aOverrideStatus)
  : mIPCClosed(false)
  , mLoadContext(aLoadContext)
  , mPBOverride(aOverrideStatus)
  , mStatus(NS_OK)
  , mDivertingFromChild(false)
  , mDivertedOnStartRequest(false)
  , mSuspendedForDiversion(false)
  , mUseUTF8(false)
{
  nsIProtocolHandler* handler;
  CallGetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "ftp", &handler);
  MOZ_ASSERT(handler, "no ftp handler");

  if (aIframeEmbedding.type() == PBrowserOrId::TPBrowserParent) {
    mTabParent = static_cast<dom::TabParent*>(aIframeEmbedding.get_PBrowserParent());
  }

  mEventQ = new ChannelEventQueue(static_cast<nsIParentChannel*>(this));
}

FTPChannelParent::~FTPChannelParent()
{
  gFtpHandler->Release();
}

void
FTPChannelParent::ActorDestroy(ActorDestroyReason why)
{
  // We may still have refcount>0 if the channel hasn't called OnStopRequest
  // yet, but we must not send any more msgs to child.
  mIPCClosed = true;
}

//-----------------------------------------------------------------------------
// FTPChannelParent::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(FTPChannelParent,
                  nsIStreamListener,
                  nsIParentChannel,
                  nsIInterfaceRequestor,
                  nsIRequestObserver,
                  nsIChannelEventSink,
                  nsIFTPChannelParentInternal)

//-----------------------------------------------------------------------------
// FTPChannelParent::PFTPChannelParent
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// FTPChannelParent methods
//-----------------------------------------------------------------------------

bool
FTPChannelParent::Init(const FTPChannelCreationArgs& aArgs)
{
  switch (aArgs.type()) {
  case FTPChannelCreationArgs::TFTPChannelOpenArgs:
  {
    const FTPChannelOpenArgs& a = aArgs.get_FTPChannelOpenArgs();
    return DoAsyncOpen(a.uri(), a.startPos(), a.entityID(), a.uploadStream(),
                       a.loadInfo());
  }
  case FTPChannelCreationArgs::TFTPChannelConnectArgs:
  {
    const FTPChannelConnectArgs& cArgs = aArgs.get_FTPChannelConnectArgs();
    return ConnectChannel(cArgs.channelId());
  }
  default:
    NS_NOTREACHED("unknown open type");
    return false;
  }
}

bool
FTPChannelParent::DoAsyncOpen(const URIParams& aURI,
                              const uint64_t& aStartPos,
                              const nsCString& aEntityID,
                              const OptionalInputStreamParams& aUploadStream,
                              const OptionalLoadInfoArgs& aLoadInfoArgs)
{
  nsresult rv;

  nsCOMPtr<nsIURI> uri = DeserializeURI(aURI);
  if (!uri)
      return false;

#ifdef DEBUG
  LOG(("FTPChannelParent DoAsyncOpen [this=%p uri=%s]\n",
       this, uri->GetSpecOrDefault().get()));
#endif

  nsCOMPtr<nsIIOService> ios(do_GetIOService(&rv));
  if (NS_FAILED(rv)) {
    return SendFailedAsyncOpen(rv);
  }

  nsCOMPtr<nsILoadInfo> loadInfo;
  rv = mozilla::ipc::LoadInfoArgsToLoadInfo(aLoadInfoArgs,
                                            getter_AddRefs(loadInfo));
  if (NS_FAILED(rv)) {
    return SendFailedAsyncOpen(rv);
  }

  NeckoOriginAttributes attrs;
  rv = loadInfo->GetOriginAttributes(&attrs);
  if (NS_FAILED(rv)) {
    return SendFailedAsyncOpen(rv);
  }

  nsCOMPtr<nsIChannel> chan;
  rv = NS_NewChannelInternal(getter_AddRefs(chan), uri, loadInfo,
                             nullptr, nullptr,
                             nsIRequest::LOAD_NORMAL, ios);

  if (NS_FAILED(rv))
    return SendFailedAsyncOpen(rv);

  mChannel = chan;

  // later on mChannel may become an HTTP channel (we'll be redirected to one
  // if we're using a proxy), but for now this is safe
  nsFtpChannel* ftpChan = static_cast<nsFtpChannel*>(mChannel.get());

  if (mPBOverride != kPBOverride_Unset) {
    ftpChan->SetPrivate(mPBOverride == kPBOverride_Private ? true : false);
  }
  rv = ftpChan->SetNotificationCallbacks(this);
  if (NS_FAILED(rv))
    return SendFailedAsyncOpen(rv);

  nsTArray<mozilla::ipc::FileDescriptor> fds;
  nsCOMPtr<nsIInputStream> upload = DeserializeInputStream(aUploadStream, fds);
  if (upload) {
    // contentType and contentLength are ignored
    rv = ftpChan->SetUploadStream(upload, EmptyCString(), 0);
    if (NS_FAILED(rv))
      return SendFailedAsyncOpen(rv);
  }

  rv = ftpChan->ResumeAt(aStartPos, aEntityID);
  if (NS_FAILED(rv))
    return SendFailedAsyncOpen(rv);

  if (loadInfo && loadInfo->GetEnforceSecurity()) {
    rv = ftpChan->AsyncOpen2(this);
  }
  else {
    rv = ftpChan->AsyncOpen(this, nullptr);
  }

  if (NS_FAILED(rv))
    return SendFailedAsyncOpen(rv);

  return true;
}

bool
FTPChannelParent::ConnectChannel(const uint32_t& channelId)
{
  nsresult rv;

  LOG(("Looking for a registered channel [this=%p, id=%d]", this, channelId));

  nsCOMPtr<nsIChannel> channel;
  rv = NS_LinkRedirectChannels(channelId, this, getter_AddRefs(channel));
  if (NS_SUCCEEDED(rv))
    mChannel = channel;

  LOG(("  found channel %p, rv=%08x", mChannel.get(), rv));

  return true;
}

mozilla::ipc::IPCResult
FTPChannelParent::RecvCancel(const nsresult& status)
{
  if (mChannel)
    mChannel->Cancel(status);

  return IPC_OK();
}

mozilla::ipc::IPCResult
FTPChannelParent::RecvSuspend()
{
  if (mChannel) {
    SuspendChannel();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
FTPChannelParent::RecvResume()
{
  if (mChannel) {
    ResumeChannel();
  }
  return IPC_OK();
}

class FTPDivertDataAvailableEvent : public ChannelEvent
{
public:
  FTPDivertDataAvailableEvent(FTPChannelParent* aParent,
                              const nsCString& data,
                              const uint64_t& offset,
                              const uint32_t& count)
  : mParent(aParent)
  , mData(data)
  , mOffset(offset)
  , mCount(count)
  {
  }

  void Run()
  {
    mParent->DivertOnDataAvailable(mData, mOffset, mCount);
  }

private:
  FTPChannelParent* mParent;
  nsCString mData;
  uint64_t mOffset;
  uint32_t mCount;
};

mozilla::ipc::IPCResult
FTPChannelParent::RecvDivertOnDataAvailable(const nsCString& data,
                                            const uint64_t& offset,
                                            const uint32_t& count)
{
  if (NS_WARN_IF(!mDivertingFromChild)) {
    MOZ_ASSERT(mDivertingFromChild,
               "Cannot RecvDivertOnDataAvailable if diverting is not set!");
    FailDiversion(NS_ERROR_UNEXPECTED);
    return IPC_FAIL_NO_REASON(this);
  }

  // Drop OnDataAvailables if the parent was canceled already.
  if (NS_FAILED(mStatus)) {
    return IPC_OK();
  }

  mEventQ->RunOrEnqueue(new FTPDivertDataAvailableEvent(this, data, offset,
                                                        count));
  return IPC_OK();
}

void
FTPChannelParent::DivertOnDataAvailable(const nsCString& data,
                                        const uint64_t& offset,
                                        const uint32_t& count)
{
  LOG(("FTPChannelParent::DivertOnDataAvailable [this=%p]\n", this));

  if (NS_WARN_IF(!mDivertingFromChild)) {
    MOZ_ASSERT(mDivertingFromChild,
               "Cannot DivertOnDataAvailable if diverting is not set!");
    FailDiversion(NS_ERROR_UNEXPECTED);
    return;
  }

  // Drop OnDataAvailables if the parent was canceled already.
  if (NS_FAILED(mStatus)) {
    return;
  }

  nsCOMPtr<nsIInputStream> stringStream;
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stringStream), data.get(),
                                      count, NS_ASSIGNMENT_DEPEND);
  if (NS_FAILED(rv)) {
    if (mChannel) {
      mChannel->Cancel(rv);
    }
    mStatus = rv;
    return;
  }

  AutoEventEnqueuer ensureSerialDispatch(mEventQ);

  rv = OnDataAvailable(mChannel, nullptr, stringStream, offset, count);

  stringStream->Close();
  if (NS_FAILED(rv)) {
    if (mChannel) {
      mChannel->Cancel(rv);
    }
    mStatus = rv;
  }
}

class FTPDivertStopRequestEvent : public ChannelEvent
{
public:
  FTPDivertStopRequestEvent(FTPChannelParent* aParent,
                            const nsresult& statusCode)
  : mParent(aParent)
  , mStatusCode(statusCode)
  {
  }

  void Run() {
    mParent->DivertOnStopRequest(mStatusCode);
  }

private:
  FTPChannelParent* mParent;
  nsresult mStatusCode;
};

mozilla::ipc::IPCResult
FTPChannelParent::RecvDivertOnStopRequest(const nsresult& statusCode)
{
  if (NS_WARN_IF(!mDivertingFromChild)) {
    MOZ_ASSERT(mDivertingFromChild,
               "Cannot RecvDivertOnStopRequest if diverting is not set!");
    FailDiversion(NS_ERROR_UNEXPECTED);
    return IPC_FAIL_NO_REASON(this);
  }

  mEventQ->RunOrEnqueue(new FTPDivertStopRequestEvent(this, statusCode));
  return IPC_OK();
}

void
FTPChannelParent::DivertOnStopRequest(const nsresult& statusCode)
{
  LOG(("FTPChannelParent::DivertOnStopRequest [this=%p]\n", this));

  if (NS_WARN_IF(!mDivertingFromChild)) {
    MOZ_ASSERT(mDivertingFromChild,
               "Cannot DivertOnStopRequest if diverting is not set!");
    FailDiversion(NS_ERROR_UNEXPECTED);
    return;
  }

  // Honor the channel's status even if the underlying transaction completed.
  nsresult status = NS_FAILED(mStatus) ? mStatus : statusCode;

  // Reset fake pending status in case OnStopRequest has already been called.
  if (mChannel) {
    nsCOMPtr<nsIForcePendingChannel> forcePendingIChan = do_QueryInterface(mChannel);
    if (forcePendingIChan) {
      forcePendingIChan->ForcePending(false);
    }
  }

  AutoEventEnqueuer ensureSerialDispatch(mEventQ);
  OnStopRequest(mChannel, nullptr, status);
}

class FTPDivertCompleteEvent : public ChannelEvent
{
public:
  explicit FTPDivertCompleteEvent(FTPChannelParent* aParent)
  : mParent(aParent)
  {
  }

  void Run() {
    mParent->DivertComplete();
  }

private:
  FTPChannelParent* mParent;
};

mozilla::ipc::IPCResult
FTPChannelParent::RecvDivertComplete()
{
  if (NS_WARN_IF(!mDivertingFromChild)) {
    MOZ_ASSERT(mDivertingFromChild,
               "Cannot RecvDivertComplete if diverting is not set!");
    FailDiversion(NS_ERROR_UNEXPECTED);
    return IPC_FAIL_NO_REASON(this);
  }

  mEventQ->RunOrEnqueue(new FTPDivertCompleteEvent(this));
  return IPC_OK();
}

void
FTPChannelParent::DivertComplete()
{
  LOG(("FTPChannelParent::DivertComplete [this=%p]\n", this));

  if (NS_WARN_IF(!mDivertingFromChild)) {
    MOZ_ASSERT(mDivertingFromChild,
               "Cannot DivertComplete if diverting is not set!");
    FailDiversion(NS_ERROR_UNEXPECTED);
    return;
  }

  nsresult rv = ResumeForDiversion();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailDiversion(NS_ERROR_UNEXPECTED);
  }
}

//-----------------------------------------------------------------------------
// FTPChannelParent::nsIRequestObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
FTPChannelParent::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  LOG(("FTPChannelParent::OnStartRequest [this=%p]\n", this));

  if (mDivertingFromChild) {
    MOZ_RELEASE_ASSERT(mDivertToListener,
                       "Cannot divert if listener is unset!");
    return mDivertToListener->OnStartRequest(aRequest, aContext);
  }

  nsCOMPtr<nsIChannel> chan = do_QueryInterface(aRequest);
  MOZ_ASSERT(chan);
  NS_ENSURE_TRUE(chan, NS_ERROR_UNEXPECTED);

  int64_t contentLength;
  chan->GetContentLength(&contentLength);
  nsCString contentType;
  chan->GetContentType(contentType);

  nsCString entityID;
  nsCOMPtr<nsIResumableChannel> resChan = do_QueryInterface(aRequest);
  MOZ_ASSERT(resChan); // both FTP and HTTP should implement nsIResumableChannel
  if (resChan) {
    resChan->GetEntityID(entityID);
  }

  PRTime lastModified = 0;
  nsCOMPtr<nsIFTPChannel> ftpChan = do_QueryInterface(aRequest);
  if (ftpChan) {
    ftpChan->GetLastModifiedTime(&lastModified);
  }
  nsCOMPtr<nsIHttpChannelInternal> httpChan = do_QueryInterface(aRequest);
  if (httpChan) {
    httpChan->GetLastModifiedTime(&lastModified);
  }

  URIParams uriparam;
  nsCOMPtr<nsIURI> uri;
  chan->GetURI(getter_AddRefs(uri));
  SerializeURI(uri, uriparam);

  if (mIPCClosed || !SendOnStartRequest(mStatus, contentLength, contentType,
                                        lastModified, entityID, uriparam)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
FTPChannelParent::OnStopRequest(nsIRequest* aRequest,
                                nsISupports* aContext,
                                nsresult aStatusCode)
{
  LOG(("FTPChannelParent::OnStopRequest: [this=%p status=%ul]\n",
       this, aStatusCode));

  if (mDivertingFromChild) {
    MOZ_RELEASE_ASSERT(mDivertToListener,
                       "Cannot divert if listener is unset!");
    return mDivertToListener->OnStopRequest(aRequest, aContext, aStatusCode);
  }

  if (mIPCClosed || !SendOnStopRequest(aStatusCode, mErrorMsg, mUseUTF8)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// FTPChannelParent::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
FTPChannelParent::OnDataAvailable(nsIRequest* aRequest,
                                  nsISupports* aContext,
                                  nsIInputStream* aInputStream,
                                  uint64_t aOffset,
                                  uint32_t aCount)
{
  LOG(("FTPChannelParent::OnDataAvailable [this=%p]\n", this));

  if (mDivertingFromChild) {
    MOZ_RELEASE_ASSERT(mDivertToListener,
                       "Cannot divert if listener is unset!");
    return mDivertToListener->OnDataAvailable(aRequest, aContext, aInputStream,
                                              aOffset, aCount);
  }

  nsCString data;
  nsresult rv = NS_ReadInputStreamToString(aInputStream, data, aCount);
  if (NS_FAILED(rv))
    return rv;

  if (mIPCClosed || !SendOnDataAvailable(mStatus, data, aOffset, aCount))
    return NS_ERROR_UNEXPECTED;

  return NS_OK;
}

//-----------------------------------------------------------------------------
// FTPChannelParent::nsIParentChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
FTPChannelParent::SetParentListener(HttpChannelParentListener* aListener)
{
  // Do not need ptr to HttpChannelParentListener.
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelParent::NotifyTrackingProtectionDisabled()
{
  // One day, this should probably be filled in.
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelParent::Delete()
{
  if (mIPCClosed || !SendDeleteSelf())
    return NS_ERROR_UNEXPECTED;

  return NS_OK;
}

//-----------------------------------------------------------------------------
// FTPChannelParent::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
FTPChannelParent::GetInterface(const nsIID& uuid, void** result)
{
  if (uuid.Equals(NS_GET_IID(nsIAuthPromptProvider)) ||
      uuid.Equals(NS_GET_IID(nsISecureBrowserUI))) {
    if (mTabParent) {
      return mTabParent->QueryInterface(uuid, result);
    }
  } else if (uuid.Equals(NS_GET_IID(nsIAuthPrompt)) ||
             uuid.Equals(NS_GET_IID(nsIAuthPrompt2))) {
    nsCOMPtr<nsIAuthPromptProvider> provider(do_QueryObject(mTabParent));
    if (provider) {
      return provider->GetAuthPrompt(nsIAuthPromptProvider::PROMPT_NORMAL,
                                     uuid,
                                     result);
    }
  }

  // Only support nsILoadContext if child channel's callbacks did too
  if (uuid.Equals(NS_GET_IID(nsILoadContext)) && mLoadContext) {
    nsCOMPtr<nsILoadContext> copy = mLoadContext;
    copy.forget(result);
    return NS_OK;
  }

  return QueryInterface(uuid, result);
}

nsresult
FTPChannelParent::SuspendChannel()
{
  nsCOMPtr<nsIChannelWithDivertableParentListener> chan =
    do_QueryInterface(mChannel);
  if (chan) {
    return chan->SuspendInternal();
  } else {
    return mChannel->Suspend();
  }
}

nsresult
FTPChannelParent::ResumeChannel()
{
  nsCOMPtr<nsIChannelWithDivertableParentListener> chan =
    do_QueryInterface(mChannel);
  if (chan) {
    return chan->ResumeInternal();
  } else {
    return mChannel->Resume();
  }
}

//-----------------------------------------------------------------------------
// FTPChannelParent::ADivertableParentChannel
//-----------------------------------------------------------------------------
nsresult
FTPChannelParent::SuspendForDiversion()
{
  MOZ_ASSERT(mChannel);
  if (NS_WARN_IF(mDivertingFromChild)) {
    MOZ_ASSERT(!mDivertingFromChild, "Already suspended for diversion!");
    return NS_ERROR_UNEXPECTED;
  }

  // Try suspending the channel. Allow it to fail, since OnStopRequest may have
  // been called and thus the channel may not be pending.
  nsresult rv = SuspendChannel();
  MOZ_ASSERT(NS_SUCCEEDED(rv) || rv == NS_ERROR_NOT_AVAILABLE);
  mSuspendedForDiversion = NS_SUCCEEDED(rv);

  // Once this is set, no more OnStart/OnData/OnStop callbacks should be sent
  // to the child.
  mDivertingFromChild = true;

  nsCOMPtr<nsIChannelWithDivertableParentListener> chan =
    do_QueryInterface(mChannel);
  if (chan) {
    chan->MessageDiversionStarted(this);
  }

  return NS_OK;
}

/* private, supporting function for ADivertableParentChannel */
nsresult
FTPChannelParent::ResumeForDiversion()
{
  MOZ_ASSERT(mChannel);
  MOZ_ASSERT(mDivertToListener);
  if (NS_WARN_IF(!mDivertingFromChild)) {
    MOZ_ASSERT(mDivertingFromChild,
               "Cannot ResumeForDiversion if not diverting!");
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIChannelWithDivertableParentListener> chan =
    do_QueryInterface(mChannel);
  if (chan) {
    chan->MessageDiversionStop();
  }

  if (mSuspendedForDiversion) {
    nsresult rv = ResumeChannel();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      FailDiversion(NS_ERROR_UNEXPECTED, true);
      return rv;
    }
    mSuspendedForDiversion = false;
  }

  // Delete() will tear down IPDL, but ref from underlying nsFTPChannel will
  // keep us alive if there's more data to be delivered to listener.
  if (NS_WARN_IF(NS_FAILED(Delete()))) {
    FailDiversion(NS_ERROR_UNEXPECTED);
    return NS_ERROR_UNEXPECTED;   
  }
  return NS_OK;
}

nsresult
FTPChannelParent::SuspendMessageDiversion()
{
  // This only need to suspend message queue.
  mEventQ->Suspend();
  return NS_OK;
}

nsresult
FTPChannelParent::ResumeMessageDiversion()
{
  // This only need to resumes message queue.
  mEventQ->Resume();
  return NS_OK;
}

void
FTPChannelParent::DivertTo(nsIStreamListener *aListener)
{
  MOZ_ASSERT(aListener);
  if (NS_WARN_IF(!mDivertingFromChild)) {
    MOZ_ASSERT(mDivertingFromChild,
               "Cannot DivertTo new listener if diverting is not set!");
    return;
  }

  if (NS_WARN_IF(mIPCClosed || !SendFlushedForDiversion())) {
    FailDiversion(NS_ERROR_UNEXPECTED);
    return;
  }

  mDivertToListener = aListener;

  // Call OnStartRequest and SendDivertMessages asynchronously to avoid
  // reentering client context.
  NS_DispatchToCurrentThread(
    NewRunnableMethod(this, &FTPChannelParent::StartDiversion));
  return;
}

void
FTPChannelParent::StartDiversion()
{
  if (NS_WARN_IF(!mDivertingFromChild)) {
    MOZ_ASSERT(mDivertingFromChild,
               "Cannot StartDiversion if diverting is not set!");
    return;
  }

  // Fake pending status in case OnStopRequest has already been called.
  if (mChannel) {
    nsCOMPtr<nsIForcePendingChannel> forcePendingIChan = do_QueryInterface(mChannel);
    if (forcePendingIChan) {
      forcePendingIChan->ForcePending(true);
    }
  }

  {
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);
    // Call OnStartRequest for the "DivertTo" listener.
    nsresult rv = OnStartRequest(mChannel, nullptr);
    if (NS_FAILED(rv)) {
      if (mChannel) {
        mChannel->Cancel(rv);
      }
      mStatus = rv;
      return;
    }
  }

  // After OnStartRequest has been called, tell FTPChannelChild to divert the
  // OnDataAvailables and OnStopRequest to this FTPChannelParent.
  if (NS_WARN_IF(mIPCClosed || !SendDivertMessages())) {
    FailDiversion(NS_ERROR_UNEXPECTED);
    return;
  }
}

class FTPFailDiversionEvent : public Runnable
{
public:
  FTPFailDiversionEvent(FTPChannelParent *aChannelParent,
                        nsresult aErrorCode,
                        bool aSkipResume)
    : mChannelParent(aChannelParent)
    , mErrorCode(aErrorCode)
    , mSkipResume(aSkipResume)
  {
    MOZ_RELEASE_ASSERT(aChannelParent);
    MOZ_RELEASE_ASSERT(NS_FAILED(aErrorCode));
  }
  NS_IMETHOD Run() override
  {
    mChannelParent->NotifyDiversionFailed(mErrorCode, mSkipResume);
    return NS_OK;
  }
private:
  RefPtr<FTPChannelParent> mChannelParent;
  nsresult mErrorCode;
  bool mSkipResume;
};

void
FTPChannelParent::FailDiversion(nsresult aErrorCode,
                                            bool aSkipResume)
{
  MOZ_RELEASE_ASSERT(NS_FAILED(aErrorCode));
  MOZ_RELEASE_ASSERT(mDivertingFromChild);
  MOZ_RELEASE_ASSERT(mDivertToListener);
  MOZ_RELEASE_ASSERT(mChannel);

  NS_DispatchToCurrentThread(
    new FTPFailDiversionEvent(this, aErrorCode, aSkipResume));
}

void
FTPChannelParent::NotifyDiversionFailed(nsresult aErrorCode,
                                        bool aSkipResume)
{
  MOZ_RELEASE_ASSERT(NS_FAILED(aErrorCode));
  MOZ_RELEASE_ASSERT(mDivertingFromChild);
  MOZ_RELEASE_ASSERT(mDivertToListener);
  MOZ_RELEASE_ASSERT(mChannel);

  mChannel->Cancel(aErrorCode);
  nsCOMPtr<nsIForcePendingChannel> forcePendingIChan = do_QueryInterface(mChannel);
  if (forcePendingIChan) {
    forcePendingIChan->ForcePending(false);
  }

  bool isPending = false;
  nsresult rv = mChannel->IsPending(&isPending);
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));

  // Resume only we suspended earlier.
  if (mSuspendedForDiversion) {
    ResumeChannel();
  }
  // Channel has already sent OnStartRequest to the child, so ensure that we
  // call it here if it hasn't already been called.
  if (!mDivertedOnStartRequest) {
    nsCOMPtr<nsIForcePendingChannel> forcePendingIChan = do_QueryInterface(mChannel);
    if (forcePendingIChan) {
      forcePendingIChan->ForcePending(true);
    }
    mDivertToListener->OnStartRequest(mChannel, nullptr);

    if (forcePendingIChan) {
      forcePendingIChan->ForcePending(false);
    }
  }
  // If the channel is pending, it will call OnStopRequest itself; otherwise, do
  // it here.
  if (!isPending) {
    mDivertToListener->OnStopRequest(mChannel, nullptr, aErrorCode);
  }
  mDivertToListener = nullptr;
  mChannel = nullptr;

  if (!mIPCClosed) {
    Unused << SendDeleteSelf();
  }
}

//-----------------------------------------------------------------------------
// FTPChannelParent::nsIChannelEventSink
//-----------------------------------------------------------------------------

NS_IMETHODIMP
FTPChannelParent::AsyncOnChannelRedirect(
                            nsIChannel *oldChannel,
                            nsIChannel *newChannel,
                            uint32_t redirectFlags,
                            nsIAsyncVerifyRedirectCallback* callback)
{
  nsCOMPtr<nsIFTPChannel> ftpChan = do_QueryInterface(newChannel);
  if (!ftpChan) {
    // when FTP is set to use HTTP proxying, we wind up getting redirected to an HTTP channel.
    nsCOMPtr<nsIHttpChannel> httpChan = do_QueryInterface(newChannel);
    if (!httpChan)
      return NS_ERROR_UNEXPECTED; 
  }
  mChannel = newChannel;
  callback->OnRedirectVerifyCallback(NS_OK);
  return NS_OK; 
}

NS_IMETHODIMP
FTPChannelParent::SetErrorMsg(const char *aMsg, bool aUseUTF8)
{
  mErrorMsg = aMsg;
  mUseUTF8 = aUseUTF8;
  return NS_OK;
}

//---------------------
} // namespace net
} // namespace mozilla

