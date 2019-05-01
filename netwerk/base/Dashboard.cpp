/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http:mozilla.org/MPL/2.0/. */

#include "mozilla/dom/NetDashboardBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/net/Dashboard.h"
#include "mozilla/net/HttpInfo.h"
#include "nsHttp.h"
#include "nsICancelable.h"
#include "nsIDNSService.h"
#include "nsIDNSRecord.h"
#include "nsIInputStream.h"
#include "nsINamed.h"
#include "nsISocketTransport.h"
#include "nsIThread.h"
#include "nsProxyRelease.h"
#include "nsSocketTransportService2.h"
#include "nsThreadUtils.h"
#include "nsURLHelper.h"
#include "mozilla/Logging.h"
#include "nsIOService.h"
#include "../cache2/CacheFileUtils.h"

using mozilla::AutoSafeJSContext;
using mozilla::dom::Sequence;
using mozilla::dom::ToJSValue;

namespace mozilla {
namespace net {

class SocketData : public nsISupports {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  SocketData() {
    mTotalSent = 0;
    mTotalRecv = 0;
    mEventTarget = nullptr;
  }

  uint64_t mTotalSent;
  uint64_t mTotalRecv;
  nsTArray<SocketInfo> mData;
  nsMainThreadPtrHandle<nsINetDashboardCallback> mCallback;
  nsIEventTarget* mEventTarget;

 private:
  virtual ~SocketData() = default;
};

static void GetErrorString(nsresult rv, nsAString& errorString);

NS_IMPL_ISUPPORTS0(SocketData)

class HttpData : public nsISupports {
  virtual ~HttpData() = default;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  HttpData() { mEventTarget = nullptr; }

  nsTArray<HttpRetParams> mData;
  nsMainThreadPtrHandle<nsINetDashboardCallback> mCallback;
  nsIEventTarget* mEventTarget;
};

NS_IMPL_ISUPPORTS0(HttpData)

class WebSocketRequest : public nsISupports {
  virtual ~WebSocketRequest() = default;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  WebSocketRequest() { mEventTarget = nullptr; }

  nsMainThreadPtrHandle<nsINetDashboardCallback> mCallback;
  nsIEventTarget* mEventTarget;
};

NS_IMPL_ISUPPORTS0(WebSocketRequest)

class DnsData : public nsISupports {
  virtual ~DnsData() = default;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  DnsData() { mEventTarget = nullptr; }

  nsTArray<DNSCacheEntries> mData;
  nsMainThreadPtrHandle<nsINetDashboardCallback> mCallback;
  nsIEventTarget* mEventTarget;
};

NS_IMPL_ISUPPORTS0(DnsData)

class ConnectionData : public nsITransportEventSink,
                       public nsITimerCallback,
                       public nsINamed {
  virtual ~ConnectionData() {
    if (mTimer) {
      mTimer->Cancel();
    }
  }

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITRANSPORTEVENTSINK
  NS_DECL_NSITIMERCALLBACK

  NS_IMETHOD GetName(nsACString& aName) override {
    aName.AssignLiteral("net::ConnectionData");
    return NS_OK;
  }

  void StartTimer(uint32_t aTimeout);
  void StopTimer();

  explicit ConnectionData(Dashboard* target)
      : mPort(0), mProtocol(nullptr), mTimeout(0) {
    mEventTarget = nullptr;
    mDashboard = target;
  }

  nsCOMPtr<nsISocketTransport> mSocket;
  nsCOMPtr<nsIInputStream> mStreamIn;
  nsCOMPtr<nsITimer> mTimer;
  nsMainThreadPtrHandle<nsINetDashboardCallback> mCallback;
  nsIEventTarget* mEventTarget;
  Dashboard* mDashboard;

  nsCString mHost;
  uint32_t mPort;
  const char* mProtocol;
  uint32_t mTimeout;

  nsString mStatus;
};

NS_IMPL_ISUPPORTS(ConnectionData, nsITransportEventSink, nsITimerCallback,
                  nsINamed)

class RcwnData : public nsISupports {
  virtual ~RcwnData() = default;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  RcwnData() { mEventTarget = nullptr; }

