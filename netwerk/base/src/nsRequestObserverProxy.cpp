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

#include "nscore.h"
#include "nsRequestObserverProxy.h"
#include "nsIRequest.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsString.h"
#include "prlog.h"

#if defined(PR_LOGGING)
static PRLogModuleInfo *gRequestObserverProxyLog;
#endif

#define LOG(args) PR_LOG(gRequestObserverProxyLog, PR_LOG_DEBUG, args)

static NS_DEFINE_CID(kEventQueueService, NS_EVENTQUEUESERVICE_CID);

//-----------------------------------------------------------------------------
// nsARequestObserverEvent internal class...
//-----------------------------------------------------------------------------

nsARequestObserverEvent::nsARequestObserverEvent(nsIRequest *request,
                                                 nsISupports *context)
    : mRequest(request)
    , mContext(context)
{
    NS_PRECONDITION(mRequest, "null pointer");

    PL_InitEvent(&mEvent, nsnull,
        (PLHandleEventProc) nsARequestObserverEvent::HandlePLEvent,
        (PLDestroyEventProc) nsARequestObserverEvent::DestroyPLEvent);
}

void PR_CALLBACK
nsARequestObserverEvent::HandlePLEvent(PLEvent *plev)
{
    nsARequestObserverEvent *ev = FromPLEvent(plev);
    NS_ASSERTION(ev, "null event");

    // Pass control to the real event handler
    if (ev)
        ev->HandleEvent();
}

void PR_CALLBACK
nsARequestObserverEvent::DestroyPLEvent(PLEvent *plev)
{
    nsARequestObserverEvent *ev = FromPLEvent(plev);
    NS_ASSERTION(ev, "null event");
    delete ev;
}

//-----------------------------------------------------------------------------
// nsOnStartRequestEvent internal class...
//-----------------------------------------------------------------------------

class nsOnStartRequestEvent : public nsARequestObserverEvent
{
    nsRequestObserverProxy *mProxy;
public:
    nsOnStartRequestEvent(nsRequestObserverProxy *proxy,
                          nsIRequest *request,
                          nsISupports *context)
        : nsARequestObserverEvent(request, context)
        , mProxy(proxy)
    {
        NS_PRECONDITION(mProxy, "null pointer");
        MOZ_COUNT_CTOR(nsOnStartRequestEvent);
        NS_ADDREF(mProxy);
    }

   ~nsOnStartRequestEvent()
    {
        MOZ_COUNT_DTOR(nsOnStartRequestEvent);
        NS_RELEASE(mProxy);
    }

    void HandleEvent()
    {
        LOG(("nsOnStartRequestEvent::HandleEvent [req=%x]\n", mRequest.get()));

        if (!mProxy->mObserver) {
            NS_NOTREACHED("already handled onStopRequest event (observer is null)");
            return;
        }

        LOG(("handle startevent=%8lX\n",(long)this));
        nsresult rv = mProxy->mObserver->OnStartRequest(mRequest, mContext);
        if (NS_FAILED(rv)) {
            LOG(("OnStartRequest failed [rv=%x] canceling request!\n", rv));
            rv = mRequest->Cancel(rv);
            NS_ASSERTION(NS_SUCCEEDED(rv), "Cancel failed for request!");
        }
    }
};

//-----------------------------------------------------------------------------
// nsOnStopRequestEvent internal class...
//-----------------------------------------------------------------------------

class nsOnStopRequestEvent : public nsARequestObserverEvent
{
    nsRequestObserverProxy *mProxy;
public:
    nsOnStopRequestEvent(nsRequestObserverProxy *proxy,
                         nsIRequest *request, nsISupports *context)
        : nsARequestObserverEvent(request, context)
        , mProxy(proxy)
    {
        NS_PRECONDITION(mProxy, "null pointer");
        MOZ_COUNT_CTOR(nsOnStopRequestEvent);
        NS_ADDREF(mProxy);
    }

   ~nsOnStopRequestEvent()
    {
        MOZ_COUNT_DTOR(nsOnStopRequestEvent);
        NS_RELEASE(mProxy);
    }

