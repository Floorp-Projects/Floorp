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
#include "nsSocketTransportService2.h"
#include "nsThreadUtils.h"
#include "nsURLHelper.h"

using mozilla::AutoSafeJSContext;
namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS5(Dashboard, nsIDashboard, nsIDashboardEventNotifier,
                              nsITransportEventSink, nsITimerCallback,
                              nsIDNSListener)
using mozilla::dom::Sequence;

struct ConnStatus
{
    nsString creationSts;
};

Dashboard::Dashboard()
{
    mEnableLogging = false;
}

Dashboard::~Dashboard()
{
    if (mDnsup.cancel)
        mDnsup.cancel->Cancel(NS_ERROR_ABORT);
}

NS_IMETHODIMP
Dashboard::RequestSockets(NetDashboardCallback* cb)
{
    if (mSock.cb)
        return NS_ERROR_FAILURE;
    mSock.cb = cb;
    mSock.data.Clear();
    mSock.thread = NS_GetCurrentThread();

    nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(this, &Dashboard::GetSocketsDispatch);
    gSocketTransportService->Dispatch(event, NS_DISPATCH_NORMAL);
    return NS_OK;
}

void
Dashboard::GetSocketsDispatch()
{
    if (gSocketTransportService) {
        gSocketTransportService->GetSocketConnections(&mSock.data);
        mSock.totalSent = gSocketTransportService->GetSentBytes();
        mSock.totalRecv = gSocketTransportService->GetReceivedBytes();
    }
    nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(this, &Dashboard::GetSockets);
    mSock.thread->Dispatch(event, NS_DISPATCH_NORMAL);
}

nsresult
Dashboard::GetSockets()
{
    AutoSafeJSContext cx;

    mozilla::dom::SocketsDict dict;
    dict.mSockets.Construct();
    dict.mSent = 0;
    dict.mReceived = 0;

    Sequence<mozilla::dom::SocketElement> &sockets = dict.mSockets.Value();

    uint32_t length = mSock.data.Length();
    if (!sockets.SetCapacity(length)) {
            mSock.cb = nullptr;
            mSock.data.Clear();
            JS_ReportOutOfMemory(cx);
            return NS_ERROR_OUT_OF_MEMORY;
    }

    for (uint32_t i = 0; i < mSock.data.Length(); i++) {
        mozilla::dom::SocketElement &socket = *sockets.AppendElement();
        CopyASCIItoUTF16(mSock.data[i].host, socket.mHost);
        socket.mPort = mSock.data[i].port;
        socket.mActive = mSock.data[i].active;
        socket.mTcp = mSock.data[i].tcp;
        socket.mSent = (double) mSock.data[i].sent;
        socket.mReceived = (double) mSock.data[i].received;
        dict.mSent += mSock.data[i].sent;
        dict.mReceived += mSock.data[i].received;
    }

    dict.mSent += mSock.totalSent;
    dict.mReceived += mSock.totalRecv;
    JS::RootedValue val(cx);
    if (!dict.ToObject(cx, JS::NullPtr(), &val)) {
        mSock.cb = nullptr;
        mSock.data.Clear();
        return NS_ERROR_FAILURE;
    }
    mSock.cb->OnDashboardDataAvailable(val);
    mSock.cb = nullptr;

    return NS_OK;
}

NS_IMETHODIMP
Dashboard::RequestHttpConnections(NetDashboardCallback* cb)
{
    if (mHttp.cb)
        return NS_ERROR_FAILURE;
    mHttp.cb = cb;
    mHttp.data.Clear();
    mHttp.thread = NS_GetCurrentThread();

    nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(this, &Dashboard::GetHttpDispatch);
    gSocketTransportService->Dispatch(event, NS_DISPATCH_NORMAL);
    return NS_OK;
}

void
Dashboard::GetHttpDispatch()
{
    HttpInfo::GetHttpConnectionData(&mHttp.data);
    nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(this, &Dashboard::GetHttpConnections);
    mHttp.thread->Dispatch(event, NS_DISPATCH_NORMAL);
}


