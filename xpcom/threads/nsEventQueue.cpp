/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsEventQueue.h"

nsEventQueueImpl::nsEventQueueImpl()
{
  NS_INIT_REFCNT();

  mEventQueue = NULL;
	
}

nsEventQueueImpl::~nsEventQueueImpl()
{
  if (NULL != mEventQueue) {
    PL_DestroyEventQueue(mEventQueue);
  }
}

NS_IMETHODIMP 
nsEventQueueImpl::Init()
{
  mEventQueue = PL_CreateNativeEventQueue("Thread event queue...", PR_GetCurrentThread());
	return NS_OK;
}

NS_IMETHODIMP
nsEventQueueImpl::InitFromPLQueue(PLEventQueue* aQueue)
{
	mEventQueue = aQueue;
	return NS_OK;
}

/* nsISupports interface implementation... */
NS_IMPL_ISUPPORTS1(nsEventQueueImpl,nsIEventQueue)

/* nsIEventQueue interface implementation... */

NS_IMETHODIMP_(PRStatus)
nsEventQueueImpl::PostEvent(PLEvent* aEvent)
{
	return PL_PostEvent(mEventQueue, aEvent);
}

NS_IMETHODIMP
nsEventQueueImpl::PostSynchronousEvent(PLEvent* aEvent, void** aResult)
{
  void* result = PL_PostSynchronousEvent(mEventQueue, aEvent);
	if (aResult)
  {
    *aResult = result;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsEventQueueImpl::EnterMonitor()
{
    PL_ENTER_EVENT_QUEUE_MONITOR(mEventQueue);
    return NS_OK;
}

NS_IMETHODIMP
nsEventQueueImpl::ExitMonitor()
{
    PL_EXIT_EVENT_QUEUE_MONITOR(mEventQueue);
    return NS_OK;
}


NS_IMETHODIMP
nsEventQueueImpl::RevokeEvents(void* owner)
{
    PL_RevokeEvents(mEventQueue, owner);
    return NS_OK;
}


NS_IMETHODIMP
nsEventQueueImpl::GetPLEventQueue(PLEventQueue** aEventQueue)
{
    *aEventQueue = mEventQueue;
    
    if (mEventQueue == NULL)
        return NS_ERROR_NULL_POINTER;

    return NS_OK;
}

NS_IMETHODIMP
nsEventQueueImpl::IsQueueOnCurrentThread(PRBool *aResult)
{
    *aResult = PL_IsQueueOnCurrentThread( mEventQueue );
    return NS_OK;
}


NS_IMETHODIMP
nsEventQueueImpl::ProcessPendingEvents()
{
	PL_ProcessPendingEvents(mEventQueue);
	return NS_OK;
}

NS_IMETHODIMP
nsEventQueueImpl::EventLoop()
{
  PL_EventLoop(mEventQueue);
  return NS_OK;
}

NS_IMETHODIMP
nsEventQueueImpl::EventAvailable(PRBool& aResult)
{
	aResult = PL_EventAvailable(mEventQueue);
	return NS_OK;
}

NS_IMETHODIMP
nsEventQueueImpl::GetEvent(PLEvent** aResult)
{
	*aResult = PL_GetEvent(mEventQueue);
	return NS_OK;
}

NS_IMETHODIMP
nsEventQueueImpl::HandleEvent(PLEvent* aEvent)
{
    PL_HandleEvent(aEvent);
    return NS_OK;
}

NS_IMETHODIMP_(PRInt32) 
nsEventQueueImpl::GetEventQueueSelectFD() 
{
  return PL_GetEventQueueSelectFD(mEventQueue);
}

NS_METHOD
nsEventQueueImpl::Create(nsISupports *aOuter,
                            REFNSIID aIID,
                            void **aResult)
{
  nsEventQueueImpl* evt = new nsEventQueueImpl();
  if (evt == NULL)
    return NS_ERROR_OUT_OF_MEMORY;
  nsresult rv = evt->QueryInterface(aIID, aResult);
  if (NS_FAILED(rv)) {
    delete evt;
  }
  return rv;
}
