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

#ifndef nsEventQueueService_h__
#define nsEventQueueService_h__

#include "nsIEventQueueService.h"
#include "nsHashtable.h"

class nsIEventQueue;
class EventQueueEntry;

// because available enumerators can't handle deletions during enumeration
class EventQueueEntryEnumerator {
public:
  EventQueueEntryEnumerator();
  virtual ~EventQueueEntryEnumerator();
  void             Reset(EventQueueEntry *aStart);
  EventQueueEntry *Get(void);
  void             Skip(EventQueueEntry *aSkip);
private:
  EventQueueEntry *mCurrent;
};

////////////////////////////////////////////////////////////////////////////////

class nsEventQueueServiceImpl : public nsIEventQueueService
{
friend class EventQueueEntry;
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
  NS_IMETHOD ResolveEventQueue(nsIEventQueue* queueOrConstant, nsIEventQueue* *resultQueue);

  NS_IMETHOD CreateFromIThread(nsIThread *aThread, nsIEventQueue **aResult);
  NS_IMETHOD CreateFromPLEventQueue(PLEventQueue* aPLEventQueue, nsIEventQueue** aResult);

  NS_IMETHOD PushThreadEventQueue(nsIEventQueue **aNewQueue);
  NS_IMETHOD PopThreadEventQueue(nsIEventQueue *aQueue);


private:
  NS_IMETHOD CreateEventQueue(PRThread *aThread);
  void       AddEventQueueEntry(EventQueueEntry *aEntry);
  void       RemoveEventQueueEntry(EventQueueEntry *aEntry);

  nsHashtable     *mEventQTable;
  EventQueueEntry *mBaseEntry;
  PRMonitor       *mEventQMonitor;
  EventQueueEntryEnumerator mEnumerator;
};

////////////////////////////////////////////////////////////////////////////////

#endif // nsEventQueueService_h__
