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

// XXX move to nsID.h or nsHashtable.h? (copied from nsComponentManager.cpp)
class ThreadKey: public nsHashKey {
private:
  const PRThread* id;
  
public:
  ThreadKey(const PRThread* aID) {
    id = aID;
  }
  
  PRUint32 HashValue(void) const {
    return (PRUint32)id;
  }

  PRBool Equals(const nsHashKey *aKey) const {
    return (id == ((const ThreadKey *) aKey)->id);
  }

  nsHashKey *Clone(void) const {
    return new ThreadKey(id);
  }
};

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

/*
 * This class maintains the data associated with each entry in the EventQueue
 * service's hash table...
 *
 * It derives from nsISupports merely as a convienence since the entries are
 * reference counted...
 */
class EventQueueEntry : public nsISupports 
{
public:
  EventQueueEntry();
  virtual ~EventQueueEntry();

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  nsIEventQueue* GetEventQueue(void);
  
	void PushQueue(void);
  void PopQueue(void);

private: 
	EventQueueStack* mQueueStack;
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

  NS_IMETHOD PushThreadEventQueue(void);
  NS_IMETHOD PopThreadEventQueue(void);

#ifdef XP_MAC
  NS_IMETHOD ProcessEvents();
#endif // XP_MAC

private:
  nsHashtable* mEventQTable;
  PRMonitor*   mEventQMonitor;
};

////////////////////////////////////////////////////////////////////////////////

#endif // nsEventQueueService_h__
