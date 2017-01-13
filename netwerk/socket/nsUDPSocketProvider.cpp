/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUDPSocketProvider.h"

#include "nspr.h"

using mozilla::OriginAttributes;

NS_IMPL_ISUPPORTS(nsUDPSocketProvider, nsISocketProvider)

nsUDPSocketProvider::~nsUDPSocketProvider()
{
}

NS_IMETHODIMP 
nsUDPSocketProvider::NewSocket(int32_t aFamily,
                               const char *aHost, 
                               int32_t aPort, 
                               nsIProxyInfo *aProxy,
                               const OriginAttributes &originAttributes,
                               uint32_t aFlags,
                               PRFileDesc * *aFileDesc, 
                               nsISupports **aSecurityInfo)
{
    NS_ENSURE_ARG_POINTER(aFileDesc);
  
    PRFileDesc* udpFD = PR_OpenUDPSocket(aFamily);
    if (!udpFD)
        return NS_ERROR_FAILURE;
  
    *aFileDesc = udpFD;
    return NS_OK;
}

NS_IMETHODIMP 
nsUDPSocketProvider::AddToSocket(int32_t aFamily,
                                 const char *aHost,
                                 int32_t aPort,
                                 nsIProxyInfo *aProxy,
                                 const OriginAttributes &originAttributes,
                                 uint32_t aFlags,
                                 struct PRFileDesc * aFileDesc,
                                 nsISupports **aSecurityInfo)
{
    // does not make sense to strap a UDP socket onto an existing socket
    NS_NOTREACHED("Cannot layer UDP socket on an existing socket");
    return NS_ERROR_UNEXPECTED;
}
