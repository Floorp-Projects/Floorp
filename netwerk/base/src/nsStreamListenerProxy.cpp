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

#include "nsStreamListenerProxy.h"
#include "nsIGenericFactory.h"
#include "nsIInputStream.h"
#include "nsIPipe.h"
#include "nsAutoLock.h"
#include "prlog.h"

#if defined(PR_LOGGING)
static PRLogModuleInfo *gStreamListenerProxyLog;
#endif

#define LOG(args) PR_LOG(gStreamListenerProxyLog, PR_LOG_DEBUG, args)

#define DEFAULT_BUFFER_SEGMENT_SIZE 4096
#define DEFAULT_BUFFER_MAX_SIZE  (4*4096)

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

//----------------------------------------------------------------------------
// nsStreamListenerProxy implementation...
//----------------------------------------------------------------------------

nsStreamListenerProxy::nsStreamListenerProxy()
    : mLock(nsnull)
    , mPendingCount(0)
    , mPipeEmptied(PR_FALSE)
    , mListenerStatus(NS_OK)
{
    NS_INIT_ISUPPORTS();
}

nsStreamListenerProxy::~nsStreamListenerProxy()
{
    if (mLock) {
        PR_DestroyLock(mLock);
        mLock = nsnull;
    }
    NS_IF_RELEASE(mObserverProxy);
}

nsresult
nsStreamListenerProxy::GetListener(nsIStreamListener **listener)
{
    NS_ENSURE_TRUE(mObserverProxy, NS_ERROR_NOT_INITIALIZED);
    nsIRequestObserver* obs = mObserverProxy->Observer();
    if (!obs)
        return NS_ERROR_NULL_POINTER;
    return CallQueryInterface(obs, listener);
}

PRUint32
nsStreamListenerProxy::GetPendingCount()
{
    return PR_AtomicSet((PRInt32 *) &mPendingCount, 0);
}

//----------------------------------------------------------------------------
// nsOnDataAvailableEvent internal class...
//----------------------------------------------------------------------------

class nsOnDataAvailableEvent : public nsARequestObserverEvent
{
public:
    nsOnDataAvailableEvent(nsStreamListenerProxy *proxy,
                           nsIRequest *request,
                           nsISupports *context,
                           nsIInputStream *source,
                           PRUint32 offset)
        : nsARequestObserverEvent(request, context)
        , mProxy(proxy)
        , mSource(source)
        , mOffset(offset)
    {
        MOZ_COUNT_CTOR(nsOnDataAvailableEvent);
        NS_PRECONDITION(mProxy, "null pointer");
        NS_ADDREF(mProxy);
    }

   ~nsOnDataAvailableEvent()
    {
        MOZ_COUNT_DTOR(nsOnDataAvailableEvent);
        NS_RELEASE(mProxy);
    }

    void HandleEvent();

protected:
    nsStreamListenerProxy    *mProxy;
    nsCOMPtr<nsIInputStream>  mSource;
    PRUint32                  mOffset;
};

void
nsOnDataAvailableEvent::HandleEvent()
{
    LOG(("nsOnDataAvailableEvent: HandleEvent [req=%x]", mRequest.get()));

    if (NS_FAILED(mProxy->GetListenerStatus())) {
        LOG(("nsOnDataAvailableEvent: Discarding event [listener_status=%x, req=%x]\n",
            mProxy->GetListenerStatus(), mRequest.get()));
        return;
    }

    nsresult status = NS_OK;
    nsresult rv = mRequest->GetStatus(&status);
    NS_ASSERTION(NS_SUCCEEDED(rv), "GetStatus failed");

    // We should only forward this event to the listener if the request is
    // still in a "good" state.  Because these events are being processed
    // asynchronously, there is a very real chance that the listener might
    // have cancelled the request after _this_ event was triggered.

    if (NS_FAILED(status)) {
        LOG(("nsOnDataAvailableEvent: Not calling OnDataAvailable [req=%x]",
            mRequest.get()));
        return;
    }

    // Find out from the listener proxy how many bytes to report.
    PRUint32 count = mProxy->GetPendingCount();

#if defined(PR_LOGGING)
    {
        PRUint32 avail;
        mSource->Available(&avail);
        LOG(("nsOnDataAvailableEvent: Calling the consumer's OnDataAvailable "
            "[offset=%u count=%u avail=%u req=%x]\n",
            mOffset, count, avail, mRequest.get()));
    }
#endif

    nsCOMPtr<nsIStreamListener> listener;
    rv = mProxy->GetListener(getter_AddRefs(listener));

    LOG(("handle dataevent=%8lX\n",(long)this));

    // Forward call to listener
    if (listener)
        rv = listener->OnDataAvailable(mRequest, mContext,
                                       mSource, mOffset, count);

    LOG(("nsOnDataAvailableEvent: Done with the consumer's OnDataAvailable "
         "[rv=%x, req=%x]\n", rv, mRequest.get()));

    // XXX Need to suspend the underlying request... must consider
    //     other pending events (such as OnStopRequest). These
    //     should not be forwarded to the listener if the request
    //     is suspended. Also, handling the Resume could be tricky.

    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        NS_NOTREACHED("listener returned NS_BASE_STREAM_WOULD_BLOCK"
                      " -- support for this is not implemented");
        rv = NS_BINDING_FAILED;
    }

    // Cancel the request on unexpected errors
    if (NS_FAILED(rv) && (rv != NS_BASE_STREAM_CLOSED)) {
        LOG(("OnDataAvailable failed [rv=%x] canceling request!\n"));
        mRequest->Cancel(rv);
    }

    mProxy->SetListenerStatus(rv);
}

