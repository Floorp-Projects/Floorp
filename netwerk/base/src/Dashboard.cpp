/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http:mozilla.org/MPL/2.0/. */

#include "mozilla/dom/NetDashboardBinding.h"
#include "mozilla/net/Dashboard.h"
#include "mozilla/net/HttpInfo.h"
#include "nsCxPusher.h"
#include "nsHttp.h"
#include "nsICancelable.h"
#include "nsIDNSService.h"
#include "nsIDNSRecord.h"
#include "nsIInputStream.h"
#include "nsISocketTransport.h"
#include "nsIThread.h"
#include "nsProxyRelease.h"
#include "nsSocketTransportService2.h"
#include "nsThreadUtils.h"
#include "nsURLHelper.h"

using mozilla::AutoSafeJSContext;
using mozilla::dom::Sequence;

namespace mozilla {
namespace net {

class SocketData
    : public nsISupports
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS

    SocketData()
    {
        mTotalSent = 0;
        mTotalRecv = 0;
        mThread = nullptr;
    }

    virtual ~SocketData()
    {
    }

    uint64_t mTotalSent;
    uint64_t mTotalRecv;
    nsTArray<SocketInfo> mData;
    nsMainThreadPtrHandle<NetDashboardCallback> mCallback;
    nsIThread *mThread;
};

NS_IMPL_ISUPPORTS0(SocketData)


class HttpData
    : public nsISupports
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS

    HttpData()
    {
        mThread = nullptr;
    }

    virtual ~HttpData()
    {
    }

    nsTArray<HttpRetParams> mData;
    nsMainThreadPtrHandle<NetDashboardCallback> mCallback;
    nsIThread *mThread;
};

NS_IMPL_ISUPPORTS0(HttpData)


class WebSocketRequest
    : public nsISupports
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS

    WebSocketRequest()
    {
        mThread = nullptr;
    }

    virtual ~WebSocketRequest()
    {
    }

    nsMainThreadPtrHandle<NetDashboardCallback> mCallback;
    nsIThread *mThread;
};

NS_IMPL_ISUPPORTS0(WebSocketRequest)


class DnsData
    : public nsISupports
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS

    DnsData()
    {
        mThread = nullptr;
    }

    virtual ~DnsData()
    {
    }

    nsTArray<DNSCacheEntries> mData;
    nsMainThreadPtrHandle<NetDashboardCallback> mCallback;
    nsIThread *mThread;
};

NS_IMPL_ISUPPORTS0(DnsData)


class ConnectionData
    : public nsITransportEventSink
    , public nsITimerCallback
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSITRANSPORTEVENTSINK
    NS_DECL_NSITIMERCALLBACK

    void StartTimer(uint32_t aTimeout);
    void StopTimer();

    ConnectionData(Dashboard *target)
    {
        mThread = nullptr;
        mDashboard = target;
    }

    virtual ~ConnectionData()
    {
        if (mTimer) {
            mTimer->Cancel();
        }
    }

    nsCOMPtr<nsISocketTransport> mSocket;
    nsCOMPtr<nsIInputStream> mStreamIn;
    nsCOMPtr<nsITimer> mTimer;
    nsMainThreadPtrHandle<NetDashboardCallback> mCallback;
    nsIThread *mThread;
    Dashboard *mDashboard;

    nsCString mHost;
    uint32_t mPort;
    const char *mProtocol;
    uint32_t mTimeout;

    nsString mStatus;
};

NS_IMPL_ISUPPORTS(ConnectionData, nsITransportEventSink, nsITimerCallback)

NS_IMETHODIMP
ConnectionData::OnTransportStatus(nsITransport *aTransport, nsresult aStatus,
                                  uint64_t aProgress, uint64_t aProgressMax)
{
    if (aStatus == NS_NET_STATUS_CONNECTED_TO) {
        StopTimer();
    }

    CopyASCIItoUTF16(Dashboard::GetErrorString(aStatus), mStatus);
    nsCOMPtr<nsIRunnable> event =
        NS_NewRunnableMethodWithArg<nsRefPtr<ConnectionData> >
        (mDashboard, &Dashboard::GetConnectionStatus, this);
    mThread->Dispatch(event, NS_DISPATCH_NORMAL);

    return NS_OK;
}

