/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http:mozilla.org/MPL/2.0/. */

#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "mozilla/net/Dashboard.h"
#include "mozilla/net/HttpInfo.h"
#include "mozilla/dom/NetDashboardBinding.h"
#include "jsapi.h"

using mozilla::AutoSafeJSContext;
namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS2(Dashboard, nsIDashboard, nsIDashboardEventNotifier)
using mozilla::dom::Sequence;

Dashboard::Dashboard()
{
    mEnableLogging = false;
}

Dashboard::~Dashboard()
{
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
    dict.mHost.Construct();
    dict.mPort.Construct();
    dict.mActive.Construct();
    dict.mTcp.Construct();
    dict.mSocksent.Construct();
    dict.mSockreceived.Construct();
    dict.mSent = 0;
    dict.mReceived = 0;

    Sequence<uint32_t> &ports = dict.mPort.Value();
    Sequence<nsString> &hosts = dict.mHost.Value();
    Sequence<bool> &active = dict.mActive.Value();
    Sequence<uint32_t> &tcp = dict.mTcp.Value();
    Sequence<double> &sent = dict.mSocksent.Value();
    Sequence<double> &received = dict.mSockreceived.Value();

    uint32_t length = mSock.data.Length();
    if (!ports.SetCapacity(length) || !hosts.SetCapacity(length) ||
        !active.SetCapacity(length) || !tcp.SetCapacity(length) ||
        !sent.SetCapacity(length) || !received.SetCapacity(length)) {
            mSock.cb = nullptr;
            mSock.data.Clear();
            JS_ReportOutOfMemory(cx);
            return NS_ERROR_OUT_OF_MEMORY;
    }

    for (uint32_t i = 0; i < mSock.data.Length(); i++) {
        CopyASCIItoUTF16(mSock.data[i].host, *hosts.AppendElement());
        *ports.AppendElement() = mSock.data[i].port;
        *active.AppendElement() = mSock.data[i].active;
        *tcp.AppendElement() = mSock.data[i].tcp;
        *sent.AppendElement() = (double) mSock.data[i].sent;
        *received.AppendElement() = (double) mSock.data[i].received;
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
    dict.mActive.Construct();
    dict.mHost.Construct();
    dict.mIdle.Construct();
    dict.mPort.Construct();
    dict.mSpdy.Construct();
    dict.mSsl.Construct();
    dict.mHalfOpens.Construct();

    using mozilla::dom::HttpConnInfoDict;
    using mozilla::dom::HalfOpenInfoDict;
    Sequence<HttpConnInfoDict> &active = dict.mActive.Value();
    Sequence<nsString> &hosts = dict.mHost.Value();
    Sequence<HttpConnInfoDict> &idle = dict.mIdle.Value();
    Sequence<HalfOpenInfoDict> &halfOpens = dict.mHalfOpens.Value();
    Sequence<uint32_t> &ports = dict.mPort.Value();
    Sequence<bool> &spdy = dict.mSpdy.Value();
    Sequence<bool> &ssl = dict.mSsl.Value();

    uint32_t length = mHttp.data.Length();
    if (!active.SetCapacity(length) || !hosts.SetCapacity(length) ||
        !idle.SetCapacity(length) || !ports.SetCapacity(length) ||
        !spdy.SetCapacity(length) || !ssl.SetCapacity(length) ||
        !halfOpens.SetCapacity(length)) {
            mHttp.cb = nullptr;
            mHttp.data.Clear();
            JS_ReportOutOfMemory(cx);
            return NS_ERROR_OUT_OF_MEMORY;
    }

    for (uint32_t i = 0; i < mHttp.data.Length(); i++) {
        CopyASCIItoUTF16(mHttp.data[i].host,*hosts.AppendElement());
        *ports.AppendElement() = mHttp.data[i].port;
        *spdy.AppendElement() = mHttp.data[i].spdy;
        *ssl.AppendElement() = mHttp.data[i].ssl;
        HttpConnInfoDict &activeInfo = *active.AppendElement();
        activeInfo.mRtt.Construct();
        activeInfo.mTtl.Construct();
        activeInfo.mProtocolVersion.Construct();
        Sequence<uint32_t> &active_rtt = activeInfo.mRtt.Value();
        Sequence<uint32_t> &active_ttl = activeInfo.mTtl.Value();
        Sequence<nsString> &active_protocolVersion = activeInfo.mProtocolVersion.Value();
        if (!active_rtt.SetCapacity(mHttp.data[i].active.Length()) ||
            !active_ttl.SetCapacity(mHttp.data[i].active.Length()) ||
            !active_protocolVersion.SetCapacity(mHttp.data[i].active.Length())) {
                mHttp.cb = nullptr;
                mHttp.data.Clear();
                JS_ReportOutOfMemory(cx);
                return NS_ERROR_OUT_OF_MEMORY;
        }
        for (uint32_t j = 0; j < mHttp.data[i].active.Length(); j++) {
            *active_rtt.AppendElement() = mHttp.data[i].active[j].rtt;
            *active_ttl.AppendElement() = mHttp.data[i].active[j].ttl;
            *active_protocolVersion.AppendElement() = mHttp.data[i].active[j].protocolVersion;
        }

        HttpConnInfoDict &idleInfo = *idle.AppendElement();
        idleInfo.mRtt.Construct();
        idleInfo.mTtl.Construct();
        idleInfo.mProtocolVersion.Construct();
        Sequence<uint32_t> &idle_rtt = idleInfo.mRtt.Value();
        Sequence<uint32_t> &idle_ttl = idleInfo.mTtl.Value();
        Sequence<nsString> &idle_protocolVersion = idleInfo.mProtocolVersion.Value();
        if (!idle_rtt.SetCapacity(mHttp.data[i].idle.Length()) ||
            !idle_ttl.SetCapacity(mHttp.data[i].idle.Length()) ||
            !idle_protocolVersion.SetCapacity(mHttp.data[i].idle.Length())) {
                mHttp.cb = nullptr;
                mHttp.data.Clear();
                JS_ReportOutOfMemory(cx);
                return NS_ERROR_OUT_OF_MEMORY;
        }
        for (uint32_t j = 0; j < mHttp.data[i].idle.Length(); j++) {
            *idle_rtt.AppendElement() = mHttp.data[i].idle[j].rtt;
            *idle_ttl.AppendElement() = mHttp.data[i].idle[j].ttl;
            *idle_protocolVersion.AppendElement() = mHttp.data[i].idle[j].protocolVersion;
        }

        HalfOpenInfoDict &allHalfOpens = *halfOpens.AppendElement();
        allHalfOpens.mSpeculative.Construct();
        Sequence<bool> allHalfOpens_speculative;
        if(!allHalfOpens_speculative.SetCapacity(mHttp.data[i].halfOpens.Length())) {
                mHttp.cb = nullptr;
                mHttp.data.Clear();
                JS_ReportOutOfMemory(cx);
                return NS_ERROR_OUT_OF_MEMORY;
        }
        allHalfOpens_speculative = allHalfOpens.mSpeculative.Value();
        for(uint32_t j = 0; j < mHttp.data[i].halfOpens.Length(); j++) {
            *allHalfOpens_speculative.AppendElement() = mHttp.data[i].halfOpens[j].speculative;
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
    dict.mEncrypted.Construct();
    dict.mHostport.Construct();
    dict.mMsgreceived.Construct();
    dict.mMsgsent.Construct();
    dict.mReceivedsize.Construct();
    dict.mSentsize.Construct();

    Sequence<bool> &encrypted = dict.mEncrypted.Value();
    Sequence<nsString> &hostport = dict.mHostport.Value();
    Sequence<uint32_t> &received = dict.mMsgreceived.Value();
    Sequence<uint32_t> &sent = dict.mMsgsent.Value();
    Sequence<double> &receivedSize = dict.mReceivedsize.Value();
    Sequence<double> &sentSize = dict.mSentsize.Value();

    uint32_t length = mWs.data.Length();
    if (!encrypted.SetCapacity(length) || !hostport.SetCapacity(length) ||
        !received.SetCapacity(length) || !sent.SetCapacity(length) ||
        !receivedSize.SetCapacity(length) || !sentSize.SetCapacity(length)) {
            mWs.cb = nullptr;
            mWs.data.Clear();
            JS_ReportOutOfMemory(cx);
            return NS_ERROR_OUT_OF_MEMORY;
    }

    mozilla::MutexAutoLock lock(mWs.lock);
    for (uint32_t i = 0; i < mWs.data.Length(); i++) {
        CopyASCIItoUTF16(mWs.data[i].mHost, *hostport.AppendElement());
        *sent.AppendElement() = mWs.data[i].mMsgSent;
        *received.AppendElement() = mWs.data[i].mMsgReceived;
        *receivedSize.AppendElement() = mWs.data[i].mSizeSent;
        *sentSize.AppendElement() = mWs.data[i].mSizeReceived;
        *encrypted.AppendElement() = mWs.data[i].mEncrypted;
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
    dict.mExpiration.Construct();
    dict.mFamily.Construct();
    dict.mHostaddr.Construct();
    dict.mHostname.Construct();

    Sequence<double> &expiration = dict.mExpiration.Value();
    Sequence<nsString> &family = dict.mFamily.Value();
    Sequence<Sequence<nsString> > &hostaddr = dict.mHostaddr.Value();
    Sequence<nsString> &hostname = dict.mHostname.Value();

    uint32_t length = mDns.data.Length();
    if (!expiration.SetCapacity(length) || !family.SetCapacity(length) ||
        !hostaddr.SetCapacity(length) || !hostname.SetCapacity(length)) {
            mDns.cb = nullptr;
            mDns.data.Clear();
            JS_ReportOutOfMemory(cx);
            return NS_ERROR_OUT_OF_MEMORY;
    }

    for (uint32_t i = 0; i < mDns.data.Length(); i++) {
        CopyASCIItoUTF16(mDns.data[i].hostname, *hostname.AppendElement());
        *expiration.AppendElement() = mDns.data[i].expiration;

        Sequence<nsString> &addrs = *hostaddr.AppendElement();
        if (!addrs.SetCapacity(mDns.data[i].hostaddr.Length())) {
            mDns.cb = nullptr;
            mDns.data.Clear();
            JS_ReportOutOfMemory(cx);
            return NS_ERROR_OUT_OF_MEMORY;
        }
        for (uint32_t j = 0; j < mDns.data[i].hostaddr.Length(); j++) {
            CopyASCIItoUTF16(mDns.data[i].hostaddr[j], *addrs.AppendElement());
        }

        if (mDns.data[i].family == PR_AF_INET6)
            CopyASCIItoUTF16("ipv6", *family.AppendElement());
        else
            CopyASCIItoUTF16("ipv4", *family.AppendElement());
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
    if (pv == SPDY_VERSION_2)
        protocolVersion.Assign(NS_LITERAL_STRING("spdy/2"));
    else
        protocolVersion.Assign(NS_LITERAL_STRING("spdy/3"));
}

} } // namespace mozilla::net
