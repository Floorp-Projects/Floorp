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

#ifndef nsIXPFCMenuContainer_h___
#define nsIXPFCMenuContainer_h___

#include "nsISupports.h"
#include "nsIShellInstance.h"
#include "nsIXPFCMenuItem.h"

// 0b396820-2f54-11d2-bede-00805f8a8dbd
#define NS_IXPFCMENUCONTAINER_IID      \
 { 0x0b396820, 0x2f54, 0x11d2, \
   {0xbe, 0xde, 0x00, 0x80, 0x5f, 0x8a, 0x8d, 0xbd} }

class nsIXPFCMenuContainer : public nsISupports 
{

public:

  /**
   * Initialize the XPFCMenuContainer
   * @result The result of the initialization, NS_Ok if no errors
   */
  NS_IMETHOD Init() = 0;

  NS_IMETHOD AddMenuItem(nsIXPFCMenuItem * aMenuItem) = 0;
  NS_IMETHOD_(void*) GetNativeHandle() = 0;
  NS_IMETHOD AddChild(nsIXPFCMenuItem * aItem) = 0;
  NS_IMETHOD Update() = 0;
  NS_IMETHOD_(nsIXPFCMenuItem *) MenuItemFromID(PRUint32 aID) = 0;

};

#endif /* nsIXPFCMenuContainer_h___ */