nsresult
Dashboard::GetHttpConnections()
{
    AutoSafeJSContext cx;

    mozilla::dom::HttpConnDict dict;
    dict.mConnections.Construct();

    using mozilla::dom::HalfOpenInfoDict;
    using mozilla::dom::HttpConnectionElement;
    using mozilla::dom::HttpConnInfo;
    Sequence<HttpConnectionElement> &connections = dict.mConnections.Value();

    uint32_t length = mHttp.data.Length();
    if (!connections.SetCapacity(length)) {
            mHttp.cb = nullptr;
            mHttp.data.Clear();
            JS_ReportOutOfMemory(cx);
            return NS_ERROR_OUT_OF_MEMORY;
    }

    for (uint32_t i = 0; i < mHttp.data.Length(); i++) {
        HttpConnectionElement &connection = *connections.AppendElement();

        CopyASCIItoUTF16(mHttp.data[i].host, connection.mHost);
        connection.mPort = mHttp.data[i].port;
        connection.mSpdy = mHttp.data[i].spdy;
        connection.mSsl = mHttp.data[i].ssl;

        connection.mActive.Construct();
        connection.mIdle.Construct();
        connection.mHalfOpens.Construct();

        Sequence<HttpConnInfo> &active = connection.mActive.Value();
        Sequence<HttpConnInfo> &idle = connection.mIdle.Value();
        Sequence<HalfOpenInfoDict> &halfOpens = connection.mHalfOpens.Value();

        if (!active.SetCapacity(mHttp.data[i].active.Length()) ||
            !idle.SetCapacity(mHttp.data[i].idle.Length()) ||
            !halfOpens.SetCapacity(mHttp.data[i].halfOpens.Length())) {
                mHttp.cb = nullptr;
                mHttp.data.Clear();
                JS_ReportOutOfMemory(cx);
                return NS_ERROR_OUT_OF_MEMORY;
        }

        for (uint32_t j = 0; j < mHttp.data[i].active.Length(); j++) {
            HttpConnInfo &info = *active.AppendElement();
            info.mRtt = mHttp.data[i].active[j].rtt;
            info.mTtl = mHttp.data[i].active[j].ttl;
            info.mProtocolVersion = mHttp.data[i].active[j].protocolVersion;
        }

        for (uint32_t j = 0; j < mHttp.data[i].idle.Length(); j++) {
            HttpConnInfo &info = *idle.AppendElement();
            info.mRtt = mHttp.data[i].idle[j].rtt;
            info.mTtl = mHttp.data[i].idle[j].ttl;
            info.mProtocolVersion = mHttp.data[i].idle[j].protocolVersion;
        }

        for (uint32_t j = 0; j < mHttp.data[i].halfOpens.Length(); j++) {
            HalfOpenInfoDict &info = *halfOpens.AppendElement();
            info.mSpeculative = mHttp.data[i].halfOpens[j].speculative;
        }
    }

    JS::RootedValue val(cx);
    if (!dict.ToObject(cx, JS::NullPtr(), &val)) {
        mHttp.cb = nullptr;
        mHttp.data.Clear();
        return NS_ERROR_FAILURE;
    }
    mHttp.cb->OnDashboardDataAvailable(val);
    mHttp.cb = nullptr;

    return NS_OK;
}


NS_IMETHODIMP
Dashboard::GetEnableLogging(bool* value)
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
        LogData data(nsCString(aHost), aSerial, aEncrypted);
        if (mWs.data.Contains(data))
            return NS_OK;
        if (!mWs.data.AppendElement(data))
            return NS_ERROR_OUT_OF_MEMORY;
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
Dashboard::RequestWebsocketConnections(NetDashboardCallback* cb)
{
    if (mWs.cb)
        return NS_ERROR_FAILURE;
    mWs.cb = cb;
    mWs.thread = NS_GetCurrentThread();

    nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(this, &Dashboard::GetWebSocketConnections);
    mWs.thread->Dispatch(event, NS_DISPATCH_NORMAL);
    return NS_OK;
}

