/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
 *   Steve Meredith <smeredith@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsNativeConnectionHelper.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsAutodialWin.h"
#include "nsIPref.h"

static NS_DEFINE_IID(kPrefServiceCID, NS_PREF_CID);

//-----------------------------------------------------------------------------
// nsSyncEvent - implements a synchronous event that runs on the main thread.
//
// XXX move this class into xpcom/proxy
//-----------------------------------------------------------------------------

class nsSyncEvent
{
public:
    nsSyncEvent() {}
    virtual ~nsSyncEvent() {}

    nsresult Dispatch();

    virtual void HandleEvent() = 0;

private:
    static void* PR_CALLBACK EventHandler    (PLEvent *self);
    static void  PR_CALLBACK EventDestructor (PLEvent *self);

};

nsresult
nsSyncEvent::Dispatch()
{
    nsresult rv;

    nsCOMPtr<nsIEventQueueService> eqs(
            do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIEventQueue> eventQ;
    rv = eqs->GetSpecialEventQueue(nsIEventQueueService::UI_THREAD_EVENT_QUEUE,
                                   getter_AddRefs(eventQ));
    if (NS_FAILED(rv)) return rv;

    PRBool onCurrentThread = PR_FALSE;
    eventQ->IsQueueOnCurrentThread(&onCurrentThread);
    if (onCurrentThread) {
        HandleEvent();
        return NS_OK;
    }

    // Otherwise build up an event and post it to the main thread's event queue
    // and block before returning.


    PLEvent *ev = new PLEvent;
    if (!ev)
        return NS_ERROR_OUT_OF_MEMORY;

    PL_InitEvent(ev, (void *) this, EventHandler, EventDestructor);

    // block until the event is answered
    eventQ->PostSynchronousEvent(ev, nsnull);

    return NS_OK;
}

void* PR_CALLBACK
nsSyncEvent::EventHandler(PLEvent *self)
{
    nsSyncEvent *ev = (nsSyncEvent *) PL_GetEventOwner(self);
    if (ev)
        ev->HandleEvent();
    
    return nsnull;
}

void PR_CALLBACK
nsSyncEvent::EventDestructor(PLEvent *self)
{
    delete self;
}


//-----------------------------------------------------------------------------
// nsOnConnectionFailedEvent
//-----------------------------------------------------------------------------

class nsOnConnectionFailedEvent : public nsSyncEvent
{
private:
    nsRASAutodial mAutodial;
    const char* mHostName;

public:
    nsOnConnectionFailedEvent(const char* hostname) : mRetry(PR_FALSE), mHostName(hostname) { }

    void HandleEvent();

    PRBool mRetry;
};

void nsOnConnectionFailedEvent::HandleEvent()
{
    mRetry = PR_FALSE;
    if (mAutodial.ShouldDialOnNetworkError()) {
        mRetry = NS_SUCCEEDED(mAutodial.DialDefault(mHostName));
    }
}


//-----------------------------------------------------------------------------
// API typically invoked on the socket transport thread
//-----------------------------------------------------------------------------


PRBool
nsNativeConnectionHelper::OnConnectionFailed(const char* strHostName)
{
    nsOnConnectionFailedEvent event(strHostName);
    if (NS_SUCCEEDED(event.Dispatch()))
        return event.mRetry;

    return PR_FALSE;
}
