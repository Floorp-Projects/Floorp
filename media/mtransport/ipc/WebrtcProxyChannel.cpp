/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcProxyChannel.h"

#include "nsHttpChannel.h"
#include "nsIChannel.h"
#include "nsIClassOfService.h"
#include "nsIContentPolicy.h"
#include "nsIEventTarget.h"
#include "nsIIOService.h"
#include "nsILoadInfo.h"
#include "nsIProtocolProxyService.h"
#include "nsIURIMutator.h"
#include "nsProxyRelease.h"
#include "nsString.h"
#include "mozilla/dom/ContentProcessManager.h"
#include "mozilla/dom/BrowserParent.h"
#include "nsISocketTransportService.h"

#include "WebrtcProxyChannelCallback.h"
#include "WebrtcProxyLog.h"

namespace mozilla {
namespace net {

class WebrtcProxyData {
 public:
  explicit WebrtcProxyData(nsTArray<uint8_t>&& aData) : mData(aData) {
    MOZ_COUNT_CTOR(WebrtcProxyData);
  }

  ~WebrtcProxyData() { MOZ_COUNT_DTOR(WebrtcProxyData); }

  const nsTArray<uint8_t>& GetData() const { return mData; }

 private:
  nsTArray<uint8_t> mData;
};

NS_IMPL_ISUPPORTS(WebrtcProxyChannel, nsIAuthPromptProvider,
                  nsIHttpUpgradeListener, nsIInputStreamCallback,
                  nsIInterfaceRequestor, nsIOutputStreamCallback,
                  nsIRequestObserver, nsIStreamListener,
                  nsIProtocolProxyCallback)

WebrtcProxyChannel::WebrtcProxyChannel(WebrtcProxyChannelCallback* aCallbacks)
    : mProxyCallbacks(aCallbacks),
      mClosed(false),
      mOpened(false),
      mWriteOffset(0),
      mAuthProvider(nullptr),
      mTransport(nullptr),
      mSocketIn(nullptr),
      mSocketOut(nullptr) {
  LOG(("WebrtcProxyChannel::WebrtcProxyChannel %p\n", this));
  mMainThread = GetMainThreadEventTarget();
  mSocketThread = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID);
  MOZ_RELEASE_ASSERT(mMainThread, "no main thread");
  MOZ_RELEASE_ASSERT(mSocketThread, "no socket thread");
}

WebrtcProxyChannel::~WebrtcProxyChannel() {
  LOG(("WebrtcProxyChannel::~WebrtcProxyChannel %p\n", this));

  NS_ProxyRelease("WebrtcProxyChannel::CleanUpAuthProvider", mMainThread,
                  mAuthProvider.forget());
}

void WebrtcProxyChannel::SetTabId(dom::TabId aTabId) {
  dom::ContentProcessManager* cpm = dom::ContentProcessManager::GetSingleton();
  dom::ContentParentId cpId = cpm->GetTabProcessId(aTabId);
  mAuthProvider = cpm->GetBrowserParentByProcessAndTabId(cpId, aTabId);
}

nsresult WebrtcProxyChannel::Write(nsTArray<uint8_t>&& aWriteData) {
  LOG(("WebrtcProxyChannel::Write %p\n", this));
  MOZ_ALWAYS_SUCCEEDS(
      mSocketThread->Dispatch(NewRunnableMethod<nsTArray<uint8_t>&&>(
          "WebrtcProxyChannel::Write", this,
          &WebrtcProxyChannel::EnqueueWrite_s, std::move(aWriteData))));

  return NS_OK;
}

nsresult WebrtcProxyChannel::Close() {
  LOG(("WebrtcProxyChannel::Close %p\n", this));

  CloseWithReason(NS_OK);

  return NS_OK;
}

void WebrtcProxyChannel::CloseWithReason(nsresult aReason) {
  LOG(("WebrtcProxyChannel::CloseWithReason %p reason=%u\n", this,
       static_cast<uint32_t>(aReason)));

  if (!OnSocketThread()) {
    MOZ_ASSERT(NS_IsMainThread(), "not on main thread");

    // Let's pretend we got an open even if we didn't to prevent an Open later.
    mOpened = true;

    MOZ_ALWAYS_SUCCEEDS(mSocketThread->Dispatch(NewRunnableMethod<nsresult>(
        "WebrtcProxyChannel::CloseWithReason", this,
        &WebrtcProxyChannel::CloseWithReason, aReason)));
    return;
  }

  if (mClosed) {
    return;
  }

  mClosed = true;

  if (mSocketIn) {
    mSocketIn->AsyncWait(nullptr, 0, 0, nullptr);
    mSocketIn = nullptr;
  }

  if (mSocketOut) {
    mSocketOut->AsyncWait(nullptr, 0, 0, nullptr);
    mSocketOut = nullptr;
  }

  if (mTransport) {
    mTransport->Close(NS_BASE_STREAM_CLOSED);
    mTransport = nullptr;
  }

  NS_ProxyRelease("WebrtcProxyChannel::CleanUpAuthProvider", mMainThread,
                  mAuthProvider.forget());
  InvokeOnClose(aReason);
}

nsresult WebrtcProxyChannel::Open(const nsCString& aHost, const int& aPort,
                                  const net::LoadInfoArgs& aArgs,
                                  const nsCString& aAlpn) {
  LOG(("WebrtcProxyChannel::Open %p\n", this));

  if (NS_WARN_IF(mOpened)) {
    LOG(("WebrtcProxyChannel %p: proxy channel already open\n", this));
    CloseWithReason(NS_ERROR_FAILURE);
    return NS_ERROR_FAILURE;
  }

  mOpened = true;
  nsCString spec = NS_LITERAL_CSTRING("http://") + aHost;

  nsresult rv = NS_MutateURI(NS_STANDARDURLMUTATOR_CONTRACTID)
                    .SetSpec(spec)
                    .SetPort(aPort)
                    .Finalize(mURI);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    CloseWithReason(NS_ERROR_FAILURE);
    return NS_ERROR_FAILURE;
  }

