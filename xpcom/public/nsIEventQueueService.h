/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef nsIEventQueueService_h__
#define nsIEventQueueService_h__

#include "nsISupports.h"
#include "prthread.h"
#include "plevent.h"

/* a6cf90dc-15b3-11d2-932e-00805f8add32 */
#define NS_IEVENTQUEUESERVICE_IID \
{ 0xa6cf90dc, 0x15b3, 0x11d2, \
  {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }


class nsIEventQueueService : public nsISupports
{
public:
  NS_IMETHOD CreateThreadEventQueue(void) = 0;
  NS_IMETHOD DestroyThreadEventQueue(void) = 0;

  NS_IMETHOD GetThreadEventQueue(PRThread* aThread, PLEventQueue** aResult) = 0;
  
#ifdef XP_MAC
// This is ment to be temporary until something better is worked out
 NS_IMETHOD ProcessEvents() = 0;
#endif
};

#endif /* nsIEventQueueService_h___ */