NS_IMETHODIMP
ConnectionData::Notify(nsITimer *aTimer)
{
    MOZ_ASSERT(aTimer == mTimer);

    if (mSocket) {
        mSocket->Close(NS_ERROR_ABORT);
        mSocket = nullptr;
        mStreamIn = nullptr;
    }

    mTimer = nullptr;

    mStatus.AssignLiteral(MOZ_UTF16("NS_ERROR_NET_TIMEOUT"));
    nsCOMPtr<nsIRunnable> event =
        NS_NewRunnableMethodWithArg<nsRefPtr<ConnectionData> >
        (mDashboard, &Dashboard::GetConnectionStatus, this);
    mThread->Dispatch(event, NS_DISPATCH_NORMAL);

    return NS_OK;
}

void
ConnectionData::StartTimer(uint32_t aTimeout)
{
    if (!mTimer) {
        mTimer = do_CreateInstance("@mozilla.org/timer;1");
    }

    mTimer->InitWithCallback(this, aTimeout * 1000,
        nsITimer::TYPE_ONE_SHOT);
}

void
ConnectionData::StopTimer()
{
    if (mTimer) {
        mTimer->Cancel();
        mTimer = nullptr;
    }
}


class LookupHelper;

class LookupArgument
    : public nsISupports
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS

    LookupArgument(nsIDNSRecord *aRecord, LookupHelper *aHelper)
    {
        mRecord = aRecord;
        mHelper = aHelper;
    }

    virtual ~LookupArgument()
    {
    }

    nsCOMPtr<nsIDNSRecord> mRecord;
    nsRefPtr<LookupHelper> mHelper;
};

NS_IMPL_ISUPPORTS0(LookupArgument)


class LookupHelper
    : public nsIDNSListener
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIDNSLISTENER

    LookupHelper() {
    }

    virtual ~LookupHelper()
    {
        if (mCancel) {
            mCancel->Cancel(NS_ERROR_ABORT);
        }
    }

    nsresult ConstructAnswer(LookupArgument *aArgument);
public:
    nsCOMPtr<nsICancelable> mCancel;
    nsMainThreadPtrHandle<NetDashboardCallback> mCallback;
    nsIThread *mThread;
    nsresult mStatus;
};

NS_IMPL_ISUPPORTS(LookupHelper, nsIDNSListener)

NS_IMETHODIMP
LookupHelper::OnLookupComplete(nsICancelable *aRequest,
                               nsIDNSRecord *aRecord, nsresult aStatus)
{
    MOZ_ASSERT(aRequest == mCancel);
    mCancel = nullptr;
    mStatus = aStatus;

    nsRefPtr<LookupArgument> arg = new LookupArgument(aRecord, this);
    nsCOMPtr<nsIRunnable> event =
        NS_NewRunnableMethodWithArg<nsRefPtr<LookupArgument> >(
        this, &LookupHelper::ConstructAnswer, arg);
    mThread->Dispatch(event, NS_DISPATCH_NORMAL);

    return NS_OK;
}

nsresult
LookupHelper::ConstructAnswer(LookupArgument *aArgument)
{

    nsIDNSRecord *aRecord = aArgument->mRecord;
    AutoSafeJSContext cx;

    mozilla::dom::DNSLookupDict dict;
    dict.mAddress.Construct();

    Sequence<nsString> &addresses = dict.mAddress.Value();

    if (NS_SUCCEEDED(mStatus)) {
        dict.mAnswer = true;
        bool hasMore;
        aRecord->HasMore(&hasMore);
        while (hasMore) {
           nsCString nextAddress;
           aRecord->GetNextAddrAsString(nextAddress);
           CopyASCIItoUTF16(nextAddress, *addresses.AppendElement());
           aRecord->HasMore(&hasMore);
        }
    } else {
        dict.mAnswer = false;
        CopyASCIItoUTF16(Dashboard::GetErrorString(mStatus), dict.mError);
    }

    JS::RootedValue val(cx);
    if (!dict.ToObject(cx, &val)) {
        return NS_ERROR_FAILURE;
    }

    this->mCallback->OnDashboardDataAvailable(val);

    return NS_OK;
}