  mLoadInfoArgs = aArgs;
  mAlpn = aAlpn;

  // We need to figure out whether a proxy needs to be used for mURI before we
  // can start on establishing a connection.
  rv = DoProxyConfigLookup();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    CloseWithReason(NS_ERROR_FAILURE);
  }

  return rv;
}

nsresult WebrtcProxyChannel::DoProxyConfigLookup() {
  nsresult rv;
  nsCOMPtr<nsIProtocolProxyService> pps =
      do_GetService(NS_PROTOCOLPROXYSERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), mURI,
                     nsContentUtils::GetSystemPrincipal(),
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                     nsIContentPolicy::TYPE_OTHER);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIEventTarget> target =
      SystemGroup::EventTargetFor(TaskCategory::Network);
  nsCOMPtr<nsICancelable> proxyRequest;

  rv = pps->AsyncResolve(channel,
                         nsIProtocolProxyService::RESOLVE_PREFER_HTTPS_PROXY |
                             nsIProtocolProxyService::RESOLVE_ALWAYS_TUNNEL,
                         this, target, getter_AddRefs(proxyRequest));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // We pick back up in OnProxyAvailable

  return NS_OK;
}

NS_IMETHODIMP WebrtcProxyChannel::OnProxyAvailable(nsICancelable* aRequest,
                                                   nsIChannel* aChannel,
                                                   nsIProxyInfo* aProxyinfo,
                                                   nsresult aResult) {
  nsresult rv;

  if (NS_SUCCEEDED(aResult) && aProxyinfo) {
    nsCString proxyType;
    rv = aProxyinfo->GetType(proxyType);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      CloseWithReason(rv);
      return rv;
    }

    if (proxyType == "http" || proxyType == "https") {
      rv = OpenWithHttpProxy();
    } else if (proxyType == "socks" || proxyType == "socks4" ||
               proxyType == "direct") {
      rv = OpenWithoutHttpProxy(aProxyinfo);
    }
  } else {
    rv = OpenWithoutHttpProxy(nullptr);
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    CloseWithReason(rv);
  }

  return rv;
}

nsresult WebrtcProxyChannel::OpenWithoutHttpProxy(
    nsIProxyInfo* aSocksProxyInfo) {
  nsCString host;
  int32_t port;

  nsresult rv = mURI->GetHost(host);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mURI->GetPort(&port);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AutoTArray<nsCString, 1> socketTypes;
  if (mSsl) {
    socketTypes.AppendElement(NS_LITERAL_CSTRING("ssl"));
  }

  nsCOMPtr<nsISocketTransportService> sts =
      do_GetService("@mozilla.org/network/socket-transport-service;1");
  rv = sts->CreateTransport(socketTypes, host, port, aSocksProxyInfo,
                            getter_AddRefs(mTransport));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIInputStream> socketIn;
  rv = mTransport->OpenInputStream(0, 0, 0, getter_AddRefs(socketIn));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  mSocketIn = do_QueryInterface(socketIn);
  if (NS_WARN_IF(!mSocketIn)) {
    return NS_ERROR_NULL_POINTER;
  }

  nsCOMPtr<nsIOutputStream> socketOut;
  rv = mTransport->OpenOutputStream(nsITransport::OPEN_UNBUFFERED, 0, 0,
                                    getter_AddRefs(socketOut));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  mSocketOut = do_QueryInterface(socketOut);
  if (NS_WARN_IF(!mSocketOut)) {
    return NS_ERROR_NULL_POINTER;
  }

  return FinishOpen();
}

