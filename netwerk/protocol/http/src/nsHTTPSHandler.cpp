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
 */

#include "nspr.h"
#include "nsHTTPSHandler.h"
#include "nsISocketTransportService.h"
#include "nsIServiceManager.h"
#include "nsIPSMComponent.h"

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);

nsHTTPSHandler::nsHTTPSHandler()
: nsHTTPHandler()
{
    nsresult rv;
    nsISupports *psm = nsnull;
    //
    // Initialize the PSM component.
    // This is to ensure that PSM is initialized on the main UI thread.
    // 
    rv = nsServiceManager::GetService( PSM_COMPONENT_CONTRACTID,
                                       NS_GET_IID(nsIPSMComponent), 
                                       (nsISupports**)&psm);  

    NS_IF_RELEASE(psm);
}

nsHTTPSHandler::~nsHTTPSHandler()
{
}

NS_METHOD
nsHTTPSHandler::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    if (aOuter) return NS_ERROR_NO_AGGREGATION;

    nsHTTPSHandler* handler = new nsHTTPSHandler();
    if (!handler) return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(handler);
    rv = handler->Init();
    if (NS_FAILED(rv)) {
        delete handler;
        return rv;
    }
    rv = handler->QueryInterface(aIID, aResult);
    NS_RELEASE(handler);
    return rv;
}

nsresult nsHTTPSHandler::CreateTransport(const char* host, 
                                         PRInt32 port, 
                                         const char* proxyHost, 
                                         PRInt32 proxyPort, 
                                         PRUint32 bufferSegmentSize, 
                                         PRUint32 bufferMaxSize,
                                         nsIChannel** o_pTrans)
{
    return CreateTransportOfType(nsnull, host, port, proxyHost, proxyPort,
                                 bufferSegmentSize, bufferMaxSize, o_pTrans);
}

nsresult nsHTTPSHandler::CreateTransportOfType(const char * type,
                                               const char* host, 
                                               PRInt32 port, 
                                               const char* proxyHost, 
                                               PRInt32 proxyPort, 
                                               PRUint32 bufferSegmentSize, 
                                               PRUint32 bufferMaxSize,
                                               nsIChannel** o_pTrans)
{
    nsresult rv;
    
    NS_WITH_SERVICE(nsISocketTransportService, sts, 
                    kSocketTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    const char * types[] = { "ssl", type };
    
    return sts->CreateTransportOfTypes( 2, types,
                                        host, 
                                        port, 
                                        proxyHost, 
                                        proxyPort, 
                                        bufferSegmentSize, 
                                        bufferMaxSize, 
                                        o_pTrans);
}
