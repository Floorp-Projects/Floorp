
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Gopher protocol code.
 *
 * The Initial Developer of the Original Code is
 * Bradley Baetz.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bradley Baetz <bbaetz@student.usyd.edu.au>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// This is based rather heavily on the datetime and finger implementations
// and documentation

#include "nspr.h"
#include "nsGopherChannel.h"
#include "nsGopherHandler.h"
#include "nsIURL.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProgressEventSink.h"
#include "nsIStandardURL.h"
#include "nsNetUtil.h"
#include "prlog.h"

#ifdef PR_LOGGING
PRLogModuleInfo* gGopherLog = nsnull;
#endif

static NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);

////////////////////////////////////////////////////////////////////////////////

nsGopherHandler::nsGopherHandler() {
#ifdef PR_LOGGING
    if (!gGopherLog)
        gGopherLog = PR_NewLogModule("nsGopherProtocol");
#endif
}

nsGopherHandler::~nsGopherHandler() {
    PR_LOG(gGopherLog, PR_LOG_ALWAYS, ("~nsGopherHandler() called"));
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsGopherHandler,
                              nsIProxiedProtocolHandler,
                              nsIProtocolHandler)

NS_METHOD
nsGopherHandler::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult) {
    nsGopherHandler* gh = new nsGopherHandler();
    if (!gh)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(gh);
    nsresult rv = gh->QueryInterface(aIID, aResult);
    NS_RELEASE(gh);
    return rv;
}
    
////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsGopherHandler::GetScheme(nsACString &result) {
    result = "gopher";
    return NS_OK;
}

NS_IMETHODIMP
nsGopherHandler::GetDefaultPort(PRInt32 *result) {
    *result = GOPHER_PORT;
    return NS_OK;
}

NS_IMETHODIMP
nsGopherHandler::GetProtocolFlags(PRUint32 *result) {
    *result = URI_NORELATIVE | ALLOWS_PROXY | ALLOWS_PROXY_HTTP;
    return NS_OK;
}

NS_IMETHODIMP
nsGopherHandler::NewURI(const nsACString &aSpec,
                        const char *aCharset,
                        nsIURI *aBaseURI,
                        nsIURI **result) {
    nsresult rv;

    nsCOMPtr<nsIStandardURL> url = do_CreateInstance(kStandardURLCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = url->Init(nsIStandardURL::URLTYPE_STANDARD, GOPHER_PORT,
                   aSpec, aCharset, aBaseURI);
    if (NS_FAILED(rv)) return rv;

    return CallQueryInterface(url, result);
}

NS_IMETHODIMP
nsGopherHandler::NewProxiedChannel(nsIURI* url, nsIProxyInfo* proxyInfo,
                                   nsIChannel* *result)
{
    nsGopherChannel *chan = new nsGopherChannel();
    if (!chan)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(chan);
    
    nsresult rv = chan->Init(url, proxyInfo);
    if (NS_FAILED(rv)) {
        NS_RELEASE(chan);
        return rv;
    }

    *result = chan;
    return rv;
}

NS_IMETHODIMP
nsGopherHandler::NewChannel(nsIURI* url, nsIChannel* *result)
{
    return NewProxiedChannel(url, nsnull, result);
}

NS_IMETHODIMP 
nsGopherHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    if (port == GOPHER_PORT)
        *_retval = PR_TRUE;
    else
        *_retval = PR_FALSE;
    return NS_OK;
}
