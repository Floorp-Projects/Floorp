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

#ifndef nsEventQueueService_h__
#define nsEventQueueService_h__

#include "nsIEventQueueService.h"
#include "nsHashtable.h"

class nsIEventQueue;

////////////////////////////////////////////////////////////////////////////////

/* This class is used with the EventQueueEntries to allow us to nest
   event queues */
class EventQueueStack
{
public:
	EventQueueStack(EventQueueStack* next = NULL);
	~EventQueueStack();

	nsIEventQueue* GetEventQueue();
	EventQueueStack* GetNext();
  void SetNext(EventQueueStack* aStack);

private:
	nsIEventQueue* mEventQueue;
	EventQueueStack* mNextQueue;
};

////////////////////////////////////////////////////////////////////////////////

class nsEventQueueServiceImpl : public nsIEventQueueService
{
public:
  nsEventQueueServiceImpl();
  virtual ~nsEventQueueServiceImpl();

  static NS_METHOD
  Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsIEventQueueService interface...
  NS_IMETHOD CreateThreadEventQueue(void);
  NS_IMETHOD DestroyThreadEventQueue(void);
  NS_IMETHOD GetThreadEventQueue(PRThread* aThread, nsIEventQueue** aResult);

  NS_IMETHOD CreateFromPLEventQueue(PLEventQueue* aPLEventQueue, nsIEventQueue** aResult);

  NS_IMETHOD PushThreadEventQueue(nsIEventQueue **aNewQueue);
  NS_IMETHOD PopThreadEventQueue(nsIEventQueue *aQueue);

#ifdef XP_MAC
  NS_IMETHOD ProcessEvents();
#endif // XP_MAC

private:
  nsHashtable* mEventQTable;
  PRMonitor*   mEventQMonitor;
};

////////////////////////////////////////////////////////////////////////////////

#endif // nsEventQueueService_h__