    void HandleEvent()
    {
        nsresult rv, status = NS_OK;

        LOG(("nsOnStopRequestEvent::HandleEvent [req=%x]\n", mRequest.get()));

        nsCOMPtr<nsIRequestObserver> observer = mProxy->mObserver;
        if (!observer) {
            NS_NOTREACHED("already handled onStopRequest event (observer is null)");
            return;
        }
        // Do not allow any more events to be handled after OnStopRequest
        mProxy->mObserver = 0;

        rv = mRequest->GetStatus(&status);
        NS_ASSERTION(NS_SUCCEEDED(rv), "GetStatus failed for request!");

        LOG(("handle stopevent=%8lX\n",(long)this));
        (void) observer->OnStopRequest(mRequest, mContext, status);
    }
};

//-----------------------------------------------------------------------------
// nsRequestObserverProxy::nsISupports implementation...
//-----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS2(nsRequestObserverProxy,
                              nsIRequestObserver,
                              nsIRequestObserverProxy)

//-----------------------------------------------------------------------------
// nsRequestObserverProxy::nsIRequestObserver implementation...
//-----------------------------------------------------------------------------

NS_IMETHODIMP 
nsRequestObserverProxy::OnStartRequest(nsIRequest *request,
                                       nsISupports *context)
{
    LOG(("nsRequestObserverProxy::OnStartRequest [this=%x req=%x]\n", this, request));

    nsOnStartRequestEvent *ev = 
        new nsOnStartRequestEvent(this, request, context);
    if (!ev)
        return NS_ERROR_OUT_OF_MEMORY;

    LOG(("post startevent=%8lX queue=%8lX\n",(long)ev,(long)mEventQ.get()));
    nsresult rv = FireEvent(ev);
    if (NS_FAILED(rv))
        delete ev;
    return rv;
}

NS_IMETHODIMP 
nsRequestObserverProxy::OnStopRequest(nsIRequest *request,
                                      nsISupports *context,
                                      nsresult status)
{
    LOG(("nsRequestObserverProxy: OnStopRequest [this=%x req=%x status=%x]\n",
        this, request, status));

    // The status argument is ignored because, by the time the OnStopRequestEvent
    // is actually processed, the status of the request may have changed :-( 
    // To make sure that an accurate status code is always used, GetStatus() is
    // called when the OnStopRequestEvent is actually processed (see above).

    nsOnStopRequestEvent *ev = 
        new nsOnStopRequestEvent(this, request, context);
    if (!ev)
        return NS_ERROR_OUT_OF_MEMORY;

    LOG(("post stopevent=%8lX queue=%8lX\n",(long)ev,(long)mEventQ.get()));
    nsresult rv = FireEvent(ev);
    if (NS_FAILED(rv))
        delete ev;
    return rv;
}

//-----------------------------------------------------------------------------
// nsRequestObserverProxy::nsIRequestObserverProxy implementation...
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsRequestObserverProxy::Init(nsIRequestObserver *observer,
                             nsIEventQueue *eventQ)
{
    NS_ENSURE_ARG_POINTER(observer);

#if defined(PR_LOGGING)
    if (!gRequestObserverProxyLog)
        gRequestObserverProxyLog = PR_NewLogModule("nsRequestObserverProxy");
#endif

    mObserver = observer;

    return SetEventQueue(eventQ);
}

//-----------------------------------------------------------------------------
// nsRequestObserverProxy implementation...
//-----------------------------------------------------------------------------

nsresult
nsRequestObserverProxy::FireEvent(nsARequestObserverEvent *event)
{
    NS_ENSURE_TRUE(mEventQ, NS_ERROR_NOT_INITIALIZED);

    PRStatus status = mEventQ->PostEvent(event->GetPLEvent());
    return status == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsRequestObserverProxy::SetEventQueue(nsIEventQueue *eq)
{
    nsresult rv = NS_OK;
    if ((eq == NS_CURRENT_EVENTQ) || (eq == NS_UI_THREAD_EVENTQ)) {
        nsCOMPtr<nsIEventQueueService> serv = do_GetService(kEventQueueService, &rv);
        if (NS_FAILED(rv)) return rv;
        rv = serv->GetSpecialEventQueue(NS_PTR_TO_INT32(eq), getter_AddRefs(mEventQ));
    }
    else
        mEventQ = eq;
    return rv;
}
