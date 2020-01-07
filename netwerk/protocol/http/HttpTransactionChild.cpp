/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "HttpTransactionChild.h"

#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/net/SocketProcessChild.h"
#include "nsInputStreamPump.h"
#include "nsHttpHandler.h"
#include "nsProxyInfo.h"
#include "nsProxyRelease.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(HttpTransactionChild, nsIRequestObserver, nsIStreamListener,
                  nsITransportEventSink);

//-----------------------------------------------------------------------------
// HttpTransactionChild <public>
//-----------------------------------------------------------------------------

HttpTransactionChild::HttpTransactionChild()
    : mCanceled(false), mStatus(NS_OK) {
  LOG(("Creating HttpTransactionChild @%p\n", this));
}

HttpTransactionChild::~HttpTransactionChild() {
  LOG(("Destroying HttpTransactionChild @%p\n", this));
}

nsProxyInfo* HttpTransactionChild::ProxyInfoCloneArgsToProxyInfo(
    const nsTArray<ProxyInfoCloneArgs>& aArgs) {
  nsProxyInfo *pi = nullptr, *first = nullptr, *last = nullptr;
  for (const ProxyInfoCloneArgs& info : aArgs) {
    pi = new nsProxyInfo(info.type(), info.host(), info.port(), info.username(),
                         info.password(), info.flags(), info.timeout(),
                         info.resolveFlags(), info.proxyAuthorizationHeader(),
                         info.connectionIsolationKey());
    if (last) {
      last->mNext = pi;
      // |mNext| will be released in |last|'s destructor.
      NS_IF_ADDREF(last->mNext);
    } else {
      first = pi;
    }
    last = pi;
  }

  return first;
}

// This function needs to be synced with nsHttpConnectionInfo::Clone.
already_AddRefed<nsHttpConnectionInfo>
HttpTransactionChild::DeserializeHttpConnectionInfoCloneArgs(
    const HttpConnectionInfoCloneArgs& aInfoArgs) {
  nsProxyInfo* pi = ProxyInfoCloneArgsToProxyInfo(aInfoArgs.proxyInfo());
  RefPtr<nsHttpConnectionInfo> cinfo;
  if (aInfoArgs.routedHost().IsEmpty()) {
    cinfo = new nsHttpConnectionInfo(
        aInfoArgs.host(), aInfoArgs.port(), aInfoArgs.npnToken(),
        aInfoArgs.username(), aInfoArgs.topWindowOrigin(), pi,
        aInfoArgs.originAttributes(), aInfoArgs.endToEndSSL(),
        aInfoArgs.isolated(), aInfoArgs.isHttp3());
  } else {
    MOZ_ASSERT(aInfoArgs.endToEndSSL());
    cinfo = new nsHttpConnectionInfo(
        aInfoArgs.host(), aInfoArgs.port(), aInfoArgs.npnToken(),
        aInfoArgs.username(), aInfoArgs.topWindowOrigin(), pi,
        aInfoArgs.originAttributes(), aInfoArgs.routedHost(),
        aInfoArgs.routedPort(), aInfoArgs.isolated(), aInfoArgs.isHttp3());
  }

  // Make sure the anonymous, insecure-scheme, and private flags are transferred
  cinfo->SetAnonymous(aInfoArgs.anonymous());
  cinfo->SetPrivate(aInfoArgs.aPrivate());
  cinfo->SetInsecureScheme(aInfoArgs.insecureScheme());
  cinfo->SetNoSpdy(aInfoArgs.noSpdy());
  cinfo->SetBeConservative(aInfoArgs.beConservative());
  cinfo->SetTlsFlags(aInfoArgs.tlsFlags());
  cinfo->SetIsTrrServiceChannel(aInfoArgs.isTrrServiceChannel());
  cinfo->SetTRRMode(static_cast<nsIRequest::TRRMode>(aInfoArgs.trrMode()));
  cinfo->SetIPv4Disabled(aInfoArgs.isIPv4Disabled());
  cinfo->SetIPv6Disabled(aInfoArgs.isIPv6Disabled());

  return cinfo.forget();
}

nsresult HttpTransactionChild::InitInternal(
    uint32_t caps, const HttpConnectionInfoCloneArgs& infoArgs,
    nsHttpRequestHead* requestHead, nsIInputStream* requestBody,
    uint64_t requestContentLength, bool requestBodyHasHeaders,
    uint64_t topLevelOuterContentWindowId, uint8_t httpTrafficCategory) {
  LOG(("HttpTransactionChild::InitInternal [this=%p caps=%x]\n", this, caps));

  RefPtr<nsHttpConnectionInfo> cinfo =
      DeserializeHttpConnectionInfoCloneArgs(infoArgs);

  nsresult rv = mTransaction->Init(
      caps, cinfo, requestHead, requestBody, requestContentLength,
      requestBodyHasHeaders, GetCurrentThreadEventTarget(),
      nullptr,  // TODO: security callback, fix in bug 1512479.
      this, topLevelOuterContentWindowId,
      static_cast<HttpTrafficCategory>(httpTrafficCategory));
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
    const uint8_t& aHttpTrafficCategory) {
  mRequestHead = aReqHeaders;
  if (aRequestBody) {
    mUploadStream = mozilla::ipc::DeserializeIPCStream(aRequestBody);
  }

  mTransaction = new nsHttpTransaction();
  nsresult rv =
      InitInternal(aCaps, aArgs, &mRequestHead, mUploadStream,
                   aReqContentLength, aReqBodyIncludesHeaders,
                   aTopLevelOuterContentWindowId, aHttpTrafficCategory);
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

  nsAutoPtr<nsHttpResponseHead> head(mTransaction->TakeResponseHead());
  Maybe<nsHttpResponseHead> optionalHead;
  if (head) {
    optionalHead = Some(*head);
  }

  Unused << SendOnStartRequest(status, optionalHead, serializedSecurityInfoOut,
                               mTransaction->ProxyConnectFailed(),
                               ToTimingStructArgs(mTransaction->Timings()));
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

  nsAutoPtr<nsHttpHeaderArray> headerArray(
      mTransaction->TakeResponseTrailers());
  Maybe<nsHttpHeaderArray> responseTrailers;
  if (headerArray) {
    responseTrailers.emplace(*headerArray);
  }

  Unused << SendOnStopRequest(aStatus, mTransaction->ResponseIsComplete(),
                              mTransaction->GetTransferSize(),
                              ToTimingStructArgs(mTransaction->Timings()),
                              responseTrailers,
                              mTransaction->HasStickyConnection());

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
      }
    }
    Unused << SendOnNetAddrUpdate(selfAddr, peerAddr);
  }

  Unused << SendOnTransportStatus(aStatus, aProgress, aProgressMax);
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
