/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http:mozilla.org/MPL/2.0/. */

#include "nsContentUtils.h"
#include "mozilla/net/Dashboard.h"
#include "mozilla/net/HttpInfo.h"

namespace mozilla {
namespace net {

NS_IMPL_THREADSAFE_ISUPPORTS2(Dashboard, nsIDashboard, nsIDashboardEventNotifier)

#define CREATE_ARRAY_OBJECT(object)                   \
JSObject* object = JS_NewArrayObject(cx, 0, nullptr); \
if (!object)                                          \
    return NS_ERROR_OUT_OF_MEMORY ;                   \

#define SET_ELEMENT(object, func, param, index)       \
if (!JS_DefineElement(cx, object, index, func(param), \
        nullptr, nullptr, JSPROP_ENUMERATE))          \
    return NS_ERROR_OUT_OF_MEMORY;                    \

#define SET_PROPERTY(finalObject, object, property)   \
val = OBJECT_TO_JSVAL(object);                        \
if (!JS_DefineProperty(cx, finalObject, #property,    \
        val, nullptr, nullptr, JSPROP_ENUMERATE))     \
    return NS_ERROR_OUT_OF_MEMORY;                    \

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
    if (gSocketTransportService)
        gSocketTransportService->GetSocketConnections(&mSock.data);
    nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(this, &Dashboard::GetSockets);
    mSock.thread->Dispatch(event, NS_DISPATCH_NORMAL);
}

nsresult
Dashboard::GetSockets()
{
    jsval val;
    JSContext* cx = nsContentUtils::GetSafeJSContext();
    JSAutoRequest request(cx);

    JSObject* finalObject = JS_NewObject(cx, nullptr, nullptr, nullptr);
    if (!finalObject)
        return NS_ERROR_OUT_OF_MEMORY;

    CREATE_ARRAY_OBJECT(hostJs);
    CREATE_ARRAY_OBJECT(portJs);
    CREATE_ARRAY_OBJECT(activeJs);
    CREATE_ARRAY_OBJECT(sentJs);
    CREATE_ARRAY_OBJECT(receivedJs);
    CREATE_ARRAY_OBJECT(tcpJs);
    CREATE_ARRAY_OBJECT(sockSentJs);
    CREATE_ARRAY_OBJECT(sockRecJs);
    mSock.totalSent = 0;
    mSock.totalRecv = 0;

    for (uint32_t i = 0; i < mSock.data.Length(); i++) {
        JSString* hostString = JS_NewStringCopyZ(cx, mSock.data[i].host.get());
        SET_ELEMENT(hostJs, STRING_TO_JSVAL, hostString, i);
        SET_ELEMENT(portJs, INT_TO_JSVAL, mSock.data[i].port, i);
        SET_ELEMENT(activeJs, BOOLEAN_TO_JSVAL, mSock.data[i].active, i);
        SET_ELEMENT(tcpJs, INT_TO_JSVAL, mSock.data[i].tcp, i);
        SET_ELEMENT(sockSentJs, DOUBLE_TO_JSVAL, (double) mSock.data[i].sent, i);
        SET_ELEMENT(sockRecJs, DOUBLE_TO_JSVAL, (double) mSock.data[i].received, i);
        mSock.totalSent += mSock.data[i].sent;
        mSock.totalRecv += mSock.data[i].received;
    }

    SET_ELEMENT(sentJs, DOUBLE_TO_JSVAL, (double) mSock.totalSent, 0);
    SET_ELEMENT(receivedJs, DOUBLE_TO_JSVAL, (double) mSock.totalRecv, 0);

    SET_PROPERTY(finalObject, hostJs, host);
    SET_PROPERTY(finalObject, portJs, port);
    SET_PROPERTY(finalObject, activeJs, active);
    SET_PROPERTY(finalObject, tcpJs, tcp);
    SET_PROPERTY(finalObject, sockSentJs, socksent);
    SET_PROPERTY(finalObject, sockRecJs, sockreceived);
    SET_PROPERTY(finalObject, sentJs,  sent);
    SET_PROPERTY(finalObject, receivedJs, received);

    val = OBJECT_TO_JSVAL(finalObject);
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
    jsval val;
    JSContext* cx = nsContentUtils::GetSafeJSContext();
    JSAutoRequest request(cx);

    JSObject* finalObject = JS_NewObject(cx, nullptr, nullptr, nullptr);
    if (!finalObject)
        return NS_ERROR_OUT_OF_MEMORY;

    CREATE_ARRAY_OBJECT(hostJs);
    CREATE_ARRAY_OBJECT(portJs);
    CREATE_ARRAY_OBJECT(activeJs);
    CREATE_ARRAY_OBJECT(idleJs);
    CREATE_ARRAY_OBJECT(spdyJs);
    CREATE_ARRAY_OBJECT(sslJs);

    for (uint32_t i = 0; i < mHttp.data.Length(); i++) {
        JSString* hostString = JS_NewStringCopyZ(cx, mHttp.data[i].host.get());
        SET_ELEMENT(hostJs, STRING_TO_JSVAL, hostString, i);
        SET_ELEMENT(portJs, INT_TO_JSVAL, mHttp.data[i].port, i);

        JSObject* rtt_Active = JS_NewArrayObject(cx, 0, nullptr);
        JSObject* timeToLive_Active = JS_NewArrayObject(cx, 0, nullptr);
        for (uint32_t j = 0; j < mHttp.data[i].active.Length(); j++) {
            SET_ELEMENT(rtt_Active, INT_TO_JSVAL, mHttp.data[i].active[j].rtt, j);
            SET_ELEMENT(timeToLive_Active, INT_TO_JSVAL, mHttp.data[i].active[j].ttl, j);
        }
        JSObject* active = JS_NewObject(cx, nullptr, nullptr, nullptr);
        SET_PROPERTY(active, rtt_Active, rtt);
        SET_PROPERTY(active, timeToLive_Active, ttl);
        SET_ELEMENT(activeJs, OBJECT_TO_JSVAL, active, i);

        JSObject* rtt_Idle = JS_NewArrayObject(cx, 0, nullptr);
        JSObject* timeToLive_Idle = JS_NewArrayObject(cx, 0, nullptr);
        for (uint32_t j = 0; j < mHttp.data[i].idle.Length(); j++) {
            SET_ELEMENT(rtt_Idle, INT_TO_JSVAL, mHttp.data[i].idle[j].rtt, j);
            SET_ELEMENT(timeToLive_Idle, INT_TO_JSVAL, mHttp.data[i].idle[j].ttl, j);
        }
        JSObject* idle = JS_NewObject(cx, nullptr, nullptr, nullptr);
        SET_PROPERTY(idle, rtt_Idle, rtt);
        SET_PROPERTY(idle, timeToLive_Idle, ttl);
        SET_ELEMENT(idleJs, OBJECT_TO_JSVAL, idle, i);

        SET_ELEMENT(spdyJs, BOOLEAN_TO_JSVAL, mHttp.data[i].spdy, i);
        SET_ELEMENT(sslJs, BOOLEAN_TO_JSVAL, mHttp.data[i].ssl, i);
    }

    SET_PROPERTY(finalObject, hostJs, host);
    SET_PROPERTY(finalObject, portJs, port);
    SET_PROPERTY(finalObject, activeJs, active);
    SET_PROPERTY(finalObject, idleJs, idle);
    SET_PROPERTY(finalObject, spdyJs, spdy);
    SET_PROPERTY(finalObject, sslJs, ssl);

    val = OBJECT_TO_JSVAL(finalObject);
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
    jsval val;
    JSString* jsstring;
    JSContext* cx = nsContentUtils::GetSafeJSContext();
    JSAutoRequest request(cx);

    JSObject* finalObject = JS_NewObject(cx, nullptr, nullptr, nullptr);
    if (!finalObject)
        return NS_ERROR_OUT_OF_MEMORY;

    CREATE_ARRAY_OBJECT(hostJs);
    CREATE_ARRAY_OBJECT(msgSentJs);
    CREATE_ARRAY_OBJECT(msgRecvJs);
    CREATE_ARRAY_OBJECT(sizeSentJs);
    CREATE_ARRAY_OBJECT(sizeRecvJs);
    CREATE_ARRAY_OBJECT(encryptJs);

    mozilla::MutexAutoLock lock(mWs.lock);
    for (uint32_t i = 0; i < mWs.data.Length(); i++) {
        jsstring = JS_NewStringCopyN(cx, mWs.data[i].mHost.get(), mWs.data[i].mHost.Length());
        SET_ELEMENT(hostJs, STRING_TO_JSVAL, jsstring, i);
        SET_ELEMENT(msgSentJs, INT_TO_JSVAL, mWs.data[i].mMsgSent, i);
        SET_ELEMENT(msgRecvJs, INT_TO_JSVAL, mWs.data[i].mMsgReceived, i);
        SET_ELEMENT(sizeSentJs, DOUBLE_TO_JSVAL, (double) mWs.data[i].mSizeSent, i);
        SET_ELEMENT(sizeRecvJs, DOUBLE_TO_JSVAL, (double) mWs.data[i].mSizeReceived, i);
        SET_ELEMENT(encryptJs, BOOLEAN_TO_JSVAL, mWs.data[i].mEncrypted, i);
    }

    SET_PROPERTY(finalObject, hostJs, hostport);
    SET_PROPERTY(finalObject, msgSentJs, msgsent);
    SET_PROPERTY(finalObject, msgRecvJs, msgreceived);
    SET_PROPERTY(finalObject, sizeSentJs, sentsize);
    SET_PROPERTY(finalObject, sizeRecvJs, receivedsize);
    SET_PROPERTY(finalObject, encryptJs, encrypted);

    val = OBJECT_TO_JSVAL(finalObject);
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
    jsval val;
    JSContext* cx = nsContentUtils::GetSafeJSContext();
    JSAutoRequest request(cx);

    JSObject* finalObject = JS_NewObject(cx, nullptr, nullptr, nullptr);
    if (!finalObject)
       return NS_ERROR_OUT_OF_MEMORY;

    CREATE_ARRAY_OBJECT(nameJs);
    CREATE_ARRAY_OBJECT(addrJs);
    CREATE_ARRAY_OBJECT(familyJs);
    CREATE_ARRAY_OBJECT(expiresJs);

    for (uint32_t i = 0; i < mDns.data.Length(); i++) {
        JSString* hostnameString = JS_NewStringCopyZ(cx, mDns.data[i].hostname.get());
        SET_ELEMENT(nameJs, STRING_TO_JSVAL, hostnameString, i);

        JSObject* addrObject = JS_NewObject(cx, nullptr, nullptr, nullptr);
        if (!addrObject)
            return NS_ERROR_OUT_OF_MEMORY;

        for (uint32_t j = 0; j < mDns.data[i].hostaddr.Length(); j++) {
            JSString* addrString = JS_NewStringCopyZ(cx, mDns.data[i].hostaddr[j].get());
            SET_ELEMENT(addrObject, STRING_TO_JSVAL, addrString, j);
        }

        SET_ELEMENT(addrJs, OBJECT_TO_JSVAL, addrObject, i);

        JSString* familyString;
        if (mDns.data[i].family == PR_AF_INET6)
            familyString = JS_NewStringCopyZ(cx, "ipv6");
        else
            familyString = JS_NewStringCopyZ(cx, "ipv4");

        SET_ELEMENT(familyJs, STRING_TO_JSVAL, familyString, i);
        SET_ELEMENT(expiresJs, DOUBLE_TO_JSVAL, (double) mDns.data[i].expiration, i);
    }

    SET_PROPERTY(finalObject, nameJs, hostname);
    SET_PROPERTY(finalObject, addrJs, hostaddr);
    SET_PROPERTY(finalObject, familyJs, family);
    SET_PROPERTY(finalObject, expiresJs, expiration);

    val = OBJECT_TO_JSVAL(finalObject);
    mDns.cb->OnDashboardDataAvailable(val);
    mDns.cb = nullptr;

    return NS_OK;
}

} } // namespace mozilla::net
