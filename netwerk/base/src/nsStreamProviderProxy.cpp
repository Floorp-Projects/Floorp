/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsStreamProviderProxy.h"
#include "nsIPipe.h"
#include "nsIRequest.h"
#include "prlog.h"

#if defined(PR_LOGGING)
static PRLogModuleInfo *gStreamProviderProxyLog;
#endif

#define LOG(args) PR_LOG(gStreamProviderProxyLog, PR_LOG_DEBUG, args)

#define DEFAULT_BUFFER_SEGMENT_SIZE 4096
#define DEFAULT_BUFFER_MAX_SIZE  (4*4096)

//----------------------------------------------------------------------------
// Design Overview
//
// A stream provider proxy maintains a pipe.  When requested to provide data
// to the request, whatever data is in the pipe (up to the amount requested)
// is provided to the request.  If there is no data in the pipe, then the 
// proxy posts an asynchronous event for more data, and returns WOULD_BLOCK
// indicating that the request should suspend itself.
//
// The request is only resumed once the event has been successfully handled;
// meaning that data has been written to the pipe.
//----------------------------------------------------------------------------

nsStreamProviderProxy::nsStreamProviderProxy()
    : mProviderStatus(NS_OK)
{
    NS_INIT_ISUPPORTS();
}

nsStreamProviderProxy::~nsStreamProviderProxy()
{
    NS_IF_RELEASE(mObserverProxy);
}

nsresult
nsStreamProviderProxy::GetProvider(nsIStreamProvider **provider)
{
    NS_ENSURE_TRUE(mObserverProxy, NS_ERROR_NOT_INITIALIZED);
    return CallQueryInterface(mObserverProxy->Observer(), provider);
}

//----------------------------------------------------------------------------
// nsOnDataWritableEvent internal class...
//----------------------------------------------------------------------------

class nsOnDataWritableEvent : public nsARequestObserverEvent
{
public:
    nsOnDataWritableEvent(nsStreamProviderProxy *proxy,
                          nsIRequest *request,
                          nsISupports *context,
                          nsIOutputStream *sink,
                          PRUint32 offset,
                          PRUint32 count)
        : nsARequestObserverEvent(request, context)
        , mProxy(proxy)
        , mSink(sink)
        , mOffset(offset)
        , mCount(count)
    {
        MOZ_COUNT_CTOR(nsOnDataWritableEvent);
        NS_PRECONDITION(mProxy, "null pointer");
        NS_ADDREF(mProxy);
    }

   ~nsOnDataWritableEvent()
    {
        MOZ_COUNT_DTOR(nsOnDataWritableEvent);
        NS_RELEASE(mProxy);
    }

    void HandleEvent();

protected:
    nsStreamProviderProxy    *mProxy; 
    nsCOMPtr<nsIOutputStream> mSink;
    PRUint32                  mOffset;
    PRUint32                  mCount;
};

void
nsOnDataWritableEvent::HandleEvent()
{
    LOG(("nsOnDataWritableEvent::HandleEvent [req=%x]", mRequest.get()));

    nsresult status = NS_OK;
    nsresult rv = mRequest->GetStatus(&status);
    NS_ASSERTION(NS_SUCCEEDED(rv), "GetStatus failed");

    //
    // We should only forward this event to the provider if the request is
    // still in a "good" state.  Because these events are being processed
    // asynchronously, there is a very real chance that the provider might
    // have cancelled the request after _this_ event was triggered.
    //
    if (NS_FAILED(status)) {
        LOG(("nsOnDataWritableEvent: Not calling OnDataWritable"));
        return;
    }

    LOG(("nsOnDataWritableEvent: Calling the consumer's OnDataWritable\n"));

    nsCOMPtr<nsIStreamProvider> provider;
    rv = mProxy->GetProvider(getter_AddRefs(provider));

    if (provider)
        rv = provider->OnDataWritable(mRequest, mContext,
                                      mSink, mOffset, mCount);

    LOG(("nsOnDataWritableEvent: Done with the consumer's OnDataWritable "
         "[rv=%x]\n", rv));

    //
    // Mask NS_BASE_STREAM_WOULD_BLOCK return values.
    //
    mProxy->SetProviderStatus(rv != NS_BASE_STREAM_WOULD_BLOCK ? rv : NS_OK);

    //
    // The request is already suspended, so unless the provider returned
    // NS_BASE_STREAM_WOULD_BLOCK, we should wake up the request.
    //
    if (rv != NS_BASE_STREAM_WOULD_BLOCK)
        mRequest->Resume();
}

