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

#ifndef nsIXPFCSubject_h___
#define nsIXPFCSubject_h___

#include "nsISupports.h"

class nsIXPFCObserver;
class nsIXPFCCommand;

//72c5bfd0-1f55-11d2-bed9-00805f8a8dbd
#define NS_IXPFC_SUBJECT_IID   \
{ 0x72c5bfd0, 0x1f55, 0x11d2,    \
{ 0xbe, 0xd9, 0x00, 0x80, 0x5f, 0x8a, 0x8d, 0xbd } }

class nsIXPFCSubject : public nsISupports
{

public:

  NS_IMETHOD Init() = 0;

  NS_IMETHOD Attach(nsIXPFCObserver * aObserver) = 0;
  NS_IMETHOD Detach(nsIXPFCObserver * aObserver) = 0;
  NS_IMETHOD Notify(nsIXPFCCommand * aCommand) = 0;

};

#endif /* nsIXPFCSubject_h___ */
