/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHttpConnectionInfo.h"
#include "nsIProtocolProxyService.h"

void
nsHttpConnectionInfo::SetOriginServer(const nsACString &host, PRInt32 port)
{
    mHost = host;
    mPort = port == -1 ? DefaultPort() : port;

    //
    // build hash key:
    //
    // the hash key uniquely identifies the connection type.  two connections
    // are "equal" if they end up talking the same protocol to the same server
    // and are both used for anonymous or non-anonymous connection only;
    // anonymity of the connection is setup later from nsHttpChannel::AsyncOpen
    // where we know we use anonymous connection (LOAD_ANONYMOUS load flag)
    //

    const char *keyHost;
    PRInt32 keyPort;

    if (mUsingHttpProxy && !mUsingSSL) {
        keyHost = ProxyHost();
        keyPort = ProxyPort();
    }
    else {
        keyHost = Host();
        keyPort = Port();
    }

    mHashKey.AssignLiteral("....");
    mHashKey.Append(keyHost);
    mHashKey.Append(':');
    mHashKey.AppendInt(keyPort);

    if (mUsingHttpProxy)
        mHashKey.SetCharAt('P', 0);
    if (mUsingSSL)
        mHashKey.SetCharAt('S', 1);

    // NOTE: for transparent proxies (e.g., SOCKS) we need to encode the proxy
    // type in the hash key (this ensures that we will continue to speak the
    // right protocol even if our proxy preferences change).
    if (!mUsingHttpProxy && ProxyHost()) {
        mHashKey.AppendLiteral(" (");
        mHashKey.Append(ProxyType());
        mHashKey.Append(')');
    }
}

nsHttpConnectionInfo*
nsHttpConnectionInfo::Clone() const
{
    nsHttpConnectionInfo* clone = new nsHttpConnectionInfo(mHost, mPort, mProxyInfo, mUsingSSL);

    // Make sure the anonymous and private flags are transferred!
    clone->SetAnonymous(GetAnonymous());
    clone->SetPrivate(GetPrivate());

    return clone;
}

bool
nsHttpConnectionInfo::ShouldForceConnectMethod()
{
    if (!mProxyInfo)
        return false;
    
    PRUint32 resolveFlags;
    nsresult rv;
    
    rv = mProxyInfo->GetResolveFlags(&resolveFlags);
    if (NS_FAILED(rv))
        return false;

    return resolveFlags & nsIProtocolProxyService::RESOLVE_ALWAYS_TUNNEL;
}
