/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsDnsService.h"
#include "nsIDNSListener.h"
#include "nsIRequest.h"
#include "prnetdb.h"


////////////////////////////////////////////////////////////////////////////////

class nsDNSRequest;

class nsDNSLookup
{
public:
    nsresult AddDNSRequest(nsDNSRequest* request);

    const char*     mHostName;
    PRHostEnt       mHostEntry;         // NSPR or platform specific hostent?
    PRIntn          mCount;
    PRBool          mComplete;
    PRIntn          mIndex;             // XXX - for round robin
    void *          mListenerQueue;     // XXX - maintain a list of nsDNSRequests.
};


class nsDNSRequest : public nsIRequest
{
    nsIDNSListener* mListener;
    nsDNSLookup*    mHostNameLookup;

    // nsIRequest methods:
    NS_DECL_NSIREQUEST
};


////////////////////////////////////////////////////////////////////////////////
// nsDNSService methods:

nsDNSService::nsDNSService()
{
    NS_INIT_REFCNT();
}

nsresult
nsDNSService::Init()
{
//    initialize DNS cache (persistent?)
#if defined(XP_MAC)
//    create Open Transport Service Provider for DNS Lookups
#elif defined(_WIN)
//    create DNS EventHandler Window
#elif defined(XP_UNIX)
//    XXXX - ?
#endif

	return NS_OK;
}


nsDNSService::~nsDNSService()
{
//    deallocate cache
#if defined(XP_MAC)
//    deallocate Open Transport Service Provider
#elif defined(_WIN)
//    dispose DNS EventHandler Window
#elif defined(XP_UNIX)
//    XXXX - ?
#endif
}


NS_IMPL_ISUPPORTS(nsDNSService, nsCOMTypeInfo<nsIDNSService>::GetIID());

NS_METHOD
nsDNSService::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
    nsDNSService* ph = new nsDNSService();
    if (ph == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(ph);
    nsresult rv = ph->QueryInterface(aIID, aResult);
    NS_RELEASE(ph);
    return rv;
}
    
////////////////////////////////////////////////////////////////////////////////
// nsIDNSService methods:


NS_IMETHODIMP
nsDNSService::Lookup(nsISupports*    ctxt,
                     const char*     hostname,
                     nsIDNSListener* listener,
                     nsIRequest*     *DNSRequest)
{
    nsresult	rv;
    PRStatus    status;
    nsHostEnt*  hostentry;
/*
    check cache for existing nsDNSLookup with matching hostname
    call OnStartLookup
    if (nsDNSLookup doesn't exist) {
        create nsDNSLookup for this hostname
        kick off DNS Lookup
    }

    if (nsDNSLookup already has at least one address) {
        call OnFound
    }

    if (nsDNSLookup is already complete) {
        call OnStopLookup
        return null
    }
 
    create nsDNSRequest
    queue nsDNSRequest on nsDNSLookup  // XXXX - potential race condition here
    return nsDNSRequest
*/

    // temporary SYNC version
    hostentry = new nsHostEnt;
    if (!hostentry)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = listener->OnStartLookup(ctxt, hostname);
	
	
    status = PR_GetHostByName(hostname, hostentry->buffer, PR_NETDB_BUF_SIZE, &hostentry->hostEnt);
    
    if (PR_SUCCESS == status)
        rv = listener->OnFound(ctxt, hostname, hostentry);	// turn ownership of hostentry over to listener?
    else
        delete hostentry;
	
    rv = listener->OnStopLookup(ctxt, hostname);
	
    return NS_OK;
}