  nsMainThreadPtrHandle<nsINetDashboardCallback> mCallback;
  nsIEventTarget* mEventTarget;
};

NS_IMPL_ISUPPORTS0(RcwnData)

NS_IMETHODIMP
ConnectionData::OnTransportStatus(nsITransport* aTransport, nsresult aStatus,
                                  int64_t aProgress, int64_t aProgressMax) {
  if (aStatus == NS_NET_STATUS_CONNECTED_TO) {
    StopTimer();
  }

  GetErrorString(aStatus, mStatus);
  mEventTarget->Dispatch(NewRunnableMethod<RefPtr<ConnectionData>>(
                             "net::Dashboard::GetConnectionStatus", mDashboard,
                             &Dashboard::GetConnectionStatus, this),
                         NS_DISPATCH_NORMAL);

  return NS_OK;
}

NS_IMETHODIMP
ConnectionData::Notify(nsITimer* aTimer) {
  MOZ_ASSERT(aTimer == mTimer);

  if (mSocket) {
    mSocket->Close(NS_ERROR_ABORT);
    mSocket = nullptr;
    mStreamIn = nullptr;
  }

  mTimer = nullptr;

  mStatus.AssignLiteral(u"NS_ERROR_NET_TIMEOUT");
  mEventTarget->Dispatch(NewRunnableMethod<RefPtr<ConnectionData>>(
                             "net::Dashboard::GetConnectionStatus", mDashboard,
                             &Dashboard::GetConnectionStatus, this),
                         NS_DISPATCH_NORMAL);

  return NS_OK;
}

void ConnectionData::StartTimer(uint32_t aTimeout) {
  if (!mTimer) {
    mTimer = NS_NewTimer();
  }

  mTimer->InitWithCallback(this, aTimeout * 1000, nsITimer::TYPE_ONE_SHOT);
}

void ConnectionData::StopTimer() {
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }
}

class LookupHelper;

class LookupArgument : public nsISupports {
  virtual ~LookupArgument() = default;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  LookupArgument(nsIDNSRecord* aRecord, LookupHelper* aHelper) {
    mRecord = aRecord;
    mHelper = aHelper;
  }

  nsCOMPtr<nsIDNSRecord> mRecord;
  RefPtr<LookupHelper> mHelper;
};

NS_IMPL_ISUPPORTS0(LookupArgument)

class LookupHelper final : public nsIDNSListener {
  virtual ~LookupHelper() {
    if (mCancel) {
      mCancel->Cancel(NS_ERROR_ABORT);
    }
  }

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDNSLISTENER

  LookupHelper() : mEventTarget{nullptr}, mStatus{NS_ERROR_NOT_INITIALIZED} {}

  nsresult ConstructAnswer(LookupArgument* aArgument);

