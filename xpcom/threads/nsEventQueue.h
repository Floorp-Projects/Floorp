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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "prmon.h"
#include "nsIEventQueue.h"
#include "nsPIEventQueueChain.h"

class nsEventQueueImpl : public nsIEventQueue,
                         public nsPIEventQueueChain
{
public:
    nsEventQueueImpl();
    virtual ~nsEventQueueImpl();

  // nsISupports interface...
    NS_DECL_ISUPPORTS
  // nsIEventQueue interface...
    NS_DECL_NSIEVENTQUEUE

    // Helpers
    static NS_METHOD Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

    static const nsCID& CID() { static nsCID cid = NS_EVENTQUEUE_CID; return cid; }

    // nsPIEventQueueChain interface
    NS_IMETHOD AppendQueue(nsIEventQueue *aQueue);
    NS_IMETHOD Unlink();
    NS_IMETHOD GetYoungest(nsIEventQueue **aQueue);
    NS_IMETHOD GetYoungestActive(nsIEventQueue **aQueue);
    NS_IMETHOD SetYounger(nsPIEventQueueChain *aQueue);
    NS_IMETHOD GetYounger(nsIEventQueue **aQueue);
    NS_IMETHOD SetElder(nsPIEventQueueChain *aQueue);
    NS_IMETHOD GetElder(nsIEventQueue **aQueue);

private:
  PLEventQueue  *mEventQueue;
  PRBool        mAcceptingEvents, // accept new events or pass them on?
                mCouldHaveEvents; // accepting new ones, or still have old ones?
  nsCOMPtr<nsPIEventQueueChain> mElderQueue; // younger can hold on to elder
  nsPIEventQueueChain *mYoungerQueue; // but elder can't hold on to younger

  void NotifyObservers(const char *aTopic);

  void CheckForDeactivation() {
    if (mCouldHaveEvents && !mAcceptingEvents && !PL_EventAvailable(mEventQueue)) {
      mCouldHaveEvents = PR_FALSE;
      NS_RELEASE_THIS(); // balance ADDREF from the constructor
    }
  }
};

