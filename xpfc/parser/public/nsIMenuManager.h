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

#ifndef nsIMenuManager_h___
#define nsIMenuManager_h___

#include "nsISupports.h"
#include "nsIXPFCMenuContainer.h"
#include "nsIXPFCCommandReceiver.h"

class nsIXPFCMenuBar;

//5e1180e0-30a9-11d2-9247-00805f8a7ab6
#define NS_IMENU_MANAGER_IID   \
{ 0x5e1180e0, 0x30a9, 0x11d2,    \
{ 0x92, 0x47, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

class nsIMenuManager : public nsISupports
{

public:

  NS_IMETHOD                 Init() = 0 ;
  NS_IMETHOD                 SetMenuBar(nsIXPFCMenuBar * aMenuBar) = 0;
  NS_IMETHOD_(nsIXPFCMenuBar *)  GetMenuBar() = 0;
  NS_IMETHOD                 AddMenuContainer(nsIXPFCMenuContainer * aMenuContainer) = 0;
  NS_IMETHOD_(nsIXPFCMenuItem *) MenuItemFromID(PRUint32 aID) = 0;
  NS_IMETHOD_(PRUint32)      GetID() = 0;
  NS_IMETHOD_(nsIXPFCCommandReceiver*) GetDefaultReceiver() = 0;
  NS_IMETHOD SetDefaultReceiver(nsIXPFCCommandReceiver* aReceiver) = 0;

};

#endif /* nsIMenuManager_h___ */
