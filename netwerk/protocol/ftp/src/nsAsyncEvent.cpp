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
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsAsyncEvent.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"

static NS_DEFINE_CID(eventQCID, NS_EVENTQUEUESERVICE_CID);

nsAsyncEvent::nsAsyncEvent(nsIChannel* channel, nsISupports* context)
    : mChannel(channel), mContext(context), mEvent(nsnull)
{ }

nsAsyncEvent::~nsAsyncEvent()
{
    if (nsnull != mEvent)
    {
        delete mEvent;
        mEvent = nsnull;
    }
}

void PR_CALLBACK nsAsyncEvent::HandlePLEvent(PLEvent* aEvent)
{
    nsAsyncEvent* ev = (nsAsyncEvent*) PL_GetEventOwner(aEvent);

    NS_ASSERTION(nsnull != ev,"null event.");

    (void)ev->HandleEvent();
}

void PR_CALLBACK nsAsyncEvent::DestroyPLEvent(PLEvent* aEvent)
{
    nsAsyncEvent* ev = (nsAsyncEvent*) PL_GetEventOwner(aEvent);

    NS_ASSERTION(nsnull != ev,"null event.");

    delete ev;
}

nsresult
nsAsyncEvent::Fire() 
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIEventQueueService> eqServ = do_GetService(eventQCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIEventQueue> eventQ;

    rv = eqServ->GetThreadEventQueue(NS_UI_THREAD, getter_AddRefs(eventQ));
    if (NS_FAILED(rv)) return rv;
    NS_PRECONDITION(nsnull == mEvent, "Init plevent only once.");
    
    mEvent = new PLEvent;
    
    PL_InitEvent(mEvent, 
                 this,
                 (PLHandleEventProc)  nsAsyncEvent::HandlePLEvent,
                 (PLDestroyEventProc) nsAsyncEvent::DestroyPLEvent);

    PRStatus status = eventQ->PostEvent(mEvent);
    return status == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE;
}


nsFTPAsyncReadEvent::nsFTPAsyncReadEvent(nsIStreamListener* listener,
                                         nsIChannel* channel,
                                         nsISupports* context)
                    : nsAsyncEvent(channel, context), mListener(listener)
{ }

NS_IMETHODIMP
nsFTPAsyncReadEvent::HandleEvent()
{
    return mChannel->AsyncRead(mListener, mContext);
}


nsFTPAsyncWriteEvent::nsFTPAsyncWriteEvent(nsIInputStream* inStream,
                                           PRUint32 writeCount,
                                           nsIStreamObserver* observer,
                                           nsIChannel* channel,
                                           nsISupports* context)
    : nsAsyncEvent(channel, context), mObserver(observer),
      mInStream(inStream), mWriteCount(writeCount)
{
}

NS_IMETHODIMP
nsFTPAsyncWriteEvent::HandleEvent()
{
    nsresult rv;
    rv = mChannel->SetTransferCount(mWriteCount);
    if (NS_FAILED(rv)) return rv;
    return mChannel->AsyncWrite(mInStream, mObserver, mContext);
}

