/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
#include "nsDateTimeChannel.h"
#include "nsDateTimeHandler.h"
#include "nsIURL.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIProgressEventSink.h"

static NS_DEFINE_CID(kSimpleURICID,            NS_SIMPLEURI_CID);

////////////////////////////////////////////////////////////////////////////////

nsDateTimeHandler::nsDateTimeHandler() {
    NS_INIT_REFCNT();
}

nsDateTimeHandler::~nsDateTimeHandler() {
}

NS_IMPL_ISUPPORTS1(nsDateTimeHandler, nsIProtocolHandler)

NS_METHOD
nsDateTimeHandler::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult) {

    nsDateTimeHandler* ph = new nsDateTimeHandler();
    if (ph == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(ph);
    nsresult rv = ph->QueryInterface(aIID, aResult);
    NS_RELEASE(ph);
    return rv;
}
    
////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsDateTimeHandler::GetScheme(char* *result) {
    *result = nsCRT::strdup("datetime");
    if (!*result) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsDateTimeHandler::GetDefaultPort(PRInt32 *result) {
    *result = DATETIME_PORT;
    return NS_OK;
}

NS_IMETHODIMP
nsDateTimeHandler::NewURI(const char *aSpec, nsIURI *aBaseURI,
                             nsIURI **result) {
    nsresult rv;

    // no concept of a relative datetime url
    NS_ASSERTION(!aBaseURI, "base url passed into datetime protocol handler");

    nsIURI* url;
    rv = nsComponentManager::CreateInstance(kSimpleURICID, nsnull,
                                            NS_GET_IID(nsIURI),
                                            (void**)&url);
    if (NS_FAILED(rv)) return rv;
    rv = url->SetSpec((char*)aSpec);
    if (NS_FAILED(rv)) {
        NS_RELEASE(url);
        return rv;
    }

    *result = url;
    return rv;
}

NS_IMETHODIMP
nsDateTimeHandler::NewChannel(nsIURI* url, nsIChannel* *result)
{
    nsresult rv;
    
    nsDateTimeChannel* channel;
    rv = nsDateTimeChannel::Create(nsnull, NS_GET_IID(nsIChannel), (void**)&channel);
    if (NS_FAILED(rv)) return rv;

    rv = channel->Init(url);
    if (NS_FAILED(rv)) {
        NS_RELEASE(channel);
        return rv;
    }

    *result = channel;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
