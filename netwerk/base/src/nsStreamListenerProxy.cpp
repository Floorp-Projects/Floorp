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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
 */

#include "nsStreamListenerProxy.h"
#include "nsIGenericFactory.h"
#include "nsIInputStream.h"
#include "nsIPipe.h"
#include "nsAutoLock.h"

#define LOG(args) PR_LOG(gStreamProxyLog, PR_LOG_DEBUG, args)

#define DEFAULT_BUFFER_SEGMENT_SIZE 2048
#define DEFAULT_BUFFER_MAX_SIZE  (4*2048)

//
//----------------------------------------------------------------------------
// Design Overview
//
// A stream listener proxy maintains a pipe.  When the request makes data
// available, the proxy copies as much of that data as possible into the pipe.
// If data was written to the pipe, then the proxy posts an asynchronous event
// corresponding to the amount of data written.  If no data could be written,
// because the pipe was full, then WOULD_BLOCK is returned to the request,
// indicating that the request should suspend itself.
//
// Once suspended in this manner, the request is only resumed when the pipe is
// emptied.
//
// XXX The current design does NOT allow the request to be "externally"
// suspended!!  For the moment this is not a problem, but it should be fixed.
//----------------------------------------------------------------------------
//

//
//----------------------------------------------------------------------------
// nsStreamListenerProxy implementation...
//----------------------------------------------------------------------------
//

nsStreamListenerProxy::nsStreamListenerProxy()
    : mLock(nsnull)
    , mPendingCount(0)
    , mPipeEmptied(PR_FALSE)
    , mListenerStatus(NS_OK)
{ }

nsStreamListenerProxy::~nsStreamListenerProxy()
{
    if (mLock) {
        PR_DestroyLock(mLock);
        mLock = nsnull;
    }
}

PRUint32
nsStreamListenerProxy::GetPendingCount()
{
    return PR_AtomicSet((PRInt32 *) &mPendingCount, 0);
}

//
//----------------------------------------------------------------------------
// nsOnDataAvailableEvent internal class...
//----------------------------------------------------------------------------
//
class nsOnDataAvailableEvent : public nsStreamObserverEvent
{
public:
    nsOnDataAvailableEvent(nsStreamProxyBase *aProxy,
                           nsIRequest *aRequest,
                           nsISupports *aContext,
                           nsIInputStream *aSource,
                           PRUint32 aOffset)
        : nsStreamObserverEvent(aProxy, aRequest, aContext)
        , mSource(aSource)
        , mOffset(aOffset)
    {
        MOZ_COUNT_CTOR(nsOnDataAvailableEvent);
    }

   ~nsOnDataAvailableEvent()
    {
        MOZ_COUNT_DTOR(nsOnDataAvailableEvent);
    }

    NS_IMETHOD HandleEvent();

protected:
   nsCOMPtr<nsIInputStream> mSource;
   PRUint32                 mOffset;
};