//----------------------------------------------------------------------------
// nsStreamProviderProxy::nsISupports implementation...
//----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS3(nsStreamProviderProxy,
                              nsIStreamProvider,
                              nsIRequestObserver,
                              nsIStreamProviderProxy)

//----------------------------------------------------------------------------
// nsStreamProviderProxy::nsIRequestObserver implementation...
//----------------------------------------------------------------------------

NS_IMETHODIMP
nsStreamProviderProxy::OnStartRequest(nsIRequest *request,
                                      nsISupports *context)
{
    NS_ENSURE_TRUE(mObserverProxy, NS_ERROR_NOT_INITIALIZED);
    return mObserverProxy->OnStartRequest(request, context);
}

NS_IMETHODIMP
nsStreamProviderProxy::OnStopRequest(nsIRequest* request,
                                     nsISupports *context,
                                     nsresult status)
{
    NS_ENSURE_TRUE(mObserverProxy, NS_ERROR_NOT_INITIALIZED);

    // Close the pipe
    mPipeIn = 0;
    mPipeOut = 0;

    return mObserverProxy->OnStopRequest(request, context, status);
}

//----------------------------------------------------------------------------
// nsStreamProviderProxy::nsIStreamProvider implementation...
//----------------------------------------------------------------------------

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
nsStreamProviderProxy::OnDataWritable(nsIRequest *request,
                                      nsISupports *context,
                                      nsIOutputStream *sink,
                                      PRUint32 offset,
                                      PRUint32 count)
{
    nsresult rv;

    LOG(("nsStreamProviderProxy: OnDataWritable [offset=%u, count=%u]\n",
        offset, count));

    NS_ENSURE_TRUE(mObserverProxy, NS_ERROR_NOT_INITIALIZED);
    NS_PRECONDITION(count > 0, "Invalid parameter");
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
    // Provide the request with whatever data is already in the pipe (not
    // exceeding count).
    //
    PRUint32 amount;
    rv = mPipeIn->Available(&amount);
    if (NS_FAILED(rv)) return rv;

    if (amount > 0) {
        amount = PR_MIN(amount, count);

        PRUint32 bytesWritten;
        rv = mPipeIn->ReadSegments(nsWriteToSink, sink, amount, &bytesWritten);
        if (NS_FAILED(rv)) return rv;

        return NS_OK;
    }

    //
    // Post an event requesting the provider for more data.
    //
    nsOnDataWritableEvent *ev =
        new nsOnDataWritableEvent(this, request, context, mPipeOut, offset, count);
    if (!ev)
        return NS_ERROR_OUT_OF_MEMORY;

    // Reuse the event queue of the observer proxy
    rv = mObserverProxy->FireEvent(ev);
    if (NS_FAILED(rv)) {
        delete ev;
        return rv;
    }
    return NS_BASE_STREAM_WOULD_BLOCK;
}

//----------------------------------------------------------------------------
// nsStreamProviderProxy::nsIStreamProviderProxy implementation...
//----------------------------------------------------------------------------

NS_IMETHODIMP
nsStreamProviderProxy::Init(nsIStreamProvider *provider,
                            nsIEventQueue *eventQ,
                            PRUint32 bufferSegmentSize,
                            PRUint32 bufferMaxSize)
{
    NS_ENSURE_ARG_POINTER(provider);

#if defined(PR_LOGGING)
    if (!gStreamProviderProxyLog)
        gStreamProviderProxyLog = PR_NewLogModule("nsStreamProviderProxy");
#endif

    // 
    // Create the request observer proxy
    //
    mObserverProxy = new nsRequestObserverProxy();
    if (!mObserverProxy) return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(mObserverProxy);

    //
    // Create the pipe
    //
    if (bufferSegmentSize == 0)
        bufferSegmentSize = DEFAULT_BUFFER_SEGMENT_SIZE;
    if (bufferMaxSize == 0)
        bufferMaxSize = DEFAULT_BUFFER_MAX_SIZE;
    // The segment size must not exceed the maximum
    bufferSegmentSize = PR_MIN(bufferMaxSize, bufferSegmentSize);

    nsresult rv = NS_NewPipe(getter_AddRefs(mPipeIn),
                             getter_AddRefs(mPipeOut),
                             bufferSegmentSize,
                             bufferMaxSize,
                             PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRequestObserver> observer = do_QueryInterface(provider);
    return mObserverProxy->Init(observer, eventQ);
}