NS_IMPL_ISUPPORTS(Dashboard, nsIDashboard, nsIDashboardEventNotifier)

Dashboard::Dashboard()
{
    mEnableLogging = false;
}

Dashboard::~Dashboard()
{
}

NS_IMETHODIMP
Dashboard::RequestSockets(NetDashboardCallback *aCallback)
{
    nsRefPtr<SocketData> socketData = new SocketData();
    socketData->mCallback =
        new nsMainThreadPtrHolder<NetDashboardCallback>(aCallback, true);
    socketData->mThread = NS_GetCurrentThread();
    nsCOMPtr<nsIRunnable> event =
        NS_NewRunnableMethodWithArg<nsRefPtr<SocketData> >
        (this, &Dashboard::GetSocketsDispatch, socketData);
    gSocketTransportService->Dispatch(event, NS_DISPATCH_NORMAL);
    return NS_OK;
}

nsresult
Dashboard::GetSocketsDispatch(SocketData *aSocketData)
{
    nsRefPtr<SocketData> socketData = aSocketData;
    if (gSocketTransportService) {
        gSocketTransportService->GetSocketConnections(&socketData->mData);
        socketData->mTotalSent = gSocketTransportService->GetSentBytes();
        socketData->mTotalRecv = gSocketTransportService->GetReceivedBytes();
    }
    nsCOMPtr<nsIRunnable> event =
        NS_NewRunnableMethodWithArg<nsRefPtr<SocketData> >
        (this, &Dashboard::GetSockets, socketData);
    socketData->mThread->Dispatch(event, NS_DISPATCH_NORMAL);
    return NS_OK;
}

nsresult
Dashboard::GetSockets(SocketData *aSocketData)
{
    nsRefPtr<SocketData> socketData = aSocketData;
    AutoSafeJSContext cx;

    mozilla::dom::SocketsDict dict;
    dict.mSockets.Construct();
    dict.mSent = 0;
    dict.mReceived = 0;

    Sequence<mozilla::dom::SocketElement> &sockets = dict.mSockets.Value();

    uint32_t length = socketData->mData.Length();
    if (!sockets.SetCapacity(length)) {
            JS_ReportOutOfMemory(cx);
            return NS_ERROR_OUT_OF_MEMORY;
    }

    for (uint32_t i = 0; i < socketData->mData.Length(); i++) {
        mozilla::dom::SocketElement &mSocket = *sockets.AppendElement();
        CopyASCIItoUTF16(socketData->mData[i].host, mSocket.mHost);
        mSocket.mPort = socketData->mData[i].port;
        mSocket.mActive = socketData->mData[i].active;
        mSocket.mTcp = socketData->mData[i].tcp;
        mSocket.mSent = (double) socketData->mData[i].sent;
        mSocket.mReceived = (double) socketData->mData[i].received;
        dict.mSent += socketData->mData[i].sent;
        dict.mReceived += socketData->mData[i].received;
    }

    dict.mSent += socketData->mTotalSent;
    dict.mReceived += socketData->mTotalRecv;
    JS::RootedValue val(cx);
    if (!dict.ToObject(cx, &val))
        return NS_ERROR_FAILURE;
    socketData->mCallback->OnDashboardDataAvailable(val);

    return NS_OK;
}

NS_IMETHODIMP
Dashboard::RequestHttpConnections(NetDashboardCallback *aCallback)
{
    nsRefPtr<HttpData> httpData = new HttpData();
    httpData->mCallback =
        new nsMainThreadPtrHolder<NetDashboardCallback>(aCallback, true);
    httpData->mThread = NS_GetCurrentThread();

    nsCOMPtr<nsIRunnable> event =
        NS_NewRunnableMethodWithArg<nsRefPtr<HttpData> >
        (this, &Dashboard::GetHttpDispatch, httpData);
    gSocketTransportService->Dispatch(event, NS_DISPATCH_NORMAL);
    return NS_OK;
}