nsresult WebrtcProxyChannel::OpenWithHttpProxy() {
  nsresult rv;
  nsCOMPtr<nsIIOService> ioService;
  ioService = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    LOG(("WebrtcProxyChannel %p: io service missing\n", this));
    return rv;
  }

  nsCOMPtr<nsILoadInfo> loadInfo;
  Maybe<net::LoadInfoArgs> loadInfoArgs = Some(mLoadInfoArgs);
  rv = LoadInfoArgsToLoadInfo(loadInfoArgs, getter_AddRefs(loadInfo));
  if (NS_FAILED(rv)) {
    LOG(("WebrtcProxyChannel %p: could not init load info\n", this));
    return rv;
  }

  // -need to always tunnel since we're using a proxy
  // -there shouldn't be an opportunity to send cookies, but explicitly disallow
  // them anyway.
  // -the previous proxy tunnel didn't support redirects e.g. 307. don't need to
  // introduce new behavior. can't follow redirects on connect anyway.
  nsCOMPtr<nsIChannel> localChannel;
  rv = ioService->NewChannelFromURIWithProxyFlags(
      mURI, nullptr,
      // Proxy flags are overridden by SetConnectOnly()
      0, loadInfo->LoadingNode(), loadInfo->LoadingPrincipal(),
      loadInfo->TriggeringPrincipal(),
      nsILoadInfo::SEC_DONT_FOLLOW_REDIRECTS | nsILoadInfo::SEC_COOKIES_OMIT |
          // We need this flag to allow loads from any origin since this channel
          // is being used to CONNECT to an HTTP proxy.
          nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
      nsIContentPolicy::TYPE_OTHER, getter_AddRefs(localChannel));
  if (NS_FAILED(rv)) {
    LOG(("WebrtcProxyChannel %p: bad open channel\n", this));
    return rv;
  }

  RefPtr<nsHttpChannel> httpChannel;
  CallQueryInterface(localChannel, httpChannel.StartAssignment());

  if (!httpChannel) {
    LOG(("WebrtcProxyChannel %p: not an http channel\n", this));
    return NS_ERROR_FAILURE;
  }

  httpChannel->SetNotificationCallbacks(this);

  // don't block webrtc proxy setup with other requests
  // often more than one of these channels will be created all at once by ICE
  nsCOMPtr<nsIClassOfService> cos = do_QueryInterface(localChannel);
  if (cos) {
    cos->AddClassFlags(nsIClassOfService::Unblocked |
                       nsIClassOfService::DontThrottle);
  } else {
    LOG(("WebrtcProxyChannel %p: could not set class of service\n", this));
    return NS_ERROR_FAILURE;
  }

  rv = httpChannel->HTTPUpgrade(mAlpn, this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  rv = httpChannel->SetConnectOnly();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = NS_MaybeOpenChannelUsingAsyncOpen(httpChannel, this);

  if (NS_FAILED(rv)) {
    LOG(("WebrtcProxyChannel %p: cannot async open\n", this));
    return rv;
  }

  // This picks back up in OnTransportAvailable once we have connected to the
  // proxy, and performed the http upgrade to switch the proxy into passthrough
  // mode.

  return NS_OK;
}

void WebrtcProxyChannel::EnqueueWrite_s(nsTArray<uint8_t>&& aWriteData) {
  LOG(("WebrtcProxyChannel::EnqueueWrite %p\n", this));
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(!mClosed, "webrtc proxy channel closed");

  mWriteQueue.emplace_back(std::move(aWriteData));

  if (mSocketOut) {
    mSocketOut->AsyncWait(this, 0, 0, nullptr);
  }
}

void WebrtcProxyChannel::InvokeOnClose(nsresult aReason) {
  LOG(("WebrtcProxyChannel::InvokeOnClose %p\n", this));

  if (!NS_IsMainThread()) {
    MOZ_ALWAYS_SUCCEEDS(mMainThread->Dispatch(NewRunnableMethod<nsresult>(
        "WebrtcProxyChannel::InvokeOnClose", this,
        &WebrtcProxyChannel::InvokeOnClose, aReason)));
    return;
  }

  MOZ_ASSERT(mProxyCallbacks, "webrtc proxy callback should be non-null");

  mProxyCallbacks->OnClose(aReason);
  mProxyCallbacks = nullptr;
}

