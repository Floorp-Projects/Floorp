/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "prmon.h"
#include "nsIEventQueue.h"
#include "nsPIEventQueueChain.h"

class nsEventQueueImpl : public nsIEventQueue,
                         public nsPIEventQueueChain
{
public:
    nsEventQueueImpl();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIEVENTTARGET
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
    ~nsEventQueueImpl();

  PLEventQueue  *mEventQueue;
  PRBool        mAcceptingEvents, // accept new events or pass them on?
                mCouldHaveEvents; // accepting new ones, or still have old ones?
  nsCOMPtr<nsPIEventQueueChain> mElderQueue; // younger can hold on to elder
  nsPIEventQueueChain *mYoungerQueue; // but elder can't hold on to younger

  void NotifyObservers(const char *aTopic);

  void CheckForDeactivation() {
    if (mCouldHaveEvents && !mAcceptingEvents && !PL_EventAvailable(mEventQueue)) {
      if (PL_IsQueueOnCurrentThread(mEventQueue)) {
        mCouldHaveEvents = PR_FALSE;
        NS_RELEASE_THIS(); // balance ADDREF from the constructor
      } else
        NS_ERROR("CheckForDeactivation called from wrong thread!");
    }
  }
};

