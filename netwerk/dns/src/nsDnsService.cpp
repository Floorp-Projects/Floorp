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

#include "nsString2.h"

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


NS_IMPL_ISUPPORTS(nsDNSService, NS_GET_IID(nsIDNSService));

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

//
// Allocate space from the buffer, aligning it to "align" before doing
// the allocation. "align" must be a power of 2.
// NOTE: this code was taken from NSPR.
static char *BufAlloc(PRIntn amount, char **bufp, PRIntn *buflenp, PRIntn align) {
	char *buf = *bufp;
	PRIntn buflen = *buflenp;

	if (align && ((long)buf & (align - 1))) {
		PRIntn skip = align - ((ptrdiff_t)buf & (align - 1));
		if (buflen < skip) {
			return 0;
		}
		buf += skip;
		buflen -= skip;
	}
	if (buflen < amount) {
		return 0;
	}
	*bufp = buf + amount;
	*buflenp = buflen - amount;
	return buf;
}

NS_IMETHODIMP
nsDNSService::Lookup(nsISupports*    ctxt,
                     const char*     hostname,
                     nsIDNSListener* listener,
                     nsIRequest*     *DNSRequest)
{
    nsresult	rv, result = NS_OK;
    PRStatus    status = PR_SUCCESS;
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
	
    PRBool numeric = PR_TRUE;
    for (const char *hostCheck = hostname; *hostCheck; hostCheck++) {
        if (!nsString2::IsDigit(*hostCheck) && (*hostCheck != '.') ) {
            numeric = PR_FALSE;
            break;
        }
    }

    PRNetAddr *netAddr;

    if (numeric) {
        // If it is numeric then try to convert it into an IP-Address
        netAddr = (PRNetAddr*)nsAllocator::Alloc(sizeof(PRNetAddr));
        status = PR_StringToNetAddr(hostname, netAddr);
        if (PR_SUCCESS == status) {
            // If it is numeric and could be converted then try to
            // look it up, otherwise try it as normal hostname
            status = PR_GetHostByAddr(netAddr, 
                                      hostentry->buffer, 
                                      PR_NETDB_BUF_SIZE, 
                                      &hostentry->hostEnt);
            if (PR_FAILURE == status) {
                // reverse lookup failed. slam the IP in and move on.
                PRHostEnt *ent = &(hostentry->hostEnt);
                PRIntn bufLen = PR_NETDB_BUF_SIZE;
                char *buffer = hostentry->buffer;
                ent->h_name = (char*)BufAlloc(PL_strlen(hostname) + 1,
                                           &buffer,
                                           &bufLen,
                                           0);
                memcpy(ent->h_name, hostname, PL_strlen(hostname) + 1);

                ent->h_aliases = (char**)BufAlloc(1 * sizeof(char*),
                                               &buffer,
                                               &bufLen,
                                               sizeof(char **));
                ent->h_aliases[0] = '\0';

                ent->h_addrtype = 2;
                ent->h_length = 4;
                ent->h_addr_list = (char**)BufAlloc(2 * sizeof(char*),
                                                 &buffer,
                                                 &bufLen,
                                                 sizeof(char **));
                ent->h_addr_list[0] = (char*)BufAlloc(ent->h_length,
                                                   &buffer,
                                                   &bufLen,
                                                   0);
                memcpy(ent->h_addr_list[0], &netAddr->inet.ip, ent->h_length);
                ent->h_addr_list[1] = '\0';
                status = PR_SUCCESS;
            }
        } else {
            // If the hostname is numeric, but we couldn't create
            // a net addr out of it, we're dealing w/ a purely numeric
            // address (no dots), try a regular gethostbyname on it.
            status = PR_GetHostByName(hostname, 
                                      hostentry->buffer, 
                                      PR_NETDB_BUF_SIZE, 
                                      &hostentry->hostEnt);
        }
        nsAllocator::Free(netAddr);
    } else {
        status = PR_GetHostByName(hostname, 
                                  hostentry->buffer, 
                                  PR_NETDB_BUF_SIZE, 
                                  &hostentry->hostEnt);
    }
    
    if (PR_SUCCESS == status) {
        rv = listener->OnFound(ctxt, hostname, hostentry);
        result = NS_OK;
    }
    else {
        result = NS_ERROR_UNKNOWN_HOST;
    }
    // XXX: The hostentry should really be reference counted so the 
    //      listener does not need to copy it...
    delete hostentry;
	
    rv = listener->OnStopLookup(ctxt, hostname, result);
	
    return result;
}