void WebrtcProxyChannel::InvokeOnConnected() {
  LOG(("WebrtcProxyChannel::InvokeOnConnected %p\n", this));

  if (!NS_IsMainThread()) {
    MOZ_ALWAYS_SUCCEEDS(mMainThread->Dispatch(
        NewRunnableMethod("WebrtcProxyChannel::InvokeOnConnected", this,
                          &WebrtcProxyChannel::InvokeOnConnected)));
    return;
  }

  MOZ_ASSERT(mProxyCallbacks, "webrtc proxy callback should be non-null");

  mProxyCallbacks->OnConnected();
}

void WebrtcProxyChannel::InvokeOnRead(nsTArray<uint8_t>&& aReadData) {
  LOG(("WebrtcProxyChannel::InvokeOnRead %p count=%zu\n", this,
       aReadData.Length()));

  if (!NS_IsMainThread()) {
    MOZ_ALWAYS_SUCCEEDS(
        mMainThread->Dispatch(NewRunnableMethod<nsTArray<uint8_t>&&>(
            "WebrtcProxyChannel::InvokeOnRead", this,
            &WebrtcProxyChannel::InvokeOnRead, std::move(aReadData))));
    return;
  }

  MOZ_ASSERT(mProxyCallbacks, "webrtc proxy callback should be non-null");

  mProxyCallbacks->OnRead(std::move(aReadData));
}

// nsIHttpUpgradeListener
NS_IMETHODIMP
WebrtcProxyChannel::OnTransportAvailable(nsISocketTransport* aTransport,
                                         nsIAsyncInputStream* aSocketIn,
                                         nsIAsyncOutputStream* aSocketOut) {
  // This is called only in the http proxy case, once we have connected to the
  // http proxy and performed the http upgrade to switch it over to passthrough
  // mode. That process is started async by OpenWithHttpProxy.
  LOG(("WebrtcProxyChannel::OnTransportAvailable %p\n", this));
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(!mTransport, "already called transport available on webrtc proxy");

  // Cancel any pending callbacks. The caller doesn't always cancel these
  // awaits. We need to make sure they don't get them.
  aSocketIn->AsyncWait(nullptr, 0, 0, nullptr);
  aSocketOut->AsyncWait(nullptr, 0, 0, nullptr);

  if (mClosed) {
    LOG(("WebrtcProxyChannel::OnTransportAvailable %p closed\n", this));
    return NS_OK;
  }

  mTransport = aTransport;
  mSocketIn = aSocketIn;
  mSocketOut = aSocketOut;

  // pulled from nr_socket_prsock.cpp
  uint32_t minBufferSize = 256 * 1024;
  nsresult rv = mTransport->SetSendBufferSize(minBufferSize);
  if (NS_FAILED(rv)) {
    LOG(("WebrtcProxyChannel::OnTransportAvailable %p send failed\n", this));
    CloseWithReason(rv);
    return rv;
  }
  rv = mTransport->SetRecvBufferSize(minBufferSize);
  if (NS_FAILED(rv)) {
    LOG(("WebrtcProxyChannel::OnTransportAvailable %p recv failed\n", this));
    CloseWithReason(rv);
    return rv;
  }

  return FinishOpen();
}

nsresult WebrtcProxyChannel::FinishOpen() {
  // mTransport, mSocketIn, and mSocketOut are all set. We may have set them in
  // OnTransportAvailable (in the http/https proxy case), or in
  // OpenWithoutHttpProxy. From here on out, this class functions the same for
  // these two cases.

  mSocketIn->AsyncWait(this, 0, 0, nullptr);

  InvokeOnConnected();

  return NS_OK;
}

NS_IMETHODIMP
WebrtcProxyChannel::OnUpgradeFailed(nsresult aErrorCode) {
  LOG(("WebrtcProxyChannel::OnUpgradeFailed %p\n", this));
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(!mTransport, "already called transport available on webrtc proxy");

  if (mClosed) {
    LOG(("WebrtcProxyChannel::OnUpgradeFailed %p closed\n", this));
    return NS_OK;
  }

  CloseWithReason(aErrorCode);

  return NS_OK;
}

// nsIRequestObserver (from nsIStreamListener)
NS_IMETHODIMP
WebrtcProxyChannel::OnStartRequest(nsIRequest* aRequest) {
  LOG(("WebrtcProxyChannel::OnStartRequest %p\n", this));

  return NS_OK;
}

