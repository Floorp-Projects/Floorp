/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUDPSocketProvider.h"

#include "nspr.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsUDPSocketProvider, nsISocketProvider)

nsUDPSocketProvider::~nsUDPSocketProvider()
{
}

NS_IMETHODIMP 
nsUDPSocketProvider::NewSocket(PRInt32 aFamily,
                               const char *aHost, 
                               PRInt32 aPort, 
                               const char *aProxyHost, 
                               PRInt32 aProxyPort,
                               PRUint32 aFlags,
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
nsUDPSocketProvider::AddToSocket(PRInt32 aFamily,
                                 const char *aHost,
                                 PRInt32 aPort,
                                 const char *aProxyHost,
                                 PRInt32 aProxyPort,
                                 PRUint32 aFlags,
                                 struct PRFileDesc * aFileDesc,
                                 nsISupports **aSecurityInfo)
{
    // does not make sense to strap a UDP socket onto an existing socket
    NS_NOTREACHED("Cannot layer UDP socket on an existing socket");
    return NS_ERROR_UNEXPECTED;
}
