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
#include "nsIDnsListener.h"
#include "nsICancelable.h"
#include "prnetdb.h"


////////////////////////////////////////////////////////////////////////////////

class nsDnsRequest;

class nsDnsLookup
{
public:
    nsresult AddDnsRequest(nsDnsRequest* request);

    const char*     mHostName;
    PRHostEnt       mHostEntry;         // NSPR or platform specific hostent?
    PRIntn          mCount;
    PRBool          mComplete;
    PRIntn          mIndex;             // XXX - for round robin
    void *          mListenerQueue;     // XXX - maintain a list of nsDnsRequests.
};


class nsDnsRequest : public nsICancelable
{
    nsIDnsListener* mListener;
    nsDnsLookup*    mHostNameLookup;

    // nsICancelable methods:
    NS_IMETHOD Cancel(void);
    NS_IMETHOD Suspend(void);
    NS_IMETHOD Resume(void);
};


////////////////////////////////////////////////////////////////////////////////
// nsDnsService methods:

nsDnsService::nsDnsService()
{
    NS_INIT_REFCNT();
}

nsresult
nsDnsService::Init()
{
//    initialize dns cache (persistent?)
#if defined(XP_MAC)
//    create Open Transport Service Provider for DNS Lookups
#elif defined(_WIN)
//    create DNS EventHandler Window
#elif define(XP_UNIX)
//    XXXX - ?
#endif

	return NS_OK;
}


nsDnsService::~nsDnsService()
{
//    deallocate cache
#if defined(XP_MAC)
//    deallocate Open Transport Service Provider
#elif defined(_WIN)
//    dispose DNS EventHandler Window
#elif define(XP_UNIX)
//    XXXX - ?
#endif
}


NS_IMPL_ISUPPORTS(nsDnsService, nsIDnsService::GetIID());

////////////////////////////////////////////////////////////////////////////////
// nsIDnsService methods:


NS_IMETHODIMP
nsDnsService::Lookup(const char*     hostname,
                     nsIDnsListener* listener,
                     nsICancelable*  *dnsRequest)
{
	nsresult	rv;
	PRStatus    status;
	nsHostEnt*  hostentry;
/*
    check cache for existing nsDnsLookup with matching hostname
    call OnStartLookup
    if (nsDnsLookup doesn't exist) {
        create nsDnsLookup for this hostname
        kick off DNS Lookup
    }

    if (nsDnsLookup already has at least one address) {
        call OnFound
    }

    if (nsDnsLookup is already complete) {
        call OnStopLookup
        return null
    }
 
    create nsDnsRequest
    queue nsDnsRequest on nsDnsLookup  // XXXX - potential race condition here
    return nsDnsRequest
*/

	// temporary SYNC version
    hostentry = new nsHostEnt;
    if (!hostentry)
        return NS_ERROR_OUT_OF_MEMORY;

	rv = listener->OnStartLookup(hostname);
	
	
    status = PR_GetHostByName(hostname, hostentry->buffer, PR_NETDB_BUF_SIZE, &hostentry->hostEnt);
    
    if (PR_SUCCESS == status)
	    rv = listener->OnFound(hostname, hostentry);	// turn ownership of hostentry over to listener?
	else
		delete hostentry;
	
	rv = listener->OnStopLookup(hostname);
	
	return NS_OK;
}

