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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsDnsService.h"
#include "nsIDNSListener.h"
#include "nsIRequest.h"
#include "nsISupportsArray.h"
#include "nsError.h"
#include "prnetdb.h"
#include "nsString.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "netCore.h"
#include "nsAutoLock.h"
#include "nsIStreamObserver.h"
#include "nsTime.h"
#ifdef DNS_TIMING
#include "prinrval.h"
#include "prtime.h"
#endif

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

////////////////////////////////////////////////////////////////////////////////
// Platform specific defines and includes
////////////////////////////////////////////////////////////////////////////////

// PC
#if defined(XP_PC) && !defined(XP_OS2)
#define WM_DNS_SHUTDOWN         (WM_USER + 200)
#endif /* XP_PC */

// MAC
#if defined(XP_MAC)
#include "pprthred.h"

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

typedef struct nsInetHostInfo {
    InetHostInfo    hostInfo;
    nsDNSLookup *   lookup;     // weak reference
} nsInetHostInfo;

typedef struct nsLookupElement {
	QElem	         qElem;
	nsDNSLookup *    lookup;    // weak reference
	
} nsLookupElement;
#endif /* XP_MAC */

// UNIX
// XXX needs implementation

nsDNSService *   nsDNSService::gService = nsnull;
PRBool           nsDNSService::gNeedLateInitialization = PR_FALSE;

////////////////////////////////////////////////////////////////////////////////

class nsDNSLookup;

class nsDNSRequest : public nsIRequest
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST

    nsDNSRequest()
        : mLookup(nsnull), mSuspendCount(0), mStatus(NS_OK)
#ifdef DNS_TIMING
        , mStartTime(PR_IntervalNow())
#endif
    { NS_INIT_REFCNT(); }
    virtual ~nsDNSRequest() {}

    nsresult Init(nsDNSLookup* lookup,
                  nsIDNSListener* userListener,
                  nsISupports* userContext);
    nsresult FireStart();
    nsresult FireStop(nsresult status);

protected:
    nsCOMPtr<nsIDNSListener>    mUserListener;
    nsCOMPtr<nsISupports>       mUserContext;
    nsDNSLookup*                mLookup;        // weak ref
    PRUint32                    mSuspendCount;
    nsresult                    mStatus;
#ifdef DNS_TIMING
    PRIntervalTime              mStartTime;
#endif
};

////////////////////////////////////////////////////////////////////////////////

class nsDNSLookup : public nsISupports
{
public:
    NS_DECL_ISUPPORTS

    nsDNSLookup();
    virtual ~nsDNSLookup(void);


    nsresult            Init(const char * hostName);
    void                Reset(void);
    const char *        HostName()  { return mHostName; }
    nsHostEnt*          HostEntry() { return &mHostEntry; }
    PRBool              IsComplete() { return mComplete; }
    nsresult            HandleRequest(nsDNSRequest* req);
    nsresult            InitiateLookup(void);
    nsresult            CompletedLookup(nsresult status);
    nsresult            Suspend(nsDNSRequest* req);
    nsresult            Resume(nsDNSRequest* req);
    static PRBool       FindCompleted(nsHashKey *aKey, void *aData, void* closure);
    static PRBool       CompletedEntry(nsHashKey *aKey, void *aData, void* closure);
    static PRBool       DeleteEntry(nsHashKey *aKey, void *aData, void* closure);
    PRBool              IsExpired() { 
#ifdef xDEBUG
        char buf[256];
        PRExplodedTime et;

        PR_ExplodeTime(mExpires, PR_LocalTimeParameters, &et);
        PR_FormatTimeUSEnglish(buf, sizeof(buf), "%c", &et);
        fprintf(stderr, "\nDNS %s expires %s\n", mHostName, buf);

        PR_ExplodeTime(nsTime(), PR_LocalTimeParameters, &et);
        PR_FormatTimeUSEnglish(buf, sizeof(buf), "%c", &et);
        fprintf(stderr, "now %s ==> %s\n", buf,
                mExpires < nsTime()
                ? "expired" : "valid");
        fflush(stderr);
#endif
        return mExpires < nsTime();
    }

    friend class nsDNSService;
    friend class nsDNSCache;

protected:
    nsCOMPtr<nsISupportsArray>  mRequests;
    char *                      mHostName;
    // Result of the DNS Lookup
    nsHostEnt                   mHostEntry;
    nsresult                    mStatus;
    PRBool                      mComplete;
    nsTime                      mExpires;

    // Platform specific portions
#if defined(XP_MAC)
    friend pascal void nsDnsServiceNotifierRoutine(void * contextPtr, OTEventCode code, OTResult result, void * cookie);
    nsresult FinishHostEntry(void);

