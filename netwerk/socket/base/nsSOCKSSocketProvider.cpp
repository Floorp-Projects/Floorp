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
#include "nsSOCKSSocketProvider.h"
#include "nsSOCKSIOLayer.h"

//////////////////////////////////////////////////////////////////////////

nsSOCKSSocketProvider::nsSOCKSSocketProvider()
{
}

nsresult
nsSOCKSSocketProvider::Init()
{
    nsresult rv = NS_OK;
    return rv;
}

nsSOCKSSocketProvider::~nsSOCKSSocketProvider()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsSOCKSSocketProvider, nsISocketProvider, nsISOCKSSocketProvider);

NS_METHOD
nsSOCKSSocketProvider::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    
    nsSOCKSSocketProvider * inst;
    
    if (NULL == aResult) {
        rv = NS_ERROR_NULL_POINTER;
        return rv;
    }
    *aResult = NULL;
    if (NULL != aOuter) {
        rv = NS_ERROR_NO_AGGREGATION;
        return rv;
    }
    
    NS_NEWXPCOM(inst, nsSOCKSSocketProvider);
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
nsSOCKSSocketProvider::NewSocket(const char *host, 
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
                                          5,          // SOCKS 5
                                          _result, 
                                          socksInfo);
    
    return (NS_FAILED(rv)) ? NS_ERROR_SOCKET_CREATE_FAILED : NS_OK;
}

NS_IMETHODIMP
nsSOCKSSocketProvider::AddToSocket(const char *host,
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
                                            5,        // SOCKS 5
                                            socket, 
                                            socksInfo);
    
    return (NS_FAILED(rv)) ? NS_ERROR_SOCKET_CREATE_FAILED : NS_OK;
}
