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
 *  Michael Lowe <michael.lowe@bigfoot.com>
 */
#ifndef nsIWindowsTimerMap_h___
#define nsIWindowsTimerMap_h___

#include "nscore.h"
#include "nsISupports.h"

class nsTimer;

/// Interface IID for nsIWindowsTimerMap
#define NS_IWINDOWSTIMERMAP_IID         \
{ 0x81eba2c6, 0x1dd2, 0x11b2,  \
{ 0xa0, 0xc7, 0x97, 0x42, 0x31, 0xc7, 0x3d, 0xcd } }


class nsIWindowsTimerMap : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IWINDOWSTIMERMAP_IID)

  NS_IMETHOD_(nsTimer*) GetTimer(UINT id)=0;
  NS_IMETHOD_(void) AddTimer(UINT timerID, nsTimer* timer)=0;
  NS_IMETHOD_(void) RemoveTimer(UINT timerID)=0;
};

#endif // nsIWindowsTimerMap_h___
