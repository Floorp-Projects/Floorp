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
 */

#ifndef nsPopUpMenu_h__
#define nsPopUpMenu_h__

#include "nsdefs.h"
#include "nsWindow.h"
#include "nsSwitchToUIThread.h"

#include "nsIPopUpMenu.h"

/**
 * Native Win32 button wrapper
 */

class nsPopUpMenu : public nsIPopUpMenu
{

public:
  nsPopUpMenu();
  virtual ~nsPopUpMenu();

  NS_DECL_ISUPPORTS
  
  NS_IMETHOD Create(nsIWidget * aParent);

  // nsIPopUpMenu Methods
  NS_IMETHOD AddItem(const nsString &aText);
  NS_IMETHOD AddItem(nsIMenuItem * aMenuItem);
  NS_IMETHOD AddMenu(nsIMenu * aMenu);
  NS_IMETHOD AddSeparator();
  NS_IMETHOD GetItemCount(PRUint32 &aCount);
  NS_IMETHOD GetItemAt(const PRUint32 aCount, nsIMenuItem *& aMenuItem);
  NS_IMETHOD InsertItemAt(const PRUint32 aCount, nsIMenuItem *& aMenuItem);
  NS_IMETHOD InsertItemAt(const PRUint32 aCount, const nsString & aMenuItemName);
  NS_IMETHOD InsertSeparator(const PRUint32 aCount);
  NS_IMETHOD RemoveItem(const PRUint32 aCount);
  NS_IMETHOD RemoveAll();
  NS_IMETHOD ShowMenu(PRInt32 aX, PRInt32 aY);
  NS_IMETHOD GetNativeData(void*& aData);
  NS_IMETHOD GetParent(nsIWidget*& aParent);

protected:

  PRUint32    mNumMenuItems;

  nsIWidget * mParent;
  HMENU       mMenu;
};

#endif // nsPopUpMenu_h__