NS_IMETHODIMP
nsOnDataAvailableEvent::HandleEvent()
{
    LOG(("nsOnDataAvailableEvent: HandleEvent [event=%x, chan=%x]", this, mRequest.get()));

    nsStreamListenerProxy *listenerProxy =
        NS_STATIC_CAST(nsStreamListenerProxy *, mProxy);

    if (NS_FAILED(listenerProxy->GetListenerStatus())) {
        LOG(("nsOnDataAvailableEvent: Discarding event [listener_status=%x, chan=%x]\n",
            listenerProxy->GetListenerStatus(), mRequest.get()));
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIStreamListener> listener = listenerProxy->GetListener();
    if (!listener) {
        LOG(("nsOnDataAvailableEvent: Already called OnStopRequest (listener is NULL), [chan=%x]\n",
            mRequest.get()));
        return NS_ERROR_FAILURE;
    }

    nsresult status = NS_OK;
    nsresult rv = mRequest->GetStatus(&status);
    NS_ASSERTION(NS_SUCCEEDED(rv), "GetStatus failed");

    //
    // We should only forward this event to the listener if the request is
    // still in a "good" state.  Because these events are being processed
    // asynchronously, there is a very real chance that the listener might
    // have cancelled the request after _this_ event was triggered.
    //
    if (NS_SUCCEEDED(status)) {
        //
        // Find out from the listener proxy how many bytes to report.
        //
        PRUint32 count = listenerProxy->GetPendingCount();

#if defined(PR_LOGGING)
        {
            PRUint32 avail;
            mSource->Available(&avail);
            LOG(("nsOnDataAvailableEvent: Calling the consumer's OnDataAvailable "
                "[offset=%u count=%u avail=%u chan=%x]\n",
                mOffset, count, avail, mRequest.get()));
        }
#endif

        // Forward call to listener
        rv = listener->OnDataAvailable(mRequest, mContext, mSource, mOffset, count);

        LOG(("nsOnDataAvailableEvent: Done with the consumer's OnDataAvailable "
            "[rv=%x, chan=%x]\n",
            rv, mRequest.get()));

        //
        // XXX Need to suspend the underlying request... must consider
        //     other pending events (such as OnStopRequest). These
        //     should not be forwarded to the listener if the request
        //     is suspended. Also, handling the Resume could be tricky.
        //
        if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
            NS_NOTREACHED("listener returned NS_BASE_STREAM_WOULD_BLOCK"
                          " -- support for this is not implemented");
            rv = NS_ERROR_NOT_IMPLEMENTED;
        }

        // Cancel the request on unexpected errors
        if (NS_FAILED(rv) && (rv != NS_BASE_STREAM_CLOSED)) {
            LOG(("OnDataAvailable failed [rv=%x] canceling request!\n"));
            mRequest->Cancel(rv);
        }

        //
        // Update the listener status
        //
        listenerProxy->SetListenerStatus(rv);
    }
    else
        LOG(("nsOnDataAvailableEvent: Not calling OnDataAvailable [chan=%x]",
            mRequest.get()));
    return NS_OK;
}

//
//----------------------------------------------------------------------------
// nsISupports implementation...
//----------------------------------------------------------------------------
//
NS_IMPL_ISUPPORTS_INHERITED3(nsStreamListenerProxy,
                             nsStreamProxyBase,
                             nsIStreamListenerProxy,
                             nsIStreamListener,
                             nsIInputStreamObserver)

//
//----------------------------------------------------------------------------
// nsIStreamObserver implementation...
//----------------------------------------------------------------------------
//
NS_IMETHODIMP
nsStreamListenerProxy::OnStartRequest(nsIRequest *aRequest,
                                      nsISupports *aContext)
{
    nsresult rv = mPipeIn->SetObserver(this);
    if (NS_FAILED(rv)) return rv;

    return nsStreamProxyBase::OnStartRequest(aRequest, aContext);
}

NS_IMETHODIMP
nsStreamListenerProxy::OnStopRequest(nsIRequest *aRequest,
                                     nsISupports *aContext,
                                     nsresult aStatus,
                                     const PRUnichar *aStatusText)
{
    mPipeIn = 0;
    mPipeOut = 0;

    return nsStreamProxyBase::OnStopRequest(aRequest, aContext,
                                            aStatus, aStatusText);
}

