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

#ifndef nsIXPFCCommandStateObserver_h___
#define nsIXPFCCommandStateObserver_h___

#include "nsISupports.h"

//2e8e55f0-621b-11d2-924c-00805f8a7ab6
#define NS_IXPFC_COMMAND_STATE_OBSERVER_IID   \
{ 0x2e8e55f0, 0x621b, 0x11d2,    \
{ 0x92, 0x4c, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

enum nsCommandState {  
  nsCommandState_eNone,
  nsCommandState_eComplete
};

class nsIXPFCCommandStateObserver : public nsISupports
{

public:

  NS_IMETHOD Init() = 0;

};

#endif /* nsIXPFCCommandStateObserver_h___ */
