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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsXPFCObserverManager_h___
#define nsXPFCObserverManager_h___

#include "nscore.h"
#include "plstr.h"
#include "prtypes.h"
#include "prmon.h"
#include "plstr.h"
#include "nsCom.h"
#include "nsIArray.h"
#include "nsIIterator.h"

#include "nsIXPFCObserverManager.h"

class nsXPFCObserverManager : public nsIXPFCObserverManager
{
public:
  nsXPFCObserverManager();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init() ;
  NS_IMETHOD Register(nsIXPFCSubject * aSubject, nsIXPFCObserver * aObserver);
  NS_IMETHOD Unregister(nsISupports * aSubjectObserver);
  NS_IMETHOD Unregister(nsIXPFCSubject * aSubject, nsIXPFCObserver * aObserver);
  NS_IMETHOD Notify(nsIXPFCSubject * aSubject, nsIXPFCCommand * aCommand);
  NS_IMETHOD RegisterForCommandState(nsIXPFCObserver * aObserver, nsCommandState aCommandState);
  NS_IMETHOD UnregisterSubject(nsIXPFCSubject * aSubject);
  NS_IMETHOD UnregisterObserver(nsIXPFCObserver * aObserver);

private:
  NS_METHOD CheckForCommandStateNotification(nsIXPFCObserver * aObserver);
  NS_METHOD SendCommandStateNotifications(nsCommandState aCommandState);

protected:
  ~nsXPFCObserverManager();

public:
  PRMonitor * monitor;

private:
  nsIArray * mList ;
  nsIArray * mState ;
  PRUint32 mNotificationCount;
  nsIXPFCSubject * mOriginalNotifier;

};

#endif /* nsXPFCObserverManager_h___ */
