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

#ifndef nsIEventQueueService_h__
#define nsIEventQueueService_h__

#include "nsISupports.h"
#include "prthread.h"
#include "plevent.h"
#include "nsIEventQueue.h"

/* a6cf90dc-15b3-11d2-932e-00805f8add32 */
#define NS_IEVENTQUEUESERVICE_IID \
{ 0xa6cf90dc, 0x15b3, 0x11d2, \
  {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

/* be761f00-a3b0-11d2-996c-0080c7cb1080 */
#define NS_EVENTQUEUESERVICE_CID \
{ 0xbe761f00, 0xa3b0, 0x11d2, \
  {0x99, 0x6c, 0x00, 0x80, 0xc7, 0xcb, 0x10, 0x80} }

#define NS_EVENTQUEUESERVICE_CONTRACTID "@mozilla.org/event-queue-service;1"
#define NS_EVENTQUEUESERVICE_CLASSNAME "Event Queue Service"

#define NS_CURRENT_THREAD    ((PRThread*)0)
#define NS_CURRENT_EVENTQ    ((nsIEventQueue*)0)

#define NS_UI_THREAD         ((PRThread*)1)
#define NS_UI_THREAD_EVENTQ  ((nsIEventQueue*)1)


class nsIThread;

class nsIEventQueueService : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IEVENTQUEUESERVICE_IID; return iid; }

  /**
   * Creates and holds a native event queue for the current thread.
   * "Native" queues have an associated callback mechanism which is
   * automatically triggered when an event is posted. See plevent.c for details.
   * @return NS_OK on success, or a host of failure indications
   */
  NS_IMETHOD CreateThreadEventQueue() = 0;

  /**
   * Creates and hold a monitored event queue for the current thread.
   * "Monitored" queues have no callback processing mechanism.
   * @return NS_OK on success, or a host of failure indications
   */
  NS_IMETHOD CreateMonitoredThreadEventQueue() = 0;

  /**
   * Somewhat misnamed, this method releases the service's hold on the event
   * queue(s) for this thread. Subsequent attempts to access this thread's
   * queue (GetThreadEventQueue, for example) may fail, though the queue itself
   * will be destroyed only after all references to it are released and the
   * queue itself is no longer actively processing events.
   * @return nonsense.
   */
  NS_IMETHOD DestroyThreadEventQueue(void) = 0;

  NS_IMETHOD CreateFromIThread(nsIThread *aThread, PRBool aNative,
                               nsIEventQueue **aResult) = 0;
  NS_IMETHOD CreateFromPLEventQueue(PLEventQueue* aPLEventQueue, nsIEventQueue** aResult) = 0;

  // Add a new event queue for the current thread, making it the "current"
  // queue. Return that queue in aNewQueue, addrefed.
  NS_IMETHOD PushThreadEventQueue(nsIEventQueue **aNewQueue) = 0;

  // release and disable the queue
  NS_IMETHOD PopThreadEventQueue(nsIEventQueue *aQueue) = 0;

  NS_IMETHOD GetThreadEventQueue(PRThread* aThread, nsIEventQueue** aResult) = 0;
  NS_IMETHOD ResolveEventQueue(nsIEventQueue* queueOrConstant, nsIEventQueue* *resultQueue) = 0;

};

#endif /* nsIEventQueueService_h___ */
