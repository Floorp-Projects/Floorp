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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Scott Furman, fur@netscape.com
 */

#include "nsCacheManager.h"
#include "nsCacheEntryChannel.h"
#include "nsIOutputStream.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsMemory.h"

nsCacheEntryTransport::nsCacheEntryTransport(
        nsCachedNetData* aCacheEntry, 
        nsITransport* aTransport,
        nsILoadGroup* aLoadGroup):
    mCacheEntry(aCacheEntry), 
    mLoadGroup(aLoadGroup),
    mTransport(aTransport)
{
    NS_ASSERTION(aCacheEntry->mTransportCount < 0xFF, "Overflowed transport counter");
    mCacheEntry->mTransportCount++;
    NS_INIT_REFCNT();
}

nsCacheEntryTransport::~nsCacheEntryTransport()
{
    mCacheEntry->mTransportCount--;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsCacheEntryTransport, 
                              nsITransport)

// A proxy for nsIOutputStream
class CacheOutputStream : public nsIOutputStream {

public:
    CacheOutputStream(nsIOutputStream *aOutputStream, 
                      nsCachedNetData *aCacheEntry):
        mOutputStream(aOutputStream), 
        mCacheEntry(aCacheEntry), 
        mStartTime(PR_IntervalNow())
        { NS_INIT_REFCNT(); }

    virtual ~CacheOutputStream() {
        mCacheEntry->NoteDownloadTime(mStartTime, PR_IntervalNow());
        mCacheEntry->ClearFlag(nsCachedNetData::UPDATE_IN_PROGRESS);
    }
    
    NS_DECL_ISUPPORTS

    NS_IMETHOD Close() {
        return mOutputStream->Close();
    }

    NS_IMETHOD Flush() { return mOutputStream->Flush(); }

    NS_IMETHOD
    Write(const char *aBuf, PRUint32 aCount, PRUint32 *aActualBytes) {
        nsresult rv;
    
        *aActualBytes = 0;
        rv = mOutputStream->Write(aBuf, aCount, aActualBytes);
        mCacheEntry->mLogicalLength += *aActualBytes;
        if (NS_FAILED(rv)) return rv;
        nsCacheManager::LimitCacheSize();
        return rv;
    }
    
