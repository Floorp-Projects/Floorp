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

#include "nsCOMPtr.h"
#include "nsDnsService.h"
#include "nsIDNSListener.h"
#include "nsIRequest.h"
#include "nsISupportsArray.h"
#include "nsError.h"
#include "prnetdb.h"

#include "nsString2.h"


#if defined(XP_MAC)
#include "pprthred.h"

static pascal void  DNSNotifierRoutine(void * contextPtr, OTEventCode code, OTResult result, void * cookie);

#if TARGET_CARBON

#define nsDNS_NOTIFIER_ROUTINE	nsDnsServiceNotifierRoutineUPP
#define INIT_OPEN_TRANSPORT()	InitOpenTransport(mClientContext, kInitOTForExtensionMask)
#define OT_OPEN_INTERNET_SERVICES(config, flags, err)	OTOpenInternetServices(config, flags, err, mClientContext)
#define OT_OPEN_ENDPOINT(config, flags, info, err)		OTOpenEndpoint(config, flags, info, err, mClientContext)

#else

#define nsDNS_NOTIFIER_ROUTINE	nsDnsServiceNotifierRoutine
#define INIT_OPEN_TRANSPORT()	InitOpenTransport()
#define OT_OPEN_INTERNET_SERVICES(config, flags, err)	OTOpenInternetServices(config, flags, err)
#define OT_OPEN_ENDPOINT(config, flags, info, err)		OTOpenEndpoint(config, flags, info, err)
#endif /* TARGET_CARBON */
#endif /* XP_MAC */

////////////////////////////////////////////////////////////////////////////////

class nsDNSRequest;
class nsDNSLookup;


#if defined(XP_MAC)

typedef struct nsInetHostInfo {
    InetHostInfo    hostInfo;
    nsDNSLookup *   lookup;
} nsInetHostInfo;

typedef struct nsLookupElement {
	QElem	         qElem;
	nsDNSLookup *    lookup;
	
} nsLookupElement;

#endif

class nsDNSLookup : public nsISupports
{
public:
	NS_DECL_ISUPPORTS

	nsDNSLookup(nsISupports * clientContext, const char * hostName, nsIDNSListener* listener);
    ~nsDNSLookup(void);

    nsresult            AddDNSRequest(nsDNSRequest* request);
    nsresult            FinishHostEntry(void);
    nsresult            CallOnFound(void);
    const char *        HostName(void)  { return mHostName; }
    nsresult            InitiateDNSLookup(nsDNSService * dnsService);


protected:
#ifdef XP_MAC
    friend pascal void  nsDnsServiceNotifierRoutine(void * contextPtr, OTEventCode code, OTResult result, void * cookie);
#endif
    char *                      mHostName;
    nsHostEnt                   mHostEntry;
    PRBool                      mComplete;
    nsresult                    mResult;

    PRIntn                      mCount;
    PRIntn                      mIndex;            // XXX - for round robin
    nsCOMPtr<nsISupportsArray>  mRequestQueue;     // XXX - maintain a list of nsDNSRequests.

#if defined(XP_MAC)
    nsLookupElement     mLookupElement;
    nsInetHostInfo      mInetHostInfo;
#endif
    nsIDNSListener *    mListener;        // belongs with nsDNSRequest
    nsISupports *       mContext;         // belongs with nsDNSRequest
};

NS_IMPL_ISUPPORTS1(nsDNSLookup, nsISupports)


class nsDNSRequest : public nsIRequest
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST

protected:
    nsCOMPtr<nsIDNSListener>    mListener;
    nsCOMPtr<nsISupports>       mContext;
    nsDNSLookup*                mHostNameLookup;
};

NS_IMPL_ISUPPORTS1(nsDNSRequest, nsIRequest)


////////////////////////////////////////////////////////////////////////////////
// utility routines:
////////////////////////////////////////////////////////////////////////////////
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


////////////////////////////////////////////////////////////////////////////////
// nsDNSLookup methods:
////////////////////////////////////////////////////////////////////////////////

nsDNSLookup::nsDNSLookup(nsISupports * clientContext, const char * hostName, nsIDNSListener* listener)
{
    mHostName = new char [PL_strlen(hostName) + 1];
    PL_strcpy(mHostName, hostName);

	mHostEntry.bufLen = PR_NETDB_BUF_SIZE;
	mHostEntry.bufPtr = mHostEntry.buffer;
	
	mCount = 0;
	mComplete = PR_FALSE;
    mResult = NS_OK;
	mIndex = 0;
	mRequestQueue = nsnull;

#if defined(XP_MAC)
    mInetHostInfo.lookup = this;
    mLookupElement.lookup = this;
    mContext = clientContext;			// belongs with nsDNSRequest
    mListener = listener;               // belongs with nsDNSRequest
#endif
}


