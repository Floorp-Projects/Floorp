/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "HttpTransactionChild.h"

#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/net/BackgroundDataBridgeParent.h"
#include "mozilla/net/InputChannelThrottleQueueChild.h"
#include "mozilla/net/SocketProcessChild.h"
#include "mozilla/StaticPrefs_network.h"
#include "nsInputStreamPump.h"
#include "nsHttpHandler.h"
#include "nsProxyInfo.h"
#include "nsProxyRelease.h"
#include "nsQueryObject.h"
#include "nsSerializationHelper.h"

using mozilla::ipc::BackgroundParent;

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(HttpTransactionChild, nsIRequestObserver, nsIStreamListener,
                  nsITransportEventSink, nsIThrottledInputChannel,
                  nsIThreadRetargetableStreamListener);

//-----------------------------------------------------------------------------
// HttpTransactionChild <public>
//-----------------------------------------------------------------------------

HttpTransactionChild::HttpTransactionChild()
    : mCanceled(false), mStatus(NS_OK), mChannelId(0), mIsDocumentLoad(false) {
  LOG(("Creating HttpTransactionChild @%p\n", this));
}

HttpTransactionChild::~HttpTransactionChild() {
  LOG(("Destroying HttpTransactionChild @%p\n", this));
}

static already_AddRefed<nsIRequestContext> CreateRequestContext(
    uint64_t aRequestContextID) {
  if (!aRequestContextID) {
    return nullptr;
  }

  nsIRequestContextService* rcsvc = gHttpHandler->GetRequestContextService();
  if (!rcsvc) {
    return nullptr;
  }

  nsCOMPtr<nsIRequestContext> requestContext;
  rcsvc->GetRequestContext(aRequestContextID, getter_AddRefs(requestContext));

  return requestContext.forget();
}

nsresult HttpTransactionChild::InitInternal(
    uint32_t caps, const HttpConnectionInfoCloneArgs& infoArgs,
    nsHttpRequestHead* requestHead, nsIInputStream* requestBody,
    uint64_t requestContentLength, bool requestBodyHasHeaders,
    uint64_t topLevelOuterContentWindowId, uint8_t httpTrafficCategory,
    uint64_t requestContextID, uint32_t classOfService, uint32_t initialRwin,
    bool responseTimeoutEnabled, uint64_t channelId,
    bool aHasTransactionObserver,
    const Maybe<H2PushedStreamArg>& aPushedStreamArg) {
  LOG(("HttpTransactionChild::InitInternal [this=%p caps=%x]\n", this, caps));

  RefPtr<nsHttpConnectionInfo> cinfo =
      nsHttpConnectionInfo::DeserializeHttpConnectionInfoCloneArgs(infoArgs);
  nsCOMPtr<nsIRequestContext> rc = CreateRequestContext(requestContextID);

  HttpTransactionShell::OnPushCallback pushCallback = nullptr;
  if (caps & NS_HTTP_ONPUSH_LISTENER) {
    RefPtr<HttpTransactionChild> self = this;
    pushCallback = [self](uint32_t aPushedStreamId, const nsACString& aUrl,
                          const nsACString& aRequestString,
                          HttpTransactionShell* aTransaction) {
      bool res = false;
      if (self->CanSend()) {
        res =
            self->SendOnH2PushStream(aPushedStreamId, PromiseFlatCString(aUrl),
                                     PromiseFlatCString(aRequestString));
      }
      return res ? NS_OK : NS_ERROR_FAILURE;
    };
  }

  std::function<void(TransactionObserverResult &&)> observer;
  if (aHasTransactionObserver) {
    nsMainThreadPtrHandle<HttpTransactionChild> handle(
        new nsMainThreadPtrHolder<HttpTransactionChild>(
            "HttpTransactionChildProxy", this, false));
    observer = [handle](TransactionObserverResult&& aResult) {
      handle->mTransactionObserverResult.emplace(std::move(aResult));
    };
  }

  RefPtr<nsHttpTransaction> transWithPushedStream;
  uint32_t pushedStreamId = 0;
  if (aPushedStreamArg) {
    HttpTransactionChild* transChild = static_cast<HttpTransactionChild*>(
        aPushedStreamArg.ref().transWithPushedStreamChild());
    transWithPushedStream = transChild->GetHttpTransaction();
    pushedStreamId = aPushedStreamArg.ref().pushedStreamId();
  }

  nsresult rv = mTransaction->Init(
      caps, cinfo, requestHead, requestBody, requestContentLength,
      requestBodyHasHeaders, GetCurrentThreadEventTarget(),
      nullptr,  // TODO: security callback, fix in bug 1512479.
      this, topLevelOuterContentWindowId,
      static_cast<HttpTrafficCategory>(httpTrafficCategory), rc, classOfService,
      initialRwin, responseTimeoutEnabled, channelId, std::move(observer),
      std::move(pushCallback), transWithPushedStream, pushedStreamId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mTransaction = nullptr;
    return rv;
  }

  Unused << mTransaction->AsyncRead(this, getter_AddRefs(mTransactionPump));
  return rv;
}

