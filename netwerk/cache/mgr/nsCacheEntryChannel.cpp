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
#include "nsIAllocator.h"

nsCacheEntryChannel::nsCacheEntryChannel(nsCachedNetData* aCacheEntry, nsIChannel* aChannel,
                                         nsILoadGroup* aLoadGroup):
    nsChannelProxy(aChannel), mCacheEntry(aCacheEntry), mLoadGroup(aLoadGroup), mLoadAttributes(0)
{
    NS_ASSERTION(aCacheEntry->mChannelCount < 0xFF, "Overflowed channel counter");
    mCacheEntry->mChannelCount++;
    NS_INIT_REFCNT();
}

nsCacheEntryChannel::~nsCacheEntryChannel()
{
    mCacheEntry->mChannelCount--;
}

NS_IMPL_ISUPPORTS3(nsCacheEntryChannel, nsISupports, nsIChannel, nsIRequest)

// A proxy for nsIOutputStream
class CacheOutputStream : public nsIOutputStream {

public:
    CacheOutputStream(nsIOutputStream *aOutputStream, nsCachedNetData *aCacheEntry):
        mOutputStream(aOutputStream), mCacheEntry(aCacheEntry), mStartTime(PR_IntervalNow())
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
    
protected:
    nsCOMPtr<nsIOutputStream> mOutputStream;
    nsCOMPtr<nsCachedNetData> mCacheEntry;

    // Time at which stream was opened
    PRIntervalTime mStartTime;
};

NS_IMPL_ISUPPORTS(CacheOutputStream, NS_GET_IID(nsIOutputStream))

NS_IMETHODIMP
nsCacheEntryChannel::OpenOutputStream(PRUint32 aStartPosition, nsIOutputStream* *aOutputStream)
{
    nsresult rv;
    nsCOMPtr<nsIOutputStream> baseOutputStream;
    
    rv = mChannel->OpenOutputStream(aStartPosition, getter_AddRefs(baseOutputStream));
    if (NS_FAILED(rv)) return rv;

    mCacheEntry->NoteUpdate();
    mCacheEntry->NoteAccess();
    mCacheEntry->mLogicalLength = aStartPosition;

    *aOutputStream = new CacheOutputStream(baseOutputStream, mCacheEntry);
    if (!*aOutputStream)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*aOutputStream);
    return NS_OK;
}

NS_IMETHODIMP
nsCacheEntryChannel::OpenInputStream(PRUint32 aStartPosition, PRInt32 aReadCount,
				     nsIInputStream* *aInputStream)
{
    mCacheEntry->NoteAccess();
    return mChannel->OpenInputStream(aStartPosition, aReadCount, aInputStream);
}

class CacheManagerStreamListener: public nsIStreamListener  {

    public:
    
    CacheManagerStreamListener(nsIStreamListener *aListener,
                               nsILoadGroup *aLoadGroup, nsIChannel *aChannel):
        mListener(aListener), mLoadGroup(aLoadGroup), mChannel(aChannel)
        { NS_INIT_REFCNT(); }

    virtual ~CacheManagerStreamListener() {}

    private:

    NS_DECL_ISUPPORTS

    NS_IMETHOD
    OnDataAvailable(nsIChannel *channel, nsISupports *aContext,
                    nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count) {
        return mListener->OnDataAvailable(mChannel, aContext, inStr, sourceOffset, count);
    }

    NS_IMETHOD
    OnStartRequest(nsIChannel *channel, nsISupports *aContext) {
        if (mLoadGroup)
            mLoadGroup->AddChannel(mChannel, aContext);
        return mListener->OnStartRequest(mChannel, aContext);
    }

    NS_IMETHOD
    OnStopRequest(nsIChannel *channel, nsISupports *aContext,
                  nsresult status, const PRUnichar *errorMsg) {
        nsresult rv;
        rv = mListener->OnStopRequest(mChannel, aContext, status, errorMsg);
        if (mLoadGroup)
            mLoadGroup->RemoveChannel(mChannel, aContext, status, errorMsg);
        return rv;
    }

    private:
    
    nsCOMPtr<nsIStreamListener>  mListener;
    nsCOMPtr<nsILoadGroup>       mLoadGroup;
    nsCOMPtr<nsIChannel>         mChannel;
};

NS_IMPL_ISUPPORTS2(CacheManagerStreamListener, nsIStreamListener, nsIStreamObserver)

NS_IMETHODIMP
nsCacheEntryChannel::AsyncRead(PRUint32 aStartPosition, PRInt32 aReadCount,
                               nsISupports *aContext, nsIStreamListener *aListener)
{
    nsresult rv;

	NS_ENSURE_ARG(aListener);

    mCacheEntry->NoteAccess();

    nsCOMPtr<nsIStreamListener> headListener;
    if (mLoadGroup) {
        mLoadGroup->GetDefaultLoadAttributes(&mLoadAttributes);

        // Create a load group "proxy" listener...
        nsCOMPtr<nsILoadGroupListenerFactory> factory;
        rv = mLoadGroup->GetGroupListenerFactory(getter_AddRefs(factory));
        if (NS_SUCCEEDED(rv) && factory) {
            rv = factory->CreateLoadGroupListener(aListener, 
                                                  getter_AddRefs(headListener));
            if (NS_FAILED(rv)) return rv;
        }
        NS_ASSERTION(headListener, "Load group listener factory did not create listener");
        if (!headListener)
            headListener = aListener;

    } else {
        headListener = aListener;
    }

    CacheManagerStreamListener* cacheManagerStreamListener;
    nsIChannel *channelForListener;

    channelForListener = mProxyChannel ? mProxyChannel.get() : NS_STATIC_CAST(nsIChannel*, this);
    cacheManagerStreamListener =
        new CacheManagerStreamListener(headListener, mLoadGroup, channelForListener);
    if (!cacheManagerStreamListener) return NS_ERROR_OUT_OF_MEMORY;
    
    NS_ADDREF(cacheManagerStreamListener);
    rv = mChannel->AsyncRead(aStartPosition, aReadCount, aContext,
                             cacheManagerStreamListener);
    NS_RELEASE(cacheManagerStreamListener);

    return rv;
}

// No async writes allowed to the cache yet
NS_IMETHODIMP
nsCacheEntryChannel::AsyncWrite(nsIInputStream *aFromStream, PRUint32 aStartPosition,
				PRInt32 aWriteCount, nsISupports *aContext,
				nsIStreamObserver *aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCacheEntryChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    return NS_OK;
}

NS_IMETHODIMP
nsCacheEntryChannel::GetLoadAttributes(nsLoadFlags *aLoadAttributes)
{
    *aLoadAttributes = mLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
nsCacheEntryChannel::SetLoadAttributes(nsLoadFlags aLoadAttributes)
{
    mLoadAttributes = aLoadAttributes;
    return NS_OK;
}

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

NS_IMETHODIMP
nsCacheEntryChannel::GetURI(nsIURI * *aURI)
{
    char* spec;
    nsresult rv;

    rv = mCacheEntry->GetUriSpec(&spec);
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    rv = serv->NewURI(spec, 0, aURI);
    nsAllocator::Free(spec);
    return rv;
}

NS_IMETHODIMP
nsCacheEntryChannel::GetOriginalURI(nsIURI * *aURI)
{
    // FIXME - should return original URI passed into NewChannel() ?
    return NS_ERROR_NOT_IMPLEMENTED;
}
