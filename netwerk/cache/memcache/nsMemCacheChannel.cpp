/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 */

#include "nsMemCache.h"
#include "nsMemCacheChannel.h"
#include "nsIStreamListener.h"
#include "nsIChannel.h"
#include "nsIStorageStream.h"
#include "nsIOutputStream.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsNetUtil.h"
#include "nsILoadGroup.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kEventQueueService, NS_EVENTQUEUESERVICE_CID);

NS_IMPL_THREADSAFE_ISUPPORTS3(nsMemCacheTransport,
                              nsITransport,
                              nsITransportRequest,
                              nsIRequest)

void
nsMemCacheTransport::NotifyStorageInUse(PRInt32 aBytesUsed)
{
    mRecord->mCache->mOccupancy += aBytesUsed;
}

/**
 * This class acts as an adaptor around a synchronous input stream to add async
 * read capabilities.  It adds methods for initiating, suspending, resuming and
 * cancelling async reads. 
 */
class AsyncReadStreamAdaptor : public nsIInputStream,
                               public nsIStreamListener
{
public:
    AsyncReadStreamAdaptor(nsMemCacheTransport* aChannel, nsIInputStream *aSyncStream):
        mSyncStream(aSyncStream), mDataAvailCursor(0),
        mRemaining((PRUint32)-1), mAvailable(0), mChannel(aChannel), mAbortStatus(NS_OK), mSuspended(PR_FALSE)
        {
            NS_INIT_REFCNT();
            NS_ADDREF(mChannel);
        }

    virtual ~AsyncReadStreamAdaptor() { 
        mChannel->mAsyncReadStream = 0;
        NS_RELEASE(mChannel);
    }
    
    NS_DECL_ISUPPORTS

    nsresult
    IsPending(PRBool* aIsPending) {
        *aIsPending = (mRemaining != 0) && NS_SUCCEEDED(mAbortStatus);
        return NS_OK;
    }

    nsresult
    Cancel(nsresult status) {
      if (NS_SUCCEEDED(mAbortStatus)) {
        mAbortStatus = status;
        if (!mChannel->mCurrentReadRequest)
            return NS_ERROR_FAILURE;
        return mEventQueueStreamListener ?
            mEventQueueStreamListener->OnStopRequest(mChannel->mCurrentReadRequest, mContext, status, nsnull):
            status;
      } else {
        // Cancel has already been called...  Do not fire another OnStopRequest!
        return NS_OK;
      }
    }

    nsresult
    Suspend(void) { mSuspended = PR_TRUE; return NS_OK; }

    nsresult
    Resume(void) {
        if (!mSuspended)
            return NS_ERROR_FAILURE;
        mSuspended = PR_FALSE;
        return NextListenerEvent();
    }

    // nsIStreamListener methods, all of which delegate to the real listener

    // This is the heart of this class.
    // The OnDataAvailable() method is always called from an event processed by
    // the system event queue.  The event is sent from an
    // AsyncReadStreamAdaptor object to itself.  This method both forwards the
    // event to the downstream listener and causes another OnDataAvailable()
    // event to be enqueued.
    NS_IMETHOD
    OnDataAvailable(nsIRequest* request, nsISupports *aContext,
                    nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count) {
        nsresult rv;

        if (!mChannel->mCurrentReadRequest)
            return NS_ERROR_FAILURE;

        rv = mDownstreamListener->OnDataAvailable(mChannel->mCurrentReadRequest, aContext, inStr, sourceOffset, count);
        if (NS_FAILED(rv)) {
            Cancel(rv);
            return rv;
        }
        if (!mSuspended && NS_SUCCEEDED(mAbortStatus)) {
            rv = NextListenerEvent();
            if (NS_FAILED(rv)) {
                Fail();
                return rv;
            }
        }
        return rv;
    }

    NS_IMETHOD
    OnStartRequest(nsIRequest* request, nsISupports *aContext) {
        nsresult rv = NS_OK;
		
		NS_ASSERTION(mDownstreamListener, "no downstream listener");

		if (mDownstreamListener) {
            if (!mChannel->mCurrentReadRequest)
                return NS_ERROR_FAILURE;
            rv = mDownstreamListener->OnStartRequest(mChannel->mCurrentReadRequest, aContext);
		}

        if (NS_FAILED(rv))
            Cancel(rv);
        return rv;
    }

    NS_IMETHOD
    OnStopRequest(nsIRequest* request, nsISupports *aContext,
                  nsresult aStatus, const PRUnichar* aStatusArg) {
        nsresult rv = NS_OK;

		
		NS_ASSERTION(mDownstreamListener, "no downstream listener");

		if (mDownstreamListener) {
            if (!mChannel->mCurrentReadRequest)
                return NS_ERROR_FAILURE;
            rv = mDownstreamListener->OnStopRequest(mChannel->mCurrentReadRequest, aContext, aStatus, aStatusArg);
            mDownstreamListener = 0;
		}
		// Tricky: causes this instance to be free'ed because mEventQueueStreamListener
		// has a circular reference back to this.
        mEventQueueStreamListener = 0;
		return rv;
    }

    // nsIInputStream methods
    NS_IMETHOD
    Available(PRUint32 *aNumBytes) { return mAvailable; }

    NS_IMETHOD
    Read(char* aBuf, PRUint32 aCount, PRUint32 *aBytesRead) {
        if (NS_FAILED(mAbortStatus))
            return NS_BASE_STREAM_CLOSED;

        *aBytesRead = 0;
        aCount = PR_MIN(aCount, mAvailable);
        nsresult rv = mSyncStream->Read(aBuf, aCount, aBytesRead);
        mAvailable -= *aBytesRead;

        if (NS_FAILED(rv) && (rv != NS_BASE_STREAM_WOULD_BLOCK)) {
            Fail();
            return rv;
        }

        return NS_OK;
    }

    NS_IMETHOD ReadSegments(nsWriteSegmentFun writer, void * closure, PRUint32 count, PRUint32 *_retval) {
        return mSyncStream->ReadSegments(writer, closure, count, _retval);
    }

    NS_IMETHOD GetNonBlocking(PRBool *aNonBlocking) {
        NS_NOTREACHED("GetNonBlocking");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD GetObserver(nsIInputStreamObserver * *aObserver) {
        NS_NOTREACHED("GetObserver");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD SetObserver(nsIInputStreamObserver * aObserver) {
        NS_NOTREACHED("SetObserver");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD
    Close() {
        nsresult rv = mSyncStream->Close();
        mSyncStream = 0;
        mContext = 0;
        mDownstreamListener = 0;
        mEventQueueStreamListener = 0;
        return rv;
    }

    NS_IMETHOD
    GetTransferCount(nsLoadFlags *aTransferCount) {
        *aTransferCount = mRemaining;
        return NS_OK;
    }

    NS_IMETHOD
    SetTransferCount(nsLoadFlags aTransferCount) {
        mRemaining = aTransferCount;
        return NS_OK;
    }

    nsresult
    AsyncRead(nsIStreamListener* aListener, nsISupports* aContext,
              PRUint32 transferOffset, PRUint32 transferCount, 
              PRUint32 flags, nsIRequest **_retval) {

        nsresult rv;
        nsIEventQueue *eventQ;

        mContext = aContext;
        mDownstreamListener = aListener;

        NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueService, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), &eventQ);
        if (NS_FAILED(rv)) return rv;

        rv = NS_NewAsyncStreamListener(getter_AddRefs(mEventQueueStreamListener),
                                       NS_STATIC_CAST(nsIStreamListener*, this), eventQ);
        NS_RELEASE(eventQ);
        if (NS_FAILED(rv)) return rv;

        if (!mChannel->mCurrentReadRequest)
            return NS_ERROR_FAILURE;

        rv = mEventQueueStreamListener->OnStartRequest(mChannel->mCurrentReadRequest, aContext);
        if (NS_FAILED(rv)) return rv;

        return NextListenerEvent();
    }

protected:

    nsresult
    Fail(void) {
        if (!mChannel->mCurrentReadRequest)
            return NS_ERROR_FAILURE;

        mAbortStatus = NS_BINDING_ABORTED;
        return mEventQueueStreamListener->OnStopRequest(mChannel->mCurrentReadRequest, mContext, 
                                                        NS_BINDING_FAILED, nsnull);
    }

    // If more data remains in the source stream that the downstream consumer
    // has not yet been notified about, fire an OnDataAvailable event.
    // Otherwise, fire an OnStopRequest event.
    nsresult
    NextListenerEvent() {
        PRUint32 available;
        nsresult rv = mSyncStream->Available(&available);
        if (NS_FAILED(rv)) return rv;
        available -= mAvailable;
        available = PR_MIN(available, mRemaining);

        if (!mChannel->mCurrentReadRequest)
            return NS_ERROR_FAILURE;

        if (available) {
            PRUint32 size = PR_MIN(available, MEM_CACHE_SEGMENT_SIZE);
            rv = mEventQueueStreamListener->OnDataAvailable(mChannel->mCurrentReadRequest, mContext, this,
                                                      mDataAvailCursor, size);
            mDataAvailCursor += size;
            mRemaining -= size;
            mAvailable += size;
            return rv;
        } else {
            rv = mEventQueueStreamListener->OnStopRequest(mChannel->mCurrentReadRequest, mContext, NS_OK, nsnull);
            AsyncReadStreamAdaptor* thisAlias = this;
            NS_RELEASE(thisAlias);
            return rv;
        }
    }
    
private:
    nsCOMPtr<nsISupports>       mContext;        // Opaque context passed to AsyncRead()
    nsCOMPtr<nsIStreamListener> mEventQueueStreamListener; // Stream listener that has been proxied
    nsCOMPtr<nsIStreamListener> mDownstreamListener; // Original stream listener
    nsCOMPtr<nsIInputStream>    mSyncStream;     // Underlying synchronous stream that is
                                                 //   being converted to an async stream
    PRUint32                    mDataAvailCursor;
    PRUint32                    mRemaining;      // Size of AsyncRead request less bytes for
                                                 //   consumer OnDataAvailable's that were fired
    PRUint32                    mAvailable;      // Number of bytes for which OnDataAvailable fired
    nsMemCacheTransport*          mChannel;        // Associated memory cache channel, strong link
                                                 //   but can not use nsCOMPtr
    nsresult                    mAbortStatus;    // Abort() has been called
    PRBool                      mSuspended;      // Suspend() has been called
};

NS_IMPL_ISUPPORTS3(AsyncReadStreamAdaptor, nsIInputStream, 
                   nsIStreamListener, nsIStreamObserver)

// The only purpose of this output stream wrapper is to adjust the cache's
// overall occupancy as new data flows into the cache entry.
class MemCacheWriteStreamWrapper : public nsIOutputStream {
public:
    MemCacheWriteStreamWrapper(nsMemCacheTransport* aChannel, nsIOutputStream *aBaseStream):
        mBaseStream(aBaseStream), mChannel(aChannel)
        {
            NS_INIT_REFCNT();
            NS_ADDREF(mChannel);
        }

    virtual ~MemCacheWriteStreamWrapper() { NS_RELEASE(mChannel); };
    
    static nsresult
    Create(nsMemCacheTransport* aChannel, nsIOutputStream *aBaseStream, nsIOutputStream* *aWrapper) {
        MemCacheWriteStreamWrapper *wrapper =
            new MemCacheWriteStreamWrapper(aChannel, aBaseStream);
        if (!wrapper) return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(wrapper);
        *aWrapper = wrapper;
        return NS_OK;
    }
    
    NS_DECL_ISUPPORTS

    NS_IMETHOD
    Close() { return mBaseStream->Close(); }

    NS_IMETHOD
    Flush() { return mBaseStream->Flush(); }

    NS_IMETHOD
    Write(const char *aBuffer, PRUint32 aCount, PRUint32 *aNumWritten) {
        *aNumWritten = 0;
        nsresult rv = mBaseStream->Write(aBuffer, aCount, aNumWritten);
        mChannel->NotifyStorageInUse(*aNumWritten);
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

private:
    nsCOMPtr<nsIOutputStream>   mBaseStream;
    nsMemCacheTransport*          mChannel;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(MemCacheWriteStreamWrapper, nsIOutputStream)

nsMemCacheTransport::nsMemCacheTransport(nsMemCacheRecord *aRecord, nsILoadGroup *aLoadGroup)
    : mRecord(aRecord), mStartOffset(0), mStatus(NS_OK)
{
    NS_INIT_REFCNT();
    mRecord->mNumTransports++;
}

nsMemCacheTransport::~nsMemCacheTransport()
{
    mRecord->mNumTransports--;
}

NS_IMETHODIMP
nsMemCacheTransport::GetName(PRUnichar* *result)
{
    NS_NOTREACHED("nsMemCacheTransport::GetName");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMemCacheTransport::IsPending(PRBool* aIsPending)
{
    *aIsPending = PR_FALSE;
    if (!mAsyncReadStream)
        return NS_OK;
    return mAsyncReadStream->IsPending(aIsPending);
}

NS_IMETHODIMP
nsMemCacheTransport::GetStatus(nsresult *status)
{
    *status = mStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsMemCacheTransport::Cancel(nsresult status)
{
    mStatus = status;
    if (!mAsyncReadStream)
        return NS_ERROR_FAILURE;
    return mAsyncReadStream->Cancel(status);
}

NS_IMETHODIMP
nsMemCacheTransport::Suspend(void)
{
    if (!mAsyncReadStream)
        return NS_ERROR_FAILURE;
    return mAsyncReadStream->Suspend();
}

NS_IMETHODIMP
nsMemCacheTransport::Resume(void)
{
    if (!mAsyncReadStream)
        return NS_ERROR_FAILURE;
    return mAsyncReadStream->Resume();
}

#if 0
NS_IMETHODIMP
nsMemCacheTransport::GetOriginalURI(nsIURI* *aURI)
{
    // Not required
    NS_NOTREACHED("nsMemCacheTransport::GetOriginalURI");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMemCacheTransport::SetOriginalURI(nsIURI* aURI)
{
    // Not required
    NS_NOTREACHED("nsMemCacheTransport::SetOriginalURI");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMemCacheTransport::GetURI(nsIURI* *aURI)
{
    // Not required to be implemented, since it is implemented by cache manager
    NS_NOTREACHED("nsMemCacheTransport::GetURI");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMemCacheTransport::SetURI(nsIURI* aURI)
{
    // Not required to be implemented, since it is implemented by cache manager
    NS_NOTREACHED("nsMemCacheTransport::SetURI");
    return NS_ERROR_NOT_IMPLEMENTED;
}
#endif

NS_IMETHODIMP
nsMemCacheTransport::OpenInputStream(PRUint32 transferOffset,
                                     PRUint32 transferCount,
                                     PRUint32 transferFlags,
                                     nsIInputStream* *aResult)

{
    nsresult rv;
    NS_ENSURE_ARG(aResult);
    if (mInputStream)
        return NS_ERROR_NOT_AVAILABLE;

    rv = mRecord->mStorageStream->NewInputStream(transferOffset, getter_AddRefs(mInputStream));
	if (NS_FAILED(rv)) return rv;
    *aResult = mInputStream;
    NS_ADDREF(*aResult);
    return rv;
}

NS_IMETHODIMP
nsMemCacheTransport::OpenOutputStream(PRUint32 transferOffset,
                                      PRUint32 transferCount,
                                      PRUint32 transferFlags,
                                      nsIOutputStream* *aResult)
{
    nsresult rv;
    NS_ENSURE_ARG(aResult);

    nsCOMPtr<nsIOutputStream> outputStream;

    PRUint32 oldLength;
    mRecord->mStorageStream->GetLength(&oldLength);
    rv = mRecord->mStorageStream->GetOutputStream(transferOffset, getter_AddRefs(outputStream));
    if (NS_FAILED(rv)) return rv;
    if (transferOffset < oldLength)
        NotifyStorageInUse(transferOffset - oldLength);

    return MemCacheWriteStreamWrapper::Create(this, outputStream, aResult);
}

NS_IMETHODIMP
nsMemCacheTransport::AsyncRead(nsIStreamListener *aListener,
                               nsISupports *aContext,
                               PRUint32 transferOffset,
                               PRUint32 transferCount,
                               PRUint32 transferFlags,
                               nsIRequest **_retval)
{
    nsCOMPtr<nsIInputStream> inputStream;
    nsresult rv = OpenInputStream(0, 0, 0, getter_AddRefs(inputStream));
    if (NS_FAILED(rv)) return rv;
    
    AsyncReadStreamAdaptor *asyncReadStreamAdaptor;
    asyncReadStreamAdaptor = new AsyncReadStreamAdaptor(this, inputStream);
    if (!asyncReadStreamAdaptor)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(asyncReadStreamAdaptor);
    mAsyncReadStream = asyncReadStreamAdaptor;

    rv = asyncReadStreamAdaptor->AsyncRead(aListener, aContext,
                                           transferOffset,
                                           transferCount,
                                           transferFlags,
                                           getter_AddRefs(mCurrentReadRequest));
    if (NS_FAILED(rv)) {
        mAsyncReadStream = nsnull;
        NS_RELEASE(asyncReadStreamAdaptor);
    }

    NS_ADDREF(*_retval=this);
    return rv;
}

NS_IMETHODIMP
nsMemCacheTransport::AsyncWrite(nsIStreamProvider *provider,
                                nsISupports *ctxt,
                                PRUint32 transferOffset,
                                PRUint32 transferCount,
                                PRUint32 transferFlags,
                                nsIRequest **_retval)
{
    // Not required to be implemented
    NS_NOTREACHED("nsMemCacheTransport::AsyncWrite");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsMemCacheTransport::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    *aSecurityInfo = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsMemCacheTransport::GetProgressEventSink(nsIProgressEventSink **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMemCacheTransport::SetProgressEventSink(nsIProgressEventSink *aProgress)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMemCacheTransport::GetTransport(nsITransport **result)
{
    NS_ENSURE_ARG_POINTER(result);
    NS_ADDREF(*result = this);
    return NS_OK;
}
