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

#ifndef nsIXPFCMenuBar_h___
#define nsIXPFCMenuBar_h___

#include "nsISupports.h"
#include "nsIShellInstance.h"
#include "nsIXPFCMenuItem.h"
#include "nsIXPFCMenuContainer.h"
#include "nsIWebViewerContainer.h"

// 52fb95f0-2cbe-11d2-9246-00805f8a7ab6
#define NS_IXPFCMENUBAR_IID      \
 { 0x52fb95f0, 0x2cbe, 0x11d2, \
   {0x92, 0x46, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6} }

class nsIXPFCMenuBar : public nsISupports
{

public:

  NS_IMETHOD SetShellContainer(nsIShellInstance * aShellInstance,
                               nsIWebViewerContainer * aWebViewerContainer) = 0 ;
  NS_IMETHOD_(nsIXPFCMenuItem *) MenuItemFromID(PRUint32 aID) = 0;
};

#endif /* nsIXPFCMenuBar_h___ */