nsresult
Dashboard::GetWebSocketConnections()
{
    AutoSafeJSContext cx;

    mozilla::dom::WebSocketDict dict;
    dict.mWebsockets.Construct();
    Sequence<mozilla::dom::WebSocketElement> &websockets = dict.mWebsockets.Value();

    mozilla::MutexAutoLock lock(mWs.lock);
    uint32_t length = mWs.data.Length();
    if (!websockets.SetCapacity(length)) {
        mWs.cb = nullptr;
        mWs.data.Clear();
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
    if (!dict.ToObject(cx, JS::NullPtr(), &val)) {
        mWs.cb = nullptr;
        mWs.data.Clear();
        return NS_ERROR_FAILURE;
    }
    mWs.cb->OnDashboardDataAvailable(val);
    mWs.cb = nullptr;

    return NS_OK;
}

NS_IMETHODIMP
Dashboard::RequestDNSInfo(NetDashboardCallback* cb)
{
    if (mDns.cb)
        return NS_ERROR_FAILURE;
    mDns.cb = cb;
    nsresult rv;
    mDns.data.Clear();
    mDns.thread = NS_GetCurrentThread();

    if (!mDns.serv) {
        mDns.serv = do_GetService("@mozilla.org/network/dns-service;1", &rv);
        if (NS_FAILED(rv))
            return rv;
    }

    nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(this, &Dashboard::GetDnsInfoDispatch);
    gSocketTransportService->Dispatch(event, NS_DISPATCH_NORMAL);
    return NS_OK;
}

void
Dashboard::GetDnsInfoDispatch()
{
    if (mDns.serv)
        mDns.serv->GetDNSCacheEntries(&mDns.data);
    nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(this, &Dashboard::GetDNSCacheEntries);
    mDns.thread->Dispatch(event, NS_DISPATCH_NORMAL);
}

nsresult
Dashboard::GetDNSCacheEntries()
{
    AutoSafeJSContext cx;

    mozilla::dom::DNSCacheDict dict;
    dict.mEntries.Construct();
    Sequence<mozilla::dom::DnsCacheEntry> &entries = dict.mEntries.Value();

    uint32_t length = mDns.data.Length();
    if (!entries.SetCapacity(length)) {
        mDns.cb = nullptr;
        mDns.data.Clear();
        JS_ReportOutOfMemory(cx);
        return NS_ERROR_OUT_OF_MEMORY;
    }

    for (uint32_t i = 0; i < mDns.data.Length(); i++) {
        mozilla::dom::DnsCacheEntry &entry = *entries.AppendElement();
        entry.mHostaddr.Construct();

        Sequence<nsString> &addrs = entry.mHostaddr.Value();
        if (!addrs.SetCapacity(mDns.data[i].hostaddr.Length())) {
            mDns.cb = nullptr;
            mDns.data.Clear();
            JS_ReportOutOfMemory(cx);
            return NS_ERROR_OUT_OF_MEMORY;
        }

        CopyASCIItoUTF16(mDns.data[i].hostname, entry.mHostname);
        entry.mExpiration = mDns.data[i].expiration;

        for (uint32_t j = 0; j < mDns.data[i].hostaddr.Length(); j++) {
            CopyASCIItoUTF16(mDns.data[i].hostaddr[j], *addrs.AppendElement());
        }

        if (mDns.data[i].family == PR_AF_INET6)
            CopyASCIItoUTF16("ipv6", entry.mFamily);
        else
            CopyASCIItoUTF16("ipv4", entry.mFamily);
    }

    JS::RootedValue val(cx);
    if (!dict.ToObject(cx, JS::NullPtr(), &val)) {
        mDns.cb = nullptr;
        mDns.data.Clear();
        return NS_ERROR_FAILURE;
    }
    mDns.cb->OnDashboardDataAvailable(val);
    mDns.cb = nullptr;

    return NS_OK;
}

NS_IMETHODIMP
Dashboard::RequestDNSLookup(const nsACString &aHost, NetDashboardCallback *cb)
{
    if (mDnsup.cb)
        return NS_ERROR_FAILURE;
    nsresult rv;

    if (!mDnsup.serv) {
        mDnsup.serv = do_GetService("@mozilla.org/network/dns-service;1", &rv);
        if (NS_FAILED(rv))
            return rv;
    }

    mDnsup.cb = cb;
    rv = mDnsup.serv->AsyncResolve(aHost, 0, this, NS_GetCurrentThread(), getter_AddRefs(mDnsup.cancel));
    if (NS_FAILED(rv)) {
        mDnsup.cb = nullptr;
        return rv;
    }

    return NS_OK;
}

NS_IMETHODIMP
Dashboard::OnLookupComplete(nsICancelable *aRequest, nsIDNSRecord *aRecord, nsresult aStatus)
{
    MOZ_ASSERT(aRequest == mDnsup.cancel);
    mDnsup.cancel = nullptr;

    AutoSafeJSContext cx;

    mozilla::dom::DNSLookupDict dict;
    dict.mAddress.Construct();

    Sequence<nsString> &addresses = dict.mAddress.Value();

    if (NS_SUCCEEDED(aStatus)) {
        dict.mAnswer = true;
        bool hasMore;
        aRecord->HasMore(&hasMore);
        while(hasMore) {
           nsCString nextAddress;
           aRecord->GetNextAddrAsString(nextAddress);
           CopyASCIItoUTF16(nextAddress, *addresses.AppendElement());
           aRecord->HasMore(&hasMore);
        }
    } else {
        dict.mAnswer = false;
        CopyASCIItoUTF16(GetErrorString(aStatus), dict.mError);
    }

    JS::RootedValue val(cx);
    if (!dict.ToObject(cx, JS::NullPtr(), &val)) {
        mDnsup.cb = nullptr;
        return NS_ERROR_FAILURE;
    }
    mDnsup.cb->OnDashboardDataAvailable(val);
    mDnsup.cb = nullptr;

    return NS_OK;
}

void
HttpConnInfo::SetHTTP1ProtocolVersion(uint8_t pv)
{
    switch (pv) {
    case NS_HTTP_VERSION_0_9:
        protocolVersion.Assign(NS_LITERAL_STRING("http/0.9"));
        break;
    case NS_HTTP_VERSION_1_0:
        protocolVersion.Assign(NS_LITERAL_STRING("http/1.0"));
        break;
    case NS_HTTP_VERSION_1_1:
        protocolVersion.Assign(NS_LITERAL_STRING("http/1.1"));
        break;
    default:
        protocolVersion.Assign(NS_LITERAL_STRING("unknown protocol version"));
    }
}

void
HttpConnInfo::SetHTTP2ProtocolVersion(uint8_t pv)
{
    if (pv == SPDY_VERSION_3) {
        protocolVersion.Assign(NS_LITERAL_STRING("spdy/3"));
    } else if (pv == SPDY_VERSION_31) {
        protocolVersion.Assign(NS_LITERAL_STRING("spdy/3.1"));
    } else {
        MOZ_ASSERT (pv == NS_HTTP2_DRAFT_VERSION);
        protocolVersion.Assign(NS_LITERAL_STRING(NS_HTTP2_DRAFT_TOKEN));
    }
}

NS_IMETHODIMP
Dashboard::RequestConnection(const nsACString& aHost, uint32_t aPort,
                             const char *aProtocol, uint32_t aTimeout,
                             NetDashboardCallback* cb)
{
    nsresult rv;
    mConn.cb = cb;
    mConn.thread = NS_GetCurrentThread();

    rv = TestNewConnection(aHost, aPort, aProtocol, aTimeout);
    if (NS_FAILED(rv)) {
        ConnStatus status;
        CopyASCIItoUTF16(GetErrorString(rv), status.creationSts);
        nsCOMPtr<nsIRunnable> event =
            NS_NewRunnableMethodWithArg<ConnStatus>(this, &Dashboard::GetConnectionStatus, status);
        mConn.thread->Dispatch(event, NS_DISPATCH_NORMAL);
        return rv;
    }

    return NS_OK;
}

nsresult
Dashboard::GetConnectionStatus(ConnStatus aStatus)
{
    AutoSafeJSContext cx;

    mozilla::dom::ConnStatusDict dict;
    dict.mStatus = aStatus.creationSts;

    JS::RootedValue val(cx);
    if (!dict.ToObject(cx, JS::NullPtr(), &val)) {
        mConn.cb = nullptr;
        return NS_ERROR_FAILURE;
    }
    mConn.cb->OnDashboardDataAvailable(val);

    return NS_OK;
}

nsresult
Dashboard::TestNewConnection(const nsACString& aHost, uint32_t aPort,
                             const char *aProtocol, uint32_t aTimeout)
{
    nsresult rv;
    if (!aHost.Length() || !net_IsValidHostName(aHost))
        return NS_ERROR_UNKNOWN_HOST;

    if (aProtocol && NS_LITERAL_STRING("ssl").EqualsASCII(aProtocol))
        rv = gSocketTransportService->CreateTransport(&aProtocol, 1, aHost,
                                                      aPort, nullptr,
                                                      getter_AddRefs(mConn.socket));
    else
        rv = gSocketTransportService->CreateTransport(nullptr, 0, aHost,
                                                      aPort, nullptr,
                                                      getter_AddRefs(mConn.socket));
    if (NS_FAILED(rv))
        return rv;

    rv = mConn.socket->SetEventSink(this, NS_GetCurrentThread());
    if (NS_FAILED(rv))
        return rv;

    rv = mConn.socket->OpenInputStream(nsITransport::OPEN_BLOCKING, 0, 0,
                                       getter_AddRefs(mConn.streamIn));
    if (NS_FAILED(rv))
        return rv;

    StartTimer(aTimeout);

    return rv;
}

NS_IMETHODIMP
Dashboard::OnTransportStatus(nsITransport *aTransport, nsresult aStatus,
                             uint64_t aProgress, uint64_t aProgressMax)
{
    if (aStatus == NS_NET_STATUS_CONNECTED_TO)
        StopTimer();

    ConnStatus status;
    CopyASCIItoUTF16(GetErrorString(aStatus), status.creationSts);
    nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethodWithArg<ConnStatus>(this, &Dashboard::GetConnectionStatus, status);
    mConn.thread->Dispatch(event, NS_DISPATCH_NORMAL);

    return NS_OK;
}

NS_IMETHODIMP
Dashboard::Notify(nsITimer *timer)
{
    if (mConn.socket) {
        mConn.socket->Close(NS_ERROR_ABORT);
        mConn.socket = nullptr;
        mConn.streamIn = nullptr;
    }

    mConn.timer = nullptr;

    ConnStatus status;
    status.creationSts.Assign(NS_LITERAL_STRING("NS_ERROR_NET_TIMEOUT"));
    nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethodWithArg<ConnStatus>(this, &Dashboard::GetConnectionStatus, status);
    mConn.thread->Dispatch(event, NS_DISPATCH_NORMAL);

    return NS_OK;
}

void
Dashboard::StartTimer(uint32_t aTimeout)
{
    if (!mConn.timer)
        mConn.timer = do_CreateInstance("@mozilla.org/timer;1");
    mConn.timer->InitWithCallback(this, aTimeout * 1000, nsITimer::TYPE_ONE_SHOT);
}

void
Dashboard::StopTimer()
{
    if (mConn.timer) {
        mConn.timer->Cancel();
        mConn.timer = nullptr;
    }
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
        if (socketTransportStatuses[i].key == rv)
            return socketTransportStatuses[i].error;

    length = sizeof(errors) / sizeof(ErrorEntry);
    for (int i = 0;i < length;i++)
        if (errors[i].key == rv)
            return errors[i].error;

    return nullptr;
}

} } // namespace mozilla::net