    nsLookupElement             mLookupElement;
    nsInetHostInfo              mInetHostInfo;
#endif

#if defined(XP_PC) && !defined(XP_OS2)
    friend static LRESULT CALLBACK nsDNSEventProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    HANDLE                      mLookupHandle;
    PRUint32                    mMsgID;
#endif
};



////////////////////////////////////////////////////////////////////////////////
// utility routines:
////////////////////////////////////////////////////////////////////////////////
//
// Allocate space from the buffer, aligning it to "align" before doing
// the allocation. "align" must be a power of 2.
// NOTE: this code was taken from NSPR.

static char *
BufAlloc(PRIntn amount, char **bufp, PRIntn *buflenp, PRIntn align) {
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
// nsDNSRequest methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDNSRequest, nsIRequest);

nsresult
nsDNSRequest::Init(nsDNSLookup* lookup,
                   nsIDNSListener* userListener,
                   nsISupports* userContext)
{
    mLookup = lookup;
    mUserListener = userListener;
    mUserContext = userContext;
    return NS_OK;
}

nsresult
nsDNSRequest::FireStart() 
{
    nsresult rv;
    NS_ASSERTION(mUserListener, "calling FireStart more than once");
    if (mUserListener == nsnull)
        return NS_ERROR_FAILURE;
    rv = mUserListener->OnStartLookup(mUserContext, mLookup->HostName());
    return rv;
}

nsresult
nsDNSRequest::FireStop(nsresult status)
{
    nsresult rv;
    NS_ASSERTION(mUserListener, "calling FireStop more than once");
    if (mUserListener == nsnull)
        return NS_ERROR_FAILURE;

    mStatus = status;
    if (NS_SUCCEEDED(status)) {
        rv = mUserListener->OnFound(mUserContext,
                                    mLookup->HostName(),
                                    mLookup->HostEntry());
        NS_ASSERTION(NS_SUCCEEDED(rv), "OnFound failed");
    }
    rv = mUserListener->OnStopLookup(mUserContext,
                                     mLookup->HostName(),
                                     status);
    NS_ASSERTION(NS_SUCCEEDED(rv), "OnStopLookup failed");

    mUserListener = null_nsCOMPtr();
    mUserContext = null_nsCOMPtr();
#ifdef DNS_TIMING
    if (nsDNSService::gService->mOut) {
        PRIntervalTime stopTime = PR_IntervalNow();
        double duration = PR_IntervalToMicroseconds(stopTime - mStartTime);
        nsDNSService::gService->mCount++;
        nsDNSService::gService->mTimes += duration;
        nsDNSService::gService->mSquaredTimes += duration * duration;
        fprintf(nsDNSService::gService->mOut, "DNS time #%d: %u us for %s\n", 
                (PRInt32)nsDNSService::gService->mCount,
                (PRInt32)duration, mLookup->HostName());
    }
#endif
    return NS_OK;
}

NS_IMETHODIMP
nsDNSRequest::IsPending(PRBool *result)
{
    *result = !mLookup->IsComplete();
    return NS_OK;
}

NS_IMETHODIMP
nsDNSRequest::GetStatus(nsresult *status)
{
    *status = mStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsDNSRequest::Cancel(nsresult status)
{
    mStatus = status;
    if (mUserListener) {
        // Hold onto a reference to ourself because if we decide to remove
        // this request from the mRequests list, it could be the last 
        // reference, causing ourself to be deleted. We need to live until
        // this method completes:
        nsCOMPtr<nsIRequest> req = this;

        (void)mLookup->Suspend(this);
        return FireStop(status);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsDNSRequest::Suspend(void)
{
    if (mSuspendCount++ == 0) {
        // Hold onto a reference to ourself because if we decide to remove
        // this request from the mRequests list, it could be the last 
        // reference, causing ourself to be deleted. We need to live until
        // this method completes:
        nsCOMPtr<nsIRequest> req = this;

        return mLookup->Suspend(this);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsDNSRequest::Resume(void)
{
    if (--mSuspendCount == 0) {
        // Hold onto a reference to ourself because if we decide to remove
        // this request from the mRequests list, it could be the last 
        // reference, causing ourself to be deleted. We need to live until
        // this method completes:
        nsCOMPtr<nsIRequest> req = this;

        return mLookup->Resume(this);
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsDNSLookup methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_THREADSAFE_ISUPPORTS0(nsDNSLookup);

nsDNSLookup::nsDNSLookup()
    : mHostName(nsnull),
      mStatus(NS_OK),
      mComplete(PR_FALSE),
      mExpires(0)
{
	NS_INIT_REFCNT();
	MOZ_COUNT_CTOR(nsDNSLookup);
}


nsresult
nsDNSLookup::Init(const char * hostName)
{
    nsresult rv;

    // Store input into member variables
    mHostName = nsCRT::strdup(hostName);
    if (mHostName == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = NS_NewISupportsArray(getter_AddRefs(mRequests));
    if (NS_FAILED(rv)) return rv;

    // Platform specific initializations
#if defined(XP_MAC)
    mInetHostInfo.lookup = this;
    mLookupElement.lookup = this;
#endif

    Reset();
    return NS_OK;
}

void
nsDNSLookup::Reset(void)
{
    // Initialize result holders
    mHostEntry.bufLen = PR_NETDB_BUF_SIZE;
    mHostEntry.bufPtr = mHostEntry.buffer;
#if defined(XP_PC) && !defined(XP_OS2)
    mMsgID    = 0;
#endif

    mComplete = PR_FALSE;
    mStatus = NS_OK;
    mExpires = LL_ZERO;
//    fprintf(stderr, "DNS reset for %s\n", mHostName);
}

nsDNSLookup::~nsDNSLookup(void)
{
    MOZ_COUNT_DTOR(nsDNSLookup);
    
    if (mHostName)
        nsCRT::free(mHostName);
}

#if defined(XP_MAC)
nsresult
nsDNSLookup::FinishHostEntry(void)
{
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
    for (i = 0; i < kMaxHostAddrs && mInetHostInfo.hostInfo.addrs[i] != nsnull; i++)
    {
        mHostEntry.hostEnt.h_addr_list[i] = BufAlloc(len, &mHostEntry.bufPtr, &mHostEntry.bufLen, len);
        NS_ASSERTION(nsnull != mHostEntry.hostEnt.h_addr_list[i],"nsHostEnt buffer full.");
        if (mHostEntry.hostEnt.h_addr_list[i] == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
        *(InetHost *)mHostEntry.hostEnt.h_addr_list[i] = mInetHostInfo.hostInfo.addrs[i];
    }

    return NS_OK;
}
#endif

nsresult
nsDNSLookup::HandleRequest(nsDNSRequest* req)
{
    nsresult rv;
 
    rv = req->FireStart();
    if (NS_FAILED(rv)) return rv;
    return Resume(req);
}

nsresult
nsDNSLookup::InitiateLookup(void)
{
    nsresult rv = NS_OK;
//    nsAutoCMonitor mon(this);	// XXX don't think we need this
	
    PRStatus status = PR_SUCCESS;

    PRBool numeric = PR_TRUE;
    for (const char *hostCheck = mHostName; *hostCheck; hostCheck++) {
        if (!nsCRT::IsAsciiDigit(*hostCheck) && (*hostCheck != '.') ) {
            numeric = PR_FALSE;
            break;
        }
    }

    if (numeric) {
        // If it is numeric then try to convert it into an IP-Address
        PRNetAddr *netAddr = (PRNetAddr*)nsAllocator::Alloc(sizeof(PRNetAddr));
        if (!netAddr) return NS_ERROR_OUT_OF_MEMORY;

        status = PR_StringToNetAddr(mHostName, netAddr);
        if (PR_SUCCESS == status) {
            // slam the IP in and move on.
            mHostEntry.bufLen = PR_NETDB_BUF_SIZE;

            PRUint32 hostNameLen = nsCRT::strlen(mHostName);
            mHostEntry.hostEnt.h_name = (char*)BufAlloc(hostNameLen + 1,
                                                        (char**)&mHostEntry.bufPtr,
                                                        &mHostEntry.bufLen,
                                                        0);
            memcpy(mHostEntry.hostEnt.h_name, mHostName, hostNameLen + 1);

            mHostEntry.hostEnt.h_aliases = (char**)BufAlloc(1 * sizeof(char*),
                                                            (char**)&mHostEntry.bufPtr,
                                                            &mHostEntry.bufLen,
                                                            sizeof(char **));
            mHostEntry.hostEnt.h_aliases[0] = '\0';

            mHostEntry.hostEnt.h_addrtype = 2;
            mHostEntry.hostEnt.h_length = 4;
            mHostEntry.hostEnt.h_addr_list = (char**)BufAlloc(2 * sizeof(char*),
                                                              (char**)&mHostEntry.bufPtr,
                                                              &mHostEntry.bufLen,
                                                              sizeof(char **));
            mHostEntry.hostEnt.h_addr_list[0] = (char*)BufAlloc(mHostEntry.hostEnt.h_length,
                                                                (char**)&mHostEntry.bufPtr,
                                                                &mHostEntry.bufLen,
                                                                0);
            memcpy(mHostEntry.hostEnt.h_addr_list[0], &netAddr->inet.ip, mHostEntry.hostEnt.h_length);
            mHostEntry.hostEnt.h_addr_list[1] = '\0';

            rv = CompletedLookup(NS_OK);
        }
        nsAllocator::Free(netAddr);
		if(PR_SUCCESS == status) 
			return rv;
    }

    // Incomming hostname is not a numeric ip address. Need to do the actual
    // dns lookup.

#if defined(XP_MAC)
    OSErr   err;
	
    err = OTInetStringToAddress(nsDNSService::gService->mServiceRef, (char *)mHostName, (InetHostInfo *)&mInetHostInfo);
    if (err != noErr)
        rv = NS_ERROR_UNEXPECTED;
#endif /* XP_MAC */

#if defined(XP_PC) && !defined(XP_OS2)
    mMsgID = nsDNSService::gService->AllocMsgID();
    if (mMsgID == 0)
        return NS_ERROR_UNEXPECTED;
    {
        nsAutoMonitor mon(nsDNSService::gService->mMonitor);    // protect against lookup completing before WSAAsyncGetHostByName returns, better to refcount lookup

        mLookupHandle = WSAAsyncGetHostByName(nsDNSService::gService->mDNSWindow, mMsgID,
                                          mHostName, (char *)&mHostEntry.hostEnt, PR_NETDB_BUF_SIZE);
        // check for error conditions
        if (mLookupHandle == nsnull) {
            rv = NS_ERROR_UNEXPECTED;  // or call WSAGetLastError() for details;
		    // While we would like to use mLookupHandle to allow for canceling the request in
		    // the future, there is a bug with winsock2 on win95 that causes WSAAsyncGetHostByName
		    // to always return the same value.  We have worked around this problem by using the
		    // msgID to identify which lookup has completed, rather than the handle, however, we
		    // still need to identify when this is a problem (by comparing the return values of
		    // the first two calls made to WSAAsyncGetHostByName), to avoid using the handle on
		    // those systems.  For more info, see bug 23709.
        }
    }
#endif /* XP_PC */

#ifdef XP_UNIX
    // temporary SYNC version
    status = PR_GetHostByName(mHostName, mHostEntry.buffer, PR_NETDB_BUF_SIZE, &(mHostEntry.hostEnt));
    if (PR_SUCCESS != status) rv = NS_ERROR_UNKNOWN_HOST;

    return CompletedLookup(rv);
#endif /* XP_UNIX */

    return rv;
}

#define EXPIRATION_INTERVAL  (15*60*1000000)         // 15 min worth of microseconds
//#define EXPIRATION_INTERVAL  (30*1000000)         // 30 sec worth of microseconds

nsresult
nsDNSLookup::CompletedLookup(nsresult status)
{
    nsresult rv;
    nsAutoCMonitor mon(this);	// protect mRequests

    mStatus = status;
    mExpires = nsTime() + nsInt64(EXPIRATION_INTERVAL);      // now + 15 minutes
    mComplete = PR_TRUE;

    while (PR_TRUE) {
        nsDNSRequest* req = (nsDNSRequest*)mRequests->ElementAt(0);
        if (req == nsnull) break;

        rv = mRequests->RemoveElementAt(0) ? NS_OK : NS_ERROR_FAILURE;  // XXX this method incorrectly returns a bool
        if (NS_FAILED(rv)) {
            NS_RELEASE(req);
            return rv;
        }

        // We can't be holding the lock around the OnFound/OnStopLookup
        // callbacks:
        mon.Exit();
        rv = req->FireStop(mStatus);
        mon.Enter();
        NS_RELEASE(req);
        NS_ASSERTION(NS_SUCCEEDED(rv), "req->FireStop() failed.");
        // continue notifying requests, even if one fails
    }
    
    // XXX remove from hashtable and release - for now
    nsStringKey key(mHostName);
    (void) nsDNSService::gService->mLookups.Remove(&key);
    
    return NS_OK;
}

nsresult
nsDNSLookup::Suspend(nsDNSRequest* req)
{
    nsresult rv;
    nsAutoCMonitor mon(this);

    if (mComplete)
        return NS_ERROR_FAILURE;

    rv = mRequests->RemoveElement(req) ? NS_OK : NS_ERROR_FAILURE;  // XXX this method incorrectly returns a bool
    if (NS_FAILED(rv)) return rv;

    PRUint32 cnt;
    rv = mRequests->Count(&cnt);
    if (NS_FAILED(rv)) return rv;
    if (cnt == 0) {
        // XXX need to do the platform-specific cancelation here
    }

    return rv;
}

nsresult
nsDNSLookup::Resume(nsDNSRequest* req)
{
    nsresult rv;
    PRUint32 reqCount;
    
    if (mComplete && !IsExpired()) {
//        fprintf(stderr, "\nDNS cache hit for %s\n", mHostName);
        rv = req->FireStop(mStatus);
        return rv;
    }

    {   // protect mRequests
        nsAutoCMonitor mon(this);

        rv = mRequests->AppendElement(req) ? NS_OK : NS_ERROR_FAILURE;  // XXX this method incorrectly returns a bool
        if (NS_FAILED(rv)) return rv;
    
        rv = mRequests->Count(&reqCount);
        if (NS_FAILED(rv)) return rv;
    }

    if (reqCount == 1) {
        // if this was the first request, then we need to kick off
        // the lookup
//        fprintf(stderr, "\nDNS cache miss for %s\n", mHostName);
        rv = InitiateLookup();
    }
    else {
//        fprintf(stderr, "DNS consolidating lookup for %s\n", mHostName);
    }
    return rv;
}

	
////////////////////////////////////////////////////////////////////////////////
// Platform specific helper routines
////////////////////////////////////////////////////////////////////////////////

#if defined(XP_MAC)
pascal void
nsDnsServiceNotifierRoutine(void * contextPtr, OTEventCode code, 
                            OTResult result, void * cookie)
{
    if (code == T_DNRSTRINGTOADDRCOMPLETE) {
        nsDNSService *  dnsService = (nsDNSService *)contextPtr;
        nsDNSLookup *   dnsLookup = ((nsInetHostInfo *)cookie)->lookup;
        PRThread *      thread;
        
        if (result != kOTNoError)
            dnsLookup->mStatus = NS_ERROR_UNKNOWN_HOST;
        
        // queue result & wake up dns service thread
        Enqueue((QElem *)&dnsLookup->mLookupElement, &dnsService->mCompletionQueue);

        dnsService->mThread->GetPRThread(&thread);
        if (thread)
            PR_Mac_PostAsyncNotify(thread);
    }
    // or else we don't handle the event
}
#endif /* XP_MAC */

#if defined(XP_PC) && !defined(XP_OS2)

struct FindCompletedClosure {
    UINT                mMsg;
    nsDNSLookup**       mResult;
};

PRBool
nsDNSLookup::FindCompleted(nsHashKey *aKey, void *aData, void* closure)
{
    nsDNSLookup* lookup = (nsDNSLookup*)aData;
    FindCompletedClosure* c = (FindCompletedClosure*)closure;
    if (lookup->mMsgID == c->mMsg) {
        *c->mResult = lookup;
        return PR_FALSE;        // quit looking
    }
    return PR_TRUE;     // keep going
}

LRESULT
nsDNSService::LookupComplete(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    nsresult rv;
    nsDNSLookup* lookup = nsnull;

    {
        nsAutoMonitor mon(mMonitor);        // protect mLookups

        // which lookup completed?  fetch lookup for this (HANDLE)wParam
        FindCompletedClosure closure = { uMsg, &lookup };
        mLookups.Enumerate(nsDNSLookup::FindCompleted, &closure);
        NS_IF_ADDREF(lookup);
    }  // exit monitor

    if (lookup) {
        int error = WSAGETASYNCERROR(lParam);
        rv = lookup->CompletedLookup(error ? NS_ERROR_UNKNOWN_HOST : NS_OK);
        NS_IF_RELEASE(lookup);
        return NS_SUCCEEDED(rv) ? 0 : -1;
    }
    return -1;  // XXX right result?
}

static LRESULT CALLBACK
nsDNSEventProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = nsnull;
    int     error = nsnull;

 	if ((uMsg >= WM_USER) && (uMsg < WM_USER+128)) {
        result = nsDNSService::gService->LookupComplete(hWnd, uMsg, wParam, lParam);
    }
    else if (uMsg == WM_DNS_SHUTDOWN) {
        // dispose DNS EventHandler Window
        (void) DestroyWindow(nsDNSService::gService->mDNSWindow);
        PostQuitMessage(0);
        result = 0;
    }
    else {
        result = DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    return result;
}
#endif /* XP_PC */

#ifdef XP_UNIX

#endif /* XP_UNIX */

////////////////////////////////////////////////////////////////////////////////
// nsDNSService methods:
////////////////////////////////////////////////////////////////////////////////

PRBool
nsDNSLookup::DeleteEntry(nsHashKey *aKey, void *aData, void* closure)
{
    nsDNSLookup* lookup = (nsDNSLookup*)aData;
    delete lookup;
    return PR_TRUE;     // keep iterating
}

nsDNSService::nsDNSService()
    : mState(NS_OK),
      mMonitor(nsnull),
      mLookups(64)
//      mLookups(nsnull, nsnull, nsDNSLookup::DeleteEntry, nsnull)
#ifdef DNS_TIMING
      ,
      mCount(0),
      mTimes(0),
      mSquaredTimes(0),
      mOut(nsnull)
#endif
{
    NS_INIT_REFCNT();
    
    NS_ASSERTION(gService==nsnull,"multiple nsDNSServices allocated!");
    gService    = this;

#if defined(XP_MAC)
    gNeedLateInitialization = PR_TRUE;
    mThreadRunning = PR_FALSE;
    mServiceRef = nsnull;

    mCompletionQueue.qFlags = 0;
    mCompletionQueue.qHead = nsnull;
    mCompletionQueue.qTail = nsnull;
    
#if TARGET_CARBON
    mClientContext = nsnull;
#endif /* TARGET_CARBON */
#endif /* defined(XP_MAC) */

#if defined(XP_PC) && !defined(XP_OS2)
    // initialize bit vector for allocating message IDs.
	int i;
	for (i=0; i<4; i++)
		mMsgIDBitVector[i] = 0;
#endif

#ifdef DNS_TIMING
    if (getenv("DNS_TIMING")) {
        mOut = fopen("dns-timing.txt", "a");
        if (mOut) {
            PRTime now = PR_Now();
            PRExplodedTime time;
            PR_ExplodeTime(now, PR_LocalTimeParameters, &time);
            char buf[128];
            PR_FormatTimeUSEnglish(buf, sizeof(buf), "%c", &time);
            fprintf(mOut, "############### DNS starting new run: %s\n", buf);
        }
    }
#endif
}

NS_IMETHODIMP
nsDNSService::Init()
{
    nsresult rv = NS_OK;

    NS_ASSERTION(mMonitor == nsnull, "nsDNSService not shut down");
    mMonitor = PR_NewMonitor();
    if (mMonitor == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

#if defined(XP_PC)
    // sync with DNS thread to allow it to create the DNS window
    nsAutoMonitor mon(mMonitor);
#endif

#if defined(XP_MAC) || defined(XP_PC)
    // create DNS thread
    NS_ASSERTION(mThread == nsnull, "nsDNSService not shut down");
    rv = NS_NewThread(getter_AddRefs(mThread), this, 0, PR_JOINABLE_THREAD);
#endif

#if defined(XP_PC)
    mon.Wait();
#endif

    return rv;
}

nsresult
nsDNSService::LateInit()
{
#if defined(XP_MAC)
//    create Open Transport Service Provider for DNS Lookups
    OSStatus    errOT;

#if TARGET_CARBON
    nsDnsServiceNotifierRoutineUPP	=  NewOTNotifyUPP(nsDnsServiceNotifierRoutine);

    errOT = OTAllocClientContext((UInt32)0, &clientContext);
    NS_ASSERTION(err == kOTNoError, "error allocating OTClientContext.");
#endif /* TARGET_CARBON */

    errOT = INIT_OPEN_TRANSPORT();
    NS_ASSERTION(errOT == kOTNoError, "InitOpenTransport failed.");

    mServiceRef = OT_OPEN_INTERNET_SERVICES(kDefaultInternetServicesPath, NULL, &errOT);
    if (errOT != kOTNoError) return NS_ERROR_UNEXPECTED;    /* no network -- oh well */
    NS_ASSERTION((mServiceRef != NULL) && (errOT == kOTNoError), "error opening OT service.");

    /* Install notify function for DNR Address To String completion */
    errOT = OTInstallNotifier(mServiceRef, nsDNS_NOTIFIER_ROUTINE, this);
    NS_ASSERTION(errOT == kOTNoError, "error installing dns notification routine.");

    /* Put us into async mode */
    if (errOT == kOTNoError) {
        errOT = OTSetAsynchronous(mServiceRef);
        NS_ASSERTION(errOT == kOTNoError, "error setting service to async mode.");
    } else {
        // if either of the two previous calls failed then dealloc service ref and return NS_ERROR_UNEXPECTED
        OSStatus status = OTCloseProvider((ProviderRef)mServiceRef);
        return NS_ERROR_UNEXPECTED;
    }
#endif

    gNeedLateInitialization = PR_FALSE;
    return NS_OK;
}


nsDNSService::~nsDNSService()
{
    nsresult rv = Shutdown();
    NS_ASSERTION(NS_SUCCEEDED(rv), "DNS shutdown failed");
    NS_ASSERTION(mThread == nsnull, "DNS shutdown failed");
    NS_ASSERTION(mLookups.Count() == 0, "didn't clean up lookups");

    if (mMonitor)
        PR_DestroyMonitor(mMonitor);

    NS_ASSERTION(gService==this,"multiple nsDNSServices allocated.");
    gService = nsnull;

#ifdef DNS_TIMING
    if (mOut) {
        double mean;
        double stddev;
        NS_MeanAndStdDev(mCount, mTimes, mSquaredTimes, &mean, &stddev);
        fprintf(mOut, "DNS lookup time: %.2f +/- %.2f us (%d lookups)\n",
                mean, stddev, (PRInt32)mCount);
        fclose(mOut);
        mOut = nsnull;
    }
#endif
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsDNSService,
                              nsIDNSService, 
                              nsIRunnable)

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

////////////////////////////////////////////////////////////////////////////////
// nsIRunnable implementation...
////////////////////////////////////////////////////////////////////////////////

nsresult
nsDNSService::InitDNSThread(void)
{
#if defined(XP_PC) && !defined(XP_OS2)
    WNDCLASS    wc;
    char *      windowClass = "Mozilla:DNSWindowClass";

    // register window class for DNS event receiver window
    memset(&wc, 0, sizeof(wc));
    wc.style            = 0;
    wc.lpfnWndProc      = nsDNSEventProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = NULL;
    wc.hIcon            = NULL;
    wc.hbrBackground    = (HBRUSH) NULL;
    wc.lpszMenuName     = (LPCSTR) NULL;
    wc.lpszClassName    = windowClass;
    RegisterClass(&wc);

    // create DNS event receiver window
    mDNSWindow = CreateWindow(windowClass, "Mozilla:DNSWindow",
                              0, 0, 0, 10, 10, NULL, NULL, NULL, NULL);

    // sync with Create thread
    nsAutoMonitor mon(mMonitor);
    mon.Notify();
#endif /* XP_PC */

    return NS_OK;
}

NS_IMETHODIMP
nsDNSService::Run(void)
{
    nsresult rv;

    rv = InitDNSThread();
    if (NS_FAILED(rv)) return rv;

#if defined(XP_PC) && !defined(XP_OS2)
    MSG msg;
    
    while(GetMessage(&msg, mDNSWindow, 0, 0)) {
        // no TranslateMessage() because we're not expecting input
        DispatchMessage(&msg);
    }
#endif /* XP_PC */

#if defined(XP_MAC)
    OSErr			    err;
    nsLookupElement *   lookupElement;
    nsDNSLookup *       lookup;
    
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
            	NS_ADDREF(lookup);
                rv = lookup->CompletedLookup(NS_OK);    // sets lookup->mComplete = PR_TRUE;
                NS_RELEASE(lookup);
                NS_ASSERTION(NS_SUCCEEDED(rv), "Completed failed");
            }
        }
	}
#endif /* XP_MAC */

    return rv;
}
    
////////////////////////////////////////////////////////////////////////////////
// nsIDNSService methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsDNSService::Lookup(const char*     hostName,
                     nsIDNSListener* userListener,
                     nsISupports*    userContext,
                     nsIRequest*     *result)
{
    nsresult rv;
    nsDNSRequest* req;

    NS_WITH_SERVICE(nsIIOService, ios, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    PRBool offline;
    rv = ios->GetOffline(&offline);
    if (NS_FAILED(rv)) return rv;
    if (offline) return NS_ERROR_OFFLINE;

    if (gNeedLateInitialization) {		// check flag without monitor for speed
        nsAutoMonitor mon(mMonitor);	// in case another thread is about to start LateInit()
		if (gNeedLateInitialization) {  // check flag again to be sure we really need to do initialization
            rv = LateInit();
            if (NS_FAILED(rv)) return rv;
        }
    }

#if !defined(XP_UNIX)
    if (mThread == nsnull)
        return NS_ERROR_OFFLINE;
#endif

    nsDNSLookup * lookup;
    rv = GetLookupEntry(hostName, &lookup);
    if (NS_FAILED(rv)) return rv;

    req = new nsDNSRequest();
    if (req == nsnull) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(req);
    rv = req->Init(lookup, userListener, userContext);
    if (NS_FAILED(rv)) goto done;

    rv = lookup->HandleRequest(req);

  done:

    NS_RELEASE(lookup);
    NS_RELEASE(req);
    if (NS_SUCCEEDED(rv)) {
        *result = req;
    }
    return rv;
}

nsresult
nsDNSService::GetLookupEntry(const char* hostName,
                             nsDNSLookup* *result)
{
    nsresult rv;
    void* prev;

    nsAutoMonitor mon(mMonitor);

    nsStringKey key(hostName);
    nsDNSLookup * lookup = (nsDNSLookup*)mLookups.Get(&key);
    if (lookup) {
        nsAutoCMonitor lmon(lookup);

        if (lookup->mComplete && lookup->IsExpired()) {
            lookup->Reset();
        }
        *result = lookup;	// already ADD_REF'd by Get();
        return NS_OK;
    }

    // no lookup entry exists for this request, either because this
    // is the first time, or an old one was cleaned out 
    
    lookup = new nsDNSLookup();
    if (lookup == nsnull) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = lookup->Init(hostName);
    if (NS_FAILED(rv)) goto done;

	NS_ADDREF(lookup);
    prev = mLookups.Put(&key, lookup);
    NS_ASSERTION(prev == nsnull, "already a nsDNSLookup entry");

	*result = lookup;
  done:
    if (NS_FAILED(rv))
        delete lookup;
    return rv;
}

PRBool
nsDNSLookup::CompletedEntry(nsHashKey *aKey, void *aData, void* closure)
{
    nsresult rv;
    nsDNSLookup* lookup = (nsDNSLookup*)aData;
    nsresult* status = (nsresult*)closure;

    rv = lookup->CompletedLookup(*status);
    NS_ASSERTION(NS_SUCCEEDED(rv), "lookup Completed failed");
    return PR_TRUE;     // keep iterating
}

NS_IMETHODIMP
nsDNSService::Shutdown()
{
    nsresult rv = NS_OK;

#if !defined(XP_UNIX)
    if (mThread == nsnull) return rv;
#endif

    {
        nsAutoMonitor mon(mMonitor);        // protect mLookups

        nsresult status = NS_BINDING_ABORTED;
        mLookups.Enumerate(nsDNSLookup::CompletedEntry, &status);
        mLookups.Reset();

#if defined(XP_MAC)

        mThreadRunning = PR_FALSE;

        // deallocate Open Transport Service Provider
        (void) OTCloseProvider((ProviderRef)mServiceRef);
        CloseOpenTransport();           // should be moved to terminate routine
        PRThread* dnsServiceThread;
        rv = mThread->GetPRThread(&dnsServiceThread);
        if (dnsServiceThread)
            PR_Mac_PostAsyncNotify(dnsServiceThread);
        rv = mThread->Join();

#elif defined(XP_PC) && !defined(XP_OS2)

        SendMessage(mDNSWindow, WM_DNS_SHUTDOWN, 0, 0);
        rv = mThread->Join();

#elif defined(XP_UNIX)
        // XXXX - ?
#endif
    }

    // Have to break the cycle here, otherwise nsDNSService holds onto the thread
    // and the thread holds onto the nsDNSService via its mRunnable
    mThread = nsnull;
    
    return rv;
}

#if defined(XP_PC)  && !defined(XP_OS2)

PRUint32
nsDNSService::AllocMsgID(void)
{
	PRUint32 i   = 0;
	PRUint32 bit = 0;
	PRUint32 element;

	while (i < 4) {
		if (~mMsgIDBitVector[i] != 0)	// break if any free bits in this element
			break;
		i++;
	}
	if (i >= 4)
		return 0;

	element = ~mMsgIDBitVector[i]; // element contains free bits marked as 1

	if ((element & 0xFFFF) == 0) {	bit |= 16;	element >>= 16;	}
	if ((element & 0x00FF) == 0) {	bit |=  8;	element >>= 8;	}
	if ((element & 0x000F) == 0) {	bit |=  4;	element >>= 4;	}
	if ((element & 0x0003) == 0) {	bit |=  2;	element >>= 2;	}
	if ((element & 0x0001) == 0)	bit |=  1;

	mMsgIDBitVector[i] |= (PRUint32)1 << bit;

	return WM_USER + i * 32 + bit;
}

void
nsDNSService::FreeMsgID(PRUint32 msgID)
{
	PRInt32 bit = msgID - WM_USER;

	NS_ASSERTION((bit >= 0) && (bit < 128),"nsDNSService::FreeMsgID reports invalid msgID.");

	mMsgIDBitVector[bit>>5] &= ~((PRUint32)1 << (bit & 0x1f));
}

#endif

////////////////////////////////////////////////////////////////////////////////
