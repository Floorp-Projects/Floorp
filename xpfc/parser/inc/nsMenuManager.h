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

#ifndef nsMenuManager_h___
#define nsMenuManager_h___

#include "nsIMenuManager.h"
#include "nsIArray.h"
#include "nsIIterator.h"

class nsMenuManager : public nsIMenuManager
{

public:
  nsMenuManager();

  NS_DECL_ISUPPORTS

  NS_IMETHOD                 Init();
  NS_IMETHOD                 SetMenuBar(nsIXPFCMenuBar * aMenuBar);
  NS_IMETHOD_(nsIXPFCMenuBar *)  GetMenuBar();
  NS_IMETHOD                 AddMenuContainer(nsIXPFCMenuContainer * aMenuContainer) ;
  NS_IMETHOD_(nsIXPFCMenuItem *) MenuItemFromID(PRUint32 aID) ;
  NS_IMETHOD_(PRUint32)      GetID();
  NS_IMETHOD_(nsIXPFCCommandReceiver*) GetDefaultReceiver() ;
  NS_IMETHOD SetDefaultReceiver(nsIXPFCCommandReceiver* aReceiver) ;

protected:
  ~nsMenuManager();

private:
  nsIXPFCMenuBar * mMenuBar;
  nsIArray * mMenuContainers;
  PRUint32    mValidMenuID;
  nsIXPFCCommandReceiver * mDefaultReceiver;

};

#endif /* nsMenuManager_h___ */
