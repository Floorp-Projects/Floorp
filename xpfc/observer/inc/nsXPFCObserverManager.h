/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsXPFCObserverManager_h___
#define nsXPFCObserverManager_h___

#include "nscore.h"
#include "plstr.h"
#include "prtypes.h"
#include "prmon.h"
#include "plstr.h"
#include "nsCom.h"
#include "nsIVector.h"
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
  NS_IMETHOD RegisterForCommandState(nsIXPFCCommandStateObserver * aCommandStateObserver, nsCommandState aCommandState);

protected:
  ~nsXPFCObserverManager();

public:
  PRMonitor * monitor;

public:
  nsIVector * mList ;
  nsIVector * mState ;

};

#endif /* nsXPFCObserverManager_h___ */
