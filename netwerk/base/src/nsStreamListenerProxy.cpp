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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
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
// A stream listener proxy maintains a pipe.  When the channel makes data
// available, the proxy copies as much of that data as possible into the pipe.
// If data was written to the pipe, then the proxy posts an asynchronous event
// corresponding to the amount of data written.  If no data could be written,
// because the pipe was full, then WOULD_BLOCK is returned to the channel,
// indicating that the channel should suspend itself.
//
// Once suspended in this manner, the channel is only resumed when the pipe is
// emptied.
//
// XXX The current design does NOT allow the channel to be "externally"
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
                           nsIChannel *aChannel,
                           nsISupports *aContext,
                           nsIInputStream *aSource,
                           PRUint32 aOffset)
        : nsStreamObserverEvent(aProxy, aChannel, aContext)
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
    LOG(("nsOnDataAvailableEvent: HandleEvent [event=%x, chan=%x]", this, mChannel.get()));

    nsStreamListenerProxy *listenerProxy =
        NS_STATIC_CAST(nsStreamListenerProxy *, mProxy);

    if (NS_FAILED(listenerProxy->GetListenerStatus())) {
        LOG(("nsOnDataAvailableEvent: Discarding event [listener_status=%x, chan=%x]\n",
            listenerProxy->GetListenerStatus(), mChannel.get()));
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIStreamListener> listener = listenerProxy->GetListener();
    if (!listener) {
        LOG(("nsOnDataAvailableEvent: Already called OnStopRequest (listener is NULL), [chan=%x]\n",
            mChannel.get()));
        return NS_ERROR_FAILURE;
    }

    nsresult status = NS_OK;
    nsresult rv = mChannel->GetStatus(&status);
    NS_ASSERTION(NS_SUCCEEDED(rv), "GetStatus failed");

    //
    // We should only forward this event to the listener if the channel is
    // still in a "good" state.  Because these events are being processed
    // asynchronously, there is a very real chance that the listener might
    // have cancelled the channel after _this_ event was triggered.
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
                mOffset, count, avail, mChannel.get()));
        }
#endif

        // Forward call to listener
        rv = listener->OnDataAvailable(mChannel, mContext, mSource, mOffset, count);

        LOG(("nsOnDataAvailableEvent: Done with the consumer's OnDataAvailable "
            "[rv=%x, chan=%x]\n",
            rv, mChannel.get()));

        //
        // XXX Need to suspend the underlying channel... must consider
        //     other pending events (such as OnStopRequest). These
        //     should not be forwarded to the listener if the channel
        //     is suspended. Also, handling the Resume could be tricky.
        //
        if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
            NS_NOTREACHED("listener returned NS_BASE_STREAM_WOULD_BLOCK"
                          " -- support for this is not implemented");
            rv = NS_ERROR_NOT_IMPLEMENTED;
        }

        //
        // Update the listener status
        //
        listenerProxy->SetListenerStatus(rv);
    }
    else
        LOG(("nsOnDataAvailableEvent: Not calling OnDataAvailable [chan=%x]",
            mChannel.get()));
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
nsStreamListenerProxy::OnStartRequest(nsIChannel *aChannel,
                                      nsISupports *aContext)
{

    return nsStreamProxyBase::OnStartRequest(aChannel, aContext);
}

NS_IMETHODIMP
nsStreamListenerProxy::OnStopRequest(nsIChannel *aChannel,
                                     nsISupports *aContext,
                                     nsresult aStatus,
                                     const PRUnichar *aStatusText)
{
    //
    // We are done with the pipe.
    //
    mPipeIn = 0;
    mPipeOut = 0;

    return nsStreamProxyBase::OnStopRequest(aChannel, aContext,
                                            aStatus, aStatusText);
}

//
//----------------------------------------------------------------------------
// nsIStreamListener implementation...
//----------------------------------------------------------------------------
//
NS_IMETHODIMP
nsStreamListenerProxy::OnDataAvailable(nsIChannel *aChannel,
                                       nsISupports *aContext,
                                       nsIInputStream *aSource,
                                       PRUint32 aOffset,
                                       PRUint32 aCount)
{
    nsresult rv;
    PRUint32 bytesWritten=0;

    LOG(("nsStreamListenerProxy: OnDataAvailable [offset=%u count=%u chan=%x]\n",
         aOffset, aCount, aChannel));

    NS_PRECONDITION(mChannelToResume == 0, "Unexpected call to OnDataAvailable");
    NS_PRECONDITION(mPipeIn, "Pipe not initialized");
    NS_PRECONDITION(mPipeOut, "Pipe not initialized");

    //
    // Any non-successful listener status gets passed back to the caller
    //
    {
        nsresult status = mListenerStatus;
        if (NS_FAILED(status)) {
            LOG(("nsStreamListenerProxy: Listener failed [status=%x chan=%x]\n", status, aChannel));
            return status;
        }
    }
    
    //
    // Enter the ChannelToResume lock
    //
    {
        nsAutoLock lock(mLock);

        // 
        // Try to copy data into the pipe.
        //
        // If the pipe is full, then suspend the calling channel.  It
        // will be resumed when the pipe is emptied.  Being inside the 
        // ChannelToResume lock ensures that the resume will follow
        // the suspend.
        //
        rv = mPipeOut->WriteFrom(aSource, aCount, &bytesWritten);

        LOG(("nsStreamListenerProxy: Wrote data to pipe [rv=%x count=%u bytesWritten=%u chan=%x]\n",
            rv, aCount, bytesWritten, aChannel));

        if (NS_FAILED(rv)) {
            if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
                LOG(("nsStreamListenerProxy: Setting channel to resume [chan=%x]\n", aChannel));
                mChannelToResume = aChannel;
            }
            return rv;
        }
        else if (bytesWritten == 0) {
            LOG(("nsStreamListenerProxy: Copied zero bytes; not posting an event [chan=%x]\n", aChannel));
            return NS_BASE_STREAM_CLOSED; // there was no more data to read!
        }
    }

    //
    // Update the pending count; return if able to piggy-back on a pending event.
    //
    PRUint32 total = PR_AtomicAdd((PRInt32 *) &mPendingCount, bytesWritten);
    if (total > bytesWritten) {
        LOG(("nsStreamListenerProxy: Piggy-backing pending event [total=%u, chan=%x]\n",
            total, aChannel));
        return NS_OK;
    }
 
    //
    // Post an event for the number of bytes actually written.
    //
    nsOnDataAvailableEvent *ev =
        new nsOnDataAvailableEvent(this, aChannel, aContext, mPipeIn, aOffset);
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

    rv = mPipeIn->SetObserver(this);
    if (NS_FAILED(rv)) return rv;

    SetReceiver(aListener);
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
    // The pipe has been emptied by the listener.  If the channel
    // has been suspended (waiting for the pipe to be emptied), then
    // go ahead and resume it.  But take care not to resume while 
    // holding the "ChannelToResume" lock.
    //
    nsCOMPtr<nsIChannel> chan;
    {
        nsAutoLock lock(mLock);
        chan = mChannelToResume;
        mChannelToResume = 0;
    }
    if (chan) {
        LOG(("nsStreamListenerProxy: Resuming channel\n"));
        chan->Resume();
    }
    return NS_OK;
}

NS_IMETHODIMP
nsStreamListenerProxy::OnClose(nsIInputStream *aInputStream)
{
    LOG(("nsStreamListenerProxy: OnClose\n"));
    return NS_OK;
}