nsDNSLookup::~nsDNSLookup(void)
{
    if (mHostName)
    	delete [] mHostName;
}


nsresult
nsDNSLookup::AddDNSRequest(nsDNSRequest* request)
{
    return NS_OK;
}


nsresult
nsDNSLookup::FinishHostEntry(void)
{
#if defined(XP_MAC)
    PRIntn  len, count, i;

    // convert InetHostInfo to PRHostEnt
    
	// copy name
    len = nsCRT::strlen(mInetHostInfo.hostInfo.name);
	mHostEntry.hostEnt.h_name = BufAlloc(len, &mHostEntry.bufPtr, &mHostEntry.bufLen, 0);
	NS_ASSERTION(nsnull != mHostEntry.hostEnt.h_name,"nsHostEnt buffer full.");
	if (mHostEntry.hostEnt.h_name == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;
	nsCRT::memcpy(mHostEntry.hostEnt.h_name, mInetHostInfo.hostInfo.name, len);
    
    // set aliases to nsnull
    mHostEntry.hostEnt.h_aliases = (char **)BufAlloc(sizeof(char *), &mHostEntry.bufPtr, &mHostEntry.bufLen, sizeof(char *));
	NS_ASSERTION(nsnull != mHostEntry.hostEnt.h_aliases,"nsHostEnt buffer full.");
    if (mHostEntry.hostEnt.h_aliases == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
	*mHostEntry.hostEnt.h_aliases = nsnull;
    
    // fill in address type
    mHostEntry.hostEnt.h_addrtype = AF_INET;
    
    // set address length
    mHostEntry.hostEnt.h_length = sizeof(long);
    
    // count addresses and allocate storage for the pointers
    for (count = 1; count < kMaxHostAddrs && mInetHostInfo.hostInfo.addrs[count] != nsnull; count++){;}
	mHostEntry.hostEnt.h_addr_list = (char **)BufAlloc(count * sizeof(char *), &mHostEntry.bufPtr, &mHostEntry.bufLen, sizeof(char *));
	NS_ASSERTION(nsnull != mHostEntry.hostEnt.h_addr_list,"nsHostEnt buffer full.");
    if (mHostEntry.hostEnt.h_addr_list == nsnull)
    	return NS_ERROR_OUT_OF_MEMORY;

    // copy addresses one at a time
    len = mHostEntry.hostEnt.h_length;
    for (i = 0; i < kMaxHostAddrs && mInetHostInfo.hostInfo.addrs[i] != nsnull; i++) {
        mHostEntry.hostEnt.h_addr_list[i] = BufAlloc(len, &mHostEntry.bufPtr, &mHostEntry.bufLen, len);
	    NS_ASSERTION(nsnull != mHostEntry.hostEnt.h_addr_list[i],"nsHostEnt buffer full.");
	    if (mHostEntry.hostEnt.h_addr_list[i] == nsnull)
	        return NS_ERROR_OUT_OF_MEMORY;
    	*(InetHost *)mHostEntry.hostEnt.h_addr_list[i] = mInetHostInfo.hostInfo.addrs[i];
    }
#endif

    mComplete = PR_TRUE;

	return NS_OK;
}


nsresult
nsDNSLookup::CallOnFound(void)
{
    nsresult result;
    // iterate through request queue calling listeners
    
    // but for now just do this
    if (NS_SUCCEEDED(mResult)) {
        result = mListener->OnFound(mContext, mHostName, &mHostEntry);
    }
    result = mListener->OnStopLookup(mContext, mHostName, mResult);

    return result;
}


nsresult
nsDNSLookup::InitiateDNSLookup(nsDNSService * dnsService)  // I don't want to have to pass this in...
{
	nsresult rv = NS_OK;
	
#if defined(XP_MAC)
	OSErr   err;
	
    err = OTInetStringToAddress(dnsService->mServiceRef, (char *)mHostName, (InetHostInfo *)&mInetHostInfo);
    if (err != noErr)
        rv = NS_ERROR_UNEXPECTED;
#endif

	return rv;
}


////////////////////////////////////////////////////////////////////////////////
// nsDNSService methods:
////////////////////////////////////////////////////////////////////////////////

#if defined(XP_MAC)
pascal void  nsDnsServiceNotifierRoutine(void * contextPtr, OTEventCode code, OTResult result, void * cookie);

pascal void  nsDnsServiceNotifierRoutine(void * contextPtr, OTEventCode code, OTResult result, void * cookie)
{
#pragma unused(contextPtr)

	if (code == T_DNRSTRINGTOADDRCOMPLETE) {
        nsDNSService *  dnsService = (nsDNSService *)contextPtr;
        nsDNSLookup *   dnsLookup = ((nsInetHostInfo *)cookie)->lookup;
        PRThread *      thread;
        
        if (result != kOTNoError)
            dnsLookup->mResult = NS_ERROR_UNKNOWN_HOST;
        
        // queue result & wake up dns service thread
        Enqueue((QElem *)&dnsLookup->mLookupElement, &dnsService->mCompletionQueue);

        dnsService->mThread->GetPRThread(&thread);
        if (thread)
	        PR_Mac_PostAsyncNotify(thread);
    }
	// or else we don't handle the event
}
#endif


nsDNSService::nsDNSService()
{
    NS_INIT_REFCNT();
    
    mThreadRunning = PR_FALSE;
    mThread = nsnull;

#if defined(XP_MAC)
    mServiceRef = nsnull;

    mCompletionQueue.qFlags = 0;
    mCompletionQueue.qHead = nsnull;
    mCompletionQueue.qTail = nsnull;
    
#if TARGET_CARBON
    mCLientContext = nsnull;
#endif /* TARGET_CARBON */
#endif /* defined(XP_MAC) */
    
    
}


nsresult
nsDNSService::Init()
{
  nsresult rv = NS_OK;

//    initialize DNS cache (persistent?)
#if defined(XP_MAC)
//    create Open Transport Service Provider for DNS Lookups
    OSStatus    errOT;


#if TARGET_CARBON
    nsDnsServiceNotifierRoutineUPP	=  NewOTNotifyUPP(nsDnsServiceNotifierRoutine);

    errOT = OTAllocClientContext((UInt32)0, &clientContext);
    NS_ASSERTION(err == kOTNoError, "error allocating OTClientContext.");
#endif


    errOT = INIT_OPEN_TRANSPORT();
    NS_ASSERTION(errOT == kOTNoError, "InitOpenTransport failed.");

    mServiceRef = OT_OPEN_INTERNET_SERVICES(kDefaultInternetServicesPath, NULL, &errOT);
    if (errOT != kOTNoError) return NS_ERROR_UNEXPECTED;    /* no network -- oh well */
    NS_ASSERTION((mServiceRef != NULL) && (errOT == kOTNoError), "error opening OT service.");

    /* Install notify function for DNR Address To String completion */
    errOT = OTInstallNotifier(mServiceRef, nsDNS_NOTIFIER_ROUTINE, this);
    NS_ASSERTION(errOT == kOTNoError, "error installing dns notification routine.");

    /* Put us into async mode */
    errOT = OTSetAsynchronous(mServiceRef);
    NS_ASSERTION(errOT == kOTNoError, "error setting service to async mode.");

	// if either of the two previous calls failed then dealloc service ref and return NS_FAILED
	
    rv = NS_NewThread(&mThread, this, 0, PR_JOINABLE_THREAD);
	


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
    CloseOpenTransport();							// should be moved to terminate routine
//    deallocate Open Transport Service Provider
#elif defined(_WIN)
//    dispose DNS EventHandler Window
#elif defined(XP_UNIX)
//    XXXX - ?
#endif
}


NS_IMPL_ISUPPORTS2(nsDNSService, nsIDNSService, nsIRunnable);


NS_METHOD
nsDNSService::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
    nsresult rv;
    
    if (aOuter != nsnull)
    	return NS_ERROR_NO_AGGREGATION;
    
    nsDNSService* dnsService = new nsDNSService();
    if (dnsService == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(dnsService);
    rv = dnsService->Init();
    if (NS_SUCCEEDED(rv)) {
        rv = dnsService->QueryInterface(aIID, aResult);
    }
    NS_RELEASE(dnsService);
    return rv;
}

//
// --------------------------------------------------------------------------
// nsIRunnable implementation...
// --------------------------------------------------------------------------
//
NS_IMETHODIMP
nsDNSService::Run(void)
{
#if defined(XP_MAC)
    OSErr			    err;
    nsLookupElement *   lookupElement;
    nsDNSLookup *       lookup;
    nsresult            rv;
    
    mThreadRunning = PR_TRUE;
	
    while (mThreadRunning) {
		
        PR_Mac_WaitForAsyncNotify(PR_INTERVAL_NO_TIMEOUT);
        // check queue for completed DNS lookups
        while ((lookupElement = (nsLookupElement *)mCompletionQueue.qHead) != nsnull) {

            err = Dequeue((QElemPtr)lookupElement, &mCompletionQueue);
            if (err)
        	    continue;		// assert
        
            lookup = lookupElement->lookup;
            // convert InetHostInfo to nsHostEnt
            rv = lookup->FinishHostEntry();
            if (NS_SUCCEEDED(rv)) {
                // put lookup in cache
            }
            
            // issue callbacks
            rv = lookup->CallOnFound();
            
            delete lookup;  // until we start caching them
            
        }
        

	}
#endif
    return NS_OK;
}

    
////////////////////////////////////////////////////////////////////////////////
// nsIDNSService methods:
////////////////////////////////////////////////////////////////////////////////


NS_IMETHODIMP
nsDNSService::Lookup(nsISupports*    clientContext,
                     const char*     hostName,
                     nsIDNSListener* listener,
                     nsIRequest*     *DNSRequest)
{
    nsresult	rv, result = NS_OK;
    nsISupports* ctxt = nsnull;

    //    initateLookupNeeded = false;
    //    lock dns service
    //    search cache for existing nsDNSLookup with matching hostname
    //    if (exists) {
    //        if (complete) {
    //            AddRef lookup
    //            unlock cache
    //            OnStartLookup
    //            OnFound
    //            OnStopLookup
    //            Release lookup
    //            return
    //        }
    //    } else {
    //        create nsDNSLookup
    //        iniateLookupNeeded = true
    //    }
    //    create nsDNSRequest & queue on nsDNSLookup
    //    unlock dns service
    //    OnStartLookup
    //    if (iniateLookupNeeded) {
    //        initiate async lookup
    //    }
    //    

#if defined(XP_MAC)

    // create nsDNSLookup
    nsDNSLookup * lookup = new nsDNSLookup(clientContext, hostName, listener);
    if (lookup == nsnull)
    	return NS_ERROR_OUT_OF_MEMORY;
        
    rv = listener->OnStartLookup(clientContext, hostName);
    // what do we do with the return value here?

    // initiate async lookup
    rv = lookup->InitiateDNSLookup(this);	

    return rv;
    
#else
    // temporary SYNC version

    PRStatus    status = PR_SUCCESS;
    nsHostEnt*  hostentry;

    hostentry = new nsHostEnt;
    if (!hostentry)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = listener->OnStartLookup(ctxt, hostName);
	
    PRBool numeric = PR_TRUE;
    for (const char *hostCheck = hostName; *hostCheck; hostCheck++) {
        if (!nsString2::IsDigit(*hostCheck) && (*hostCheck != '.') ) {
            numeric = PR_FALSE;
            break;
        }
    }

    PRNetAddr *netAddr;

    if (numeric) {
        // If it is numeric then try to convert it into an IP-Address
        netAddr = (PRNetAddr*)nsAllocator::Alloc(sizeof(PRNetAddr));
        status = PR_StringToNetAddr(hostName, netAddr);
        if (PR_SUCCESS == status) {
            // slam the IP in and move on.
            PRHostEnt *ent = &(hostentry->hostEnt);
            PRIntn bufLen = PR_NETDB_BUF_SIZE;
            char *buffer = hostentry->buffer;
            ent->h_name = (char*)BufAlloc(PL_strlen(hostName) + 1,
                                       &buffer,
                                       &bufLen,
                                       0);
            memcpy(ent->h_name, hostName, PL_strlen(hostName) + 1);

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
        } else {
            // If the hostname is numeric, but we couldn't create
            // a net addr out of it, we're dealing w/ a purely numeric
            // address (no dots), try a regular gethostbyname on it.
            status = PR_GetHostByName(hostName, 
                                      hostentry->buffer, 
                                      PR_NETDB_BUF_SIZE, 
                                      &hostentry->hostEnt);
        }
        nsAllocator::Free(netAddr);
    } else {
        status = PR_GetHostByName(hostName, 
                                  hostentry->buffer, 
                                  PR_NETDB_BUF_SIZE, 
                                  &hostentry->hostEnt);
    }
    
    if (PR_SUCCESS == status) {
        rv = listener->OnFound(ctxt, hostName, hostentry);
        result = NS_OK;
    }
    else {
        result = NS_ERROR_UNKNOWN_HOST;
    }
    // XXX: The hostentry should really be reference counted so the 
    //      listener does not need to copy it...
    delete hostentry;
	
    rv = listener->OnStopLookup(ctxt, hostName, result);
	
    return result;
#endif
}