//
//----------------------------------------------------------------------------
// nsIStreamListener implementation...
//----------------------------------------------------------------------------
//
NS_IMETHODIMP
nsStreamListenerProxy::OnDataAvailable(nsIRequest *aRequest,
                                       nsISupports *aContext,
                                       nsIInputStream *aSource,
                                       PRUint32 aOffset,
                                       PRUint32 aCount)
{
    nsresult rv;
    PRUint32 bytesWritten=0;

    LOG(("nsStreamListenerProxy: OnDataAvailable [offset=%u count=%u chan=%x]\n",
         aOffset, aCount, aRequest));

    NS_PRECONDITION(mRequestToResume == 0, "Unexpected call to OnDataAvailable");
    NS_PRECONDITION(mPipeIn, "Pipe not initialized");
    NS_PRECONDITION(mPipeOut, "Pipe not initialized");

    //
    // Any non-successful listener status gets passed back to the caller
    //
    {
        nsresult status = mListenerStatus;
        if (NS_FAILED(status)) {
            LOG(("nsStreamListenerProxy: Listener failed [status=%x chan=%x]\n", status, aRequest));
            return status;
        }
    }
    
    while (1) {
        mPipeEmptied = PR_FALSE;
        // 
        // Try to copy data into the pipe.
        //
        // If the pipe is full, then we return NS_BASE_STREAM_WOULD_BLOCK
        // so the calling request can suspend itself.  If, however, we detect
        // that the pipe was emptied during this time, we retry copying data
        // into the pipe.
        //
        rv = mPipeOut->WriteFrom(aSource, aCount, &bytesWritten);

        LOG(("nsStreamListenerProxy: Wrote data to pipe [rv=%x count=%u bytesWritten=%u chan=%x]\n",
            rv, aCount, bytesWritten, aRequest));

        if (NS_FAILED(rv)) {
            if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
                nsAutoLock lock(mLock);
                if (mPipeEmptied) {
                    LOG(("nsStreamListenerProxy: Pipe emptied; looping back [chan=%x]\n", aRequest));
                    continue;
                }
                LOG(("nsStreamListenerProxy: Pipe full; setting request to resume [chan=%x]\n", aRequest));
                mRequestToResume = aRequest;
            }
            return rv;
        }
        if (bytesWritten == 0) {
            LOG(("nsStreamListenerProxy: Copied zero bytes; not posting an event [chan=%x]\n", aRequest));
            return NS_OK;
        }

        // Copied something into the pipe...
        break;
    }

    //
    // Update the pending count; return if able to piggy-back on a pending event.
    //
    PRUint32 total = PR_AtomicAdd((PRInt32 *) &mPendingCount, bytesWritten);
    if (total > bytesWritten) {
        LOG(("nsStreamListenerProxy: Piggy-backing pending event [total=%u, chan=%x]\n",
            total, aRequest));
        return NS_OK;
    }
 
    //
    // Post an event for the number of bytes actually written.
    //
    nsOnDataAvailableEvent *ev =
        new nsOnDataAvailableEvent(this, aRequest, aContext, mPipeIn, aOffset);
    if (!ev) return NS_ERROR_OUT_OF_MEMORY;

    rv = ev->FireEvent(GetEventQueue());
    if (NS_FAILED(rv)) {
        delete ev;
        return rv;
    }
    return NS_OK;
}

//
//----------------------------------------------------------------------------
// nsIStreamListenerProxy implementation...
//----------------------------------------------------------------------------
//
NS_IMETHODIMP
nsStreamListenerProxy::Init(nsIStreamListener *aListener,
                            nsIEventQueue *aEventQ,
                            PRUint32 aBufferSegmentSize,
                            PRUint32 aBufferMaxSize)
{
    NS_PRECONDITION(GetReceiver() == nsnull, "Listener already set");
    NS_PRECONDITION(GetEventQueue() == nsnull, "Event queue already set");

    //
    // Create the ChannelToResume lock
    //
    mLock = PR_NewLock();
    if (!mLock) return NS_ERROR_OUT_OF_MEMORY;

    //
    // Create the pipe
    //
    if (aBufferSegmentSize == 0)
        aBufferSegmentSize = DEFAULT_BUFFER_SEGMENT_SIZE;
    if (aBufferMaxSize == 0)
        aBufferMaxSize = DEFAULT_BUFFER_MAX_SIZE;
    // The segment size must not exceed the maximum
    aBufferSegmentSize = PR_MIN(aBufferMaxSize, aBufferSegmentSize);

    nsresult rv = NS_NewPipe(getter_AddRefs(mPipeIn),
                             getter_AddRefs(mPipeOut),
                             aBufferSegmentSize,
                             aBufferMaxSize,
                             PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStreamObserver> observer = do_QueryInterface(aListener);
    SetReceiver(observer);
    return SetEventQueue(aEventQ);
}

//
//----------------------------------------------------------------------------
// nsIInputStreamObserver implementation...
//----------------------------------------------------------------------------
//
NS_IMETHODIMP
nsStreamListenerProxy::OnEmpty(nsIInputStream *aInputStream)
{
    LOG(("nsStreamListenerProxy: OnEmpty\n"));
    //
    // The pipe has been emptied by the listener.  If the request
    // has been suspended (waiting for the pipe to be emptied), then
    // go ahead and resume it.  But take care not to resume while 
    // holding the "ChannelToResume" lock.
    //
    nsCOMPtr<nsIRequest> req;
    {
        nsAutoLock lock(mLock);
        if (mRequestToResume) {
            req = mRequestToResume;
            mRequestToResume = 0;
        }
        mPipeEmptied = PR_TRUE; // Flag this call
    }
    if (req) {
        LOG(("nsStreamListenerProxy: Resuming request\n"));
        req->Resume();
    }
    return NS_OK;
}

NS_IMETHODIMP
nsStreamListenerProxy::OnClose(nsIInputStream *aInputStream)
{
    LOG(("nsStreamListenerProxy: OnClose\n"));
    return NS_OK;
}