nsresult
Dashboard::GetHttpDispatch(HttpData *aHttpData)
{
    nsRefPtr<HttpData> httpData = aHttpData;
    HttpInfo::GetHttpConnectionData(&httpData->mData);
    nsCOMPtr<nsIRunnable> event =
        NS_NewRunnableMethodWithArg<nsRefPtr<HttpData> >
        (this, &Dashboard::GetHttpConnections, httpData);
    httpData->mThread->Dispatch(event, NS_DISPATCH_NORMAL);
    return NS_OK;
}


nsresult
Dashboard::GetHttpConnections(HttpData *aHttpData)
{
    nsRefPtr<HttpData> httpData = aHttpData;
    AutoSafeJSContext cx;

    mozilla::dom::HttpConnDict dict;
    dict.mConnections.Construct();

    using mozilla::dom::HalfOpenInfoDict;
    using mozilla::dom::HttpConnectionElement;
    using mozilla::dom::HttpConnInfo;
    Sequence<HttpConnectionElement> &connections = dict.mConnections.Value();

    uint32_t length = httpData->mData.Length();
    if (!connections.SetCapacity(length)) {
            JS_ReportOutOfMemory(cx);
            return NS_ERROR_OUT_OF_MEMORY;
    }

    for (uint32_t i = 0; i < httpData->mData.Length(); i++) {
        HttpConnectionElement &connection = *connections.AppendElement();

        CopyASCIItoUTF16(httpData->mData[i].host, connection.mHost);
        connection.mPort = httpData->mData[i].port;
        connection.mSpdy = httpData->mData[i].spdy;
        connection.mSsl = httpData->mData[i].ssl;

        connection.mActive.Construct();
        connection.mIdle.Construct();
        connection.mHalfOpens.Construct();

        Sequence<HttpConnInfo> &active = connection.mActive.Value();
        Sequence<HttpConnInfo> &idle = connection.mIdle.Value();
        Sequence<HalfOpenInfoDict> &halfOpens = connection.mHalfOpens.Value();

        if (!active.SetCapacity(httpData->mData[i].active.Length()) ||
            !idle.SetCapacity(httpData->mData[i].idle.Length()) ||
            !halfOpens.SetCapacity(httpData->mData[i].halfOpens.Length())) {
                JS_ReportOutOfMemory(cx);
                return NS_ERROR_OUT_OF_MEMORY;
        }

        for (uint32_t j = 0; j < httpData->mData[i].active.Length(); j++) {
            HttpConnInfo &info = *active.AppendElement();
            info.mRtt = httpData->mData[i].active[j].rtt;
            info.mTtl = httpData->mData[i].active[j].ttl;
            info.mProtocolVersion =
                httpData->mData[i].active[j].protocolVersion;
        }

        for (uint32_t j = 0; j < httpData->mData[i].idle.Length(); j++) {
            HttpConnInfo &info = *idle.AppendElement();
            info.mRtt = httpData->mData[i].idle[j].rtt;
            info.mTtl = httpData->mData[i].idle[j].ttl;
            info.mProtocolVersion = httpData->mData[i].idle[j].protocolVersion;
        }

        for (uint32_t j = 0; j < httpData->mData[i].halfOpens.Length(); j++) {
            HalfOpenInfoDict &info = *halfOpens.AppendElement();
            info.mSpeculative = httpData->mData[i].halfOpens[j].speculative;
        }
    }

    JS::RootedValue val(cx);
    if (!dict.ToObject(cx, &val)) {
        return NS_ERROR_FAILURE;
    }

    httpData->mCallback->OnDashboardDataAvailable(val);

    return NS_OK;
}

NS_IMETHODIMP
Dashboard::GetEnableLogging(bool *value)
{
    *value = mEnableLogging;
    return NS_OK;
}

NS_IMETHODIMP
Dashboard::SetEnableLogging(const bool value)
{
    mEnableLogging = value;
    return NS_OK;
}

