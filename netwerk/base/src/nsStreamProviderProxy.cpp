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

#include "nsStreamProviderProxy.h"
#include "nsIPipe.h"

#define LOG(args) PR_LOG(gStreamProxyLog, PR_LOG_DEBUG, args)

#define DEFAULT_BUFFER_SEGMENT_SIZE 2048
#define DEFAULT_BUFFER_MAX_SIZE  (4*2048)

//
//----------------------------------------------------------------------------
// Design Overview
//
// A stream provider proxy maintains a pipe.  When requested to provide data
// to the channel, whatever data is in the pipe (up to the amount requested)
// is provided to the channel.  If there is no data in the pipe, then the 
// proxy posts an asynchronous event for more data, and returns WOULD_BLOCK
// indicating that the channel should suspend itself.
//
// The channel is only resumed once the event has been successfully handled;
// meaning that data has been written to the pipe.
//----------------------------------------------------------------------------
//

nsStreamProviderProxy::nsStreamProviderProxy()
    : mProviderStatus(NS_OK)
{ }

nsStreamProviderProxy::~nsStreamProviderProxy()
{ }

//
//----------------------------------------------------------------------------
// nsOnDataWritableEvent internal class...
//----------------------------------------------------------------------------
//
class nsOnDataWritableEvent : public nsStreamObserverEvent
{
public:
    nsOnDataWritableEvent(nsStreamProxyBase *aProxy,
                         nsIChannel *aChannel,
                         nsISupports *aContext,
                         nsIOutputStream *aSink,
                         PRUint32 aOffset,
                         PRUint32 aCount)
        : nsStreamObserverEvent(aProxy, aChannel, aContext)
        , mSink(aSink)
        , mOffset(aOffset)
        , mCount(aCount)
    {
        MOZ_COUNT_CTOR(nsOnDataWritableEvent);
    }

   ~nsOnDataWritableEvent()
    {
        MOZ_COUNT_DTOR(nsOnDataWritableEvent);
    }

    NS_IMETHOD HandleEvent();

protected:
   nsCOMPtr<nsIOutputStream> mSink;
   PRUint32                  mOffset;
   PRUint32                  mCount;
};

NS_IMETHODIMP
nsOnDataWritableEvent::HandleEvent()
{
    LOG(("nsOnDataWritableEvent: HandleEvent [event=%x chan=%x]",
        this, mChannel.get()));

    nsStreamProviderProxy *providerProxy = 
        NS_STATIC_CAST(nsStreamProviderProxy *, mProxy);

    nsCOMPtr<nsIStreamProvider> provider = providerProxy->GetProvider();
    if (!provider) {
        LOG(("nsOnDataWritableEvent: Already called OnStopRequest (provider is NULL)\n"));
        return NS_ERROR_FAILURE;
    }

    nsresult status = NS_OK;
    nsresult rv = mChannel->GetStatus(&status);
    NS_ASSERTION(NS_SUCCEEDED(rv), "GetStatus failed");

    //
    // We should only forward this event to the provider if the channel is
    // still in a "good" state.  Because these events are being processed
    // asynchronously, there is a very real chance that the provider might
    // have cancelled the channel after _this_ event was triggered.
    //
    if (NS_SUCCEEDED(status)) {
        LOG(("nsOnDataWritableEvent: Calling the consumer's OnDataWritable\n"));
        rv = provider->OnDataWritable(mChannel, mContext, mSink, mOffset, mCount);
        LOG(("nsOnDataWritableEvent: Done with the consumer's OnDataWritable [rv=%x]\n", rv));

        //
        // Mask NS_BASE_STREAM_WOULD_BLOCK return values.
        //
        providerProxy->SetProviderStatus(
            rv != NS_BASE_STREAM_WOULD_BLOCK ? rv : NS_OK);

        //
        // The channel is already suspended, so unless the provider returned
        // NS_BASE_STREAM_WOULD_BLOCK, we should wake up the channel.
        //
        if (rv != NS_BASE_STREAM_WOULD_BLOCK)
            mChannel->Resume();
    }
    else
        LOG(("nsOnDataWritableEvent: Not calling OnDataWritable"));
    return NS_OK;
}