 public:
  nsCOMPtr<nsICancelable> mCancel;
  nsMainThreadPtrHandle<nsINetDashboardCallback> mCallback;
  nsIEventTarget* mEventTarget;
  nsresult mStatus;
};

NS_IMPL_ISUPPORTS(LookupHelper, nsIDNSListener)

NS_IMETHODIMP
LookupHelper::OnLookupComplete(nsICancelable* aRequest, nsIDNSRecord* aRecord,
                               nsresult aStatus) {
  MOZ_ASSERT(aRequest == mCancel);
  mCancel = nullptr;
  mStatus = aStatus;

  RefPtr<LookupArgument> arg = new LookupArgument(aRecord, this);
  mEventTarget->Dispatch(NewRunnableMethod<RefPtr<LookupArgument>>(
                             "net::LookupHelper::ConstructAnswer", this,
                             &LookupHelper::ConstructAnswer, arg),
                         NS_DISPATCH_NORMAL);

  return NS_OK;
}

NS_IMETHODIMP
LookupHelper::OnLookupByTypeComplete(nsICancelable* aRequest,
                                     nsIDNSByTypeRecord* aRes,
                                     nsresult aStatus) {
  return NS_OK;
}

nsresult LookupHelper::ConstructAnswer(LookupArgument* aArgument) {
  nsIDNSRecord* aRecord = aArgument->mRecord;
  AutoSafeJSContext cx;

  mozilla::dom::DNSLookupDict dict;
  dict.mAddress.Construct();

  Sequence<nsString>& addresses = dict.mAddress.Value();

  if (NS_SUCCEEDED(mStatus)) {
    dict.mAnswer = true;
    bool hasMore;
    aRecord->HasMore(&hasMore);
    while (hasMore) {
      nsString* nextAddress = addresses.AppendElement(fallible);
      if (!nextAddress) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      nsCString nextAddressASCII;
      aRecord->GetNextAddrAsString(nextAddressASCII);
      CopyASCIItoUTF16(nextAddressASCII, *nextAddress);
      aRecord->HasMore(&hasMore);
    }
  } else {
    dict.mAnswer = false;
    GetErrorString(mStatus, dict.mError);
  }

  JS::RootedValue val(cx);
  if (!ToJSValue(cx, dict, &val)) {
    return NS_ERROR_FAILURE;
  }

  this->mCallback->OnDashboardDataAvailable(val);

  return NS_OK;
}

NS_IMPL_ISUPPORTS(Dashboard, nsIDashboard, nsIDashboardEventNotifier)

Dashboard::Dashboard() { mEnableLogging = false; }

NS_IMETHODIMP
Dashboard::RequestSockets(nsINetDashboardCallback* aCallback) {
  RefPtr<SocketData> socketData = new SocketData();
  socketData->mCallback = new nsMainThreadPtrHolder<nsINetDashboardCallback>(
      "nsINetDashboardCallback", aCallback, true);
  socketData->mEventTarget = GetCurrentThreadEventTarget();
  gSocketTransportService->Dispatch(
      NewRunnableMethod<RefPtr<SocketData>>(
          "net::Dashboard::GetSocketsDispatch", this,
          &Dashboard::GetSocketsDispatch, socketData),
      NS_DISPATCH_NORMAL);
  return NS_OK;
}

nsresult Dashboard::GetSocketsDispatch(SocketData* aSocketData) {
  RefPtr<SocketData> socketData = aSocketData;
  if (gSocketTransportService) {
    gSocketTransportService->GetSocketConnections(&socketData->mData);
    socketData->mTotalSent = gSocketTransportService->GetSentBytes();
    socketData->mTotalRecv = gSocketTransportService->GetReceivedBytes();
  }
  socketData->mEventTarget->Dispatch(
      NewRunnableMethod<RefPtr<SocketData>>("net::Dashboard::GetSockets", this,
                                            &Dashboard::GetSockets, socketData),
      NS_DISPATCH_NORMAL);
  return NS_OK;
}

nsresult Dashboard::GetSockets(SocketData* aSocketData) {
  RefPtr<SocketData> socketData = aSocketData;
  AutoSafeJSContext cx;

  mozilla::dom::SocketsDict dict;
  dict.mSockets.Construct();
  dict.mSent = 0;
  dict.mReceived = 0;

  Sequence<mozilla::dom::SocketElement>& sockets = dict.mSockets.Value();

  uint32_t length = socketData->mData.Length();
  if (!sockets.SetCapacity(length, fallible)) {
    JS_ReportOutOfMemory(cx);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (uint32_t i = 0; i < socketData->mData.Length(); i++) {
    dom::SocketElement& mSocket = *sockets.AppendElement(fallible);
    CopyASCIItoUTF16(socketData->mData[i].host, mSocket.mHost);
    mSocket.mPort = socketData->mData[i].port;
    mSocket.mActive = socketData->mData[i].active;
    mSocket.mTcp = socketData->mData[i].tcp;
    mSocket.mSent = (double)socketData->mData[i].sent;
    mSocket.mReceived = (double)socketData->mData[i].received;
    dict.mSent += socketData->mData[i].sent;
    dict.mReceived += socketData->mData[i].received;
  }

  dict.mSent += socketData->mTotalSent;
  dict.mReceived += socketData->mTotalRecv;
  JS::RootedValue val(cx);
  if (!ToJSValue(cx, dict, &val)) return NS_ERROR_FAILURE;
  socketData->mCallback->OnDashboardDataAvailable(val);

  return NS_OK;
}

NS_IMETHODIMP
Dashboard::RequestHttpConnections(nsINetDashboardCallback* aCallback) {
  RefPtr<HttpData> httpData = new HttpData();
  httpData->mCallback = new nsMainThreadPtrHolder<nsINetDashboardCallback>(
      "nsINetDashboardCallback", aCallback, true);
  httpData->mEventTarget = GetCurrentThreadEventTarget();

  gSocketTransportService->Dispatch(NewRunnableMethod<RefPtr<HttpData>>(
                                        "net::Dashboard::GetHttpDispatch", this,
                                        &Dashboard::GetHttpDispatch, httpData),
                                    NS_DISPATCH_NORMAL);
  return NS_OK;
}

nsresult Dashboard::GetHttpDispatch(HttpData* aHttpData) {
  RefPtr<HttpData> httpData = aHttpData;
  HttpInfo::GetHttpConnectionData(&httpData->mData);
  httpData->mEventTarget->Dispatch(
      NewRunnableMethod<RefPtr<HttpData>>("net::Dashboard::GetHttpConnections",
                                          this, &Dashboard::GetHttpConnections,
                                          httpData),
      NS_DISPATCH_NORMAL);
  return NS_OK;
}

nsresult Dashboard::GetHttpConnections(HttpData* aHttpData) {
  RefPtr<HttpData> httpData = aHttpData;
  AutoSafeJSContext cx;

  mozilla::dom::HttpConnDict dict;
  dict.mConnections.Construct();

  using mozilla::dom::HalfOpenInfoDict;
  using mozilla::dom::HttpConnectionElement;
  using mozilla::dom::HttpConnInfo;
  Sequence<HttpConnectionElement>& connections = dict.mConnections.Value();

  uint32_t length = httpData->mData.Length();
  if (!connections.SetCapacity(length, fallible)) {
    JS_ReportOutOfMemory(cx);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (uint32_t i = 0; i < httpData->mData.Length(); i++) {
    HttpConnectionElement& connection = *connections.AppendElement(fallible);

    CopyASCIItoUTF16(httpData->mData[i].host, connection.mHost);
    connection.mPort = httpData->mData[i].port;
    connection.mSpdy = httpData->mData[i].spdy;
    connection.mSsl = httpData->mData[i].ssl;

    connection.mActive.Construct();
    connection.mIdle.Construct();
    connection.mHalfOpens.Construct();

    Sequence<HttpConnInfo>& active = connection.mActive.Value();
    Sequence<HttpConnInfo>& idle = connection.mIdle.Value();
    Sequence<HalfOpenInfoDict>& halfOpens = connection.mHalfOpens.Value();

    if (!active.SetCapacity(httpData->mData[i].active.Length(), fallible) ||
        !idle.SetCapacity(httpData->mData[i].idle.Length(), fallible) ||
        !halfOpens.SetCapacity(httpData->mData[i].halfOpens.Length(),
                               fallible)) {
      JS_ReportOutOfMemory(cx);
      return NS_ERROR_OUT_OF_MEMORY;
    }

    for (uint32_t j = 0; j < httpData->mData[i].active.Length(); j++) {
      HttpConnInfo& info = *active.AppendElement(fallible);
      info.mRtt = httpData->mData[i].active[j].rtt;
      info.mTtl = httpData->mData[i].active[j].ttl;
      info.mProtocolVersion = httpData->mData[i].active[j].protocolVersion;
    }

    for (uint32_t j = 0; j < httpData->mData[i].idle.Length(); j++) {
      HttpConnInfo& info = *idle.AppendElement(fallible);
      info.mRtt = httpData->mData[i].idle[j].rtt;
      info.mTtl = httpData->mData[i].idle[j].ttl;
      info.mProtocolVersion = httpData->mData[i].idle[j].protocolVersion;
    }

    for (uint32_t j = 0; j < httpData->mData[i].halfOpens.Length(); j++) {
      HalfOpenInfoDict& info = *halfOpens.AppendElement(fallible);
      info.mSpeculative = httpData->mData[i].halfOpens[j].speculative;
    }
  }

  JS::RootedValue val(cx);
  if (!ToJSValue(cx, dict, &val)) {
    return NS_ERROR_FAILURE;
  }

  httpData->mCallback->OnDashboardDataAvailable(val);

  return NS_OK;
}

NS_IMETHODIMP
Dashboard::GetEnableLogging(bool* value) {
  *value = mEnableLogging;
  return NS_OK;
}

NS_IMETHODIMP
Dashboard::SetEnableLogging(const bool value) {
  mEnableLogging = value;
  return NS_OK;
}

NS_IMETHODIMP
Dashboard::AddHost(const nsACString& aHost, uint32_t aSerial, bool aEncrypted) {
  if (mEnableLogging) {
    mozilla::MutexAutoLock lock(mWs.lock);
    LogData mData(nsCString(aHost), aSerial, aEncrypted);
    if (mWs.data.Contains(mData)) {
      return NS_OK;
    }
    if (!mWs.data.AppendElement(mData)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
Dashboard::RemoveHost(const nsACString& aHost, uint32_t aSerial) {
  if (mEnableLogging) {
    mozilla::MutexAutoLock lock(mWs.lock);
    int32_t index = mWs.IndexOf(nsCString(aHost), aSerial);
    if (index == -1) return NS_ERROR_FAILURE;
    mWs.data.RemoveElementAt(index);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
Dashboard::NewMsgSent(const nsACString& aHost, uint32_t aSerial,
                      uint32_t aLength) {
  if (mEnableLogging) {
    mozilla::MutexAutoLock lock(mWs.lock);
    int32_t index = mWs.IndexOf(nsCString(aHost), aSerial);
    if (index == -1) return NS_ERROR_FAILURE;
    mWs.data[index].mMsgSent++;
    mWs.data[index].mSizeSent += aLength;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
Dashboard::NewMsgReceived(const nsACString& aHost, uint32_t aSerial,
                          uint32_t aLength) {
  if (mEnableLogging) {
    mozilla::MutexAutoLock lock(mWs.lock);
    int32_t index = mWs.IndexOf(nsCString(aHost), aSerial);
    if (index == -1) return NS_ERROR_FAILURE;
    mWs.data[index].mMsgReceived++;
    mWs.data[index].mSizeReceived += aLength;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
Dashboard::RequestWebsocketConnections(nsINetDashboardCallback* aCallback) {
  RefPtr<WebSocketRequest> wsRequest = new WebSocketRequest();
  wsRequest->mCallback = new nsMainThreadPtrHolder<nsINetDashboardCallback>(
      "nsINetDashboardCallback", aCallback, true);
  wsRequest->mEventTarget = GetCurrentThreadEventTarget();

  wsRequest->mEventTarget->Dispatch(
      NewRunnableMethod<RefPtr<WebSocketRequest>>(
          "net::Dashboard::GetWebSocketConnections", this,
          &Dashboard::GetWebSocketConnections, wsRequest),
      NS_DISPATCH_NORMAL);
  return NS_OK;
}

nsresult Dashboard::GetWebSocketConnections(WebSocketRequest* aWsRequest) {
  RefPtr<WebSocketRequest> wsRequest = aWsRequest;
  AutoSafeJSContext cx;

  mozilla::dom::WebSocketDict dict;
  dict.mWebsockets.Construct();
  Sequence<mozilla::dom::WebSocketElement>& websockets =
      dict.mWebsockets.Value();

  mozilla::MutexAutoLock lock(mWs.lock);
  uint32_t length = mWs.data.Length();
  if (!websockets.SetCapacity(length, fallible)) {
    JS_ReportOutOfMemory(cx);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (uint32_t i = 0; i < mWs.data.Length(); i++) {
    dom::WebSocketElement& websocket = *websockets.AppendElement(fallible);
    CopyASCIItoUTF16(mWs.data[i].mHost, websocket.mHostport);
    websocket.mMsgsent = mWs.data[i].mMsgSent;
    websocket.mMsgreceived = mWs.data[i].mMsgReceived;
    websocket.mSentsize = mWs.data[i].mSizeSent;
    websocket.mReceivedsize = mWs.data[i].mSizeReceived;
    websocket.mEncrypted = mWs.data[i].mEncrypted;
  }

  JS::RootedValue val(cx);
  if (!ToJSValue(cx, dict, &val)) {
    return NS_ERROR_FAILURE;
  }
  wsRequest->mCallback->OnDashboardDataAvailable(val);

  return NS_OK;
}

NS_IMETHODIMP
Dashboard::RequestDNSInfo(nsINetDashboardCallback* aCallback) {
  RefPtr<DnsData> dnsData = new DnsData();
  dnsData->mCallback = new nsMainThreadPtrHolder<nsINetDashboardCallback>(
      "nsINetDashboardCallback", aCallback, true);

  nsresult rv;
  dnsData->mData.Clear();
  dnsData->mEventTarget = GetCurrentThreadEventTarget();

  if (!mDnsService) {
    mDnsService = do_GetService("@mozilla.org/network/dns-service;1", &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  gSocketTransportService->Dispatch(
      NewRunnableMethod<RefPtr<DnsData>>("net::Dashboard::GetDnsInfoDispatch",
                                         this, &Dashboard::GetDnsInfoDispatch,
                                         dnsData),
      NS_DISPATCH_NORMAL);
  return NS_OK;
}

nsresult Dashboard::GetDnsInfoDispatch(DnsData* aDnsData) {
  RefPtr<DnsData> dnsData = aDnsData;
  if (mDnsService) {
    mDnsService->GetDNSCacheEntries(&dnsData->mData);
  }
  dnsData->mEventTarget->Dispatch(
      NewRunnableMethod<RefPtr<DnsData>>("net::Dashboard::GetDNSCacheEntries",
                                         this, &Dashboard::GetDNSCacheEntries,
                                         dnsData),
      NS_DISPATCH_NORMAL);
  return NS_OK;
}

nsresult Dashboard::GetDNSCacheEntries(DnsData* dnsData) {
  AutoSafeJSContext cx;

  mozilla::dom::DNSCacheDict dict;
  dict.mEntries.Construct();
  Sequence<mozilla::dom::DnsCacheEntry>& entries = dict.mEntries.Value();

  uint32_t length = dnsData->mData.Length();
  if (!entries.SetCapacity(length, fallible)) {
    JS_ReportOutOfMemory(cx);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (uint32_t i = 0; i < dnsData->mData.Length(); i++) {
    dom::DnsCacheEntry& entry = *entries.AppendElement(fallible);
    entry.mHostaddr.Construct();

    Sequence<nsString>& addrs = entry.mHostaddr.Value();
    if (!addrs.SetCapacity(dnsData->mData[i].hostaddr.Length(), fallible)) {
      JS_ReportOutOfMemory(cx);
      return NS_ERROR_OUT_OF_MEMORY;
    }

    CopyASCIItoUTF16(dnsData->mData[i].hostname, entry.mHostname);
    entry.mExpiration = dnsData->mData[i].expiration;
    entry.mTrr = dnsData->mData[i].TRR;

    for (uint32_t j = 0; j < dnsData->mData[i].hostaddr.Length(); j++) {
      nsString* addr = addrs.AppendElement(fallible);
      if (!addr) {
        JS_ReportOutOfMemory(cx);
        return NS_ERROR_OUT_OF_MEMORY;
      }
      CopyASCIItoUTF16(dnsData->mData[i].hostaddr[j], *addr);
    }

    if (dnsData->mData[i].family == PR_AF_INET6) {
      entry.mFamily.AssignLiteral(u"ipv6");
    } else {
      entry.mFamily.AssignLiteral(u"ipv4");
    }
  }

  JS::RootedValue val(cx);
  if (!ToJSValue(cx, dict, &val)) {
    return NS_ERROR_FAILURE;
  }
  dnsData->mCallback->OnDashboardDataAvailable(val);

  return NS_OK;
}

NS_IMETHODIMP
Dashboard::RequestDNSLookup(const nsACString& aHost,
                            nsINetDashboardCallback* aCallback) {
  nsresult rv;

  if (!mDnsService) {
    mDnsService = do_GetService("@mozilla.org/network/dns-service;1", &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  RefPtr<LookupHelper> helper = new LookupHelper();
  helper->mCallback = new nsMainThreadPtrHolder<nsINetDashboardCallback>(
      "nsINetDashboardCallback", aCallback, true);
  helper->mEventTarget = GetCurrentThreadEventTarget();
  OriginAttributes attrs;
  rv = mDnsService->AsyncResolveNative(aHost, 0, helper.get(),
                                       NS_GetCurrentThread(), attrs,
                                       getter_AddRefs(helper->mCancel));
  return rv;
}

NS_IMETHODIMP
Dashboard::RequestRcwnStats(nsINetDashboardCallback* aCallback) {
  RefPtr<RcwnData> rcwnData = new RcwnData();
  rcwnData->mEventTarget = GetCurrentThreadEventTarget();
  rcwnData->mCallback = new nsMainThreadPtrHolder<nsINetDashboardCallback>(
      "nsINetDashboardCallback", aCallback, true);

  return rcwnData->mEventTarget->Dispatch(
      NewRunnableMethod<RefPtr<RcwnData>>("net::Dashboard::GetRcwnData", this,
                                          &Dashboard::GetRcwnData, rcwnData),
      NS_DISPATCH_NORMAL);
}

nsresult Dashboard::GetRcwnData(RcwnData* aData) {
  AutoSafeJSContext cx;
  mozilla::dom::RcwnStatus dict;

  dict.mTotalNetworkRequests = gIOService->GetTotalRequestNumber();
  dict.mRcwnCacheWonCount = gIOService->GetCacheWonRequestNumber();
  dict.mRcwnNetWonCount = gIOService->GetNetWonRequestNumber();

  uint32_t cacheSlow, cacheNotSlow;
  CacheFileUtils::CachePerfStats::GetSlowStats(&cacheSlow, &cacheNotSlow);
  dict.mCacheSlowCount = cacheSlow;
  dict.mCacheNotSlowCount = cacheNotSlow;

  dict.mPerfStats.Construct();
  Sequence<mozilla::dom::RcwnPerfStats>& perfStats = dict.mPerfStats.Value();
  uint32_t length = CacheFileUtils::CachePerfStats::LAST;
  if (!perfStats.SetCapacity(length, fallible)) {
    JS_ReportOutOfMemory(cx);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (uint32_t i = 0; i < length; i++) {
    CacheFileUtils::CachePerfStats::EDataType perfType =
        static_cast<CacheFileUtils::CachePerfStats::EDataType>(i);
    dom::RcwnPerfStats& elem = *perfStats.AppendElement(fallible);
    elem.mAvgShort =
        CacheFileUtils::CachePerfStats::GetAverage(perfType, false);
    elem.mAvgLong = CacheFileUtils::CachePerfStats::GetAverage(perfType, true);
    elem.mStddevLong =
        CacheFileUtils::CachePerfStats::GetStdDev(perfType, true);
  }

  JS::RootedValue val(cx);
  if (!ToJSValue(cx, dict, &val)) {
    return NS_ERROR_FAILURE;
  }

  aData->mCallback->OnDashboardDataAvailable(val);

  return NS_OK;
}

void HttpConnInfo::SetHTTP1ProtocolVersion(HttpVersion pv) {
  switch (pv) {
    case HttpVersion::v0_9:
      protocolVersion.AssignLiteral(u"http/0.9");
      break;
    case HttpVersion::v1_0:
      protocolVersion.AssignLiteral(u"http/1.0");
      break;
    case HttpVersion::v1_1:
      protocolVersion.AssignLiteral(u"http/1.1");
      break;
    case HttpVersion::v2_0:
      protocolVersion.AssignLiteral(u"http/2.0");
      break;
    default:
      protocolVersion.AssignLiteral(u"unknown protocol version");
  }
}

void HttpConnInfo::SetHTTP2ProtocolVersion(SpdyVersion pv) {
  MOZ_ASSERT(pv == SpdyVersion::HTTP_2);
  protocolVersion.AssignLiteral(u"h2");
}

NS_IMETHODIMP
Dashboard::GetLogPath(nsACString& aLogPath) {
  aLogPath.SetLength(2048);
  uint32_t len = LogModule::GetLogFile(aLogPath.BeginWriting(), 2048);
  aLogPath.SetLength(len);
  return NS_OK;
}

NS_IMETHODIMP
Dashboard::RequestConnection(const nsACString& aHost, uint32_t aPort,
                             const char* aProtocol, uint32_t aTimeout,
                             nsINetDashboardCallback* aCallback) {
  nsresult rv;
  RefPtr<ConnectionData> connectionData = new ConnectionData(this);
  connectionData->mHost = aHost;
  connectionData->mPort = aPort;
  connectionData->mProtocol = aProtocol;
  connectionData->mTimeout = aTimeout;

  connectionData->mCallback =
      new nsMainThreadPtrHolder<nsINetDashboardCallback>(
          "nsINetDashboardCallback", aCallback, true);
  connectionData->mEventTarget = GetCurrentThreadEventTarget();

  rv = TestNewConnection(connectionData);
  if (NS_FAILED(rv)) {
    mozilla::net::GetErrorString(rv, connectionData->mStatus);
    connectionData->mEventTarget->Dispatch(
        NewRunnableMethod<RefPtr<ConnectionData>>(
            "net::Dashboard::GetConnectionStatus", this,
            &Dashboard::GetConnectionStatus, connectionData),
        NS_DISPATCH_NORMAL);
    return rv;
  }

  return NS_OK;
}

nsresult Dashboard::GetConnectionStatus(ConnectionData* aConnectionData) {
  RefPtr<ConnectionData> connectionData = aConnectionData;
  AutoSafeJSContext cx;

  mozilla::dom::ConnStatusDict dict;
  dict.mStatus = connectionData->mStatus;

  JS::RootedValue val(cx);
  if (!ToJSValue(cx, dict, &val)) return NS_ERROR_FAILURE;

  connectionData->mCallback->OnDashboardDataAvailable(val);

  return NS_OK;
}

nsresult Dashboard::TestNewConnection(ConnectionData* aConnectionData) {
  RefPtr<ConnectionData> connectionData = aConnectionData;

  nsresult rv;
  if (!connectionData->mHost.Length() ||
      !net_IsValidHostName(connectionData->mHost)) {
    return NS_ERROR_UNKNOWN_HOST;
  }

  if (connectionData->mProtocol &&
      NS_LITERAL_STRING("ssl").EqualsASCII(connectionData->mProtocol)) {
    rv = gSocketTransportService->CreateTransport(
        &connectionData->mProtocol, 1, connectionData->mHost,
        connectionData->mPort, nullptr,
        getter_AddRefs(connectionData->mSocket));
  } else {
    rv = gSocketTransportService->CreateTransport(
        nullptr, 0, connectionData->mHost, connectionData->mPort, nullptr,
        getter_AddRefs(connectionData->mSocket));
  }
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = connectionData->mSocket->SetEventSink(connectionData,
                                             GetCurrentThreadEventTarget());
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = connectionData->mSocket->OpenInputStream(
      nsITransport::OPEN_BLOCKING, 0, 0,
      getter_AddRefs(connectionData->mStreamIn));
  if (NS_FAILED(rv)) {
    return rv;
  }

  connectionData->StartTimer(connectionData->mTimeout);

  return rv;
}

typedef struct {
  nsresult key;
  const char* error;
} ErrorEntry;

#undef ERROR
#define ERROR(key, val) \
  { key, #key }

ErrorEntry socketTransportStatuses[] = {
    ERROR(NS_NET_STATUS_RESOLVING_HOST, FAILURE(3)),
    ERROR(NS_NET_STATUS_RESOLVED_HOST, FAILURE(11)),
    ERROR(NS_NET_STATUS_CONNECTING_TO, FAILURE(7)),
    ERROR(NS_NET_STATUS_CONNECTED_TO, FAILURE(4)),
    ERROR(NS_NET_STATUS_TLS_HANDSHAKE_STARTING, FAILURE(12)),
    ERROR(NS_NET_STATUS_TLS_HANDSHAKE_ENDED, FAILURE(13)),
    ERROR(NS_NET_STATUS_SENDING_TO, FAILURE(5)),
    ERROR(NS_NET_STATUS_WAITING_FOR, FAILURE(10)),
    ERROR(NS_NET_STATUS_RECEIVING_FROM, FAILURE(6)),
};
#undef ERROR

static void GetErrorString(nsresult rv, nsAString& errorString) {
  for (auto& socketTransportStatus : socketTransportStatuses) {
    if (socketTransportStatus.key == rv) {
      errorString.AssignASCII(socketTransportStatus.error);
      return;
    }
  }
  nsAutoCString errorCString;
  mozilla::GetErrorName(rv, errorCString);
  CopyUTF8toUTF16(errorCString, errorString);
}

}  // namespace net
}  // namespace mozilla
