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

#ifndef nsIXPFCObserverManager_h___
#define nsIXPFCObserverManager_h___

#include "nsISupports.h"

class nsIXPFCSubject;
class nsIXPFCObserver;
class nsIXPFCCommand;

//d28a5590-1f56-11d2-bed9-00805f8a8dbd
#define NS_IXPFC_OBSERVERMANAGER_IID   \
{ 0xd28a5590, 0x1f56, 0x11d2,    \
{ 0xbe, 0xd9, 0x00, 0x80, 0x5f, 0x8a, 0x8d, 0xbd } }

enum nsCommandState {  
  nsCommandState_eNone,
  nsCommandState_eComplete
};


class nsIXPFCObserverManager : public nsISupports
{

public:

  NS_IMETHOD Init() = 0;

  NS_IMETHOD Register(nsIXPFCSubject * aSubject, nsIXPFCObserver * aObserver) = 0;
  NS_IMETHOD Unregister(nsISupports * aSubjectObserver) = 0;
  NS_IMETHOD Unregister(nsIXPFCSubject * aSubject, nsIXPFCObserver * aObserver) = 0;
  NS_IMETHOD Notify(nsIXPFCSubject * aSubject, nsIXPFCCommand * aCommand) = 0;

  NS_IMETHOD RegisterForCommandState(nsIXPFCObserver * aObserver, nsCommandState aCommandState) = 0;
  NS_IMETHOD UnregisterSubject(nsIXPFCSubject * aSubject) = 0;
  NS_IMETHOD UnregisterObserver(nsIXPFCObserver * aObserver) = 0;

};

#endif /* nsIXPFCObserverManager_h___ */
