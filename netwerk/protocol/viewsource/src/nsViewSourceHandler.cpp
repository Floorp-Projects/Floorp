/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Chak Nanga <chak@netscape.com>
 */

#include "nsViewSourceHandler.h"
#include "nsViewSourceChannel.h"
#include "nsIURL.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsXPIDLString.h"

static NS_DEFINE_CID(kSimpleURICID,     NS_SIMPLEURI_CID);

////////////////////////////////////////////////////////////////////////////////

nsViewSourceHandler::nsViewSourceHandler() {
    NS_INIT_REFCNT();
}

nsViewSourceHandler::~nsViewSourceHandler() {
}

NS_IMPL_ISUPPORTS1(nsViewSourceHandler, nsIProtocolHandler)

NS_METHOD
nsViewSourceHandler::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult) {
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsViewSourceHandler* ph = new nsViewSourceHandler();
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
nsViewSourceHandler::GetScheme(char* *result) {
    *result = nsCRT::strdup("view-source");
    if (!*result) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsViewSourceHandler::GetDefaultPort(PRInt32 *result) {
    *result = -1;
    return NS_OK;
}

NS_IMETHODIMP
nsViewSourceHandler::GetProtocolFlags(PRUint32 *result) {
    *result = URI_NORELATIVE | URI_NOAUTH;
    return NS_OK;
}

NS_IMETHODIMP
nsViewSourceHandler::NewURI(const char *aSpec, nsIURI *aBaseURI,
                               nsIURI **result) {
    nsresult rv;
    nsIURI* uri;

    rv = nsComponentManager::CreateInstance(kSimpleURICID, nsnull, NS_GET_IID(nsIURI), (void**)&uri);
    if (NS_FAILED(rv)) return rv;
    rv = uri->SetSpec((char*)aSpec);
    if (NS_FAILED(rv)) return rv;

    *result = uri;
    return rv;
}

NS_IMETHODIMP
nsViewSourceHandler::NewChannel(nsIURI* uri, nsIChannel* *result)
{
    nsresult rv;
    
    nsViewSourceChannel* channel;
    rv = nsViewSourceChannel::Create(nsnull, NS_GET_IID(nsIChannel), (void**)&channel);
    if (NS_FAILED(rv)) return rv;

    rv = channel->Init(uri);
    if (NS_FAILED(rv)) {
        NS_RELEASE(channel);
        return rv;
    }

    *result = channel;
    return NS_OK;
}

NS_IMETHODIMP 
nsViewSourceHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // don't override anything.  
    *_retval = PR_FALSE;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
