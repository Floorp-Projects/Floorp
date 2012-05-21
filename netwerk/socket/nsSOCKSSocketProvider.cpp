/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIServiceManager.h"
#include "nsSOCKSSocketProvider.h"
#include "nsSOCKSIOLayer.h"
#include "nsCOMPtr.h"
#include "nsNetError.h"

//////////////////////////////////////////////////////////////////////////

NS_IMPL_THREADSAFE_ISUPPORTS1(nsSOCKSSocketProvider, nsISocketProvider)

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
nsSOCKSSocketProvider::NewSocket(PRInt32 family,
                                 const char *host, 
                                 PRInt32 port,
                                 const char *proxyHost,
                                 PRInt32 proxyPort,
                                 PRUint32 flags,
                                 PRFileDesc **result, 
                                 nsISupports **socksInfo)
{
    PRFileDesc *sock;
    
    sock = PR_OpenTCPSocket(family);
    if (!sock)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = nsSOCKSIOLayerAddToSocket(family,
                                            host, 
                                            port,
                                            proxyHost,
                                            proxyPort,
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
nsSOCKSSocketProvider::AddToSocket(PRInt32 family,
                                   const char *host,
                                   PRInt32 port,
                                   const char *proxyHost,
                                   PRInt32 proxyPort,
                                   PRUint32 flags,
                                   PRFileDesc *sock, 
                                   nsISupports **socksInfo)
{
    nsresult rv = nsSOCKSIOLayerAddToSocket(family,
                                            host, 
                                            port,
                                            proxyHost,
                                            proxyPort,
                                            mVersion,
                                            flags,
                                            sock, 
                                            socksInfo);
    
    if (NS_FAILED(rv))
        rv = NS_ERROR_SOCKET_CREATE_FAILED;
    return rv;
}