    NS_IMETHOD
    WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *_retval) {
        NS_NOTREACHED("WriteFrom");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD
    WriteSegments(nsReadSegmentFun reader, void * closure, PRUint32 count, PRUint32 *_retval) {
        NS_NOTREACHED("WriteSegments");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD
    GetNonBlocking(PRBool *aNonBlocking) {
        NS_NOTREACHED("GetNonBlocking");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD
    SetNonBlocking(PRBool aNonBlocking) {
        NS_NOTREACHED("SetNonBlocking");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD
    GetObserver(nsIOutputStreamObserver * *aObserver) {
        NS_NOTREACHED("GetObserver");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD
    SetObserver(nsIOutputStreamObserver * aObserver) {
        NS_NOTREACHED("SetObserver");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

protected:
    nsCOMPtr<nsIOutputStream> mOutputStream;
    nsCOMPtr<nsCachedNetData> mCacheEntry;

    // Time at which stream was opened
    PRIntervalTime mStartTime;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(CacheOutputStream, nsIOutputStream)

NS_IMETHODIMP
nsCacheEntryTransport::OpenOutputStream(PRUint32 transferOffset,
                                        PRUint32 transferCount,
                                        PRUint32 transferFlags,
                                        nsIOutputStream* *aOutputStream)
{
    nsresult rv;
    nsCOMPtr<nsIOutputStream> baseOutputStream;
    
    rv = mTransport->OpenOutputStream(transferOffset,
                                      transferCount,
                                      transferFlags,
                                      getter_AddRefs(baseOutputStream));
    if (NS_FAILED(rv)) return rv;

    mCacheEntry->NoteAccess();
    mCacheEntry->NoteUpdate();

    *aOutputStream = new CacheOutputStream(baseOutputStream, mCacheEntry);
    if (!*aOutputStream)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*aOutputStream);
    return NS_OK;
}

NS_IMETHODIMP
nsCacheEntryTransport::OpenInputStream(PRUint32 transferOffset,
                                       PRUint32 transferCount,
                                       PRUint32 transferFlags,
                                       nsIInputStream* *aInputStream)
{
    mCacheEntry->NoteAccess();

    return mTransport->OpenInputStream(transferOffset,
                                       transferCount,
                                       transferFlags,
                                       aInputStream);
}

NS_IMETHODIMP
nsCacheEntryTransport::AsyncRead(nsIStreamListener *aListener,
                                 nsISupports *aContext,
                                 PRUint32 transferOffset,
                                 PRUint32 transferCount,
                                 PRUint32 transferFlags,
                                 nsIRequest **_retval)
{
    nsresult rv;

	NS_ENSURE_ARG(aListener);

    mCacheEntry->NoteAccess();

    rv = mTransport->AsyncRead(aListener,
                               aContext,
                               transferOffset,
                               transferCount,
                               transferFlags,
                               _retval);

    return rv;
}

// No async writes allowed to the cache yet
NS_IMETHODIMP
nsCacheEntryTransport::AsyncWrite(nsIStreamProvider *aProvider,
                                  nsISupports *ctxt, 
                                  PRUint32 transferOffset, 
                                  PRUint32 transferCount, 
                                  PRUint32 transferFlags,
                                  nsIRequest **_retval)
{
    NS_NOTREACHED("nsCacheEntryTransport::AsyncWrite");
    return NS_ERROR_NOT_IMPLEMENTED;
}

#if 0
NS_IMETHODIMP 
nsCacheEntryTransport::SetLoadGroup(nsILoadGroup * aLoadGroup)
{
    return mTransport->SetLoadGroup(aLoadGroup);
}

NS_IMETHODIMP
nsCacheEntryTransport::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    return NS_OK;
}

NS_IMETHODIMP
nsCacheEntryTransport::GetLoadAttributes(nsLoadFlags *aLoadAttributes)
{
    return mTransport->GetLoadAttributes(aLoadAttributes);
}

NS_IMETHODIMP
nsCacheEntryTransport::SetLoadAttributes(nsLoadFlags aLoadAttributes)
{
    return mTransport->SetLoadAttributes(aLoadAttributes);
}

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

NS_IMETHODIMP
nsCacheEntryTransport::GetURI(nsIURI * *aURI)
{
    char* spec;
    nsresult rv;

    rv = mCacheEntry->GetUriSpec(&spec);
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    rv = serv->NewURI(spec, nsnull, aURI);
    nsMemory::Free(spec);
    return rv;
}

NS_IMETHODIMP
nsCacheEntryTransport::SetURI(nsIURI * aOriginalURI) 
{ 
    return mTransport->SetURI(aOriginalURI); 
} 


NS_IMETHODIMP
nsCacheEntryTransport::SetOriginalURI(nsIURI * aOriginalURI) 
{ 
    return mTransport->SetOriginalURI(aOriginalURI); 
} 

NS_IMETHODIMP
nsCacheEntryTransport::GetOriginalURI(nsIURI * *aURI)
{
    // FIXME - should return original URI passed into NewTransport() ?
    NS_NOTREACHED("nsCacheEntryTransport::GetOriginalURI");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsCacheEntryTransport::GetNotificationCallbacks(nsIInterfaceRequestor * *aNotificationCallbacks) 
{ 
    return mTransport->GetNotificationCallbacks(aNotificationCallbacks); 
}

NS_IMETHODIMP 
nsCacheEntryTransport::SetNotificationCallbacks(nsIInterfaceRequestor * aNotificationCallbacks) 
{ 
    return mTransport->SetNotificationCallbacks(aNotificationCallbacks); 
} 

NS_IMETHODIMP 
nsCacheEntryTransport::GetOwner(nsISupports * *aOwner) 
{ 
    return mTransport->GetOwner(aOwner); 
}

NS_IMETHODIMP 
nsCacheEntryTransport::SetOwner(nsISupports * aOwner) 
{ 
    return mTransport->SetOwner(aOwner); 
}
#endif

NS_IMETHODIMP 
nsCacheEntryTransport::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    return mTransport->GetSecurityInfo(aSecurityInfo);
}

NS_IMETHODIMP
nsCacheEntryTransport::GetProgressEventSink(nsIProgressEventSink **aResult)
{
    return mTransport->GetProgressEventSink(aResult);
}

NS_IMETHODIMP
nsCacheEntryTransport::SetProgressEventSink(nsIProgressEventSink *aProgress)
{
    return mTransport->SetProgressEventSink(aProgress);
}
