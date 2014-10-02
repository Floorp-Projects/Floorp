/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpConnectionInfo_h__
#define nsHttpConnectionInfo_h__

#include "nsHttp.h"
#include "nsProxyInfo.h"
#include "nsCOMPtr.h"
#include "nsStringFwd.h"

extern PRLogModuleInfo *gHttpLog;

//-----------------------------------------------------------------------------
// nsHttpConnectionInfo - holds the properties of a connection
//-----------------------------------------------------------------------------

// http:// uris through a proxy will all share the same CI, because they can
// all use the same connection. (modulo pb and anonymous flags). They just use
// the proxy as the origin host name.
// however, https:// uris tunnel through the proxy so they will have different
// CIs - the CI reflects both the proxy and the origin.
// however, proxy conenctions made with http/2 (or spdy) can tunnel to the origin
// and multiplex non tunneled transactions at the same time, so they have a
// special wildcard CI that accepts all origins through that proxy.

namespace mozilla { namespace net {

class nsHttpConnectionInfo
{
public:
    nsHttpConnectionInfo(const nsACString &host, int32_t port,
                         const nsACString &username,
                         nsProxyInfo* proxyInfo,
                         bool endToEndSSL = false);

private:
    virtual ~nsHttpConnectionInfo()
    {
        PR_LOG(gHttpLog, 4, ("Destroying nsHttpConnectionInfo @%x\n", this));
    }

public:
    const nsAFlatCString &HashKey() const { return mHashKey; }

    void SetOriginServer(const nsACString &host, int32_t port);

    void SetOriginServer(const char *host, int32_t port)
    {
        SetOriginServer(nsDependentCString(host), port);
    }

    // OK to treat these as an infalible allocation
    nsHttpConnectionInfo* Clone() const;
    nsresult CreateWildCard(nsHttpConnectionInfo **outParam);

    const char *ProxyHost() const { return mProxyInfo ? mProxyInfo->Host().get() : nullptr; }
    int32_t     ProxyPort() const { return mProxyInfo ? mProxyInfo->Port() : -1; }
    const char *ProxyType() const { return mProxyInfo ? mProxyInfo->Type() : nullptr; }

    // Compare this connection info to another...
    // Two connections are 'equal' if they end up talking the same
    // protocol to the same server. This is needed to properly manage
    // persistent connections to proxies
    // Note that we don't care about transparent proxies -
    // it doesn't matter if we're talking via socks or not, since
    // a request will end up at the same host.
    bool Equals(const nsHttpConnectionInfo *info)
    {
        return mHashKey.Equals(info->HashKey());
    }

    const char   *Host() const           { return mHost.get(); }
    int32_t       Port() const           { return mPort; }
    const char   *Username() const       { return mUsername.get(); }
    nsProxyInfo  *ProxyInfo()            { return mProxyInfo; }
    int32_t       DefaultPort() const    { return mEndToEndSSL ? NS_HTTPS_DEFAULT_PORT : NS_HTTP_DEFAULT_PORT; }
    void          SetAnonymous(bool anon)
                                         { mHashKey.SetCharAt(anon ? 'A' : '.', 2); }
    bool          GetAnonymous() const   { return mHashKey.CharAt(2) == 'A'; }
    void          SetPrivate(bool priv)  { mHashKey.SetCharAt(priv ? 'P' : '.', 3); }
    bool          GetPrivate() const     { return mHashKey.CharAt(3) == 'P'; }

    const nsCString &GetHost() { return mHost; }

    // Returns true for any kind of proxy (http, socks, https, etc..)
    bool UsingProxy();

    // Returns true when proxying over HTTP or HTTPS
    bool UsingHttpProxy() const { return mUsingHttpProxy || mUsingHttpsProxy; }

    // Returns true when proxying over HTTPS
    bool UsingHttpsProxy() const { return mUsingHttpsProxy; }

    // Returns true when a resource is in SSL end to end (e.g. https:// uri)
    bool EndToEndSSL() const { return mEndToEndSSL; }

    // Returns true when at least first hop is SSL (e.g. proxy over https or https uri)
    bool FirstHopSSL() const { return mEndToEndSSL || mUsingHttpsProxy; }

    // Returns true when CONNECT is used to tunnel through the proxy (e.g. https:// or ws://)
    bool UsingConnect() const { return mUsingConnect; }

    // Returns true when mHost is an RFC1918 literal.
    bool HostIsLocalIPLiteral() const;

private:
    nsCString              mHashKey;
    nsCString              mHost;
    int32_t                mPort;
    nsCString              mUsername;
    nsCOMPtr<nsProxyInfo>  mProxyInfo;
    bool                   mUsingHttpProxy;
    bool                   mUsingHttpsProxy;
    bool                   mEndToEndSSL;
    bool                   mUsingConnect;  // if will use CONNECT with http proxy

// for nsRefPtr
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(nsHttpConnectionInfo)
};

}} // namespace mozilla::net

#endif // nsHttpConnectionInfo_h__
