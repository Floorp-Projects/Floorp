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

#ifndef nsIXPFCCommandReceiver_h___
#define nsIXPFCCommandReceiver_h___

#include "nsISupports.h"
#include "nsIXPFCCommand.h"
#include "nsGUIEvent.h"

class nsIXPFCCommand;

//2c4b94b0-1f48-11d2-bed9-00805f8a8dbd
#define NS_IXPFC_COMMANDRECEIVER_IID   \
{ 0x2c4b94b0, 0x1f48, 0x11d2,    \
{ 0xbe, 0xd9, 0x00, 0x80, 0x5f, 0x8a, 0x8d, 0xbd } }

class nsIXPFCCommandReceiver : public nsISupports
{

public:

  NS_IMETHOD Init() = 0;

  NS_IMETHOD_(nsEventStatus) Action(nsIXPFCCommand * aCommand) = 0;

};

#endif /* nsIXPFCCommandReceiver_h___ */
