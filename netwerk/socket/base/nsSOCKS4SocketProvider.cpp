/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Justin Bradford <jab@atdot.org>
 */

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsSOCKS4SocketProvider.h"
#include "nsSOCKSIOLayer.h"

//////////////////////////////////////////////////////////////////////////

nsSOCKS4SocketProvider::nsSOCKS4SocketProvider()
{
}

nsresult
nsSOCKS4SocketProvider::Init()
{
    nsresult rv = NS_OK;
    return rv;
}

nsSOCKS4SocketProvider::~nsSOCKS4SocketProvider()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsSOCKS4SocketProvider, nsISocketProvider, nsISOCKS4SocketProvider);

NS_METHOD
nsSOCKS4SocketProvider::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    
    nsSOCKS4SocketProvider * inst;
    
    if (NULL == aResult) {
        rv = NS_ERROR_NULL_POINTER;
        return rv;
    }
    *aResult = NULL;
    if (NULL != aOuter) {
        rv = NS_ERROR_NO_AGGREGATION;
        return rv;
    }
    
    NS_NEWXPCOM(inst, nsSOCKS4SocketProvider);
    if (NULL == inst) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        return rv;
    }
    NS_ADDREF(inst);
    rv = inst->QueryInterface(aIID, aResult);
    NS_RELEASE(inst);
    
    return rv;
}

NS_IMETHODIMP
nsSOCKS4SocketProvider::NewSocket(const char *host, 
                                 PRInt32 port,
                                 const char *proxyHost,
                                 PRInt32 proxyPort,
                                 PRFileDesc **_result, 
                                 nsISupports **socksInfo)
{
    nsresult rv = nsSOCKSIOLayerNewSocket(host, 
                                          port,
                                          proxyHost,
                                          proxyPort,
                                          4,          // SOCKS 4
                                          _result, 
                                          socksInfo);
    
    return (NS_FAILED(rv)) ? NS_ERROR_SOCKET_CREATE_FAILED : NS_OK;
}

NS_IMETHODIMP
nsSOCKS4SocketProvider::AddToSocket(const char *host,
                                   PRInt32 port,
                                   const char *proxyHost,
                                   PRInt32 proxyPort,
                                   PRFileDesc *socket, 
                                   nsISupports **socksInfo)
{
    nsresult rv = nsSOCKSIOLayerAddToSocket(host, 
                                            port,
                                            proxyHost,
                                            proxyPort,
                                            4,        // SOCKS 4
                                            socket, 
                                            socksInfo);
    
    return (NS_FAILED(rv)) ? NS_ERROR_SOCKET_CREATE_FAILED : NS_OK;
}