//
//----------------------------------------------------------------------------
// nsISupports implementation...
//----------------------------------------------------------------------------
//
NS_IMPL_ISUPPORTS_INHERITED2(nsStreamProviderProxy,
                             nsStreamProxyBase,
                             nsIStreamProviderProxy,
                             nsIStreamProvider)

//
//----------------------------------------------------------------------------
// nsIStreamObserver implementation...
//----------------------------------------------------------------------------
//
NS_IMETHODIMP
nsStreamProviderProxy::OnStartRequest(nsIChannel *aChannel,
                                      nsISupports *aContext)
{
    return nsStreamProxyBase::OnStartRequest(aChannel, aContext);
}

NS_IMETHODIMP
nsStreamProviderProxy::OnStopRequest(nsIChannel *aChannel,
                                     nsISupports *aContext,
                                     nsresult aStatus,
                                     const PRUnichar *aStatusText)
{
    //
    // Close the pipe
    //
    mPipeIn = 0;
    mPipeOut = 0;

    return nsStreamProxyBase::OnStopRequest(aChannel, aContext,
                                            aStatus, aStatusText);
}

//
//----------------------------------------------------------------------------
// nsIStreamProvider implementation...
//----------------------------------------------------------------------------
//
static NS_METHOD
nsWriteToSink(nsIInputStream *source,
              void *closure,
              const char *fromRawSegment,
              PRUint32 offset,
              PRUint32 count,
              PRUint32 *writeCount)
{
    nsIOutputStream *sink = (nsIOutputStream *) closure;
    return sink->Write(fromRawSegment, count, writeCount);
}

NS_IMETHODIMP
nsStreamProviderProxy::OnDataWritable(nsIChannel *aChannel,
                                      nsISupports *aContext,
                                      nsIOutputStream *aSink,
                                      PRUint32 aOffset,
                                      PRUint32 aCount)
{
    nsresult rv;

    LOG(("nsStreamProviderProxy: OnDataWritable [offset=%u, count=%u]\n",
        aOffset, aCount));

    NS_PRECONDITION(aCount > 0, "Invalid parameter");
    NS_PRECONDITION(mPipeIn, "Pipe not initialized");
    NS_PRECONDITION(mPipeOut, "Pipe not initialized");
    NS_PRECONDITION(mProviderStatus != NS_BASE_STREAM_WOULD_BLOCK,
                    "Invalid provider status");

    //
    // Any non-successful provider status gets passed back to the caller
    //
    if (NS_FAILED(mProviderStatus)) {
        LOG(("nsStreamProviderProxy: Provider failed [status=%x]\n", mProviderStatus));
        return mProviderStatus;
    }

    //
    // Provide the channel with whatever data is already in the pipe (not
    // exceeding aCount).
    //
    PRUint32 count;
    rv = mPipeIn->Available(&count);
    if (NS_FAILED(rv)) return rv;

    if (count > 0) {
        count = PR_MIN(count, aCount);

        PRUint32 bytesWritten;
        rv = mPipeIn->ReadSegments(nsWriteToSink, aSink, count, &bytesWritten);
        if (NS_FAILED(rv)) return rv;

        return NS_OK;
    }

    //
    // Post an event requesting the provider for more data.
    //
    nsOnDataWritableEvent *ev =
        new nsOnDataWritableEvent(this, aChannel, aContext,
                                 mPipeOut, aOffset, aCount);
    if (!ev)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = ev->FireEvent(GetEventQueue());
    if (NS_FAILED(rv)) {
        delete ev;
        return rv;
    }
    return NS_BASE_STREAM_WOULD_BLOCK;
}

//
//----------------------------------------------------------------------------
// nsIStreamProviderProxy implementation...
//----------------------------------------------------------------------------
//
NS_IMETHODIMP
nsStreamProviderProxy::Init(nsIStreamProvider *aProvider,
                            nsIEventQueue *aEventQ,
                            PRUint32 aBufferSegmentSize,
                            PRUint32 aBufferMaxSize)
{
    NS_PRECONDITION(GetReceiver() == nsnull, "Listener already set");
    NS_PRECONDITION(GetEventQueue() == nsnull, "Event queue already set");

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

    SetReceiver(aProvider);
    return SetEventQueue(aEventQ);
}