NS_IMETHODIMP
Dashboard::AddHost(const nsACString& aHost, uint32_t aSerial, bool aEncrypted)
{
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
Dashboard::RemoveHost(const nsACString& aHost, uint32_t aSerial)
{
    if (mEnableLogging) {
        mozilla::MutexAutoLock lock(mWs.lock);
        int32_t index = mWs.IndexOf(nsCString(aHost), aSerial);
        if (index == -1)
            return NS_ERROR_FAILURE;
        mWs.data.RemoveElementAt(index);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
Dashboard::NewMsgSent(const nsACString& aHost, uint32_t aSerial, uint32_t aLength)
{
    if (mEnableLogging) {
        mozilla::MutexAutoLock lock(mWs.lock);
        int32_t index = mWs.IndexOf(nsCString(aHost), aSerial);
        if (index == -1)
            return NS_ERROR_FAILURE;
        mWs.data[index].mMsgSent++;
        mWs.data[index].mSizeSent += aLength;
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
Dashboard::NewMsgReceived(const nsACString& aHost, uint32_t aSerial, uint32_t aLength)
{
    if (mEnableLogging) {
        mozilla::MutexAutoLock lock(mWs.lock);
        int32_t index = mWs.IndexOf(nsCString(aHost), aSerial);
        if (index == -1)
            return NS_ERROR_FAILURE;
        mWs.data[index].mMsgReceived++;
        mWs.data[index].mSizeReceived += aLength;
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
Dashboard::RequestWebsocketConnections(NetDashboardCallback *aCallback)
{
    nsRefPtr<WebSocketRequest> wsRequest = new WebSocketRequest();
    wsRequest->mCallback =
        new nsMainThreadPtrHolder<NetDashboardCallback>(aCallback, true);
    wsRequest->mThread = NS_GetCurrentThread();

    nsCOMPtr<nsIRunnable> event =
        NS_NewRunnableMethodWithArg<nsRefPtr<WebSocketRequest> >
        (this, &Dashboard::GetWebSocketConnections, wsRequest);
    wsRequest->mThread->Dispatch(event, NS_DISPATCH_NORMAL);
    return NS_OK;
}

nsresult
Dashboard::GetWebSocketConnections(WebSocketRequest *aWsRequest)
{
    nsRefPtr<WebSocketRequest> wsRequest = aWsRequest;
    AutoSafeJSContext cx;

    mozilla::dom::WebSocketDict dict;
    dict.mWebsockets.Construct();
    Sequence<mozilla::dom::WebSocketElement> &websockets =
        dict.mWebsockets.Value();

    mozilla::MutexAutoLock lock(mWs.lock);
    uint32_t length = mWs.data.Length();
    if (!websockets.SetCapacity(length)) {
        JS_ReportOutOfMemory(cx);
        return NS_ERROR_OUT_OF_MEMORY;
    }

    for (uint32_t i = 0; i < mWs.data.Length(); i++) {
        mozilla::dom::WebSocketElement &websocket = *websockets.AppendElement();
        CopyASCIItoUTF16(mWs.data[i].mHost, websocket.mHostport);
        websocket.mMsgsent = mWs.data[i].mMsgSent;
        websocket.mMsgreceived = mWs.data[i].mMsgReceived;
        websocket.mSentsize = mWs.data[i].mSizeSent;
        websocket.mReceivedsize = mWs.data[i].mSizeReceived;
        websocket.mEncrypted = mWs.data[i].mEncrypted;
    }

    JS::RootedValue val(cx);
    if (!dict.ToObject(cx, &val)) {
        return NS_ERROR_FAILURE;
    }
    wsRequest->mCallback->OnDashboardDataAvailable(val);

    return NS_OK;
}

NS_IMETHODIMP
Dashboard::RequestDNSInfo(NetDashboardCallback *aCallback)
{
    nsRefPtr<DnsData> dnsData = new DnsData();
    dnsData->mCallback =
        new nsMainThreadPtrHolder<NetDashboardCallback>(aCallback, true);

    nsresult rv;
    dnsData->mData.Clear();
    dnsData->mThread = NS_GetCurrentThread();

    if (!mDnsService) {
        mDnsService = do_GetService("@mozilla.org/network/dns-service;1", &rv);
        if (NS_FAILED(rv)) {
            return rv;
        }
    }

    nsCOMPtr<nsIRunnable> event =
        NS_NewRunnableMethodWithArg<nsRefPtr<DnsData> >
        (this, &Dashboard::GetDnsInfoDispatch, dnsData);
    gSocketTransportService->Dispatch(event, NS_DISPATCH_NORMAL);
    return NS_OK;
}

nsresult
Dashboard::GetDnsInfoDispatch(DnsData *aDnsData)
{
    nsRefPtr<DnsData> dnsData = aDnsData;
    if (mDnsService) {
        mDnsService->GetDNSCacheEntries(&dnsData->mData);
    }
    nsCOMPtr<nsIRunnable> event =
        NS_NewRunnableMethodWithArg<nsRefPtr<DnsData> >
        (this, &Dashboard::GetDNSCacheEntries, dnsData);
    dnsData->mThread->Dispatch(event, NS_DISPATCH_NORMAL);
    return NS_OK;
}

nsresult
Dashboard::GetDNSCacheEntries(DnsData *dnsData)
{
    AutoSafeJSContext cx;

    mozilla::dom::DNSCacheDict dict;
    dict.mEntries.Construct();
    Sequence<mozilla::dom::DnsCacheEntry> &entries = dict.mEntries.Value();

    uint32_t length = dnsData->mData.Length();
    if (!entries.SetCapacity(length)) {
        JS_ReportOutOfMemory(cx);
        return NS_ERROR_OUT_OF_MEMORY;
    }

    for (uint32_t i = 0; i < dnsData->mData.Length(); i++) {
        mozilla::dom::DnsCacheEntry &entry = *entries.AppendElement();
        entry.mHostaddr.Construct();

        Sequence<nsString> &addrs = entry.mHostaddr.Value();
        if (!addrs.SetCapacity(dnsData->mData[i].hostaddr.Length())) {
            JS_ReportOutOfMemory(cx);
            return NS_ERROR_OUT_OF_MEMORY;
        }

        CopyASCIItoUTF16(dnsData->mData[i].hostname, entry.mHostname);
        entry.mExpiration = dnsData->mData[i].expiration;

        for (uint32_t j = 0; j < dnsData->mData[i].hostaddr.Length(); j++) {
            CopyASCIItoUTF16(dnsData->mData[i].hostaddr[j],
                *addrs.AppendElement());
        }

        if (dnsData->mData[i].family == PR_AF_INET6) {
            CopyASCIItoUTF16("ipv6", entry.mFamily);
        } else {
            CopyASCIItoUTF16("ipv4", entry.mFamily);
        }
    }

    JS::RootedValue val(cx);
    if (!dict.ToObject(cx, &val)) {
        return NS_ERROR_FAILURE;
    }
    dnsData->mCallback->OnDashboardDataAvailable(val);

    return NS_OK;
}

NS_IMETHODIMP
Dashboard::RequestDNSLookup(const nsACString &aHost,
                            NetDashboardCallback *aCallback)
{
    nsresult rv;

    if (!mDnsService) {
        mDnsService = do_GetService("@mozilla.org/network/dns-service;1", &rv);
        if (NS_FAILED(rv)) {
            return rv;
        }
    }

    nsRefPtr<LookupHelper> helper = new LookupHelper();
    helper->mCallback =
        new nsMainThreadPtrHolder<NetDashboardCallback>(aCallback, true);
    helper->mThread = NS_GetCurrentThread();
    rv = mDnsService->AsyncResolve(aHost, 0, helper.get(),
                                   NS_GetCurrentThread(),
                                   getter_AddRefs(helper->mCancel));
    return rv;
}

void
HttpConnInfo::SetHTTP1ProtocolVersion(uint8_t pv)
{
    switch (pv) {
    case NS_HTTP_VERSION_0_9:
        protocolVersion.AssignLiteral(MOZ_UTF16("http/0.9"));
        break;
    case NS_HTTP_VERSION_1_0:
        protocolVersion.AssignLiteral(MOZ_UTF16("http/1.0"));
        break;
    case NS_HTTP_VERSION_1_1:
        protocolVersion.AssignLiteral(MOZ_UTF16("http/1.1"));
        break;
    case NS_HTTP_VERSION_2_0:
        protocolVersion.AssignLiteral(MOZ_UTF16("http/2.0"));
        break;
    default:
        protocolVersion.AssignLiteral(MOZ_UTF16("unknown protocol version"));
    }
}

void
HttpConnInfo::SetHTTP2ProtocolVersion(uint8_t pv)
{
    if (pv == SPDY_VERSION_3) {
        protocolVersion.AssignLiteral(MOZ_UTF16("spdy/3"));
    } else if (pv == SPDY_VERSION_31) {
        protocolVersion.AssignLiteral(MOZ_UTF16("spdy/3.1"));
    } else {
        MOZ_ASSERT (pv == NS_HTTP2_DRAFT_VERSION);
        protocolVersion.Assign(NS_LITERAL_STRING(NS_HTTP2_DRAFT_TOKEN));
    }
}

NS_IMETHODIMP
Dashboard::RequestConnection(const nsACString& aHost, uint32_t aPort,
                             const char *aProtocol, uint32_t aTimeout,
                             NetDashboardCallback *aCallback)
{
    nsresult rv;
    nsRefPtr<ConnectionData> connectionData = new ConnectionData(this);
    connectionData->mHost = aHost;
    connectionData->mPort = aPort;
    connectionData->mProtocol = aProtocol;
    connectionData->mTimeout = aTimeout;

    connectionData->mCallback =
        new nsMainThreadPtrHolder<NetDashboardCallback>(aCallback, true);
    connectionData->mThread = NS_GetCurrentThread();

    rv = TestNewConnection(connectionData);
    if (NS_FAILED(rv)) {
        CopyASCIItoUTF16(GetErrorString(rv), connectionData->mStatus);
        nsCOMPtr<nsIRunnable> event =
            NS_NewRunnableMethodWithArg<nsRefPtr<ConnectionData> >
            (this, &Dashboard::GetConnectionStatus, connectionData);
        connectionData->mThread->Dispatch(event, NS_DISPATCH_NORMAL);
        return rv;
    }

    return NS_OK;
}

nsresult
Dashboard::GetConnectionStatus(ConnectionData *aConnectionData)
{
    nsRefPtr<ConnectionData> connectionData = aConnectionData;
    AutoSafeJSContext cx;

    mozilla::dom::ConnStatusDict dict;
    dict.mStatus = connectionData->mStatus;

    JS::RootedValue val(cx);
    if (!dict.ToObject(cx, &val))
        return NS_ERROR_FAILURE;

    connectionData->mCallback->OnDashboardDataAvailable(val);

    return NS_OK;
}

nsresult
Dashboard::TestNewConnection(ConnectionData *aConnectionData)
{
    nsRefPtr<ConnectionData> connectionData = aConnectionData;

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
            nullptr, 0, connectionData->mHost,
            connectionData->mPort, nullptr,
            getter_AddRefs(connectionData->mSocket));
    }
    if (NS_FAILED(rv)) {
        return rv;
    }

    rv = connectionData->mSocket->SetEventSink(connectionData,
        NS_GetCurrentThread());
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

typedef struct
{
    nsresult key;
    const char *error;
} ErrorEntry;

#undef ERROR
#define ERROR(key, val) {key, #key}

ErrorEntry errors[] = {
    #include "ErrorList.h"
};

ErrorEntry socketTransportStatuses[] = {
        ERROR(NS_NET_STATUS_RESOLVING_HOST,  FAILURE(3)),
        ERROR(NS_NET_STATUS_RESOLVED_HOST,   FAILURE(11)),
        ERROR(NS_NET_STATUS_CONNECTING_TO,   FAILURE(7)),
        ERROR(NS_NET_STATUS_CONNECTED_TO,    FAILURE(4)),
        ERROR(NS_NET_STATUS_SENDING_TO,      FAILURE(5)),
        ERROR(NS_NET_STATUS_WAITING_FOR,     FAILURE(10)),
        ERROR(NS_NET_STATUS_RECEIVING_FROM,  FAILURE(6)),
};
#undef ERROR

const char *
Dashboard::GetErrorString(nsresult rv)
{
    int length = sizeof(socketTransportStatuses) / sizeof(ErrorEntry);
    for (int i = 0;i < length;i++)
        if (socketTransportStatuses[i].key == rv) {
            return socketTransportStatuses[i].error;
        }

    length = sizeof(errors) / sizeof(ErrorEntry);
    for (int i = 0;i < length;i++)
        if (errors[i].key == rv) {
            return errors[i].error;
        }

    return nullptr;
}

} } // namespace mozilla::net
