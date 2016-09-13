/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIServiceManager.h"
#include "nsNamedPipeIOLayer.h"
#include "nsSOCKSSocketProvider.h"
#include "nsSOCKSIOLayer.h"
#include "nsCOMPtr.h"
#include "nsError.h"

//////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS(nsSOCKSSocketProvider, nsISocketProvider)

nsresult
nsSOCKSSocketProvider::CreateV4(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    nsCOMPtr<nsISocketProvider> inst =
            new nsSOCKSSocketProvider(NS_SOCKS_VERSION_4);
    if (!inst)
        rv = NS_ERROR_OUT_OF_MEMORY;
    else
        rv = inst->QueryInterface(aIID, aResult); 
    return rv;
}

nsresult
nsSOCKSSocketProvider::CreateV5(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    nsCOMPtr<nsISocketProvider> inst =
            new nsSOCKSSocketProvider(NS_SOCKS_VERSION_5);
    if (!inst)
        rv = NS_ERROR_OUT_OF_MEMORY;
    else
        rv = inst->QueryInterface(aIID, aResult); 
    return rv;
}

NS_IMETHODIMP
nsSOCKSSocketProvider::NewSocket(int32_t family,
                                 const char *host, 
                                 int32_t port,
                                 nsIProxyInfo *proxy,
                                 const nsACString &firstPartyDomain,
                                 uint32_t flags,
                                 PRFileDesc **result,
                                 nsISupports **socksInfo)
{
    PRFileDesc *sock;

#if defined(XP_WIN)
    nsAutoCString proxyHost;
    proxy->GetHost(proxyHost);
    if (IsNamedPipePath(proxyHost)) {
        sock = CreateNamedPipeLayer();
    } else
#endif
    {
        sock = PR_OpenTCPSocket(family);
        if (!sock) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    nsresult rv = nsSOCKSIOLayerAddToSocket(family,
                                            host, 
                                            port,
                                            proxy,
                                            mVersion,
                                            flags,
                                            sock, 
                                            socksInfo);
    if (NS_SUCCEEDED(rv)) {
        *result = sock;
        return NS_OK;
    }

    return NS_ERROR_SOCKET_CREATE_FAILED;
}

NS_IMETHODIMP
nsSOCKSSocketProvider::AddToSocket(int32_t family,
                                   const char *host,
                                   int32_t port,
                                   nsIProxyInfo *proxy,
                                   const nsACString &firstPartyDomain,
                                   uint32_t flags,
                                   PRFileDesc *sock,
                                   nsISupports **socksInfo)
{
    nsresult rv = nsSOCKSIOLayerAddToSocket(family,
                                            host, 
                                            port,
                                            proxy,
                                            mVersion,
                                            flags,
                                            sock, 
                                            socksInfo);
    
    if (NS_FAILED(rv))
        rv = NS_ERROR_SOCKET_CREATE_FAILED;
    return rv;
}
