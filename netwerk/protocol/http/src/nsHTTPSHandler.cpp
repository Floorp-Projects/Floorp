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
 *      Gagan Saksena <gagan@netscape.com>
 */

#include "nspr.h"
#include "nsHTTPSHandler.h"
#include "nsISocketTransportService.h"
#include "nsIServiceManager.h"
#include "nsISecurityManagerComponent.h"

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);

nsHTTPSHandler::nsHTTPSHandler()
: nsHTTPHandler()
{
}

nsHTTPSHandler::~nsHTTPSHandler()
{
}

nsresult
nsHTTPSHandler::Init() {
    nsresult rv = NS_OK;

    // init nsHTTPHandler *first*
    rv = nsHTTPHandler::Init();
    if (NS_FAILED(rv)) return rv;

    // done to "init" psm.
    nsCOMPtr<nsISupports> psm(do_GetService(PSM_COMPONENT_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;

    if (mScheme) nsCRT::free(mScheme);
    mScheme = nsCRT::strdup("https");
    if (!mScheme) return NS_ERROR_OUT_OF_MEMORY;
    return rv;
}

nsresult nsHTTPSHandler::CreateTransport(const char* host, 
                                         PRInt32 port, 
                                         const char* proxyHost, 
                                         PRInt32 proxyPort, 
                                         nsITransport** o_pTrans)
{
    return CreateTransportOfType(nsnull, host, port, proxyHost, proxyPort, o_pTrans);
}

nsresult nsHTTPSHandler::CreateTransportOfType(const char * type,
                                               const char* host, 
                                               PRInt32 port, 
                                               const char* proxyHost, 
                                               PRInt32 proxyPort, 
                                               nsITransport** o_pTrans)
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
                                        0,
                                        0,
                                        o_pTrans);
}
