/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsIEventQueue_h__
#define nsIEventQueue_h__

#include "nsISupports.h"
#include "prthread.h"
#include "plevent.h"

// {176AFB41-00A4-11d3-9F2A-00400553EEF0}
#define NS_IEVENTQUEUE_IID \
{ 0x176afb41, 0xa4, 0x11d3, { 0x9f, 0x2a, 0x0, 0x40, 0x5, 0x53, 0xee, 0xf0 } }

// {13D86C61-00A9-11d3-9F2A-00400553EEF0}
#define NS_EVENTQUEUE_CID \
{ 0x13d86c61, 0xa9, 0x11d3, { 0x9f, 0x2a, 0x0, 0x40, 0x5, 0x53, 0xee, 0xf0 } }

#define NS_EVENTQUEUE_CONTRACTID "@mozilla.org/event-queue;1"
#define NS_EVENTQUEUE_CLASSNAME "Event Queue"

class nsIEventQueue : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IEVENTQUEUE_IID);
	
	NS_IMETHOD InitEvent(PLEvent* aEvent, void* owner, 
		                 PLHandleEventProc handler, PLDestroyEventProc destructor) = 0;
    
	NS_IMETHOD_(PRStatus) PostEvent(PLEvent* aEvent) = 0;
    NS_IMETHOD PostSynchronousEvent(PLEvent* aEvent, void** aResult) = 0;
	
    NS_IMETHOD ProcessPendingEvents() = 0;
    NS_IMETHOD EventLoop() = 0;

    NS_IMETHOD EventAvailable(PRBool& aResult) = 0;
    NS_IMETHOD GetEvent(PLEvent** aResult) = 0;
    NS_IMETHOD HandleEvent(PLEvent* aEvent) = 0;

    NS_IMETHOD WaitForEvent(PLEvent** aResult) = 0;

    NS_IMETHOD_(PRInt32) GetEventQueueSelectFD() = 0;

    NS_IMETHOD Init(PRBool aNative) = 0;
    NS_IMETHOD InitFromPRThread(PRThread* thread, PRBool aNative) = 0;
    NS_IMETHOD InitFromPLQueue(PLEventQueue* aQueue) = 0;

    NS_IMETHOD EnterMonitor() = 0;
    NS_IMETHOD ExitMonitor() = 0;

    NS_IMETHOD RevokeEvents(void* owner) = 0;
    NS_IMETHOD GetPLEventQueue(PLEventQueue** aEventQueue) = 0;

    NS_IMETHOD IsQueueOnCurrentThread(PRBool *aResult) = 0;
    NS_IMETHOD IsQueueNative(PRBool *aResult) = 0;

    // effectively kill the queue. warning: the queue is allowed to delete
    // itself any time after this.
    NS_IMETHOD StopAcceptingEvents() = 0;
};

#endif /* nsIEventQueue_h___ */