mozilla::ipc::IPCResult HttpTransactionChild::RecvCancelPump(
    const nsresult& aStatus) {
  LOG(("HttpTransactionChild::RecvCancelPump start [this=%p]\n", this));
  CancelInternal(aStatus);
  return IPC_OK();
}

void HttpTransactionChild::CancelInternal(nsresult aStatus) {
  MOZ_ASSERT(NS_FAILED(aStatus));

  mCanceled = true;
  mStatus = aStatus;
  if (mTransactionPump) {
    mTransactionPump->Cancel(mStatus);
  }
}

mozilla::ipc::IPCResult HttpTransactionChild::RecvSuspendPump() {
  LOG(("HttpTransactionChild::RecvSuspendPump start [this=%p]\n", this));

  if (mTransactionPump) {
    mTransactionPump->Suspend();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult HttpTransactionChild::RecvResumePump() {
  LOG(("HttpTransactionChild::RecvResumePump start [this=%p]\n", this));

  if (mTransactionPump) {
    mTransactionPump->Resume();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult HttpTransactionChild::RecvInit(
    const uint32_t& aCaps, const HttpConnectionInfoCloneArgs& aArgs,
    const nsHttpRequestHead& aReqHeaders, const Maybe<IPCStream>& aRequestBody,
    const uint64_t& aReqContentLength, const bool& aReqBodyIncludesHeaders,
    const uint64_t& aTopLevelOuterContentWindowId,
    const uint8_t& aHttpTrafficCategory, const uint64_t& aRequestContextID,
    const uint32_t& aClassOfService, const uint32_t& aInitialRwin,
    const bool& aResponseTimeoutEnabled, const uint64_t& aChannelId,
    const bool& aHasTransactionObserver,
    const Maybe<H2PushedStreamArg>& aPushedStreamArg,
    const mozilla::Maybe<PInputChannelThrottleQueueChild*>& aThrottleQueue,
    const bool& aIsDocumentLoad) {
  mRequestHead = aReqHeaders;
  if (aRequestBody) {
    mUploadStream = mozilla::ipc::DeserializeIPCStream(aRequestBody);
  }

  mTransaction = new nsHttpTransaction();
  mChannelId = aChannelId;
  mIsDocumentLoad = aIsDocumentLoad;

  if (aThrottleQueue.isSome()) {
    mThrottleQueue =
        static_cast<InputChannelThrottleQueueChild*>(aThrottleQueue.ref());
  }

  nsresult rv = InitInternal(
      aCaps, aArgs, &mRequestHead, mUploadStream, aReqContentLength,
      aReqBodyIncludesHeaders, aTopLevelOuterContentWindowId,
      aHttpTrafficCategory, aRequestContextID, aClassOfService, aInitialRwin,
      aResponseTimeoutEnabled, aChannelId, aHasTransactionObserver,
      aPushedStreamArg);
  if (NS_FAILED(rv)) {
    LOG(("HttpTransactionChild::RecvInit: [this=%p] InitInternal failed!\n",
         this));
    mTransaction = nullptr;
    SendOnInitFailed(rv);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult HttpTransactionChild::RecvUpdateClassOfService(
    const uint32_t& classOfService) {
  LOG(("HttpTransactionChild::RecvUpdateClassOfService start [this=%p]\n",
       this));
  if (mTransaction) {
    mTransaction->SetClassOfService(classOfService);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult HttpTransactionChild::RecvSetDNSWasRefreshed() {
  LOG(("HttpTransactionChild::SetDNSWasRefreshed [this=%p]\n", this));
  if (mTransaction) {
    mTransaction->SetDNSWasRefreshed();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult HttpTransactionChild::RecvDontReuseConnection() {
  LOG(("HttpTransactionChild::RecvDontReuseConnection [this=%p]\n", this));
  if (mTransaction) {
    mTransaction->DontReuseConnection();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult HttpTransactionChild::RecvSetH2WSConnRefTaken() {
  LOG(("HttpTransactionChild::RecvSetH2WSConnRefTaken [this=%p]\n", this));
  if (mTransaction) {
    mTransaction->SetH2WSConnRefTaken();
  }
  return IPC_OK();
}

void HttpTransactionChild::ActorDestroy(ActorDestroyReason aWhy) {
  LOG(("HttpTransactionChild::ActorDestroy [this=%p]\n", this));
  mTransaction = nullptr;
  mTransactionPump = nullptr;
}

nsHttpTransaction* HttpTransactionChild::GetHttpTransaction() {
  return mTransaction.get();
}

//-----------------------------------------------------------------------------
// HttpTransactionChild <nsIStreamListener>
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpTransactionChild::OnDataAvailable(nsIRequest* aRequest,
                                      nsIInputStream* aInputStream,
                                      uint64_t aOffset, uint32_t aCount) {
  LOG(("HttpTransactionChild::OnDataAvailable [this=%p, aOffset= %" PRIu64
       " aCount=%" PRIu32 "]\n",
       this, aOffset, aCount));

  // Don't bother sending IPC if already canceled.
  if (mCanceled) {
    return mStatus;
  }

  // TODO: send string data in chunks and handle errors. Bug 1600129.
  nsCString data;
  nsresult rv = NS_ReadInputStreamToString(aInputStream, data, aCount);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (NS_IsMainThread()) {
    if (!CanSend()) {
      return NS_ERROR_FAILURE;
    }

    LOG(("  ODA to parent process"));
    Unused << SendOnDataAvailable(data, aOffset, aCount, false);
    return NS_OK;
  }

  ipc::AssertIsOnBackgroundThread();
  MOZ_ASSERT(mDataBridgeParent);

  if (!mDataBridgeParent->CanSend()) {
    return NS_ERROR_FAILURE;
  }

  bool dataSentToContentProcess =
      mDataBridgeParent->SendOnTransportAndData(aOffset, aCount, data);
  LOG(("  ODA to content process, dataSentToContentProcess=%d",
       dataSentToContentProcess));
  if (!dataSentToContentProcess) {
    MOZ_ASSERT(false, "Send ODA to content process failed");
    return NS_ERROR_FAILURE;
  }

  // We still need to send ODA to parent process, because the data needs to be
  // saved in cache. Note that we set dataSentToChildProcess to true, to this
  // ODA will not be sent to child process.
  RefPtr<HttpTransactionChild> self = this;
  rv = NS_DispatchToMainThread(
      NS_NewRunnableFunction(
          "HttpTransactionChild::OnDataAvailable",
          [self, offset(aOffset), count(aCount), data(data)]() {
            if (!self->SendOnDataAvailable(data, offset, count, true)) {
              self->CancelInternal(NS_ERROR_FAILURE);
            }
          }),
      NS_DISPATCH_NORMAL);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  return NS_OK;
}

static TimingStructArgs ToTimingStructArgs(TimingStruct aTiming) {
  TimingStructArgs args;
  args.domainLookupStart() = aTiming.domainLookupStart;
  args.domainLookupEnd() = aTiming.domainLookupEnd;
  args.connectStart() = aTiming.connectStart;
  args.tcpConnectEnd() = aTiming.tcpConnectEnd;
  args.secureConnectionStart() = aTiming.secureConnectionStart;
  args.connectEnd() = aTiming.connectEnd;
  args.requestStart() = aTiming.requestStart;
  args.responseStart() = aTiming.responseStart;
  args.responseEnd() = aTiming.responseEnd;
  return args;
}

// The maximum number of bytes to consider when attempting to sniff.
// See https://mimesniff.spec.whatwg.org/#reading-the-resource-header.
static const uint32_t MAX_BYTES_SNIFFED = 1445;

static void GetDataForSniffer(void* aClosure, const uint8_t* aData,
                              uint32_t aCount) {
  nsTArray<uint8_t>* outData = static_cast<nsTArray<uint8_t>*>(aClosure);
  outData->AppendElements(aData, std::min(aCount, MAX_BYTES_SNIFFED));
}

bool HttpTransactionChild::CanSendODAToContentProcessDirectly(
    const Maybe<nsHttpResponseHead>& aHead) {
  if (!StaticPrefs::network_send_ODA_to_content_directly()) {
    return false;
  }

  // If this is a document load, the content process that receives ODA is not
  // decided yet, so don't bother to do the rest check.
  if (mIsDocumentLoad) {
    return false;
  }

  if (!aHead) {
    return false;
  }

  // We only need to deliver ODA when the response is succeed.
  if (aHead->Status() != 200) {
    return false;
  }

  // UnknownDecoder could be used in parent process, so we can't send ODA to
  // content process.
  if (!aHead->HasContentType()) {
    return false;
  }

  return true;
}

NS_IMETHODIMP
HttpTransactionChild::OnStartRequest(nsIRequest* aRequest) {
  LOG(("HttpTransactionChild::OnStartRequest start [this=%p] mTransaction=%p\n",
       this, mTransaction.get()));

  // Don't bother sending IPC to parent process if already canceled.
  if (mCanceled) {
    return mStatus;
  }

  if (!CanSend()) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(mTransaction);

  nsresult status;
  aRequest->GetStatus(&status);

  nsCString serializedSecurityInfoOut;
  nsCOMPtr<nsISupports> secInfoSupp = mTransaction->SecurityInfo();
  if (secInfoSupp) {
    nsCOMPtr<nsISerializable> secInfoSer = do_QueryInterface(secInfoSupp);
    if (secInfoSer) {
      NS_SerializeToString(secInfoSer, serializedSecurityInfoOut);
    }
  }

  UniquePtr<nsHttpResponseHead> head(mTransaction->TakeResponseHead());
  Maybe<nsHttpResponseHead> optionalHead;
  nsTArray<uint8_t> dataForSniffer;
  if (head) {
    optionalHead = Some(*head);
    if (mTransaction->Caps() & NS_HTTP_CALL_CONTENT_SNIFFER) {
      nsAutoCString contentTypeOptionsHeader;
      if (!(head->GetContentTypeOptionsHeader(contentTypeOptionsHeader) &&
            contentTypeOptionsHeader.EqualsIgnoreCase("nosniff"))) {
        RefPtr<nsInputStreamPump> pump = do_QueryObject(mTransactionPump);
        pump->PeekStream(GetDataForSniffer, &dataForSniffer);
      }
    }
  }

  if (CanSendODAToContentProcessDirectly(optionalHead)) {
    Maybe<RefPtr<BackgroundDataBridgeParent>> dataBridgeParent =
        SocketProcessChild::GetSingleton()->GetAndRemoveDataBridge(mChannelId);
    // Check if there is a registered BackgroundDataBridgeParent.
    if (dataBridgeParent) {
      mDataBridgeParent = std::move(dataBridgeParent.ref());

      nsCOMPtr<nsIThread> backgroundThread =
          mDataBridgeParent->GetBackgroundThread();
      nsCOMPtr<nsIThreadRetargetableRequest> retargetableTransactionPump;
      retargetableTransactionPump = do_QueryObject(mTransactionPump);
      // nsInputStreamPump should implement this interface.
      MOZ_ASSERT(retargetableTransactionPump);

      nsresult rv =
          retargetableTransactionPump->RetargetDeliveryTo(backgroundThread);
      LOG((" Retarget to background thread [this=%p rv=%08x]\n", this,
           static_cast<uint32_t>(rv)));
      if (NS_FAILED(rv)) {
        mDataBridgeParent->Destroy();
        mDataBridgeParent = nullptr;
      }
    }
  }

  int32_t proxyConnectResponseCode =
      mTransaction->GetProxyConnectResponseCode();

  Unused << SendOnStartRequest(status, optionalHead, serializedSecurityInfoOut,
                               mTransaction->ProxyConnectFailed(),
                               ToTimingStructArgs(mTransaction->Timings()),
                               proxyConnectResponseCode, dataForSniffer);
  return NS_OK;
}

NS_IMETHODIMP
HttpTransactionChild::OnStopRequest(nsIRequest* aRequest, nsresult aStatus) {
  LOG(("HttpTransactionChild::OnStopRequest [this=%p]\n", this));

  if (mDataBridgeParent) {
    mDataBridgeParent->Destroy();
    mDataBridgeParent = nullptr;
  }

  // Don't bother sending IPC to parent process if already canceled.
  if (mCanceled) {
    return mStatus;
  }

  if (!CanSend()) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(mTransaction);

  UniquePtr<nsHttpHeaderArray> headerArray(
      mTransaction->TakeResponseTrailers());
  Maybe<nsHttpHeaderArray> responseTrailers;
  if (headerArray) {
    responseTrailers.emplace(*headerArray);
  }

  Unused << SendOnStopRequest(
      aStatus, mTransaction->ResponseIsComplete(),
      mTransaction->GetTransferSize(),
      ToTimingStructArgs(mTransaction->Timings()), responseTrailers,
      mTransaction->HasStickyConnection(), mTransactionObserverResult);

  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpTransactionChild <nsITransportEventSink>
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpTransactionChild::OnTransportStatus(nsITransport* aTransport,
                                        nsresult aStatus, int64_t aProgress,
                                        int64_t aProgressMax) {
  LOG(("HttpTransactionChild::OnTransportStatus [this=%p status=%" PRIx32
       " progress=%" PRId64 "]\n",
       this, static_cast<uint32_t>(aStatus), aProgress));

  if (!CanSend()) {
    return NS_OK;
  }

  if (aStatus == NS_NET_STATUS_CONNECTED_TO ||
      aStatus == NS_NET_STATUS_WAITING_FOR) {
    NetAddr selfAddr;
    NetAddr peerAddr;
    bool isTrr = false;
    if (mTransaction) {
      mTransaction->GetNetworkAddresses(selfAddr, peerAddr, isTrr);
    } else {
      nsCOMPtr<nsISocketTransport> socketTransport =
          do_QueryInterface(aTransport);
      if (socketTransport) {
        socketTransport->GetSelfAddr(&selfAddr);
        socketTransport->GetPeerAddr(&peerAddr);
        socketTransport->ResolvedByTRR(&isTrr);
      }
    }
    Unused << SendOnNetAddrUpdate(selfAddr, peerAddr, isTrr);
  }

  Unused << SendOnTransportStatus(aStatus, aProgress, aProgressMax);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsIThrottledInputChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpTransactionChild::SetThrottleQueue(nsIInputChannelThrottleQueue* aQueue) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
HttpTransactionChild::GetThrottleQueue(nsIInputChannelThrottleQueue** aQueue) {
  nsCOMPtr<nsIInputChannelThrottleQueue> queue =
      static_cast<nsIInputChannelThrottleQueue*>(mThrottleQueue.get());
  queue.forget(aQueue);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// EventSourceImpl::nsIThreadRetargetableStreamListener
//-----------------------------------------------------------------------------
NS_IMETHODIMP
HttpTransactionChild::CheckListenerChain() {
  MOZ_ASSERT(NS_IsMainThread(), "Should be on the main thread!");
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
