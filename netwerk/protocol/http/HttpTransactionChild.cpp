/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "HttpTransactionChild.h"

#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/net/InputChannelThrottleQueueChild.h"
#include "mozilla/net/SocketProcessChild.h"
#include "nsInputStreamPump.h"
#include "nsHttpHandler.h"
#include "nsProxyInfo.h"
#include "nsProxyRelease.h"
#include "nsQueryObject.h"
#include "nsSerializationHelper.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(HttpTransactionChild, nsIRequestObserver, nsIStreamListener,
                  nsITransportEventSink, nsIThrottledInputChannel);

//-----------------------------------------------------------------------------
// HttpTransactionChild <public>
//-----------------------------------------------------------------------------

HttpTransactionChild::HttpTransactionChild()
    : mCanceled(false), mStatus(NS_OK), mChannelId(0) {
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
  MOZ_ASSERT(NS_FAILED(aStatus));

  mCanceled = true;
  mStatus = aStatus;
  if (mTransactionPump) {
    mTransactionPump->Cancel(mStatus);
  }

  return IPC_OK();
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
    const mozilla::Maybe<PInputChannelThrottleQueueChild*>& aThrottleQueue) {
  mRequestHead = aReqHeaders;
  if (aRequestBody) {
    mUploadStream = mozilla::ipc::DeserializeIPCStream(aRequestBody);
  }

  mTransaction = new nsHttpTransaction();
  mChannelId = aChannelId;

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

  if (!CanSend()) {
    return NS_ERROR_FAILURE;
  }

  // TODO: send string data in chunks and handle errors. Bug 1600129.
  nsCString data;
  nsresult rv = NS_ReadInputStreamToString(aInputStream, data, aCount);
  if (NS_FAILED(rv)) {
    return rv;
  }

  Unused << SendOnDataAvailable(data, aOffset, aCount);
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

  int32_t proxyConnectResponseCode =
      mTransaction->GetProxyConnectResponseCode();

  Unused << SendOnStartRequest(status, optionalHead, serializedSecurityInfoOut,
                               mTransaction->ProxyConnectFailed(),
                               ToTimingStructArgs(mTransaction->Timings()),
                               proxyConnectResponseCode, dataForSniffer);
  LOG(("HttpTransactionChild::OnStartRequest end [this=%p]\n", this));
  return NS_OK;
}

NS_IMETHODIMP
HttpTransactionChild::OnStopRequest(nsIRequest* aRequest, nsresult aStatus) {
  LOG(("HttpTransactionChild::OnStopRequest [this=%p]\n", this));

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

}  // namespace net
}  // namespace mozilla
