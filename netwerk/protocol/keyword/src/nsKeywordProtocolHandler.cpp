/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsKeywordProtocolHandler.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsEscape.h"
#include "nsIPref.h"
#include "nsXPIDLString.h"
#include "nsNetCID.h"

static NS_DEFINE_CID(kSimpleURICID,     NS_SIMPLEURI_CID);
static NS_DEFINE_CID(kIOServiceCID,     NS_IOSERVICE_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

////////////////////////////////////////////////////////////////////////////////

nsKeywordProtocolHandler::nsKeywordProtocolHandler() {
    NS_INIT_REFCNT();
}

nsresult
nsKeywordProtocolHandler::Init() {
    nsresult rv = NS_OK;
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID, &rv));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString url;
    rv = prefs->CopyCharPref("keyword.URL", getter_Copies(url));
    // if we can't find a keyword.URL keywords won't work.
    if (NS_FAILED(rv) || !url || !*url) return NS_ERROR_FAILURE;

    mKeywordURL.Assign(url);

    return NS_OK;
}

nsKeywordProtocolHandler::~nsKeywordProtocolHandler() {
}

NS_IMPL_ISUPPORTS1(nsKeywordProtocolHandler, nsIProtocolHandler)

NS_METHOD
nsKeywordProtocolHandler::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult) {
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsKeywordProtocolHandler* ph = new nsKeywordProtocolHandler();
    if (ph == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(ph);
    nsresult rv = ph->Init();
    if (NS_SUCCEEDED(rv)) {
        rv = ph->QueryInterface(aIID, aResult);
    }
    NS_RELEASE(ph);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsKeywordProtocolHandler::GetScheme(char* *result) {
    *result = nsCRT::strdup("keyword");
    if (!*result) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsKeywordProtocolHandler::GetDefaultPort(PRInt32 *result) {
    *result = -1;
    return NS_OK;
}

NS_IMETHODIMP
nsKeywordProtocolHandler::GetProtocolFlags(PRUint32 *result) {
    *result = URI_NORELATIVE | URI_NOAUTH;
    return NS_OK;
}

// digests a spec _without_ the preceeding "keyword:" scheme.
static char *
MangleKeywordIntoHTTPURL(const char *aSpec, const char *aHTTPURL) {
    char * unescaped = nsCRT::strdup(aSpec);
    if(unescaped == nsnull) 
       return nsnull;

    nsUnescape(unescaped);

    // build up a request to the keyword server.
    nsCAutoString query;

    // pull out the "go" action word, or '?', if any
    char one = unescaped[0], two = unescaped[1];
    if (one == '?') {                            // "?blah"
        query = unescaped+1;
    } else if ( (one == 'g' || one == 'G')       //
                            &&                   //
                (two == 'o' || two == 'O')       // "g[G]o[O] blah"
                            &&                   //
                     (unescaped[2] == ' ') ) {      //

        query = unescaped+3;
    } else {
        query = unescaped;
    }

    nsMemory::Free(unescaped);

    query.Trim(" "); // pull leading/trailing spaces.

    // encode
    char * encQuery = nsEscape(query.get(), url_Path);
    if (!encQuery) return nsnull;
    query = encQuery;
    nsMemory::Free(encQuery);

    // prepend the query with the keyword url
    // XXX this url should come from somewhere else
    query.Insert(aHTTPURL, 0);

    return query.ToNewCString();
}

// digests a spec of the form "keyword:blah"
NS_IMETHODIMP
nsKeywordProtocolHandler::NewURI(const char *aSpec, nsIURI *aBaseURI,
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
nsKeywordProtocolHandler::NewChannel(nsIURI* uri, nsIChannel* *result)
{
    nsresult rv;

    NS_ASSERTION((mKeywordURL.Length() > 0), "someone's trying to use the keyword handler even though it hasn't been init'd");

    nsXPIDLCString path;
    rv = uri->GetPath(getter_Copies(path));
    if (NS_FAILED(rv)) return rv;

    char *httpSpec = MangleKeywordIntoHTTPURL(path, mKeywordURL.get());
    if (!httpSpec) return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsIIOService> serv(do_GetService(kIOServiceCID, &rv));
    if (NS_FAILED(rv)) return rv;

    // now we have an HTTP url, give the user an HTTP channel
    rv = serv->NewChannel(httpSpec, nsnull, result);
    nsMemory::Free(httpSpec);
    return rv;

}

NS_IMETHODIMP 
nsKeywordProtocolHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // don't override anything.  
    *_retval = PR_FALSE;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
