/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *  Zack Rusin <zack@kde.org>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Lars Knoll <knoll@kde.org>
 *   Zack Rusin <zack@kde.org>
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
#include "nsAppShell.h"

#include "nsEventQueueWatcher.h"

#include "nsIEventQueueService.h"
#include "nsIServiceManagerUtils.h"
#include "nsIEventQueue.h"

#include <qapplication.h>

nsAppShell::nsAppShell()
{
}

nsAppShell::~nsAppShell()
{
}

//-------------------------------------------------------------------------
// nsISupports implementation macro
//-------------------------------------------------------------------------
NS_IMPL_ISUPPORTS1(nsAppShell, nsIAppShell)

NS_IMETHODIMP
nsAppShell::Create(int *argc, char **argv)
{
    qDebug("Qt: nsAppShell::create");

    /**
     * If !qApp then it means we're not embedding
     * but running a full application.
     */
    if (!qApp) {
        //oh, this is fun. yes, argc can be null here
        int argcSafe = 0;
        if (argc)
            argcSafe = *argc;

        new QApplication(argcSafe, argv);

        if (argc)
            *argc = argcSafe;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsAppShell::Run(void)
{
    if (!mEventQueue)
        Spinup();

    if (!mEventQueue)
        return NS_ERROR_NOT_INITIALIZED;

    qApp->exec();

    Spindown();

    return NS_OK;
}

//i don't like this method flow, but i left it because it follows what
//other ports are doing here
NS_IMETHODIMP
nsAppShell::Spinup()
{
    nsresult rv = NS_OK;

    // Get the event queue service
    nsCOMPtr<nsIEventQueueService> eventQService =
        do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID,&rv);

    if (NS_FAILED(rv)) {
        NS_WARNING("Could not obtain event queue service");
        return rv;
    }

    //Get the event queue for the thread.
    rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD,
                                            getter_AddRefs(mEventQueue));

    // If a queue already present use it.
    if (mEventQueue)
        goto done;

    // Create the event queue for the thread
    rv = eventQService->CreateThreadEventQueue();
    if (NS_FAILED(rv)) {
        NS_WARNING("Could not create the thread event queue");
        return rv;
    }

    // Ask again for the event queue now that we have create one.
    rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD,getter_AddRefs(mEventQueue));
    if (NS_FAILED(rv)) {
        NS_ASSERTION("Could not obtain the thread event queue",PR_FALSE);
        return rv;
    }
done:
    AddEventQueue(mEventQueue);

    return NS_OK;
}

NS_IMETHODIMP
nsAppShell::Spindown(void)
{
    // stop listening to the event queue
    if (mEventQueue) {
        RemoveEventQueue(mEventQueue);
        mEventQueue->ProcessPendingEvents();
        mEventQueue = nsnull;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsAppShell::ListenToEventQueue(nsIEventQueue *aQueue, PRBool aListen)
{
    // whoever came up with the idea of passing bool to this
    // method to decide what its effect should be is getting
    // his ass kicked

    if (aListen)
        AddEventQueue(aQueue);
    else
        RemoveEventQueue(aQueue);

    return NS_OK;
}

void
nsAppShell::AddEventQueue(nsIEventQueue *aQueue)
{
    nsEventQueueWatcher *que = 0;

    if ((que = mQueueDict.find(aQueue->GetEventQueueSelectFD()))) {
        que->ref();
    }
    else {
        mQueueDict.insert(aQueue->GetEventQueueSelectFD(),
                          new nsEventQueueWatcher(aQueue, qApp));
    }
}

void
nsAppShell::RemoveEventQueue(nsIEventQueue *aQueue)
{
    nsEventQueueWatcher *qtQueue = 0;

    if ((qtQueue = mQueueDict.find(aQueue->GetEventQueueSelectFD()))) {
        qtQueue->DataReceived();
        qtQueue->deref();
        if (qtQueue->count <= 0) {
            mQueueDict.take(aQueue->GetEventQueueSelectFD());
            delete qtQueue;
        }
    }
}

NS_IMETHODIMP
nsAppShell::GetNativeEvent(PRBool &aRealEvent, void * &aEvent)
{
    aRealEvent = PR_FALSE;
    aEvent = 0;

    return NS_OK;
}

NS_IMETHODIMP
nsAppShell::DispatchNativeEvent(PRBool aRealEvent, void *aEvent)
{
    if (!mEventQueue)
        return NS_ERROR_NOT_INITIALIZED;

    qApp->processEvents();

    return NS_OK;
}

NS_IMETHODIMP
nsAppShell::Exit(void)
{
    qApp->exit(0);
    return NS_OK;
}