NS_IMETHODIMP
WebrtcProxyChannel::OnStopRequest(nsIRequest* aRequest, nsresult aStatusCode) {
  LOG(("WebrtcProxyChannel::OnStopRequest %p status=%u\n", this,
       static_cast<uint32_t>(aStatusCode)));

  // see nsHttpChannel::ProcessFailedProxyConnect for most error codes
  if (NS_FAILED(aStatusCode)) {
    CloseWithReason(aStatusCode);
    return aStatusCode;
  }

  return NS_OK;
}

// nsIStreamListener
NS_IMETHODIMP
WebrtcProxyChannel::OnDataAvailable(nsIRequest* aRequest,
                                    nsIInputStream* aInputStream,
                                    uint64_t aOffset, uint32_t aCount) {
  LOG(("WebrtcProxyChannel::OnDataAvailable %p count=%u\n", this, aCount));
  MOZ_ASSERT(0, "unreachable data available");
  return NS_OK;
}

// nsIInputStreamCallback
NS_IMETHODIMP
WebrtcProxyChannel::OnInputStreamReady(nsIAsyncInputStream* in) {
  LOG(("WebrtcProxyChannel::OnInputStreamReady %p unwritten=%zu\n", this,
       CountUnwrittenBytes()));
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(!mClosed, "webrtc proxy channel closed");
  MOZ_ASSERT(mTransport, "webrtc proxy channel not connected");
  MOZ_ASSERT(mSocketIn == in, "wrong input stream");

  char buffer[9216];
  uint32_t remainingCapacity = sizeof(buffer);
  uint32_t read = 0;

  while (remainingCapacity > 0) {
    uint32_t count = 0;
    nsresult rv = mSocketIn->Read(buffer + read, remainingCapacity, &count);
    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
      break;
    }

    if (NS_FAILED(rv)) {
      LOG(("WebrtcProxyChannel::OnInputStreamReady %p failed %u\n", this,
           static_cast<uint32_t>(rv)));
      CloseWithReason(rv);
      return rv;
    }

    // base stream closed
    if (count == 0) {
      LOG(("WebrtcProxyChannel::OnInputStreamReady %p connection closed\n",
           this));
      CloseWithReason(NS_ERROR_FAILURE);
      return NS_OK;
    }

    remainingCapacity -= count;
    read += count;
  }

  if (read > 0) {
    nsTArray<uint8_t> array(read);
    array.AppendElements(buffer, read);

    InvokeOnRead(std::move(array));
  }

  mSocketIn->AsyncWait(this, 0, 0, nullptr);

  return NS_OK;
}

// nsIOutputStreamCallback
NS_IMETHODIMP
WebrtcProxyChannel::OnOutputStreamReady(nsIAsyncOutputStream* out) {
  LOG(("WebrtcProxyChannel::OnOutputStreamReady %p unwritten=%zu\n", this,
       CountUnwrittenBytes()));
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(!mClosed, "webrtc proxy channel closed");
  MOZ_ASSERT(mTransport, "webrtc proxy channel not connected");
  MOZ_ASSERT(mSocketOut == out, "wrong output stream");

  while (!mWriteQueue.empty()) {
    const WebrtcProxyData& data = mWriteQueue.front();

    char* buffer = reinterpret_cast<char*>(
                       const_cast<uint8_t*>(data.GetData().Elements())) +
                   mWriteOffset;
    uint32_t toWrite = data.GetData().Length() - mWriteOffset;

    uint32_t wrote = 0;
    nsresult rv = mSocketOut->Write(buffer, toWrite, &wrote);
    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
      mSocketOut->AsyncWait(this, 0, 0, nullptr);
      return NS_OK;
    }

    if (NS_FAILED(rv)) {
      LOG(("WebrtcProxyChannel::OnOutputStreamReady %p failed %u\n", this,
           static_cast<uint32_t>(rv)));
      CloseWithReason(rv);
      return NS_OK;
    }

    mWriteOffset += wrote;

    if (toWrite == wrote) {
      mWriteOffset = 0;
      mWriteQueue.pop_front();
    }
  }

  return NS_OK;
}

// nsIInterfaceRequestor
NS_IMETHODIMP
WebrtcProxyChannel::GetInterface(const nsIID& iid, void** result) {
  LOG(("WebrtcProxyChannel::GetInterface %p\n", this));

  return QueryInterface(iid, result);
}

size_t WebrtcProxyChannel::CountUnwrittenBytes() const {
  size_t count = 0;

  for (const WebrtcProxyData& data : mWriteQueue) {
    count += data.GetData().Length();
  }

  MOZ_ASSERT(count >= mWriteOffset, "offset exceeds write buffer length");

  count -= mWriteOffset;

  return count;
}

}  // namespace net
}  // namespace mozilla