//----------------------------------------------------------------------------
// nsStreamListenerProxy::nsISupports implementation...
//----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS4(nsStreamListenerProxy,
                              nsIStreamListener,
                              nsIRequestObserver,
                              nsIStreamListenerProxy,
                              nsIInputStreamObserver)

//----------------------------------------------------------------------------
// nsStreamListenerProxy::nsIRequestObserver implementation...
//----------------------------------------------------------------------------

NS_IMETHODIMP
nsStreamListenerProxy::OnStartRequest(nsIRequest *request,
                                      nsISupports *context)
{
    NS_ENSURE_TRUE(mObserverProxy, NS_ERROR_NOT_INITIALIZED);

    // This will create a cyclic reference between the pipe and |this|, which
    // will be broken when onStopRequest is called.
    nsresult rv = mPipeIn->SetObserver(this);
    if (NS_FAILED(rv)) return rv;

    return mObserverProxy->OnStartRequest(request, context);
}

NS_IMETHODIMP
nsStreamListenerProxy::OnStopRequest(nsIRequest *request,
                                     nsISupports *context,
                                     nsresult status)
{
    NS_ENSURE_TRUE(mObserverProxy, NS_ERROR_NOT_INITIALIZED);

    mPipeIn = 0;
    mPipeOut = 0;

    return mObserverProxy->OnStopRequest(request, context, status);
}

//----------------------------------------------------------------------------
// nsIStreamListener implementation...
//----------------------------------------------------------------------------

NS_IMETHODIMP
nsStreamListenerProxy::OnDataAvailable(nsIRequest *request,
                                       nsISupports *context,
                                       nsIInputStream *source,
                                       PRUint32 offset,
                                       PRUint32 count)
{
    nsresult rv;
    PRUint32 bytesWritten=0;

    LOG(("nsStreamListenerProxy: OnDataAvailable [offset=%u count=%u req=%x]\n",
         offset, count, request));

    NS_ENSURE_TRUE(mObserverProxy, NS_ERROR_NOT_INITIALIZED);
    NS_PRECONDITION(mRequestToResume == 0, "Unexpected call to OnDataAvailable");
    NS_PRECONDITION(mPipeIn, "Pipe not initialized");
    NS_PRECONDITION(mPipeOut, "Pipe not initialized");

    //
    // Any non-successful listener status gets passed back to the caller
    //
    {
        nsresult status = mListenerStatus;
        if (NS_FAILED(status)) {
            LOG(("nsStreamListenerProxy: Listener failed [status=%x req=%x]\n",
                status, request));
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
        rv = mPipeOut->WriteFrom(source, count, &bytesWritten);

        LOG(("nsStreamListenerProxy: Wrote data to pipe [rv=%x count=%u bytesWritten=%u req=%x]\n",
            rv, count, bytesWritten, request));

        if (NS_FAILED(rv)) {
            if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
                nsAutoLock lock(mLock);
                if (mPipeEmptied) {
                    LOG(("nsStreamListenerProxy: Pipe emptied; looping back [req=%x]\n", request));
                    continue;
                }
                LOG(("nsStreamListenerProxy: Pipe full; setting request to resume [req=%x]\n", request));
                mRequestToResume = request;
            }
            return rv;
        }
        if (bytesWritten == 0) {
            LOG(("nsStreamListenerProxy: Copied zero bytes; not posting an event [req=%x]\n", request));
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
        LOG(("nsStreamListenerProxy: Piggy-backing pending event [total=%u, req=%x]\n",
            total, request));
        return NS_OK;
    }
 
    //
    // Post an event for the number of bytes actually written.
    //
    nsOnDataAvailableEvent *ev =
        new nsOnDataAvailableEvent(this, request, context, mPipeIn, offset);
    if (!ev) return NS_ERROR_OUT_OF_MEMORY;

    // Reuse the event queue of the observer proxy
    LOG(("post dataevent=%8lX queue=%8lX\n",(long)ev,(long)mObserverProxy->EventQueue()));
    rv = mObserverProxy->FireEvent(ev);
    if (NS_FAILED(rv))
        delete ev;
    return rv;
}

//----------------------------------------------------------------------------
// nsStreamListenerProxy::nsIStreamListenerProxy implementation...
//----------------------------------------------------------------------------

NS_IMETHODIMP
nsStreamListenerProxy::Init(nsIStreamListener *listener,
                            nsIEventQueue *eventQ,
                            PRUint32 bufferSegmentSize,
                            PRUint32 bufferMaxSize)
{
    NS_ENSURE_ARG_POINTER(listener);

#if defined(PR_LOGGING)
    if (!gStreamListenerProxyLog)
        gStreamListenerProxyLog = PR_NewLogModule("nsStreamListenerProxy");
#endif

    //
    // Create the RequestToResume lock
    //
    mLock = PR_NewLock();
    if (!mLock) return NS_ERROR_OUT_OF_MEMORY;

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

    nsCOMPtr<nsIRequestObserver> observer = do_QueryInterface(listener);
    return mObserverProxy->Init(observer, eventQ);
}

//----------------------------------------------------------------------------
// nsStreamListenerProxy::nsIInputStreamObserver implementation...
//----------------------------------------------------------------------------

NS_IMETHODIMP
nsStreamListenerProxy::OnEmpty(nsIInputStream *inputStream)
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
