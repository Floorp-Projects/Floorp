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

#ifndef nsPopUpMenu_h__
#define nsPopUpMenu_h__

#include "nsdefs.h"
#include "nsWindow.h"
#include "nsSwitchToUIThread.h"

#include "nsIPopUpMenu.h"

/**
 * Native Win32 button wrapper
 */

class nsPopUpMenu :  public nsWindow,
                     public nsIPopUpMenu
{

public:
  nsPopUpMenu();
  virtual ~nsPopUpMenu();

  // nsISupports
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  
  NS_IMETHOD   Create(nsIWidget *aParent,
                         const nsRect &aRect,
                         EVENT_CALLBACK aHandleEventFunction,
                         nsIDeviceContext *aContext,
                         nsIAppShell *aAppShell = nsnull,
                         nsIToolkit *aToolkit = nsnull,
                         nsWidgetInitData *aInitData = nsnull);
  NS_IMETHOD   Create(nsNativeWidget aParent,
                         const nsRect &aRect,
                         EVENT_CALLBACK aHandleEventFunction,
                         nsIDeviceContext *aContext,
                         nsIAppShell *aAppShell = nsnull,
                         nsIToolkit *aToolkit = nsnull,
                         nsWidgetInitData *aInitData = nsnull);

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
  NS_IMETHOD  GetNativeData(void*& aData);

  virtual PRBool OnMove(PRInt32 aX, PRInt32 aY);
  virtual PRBool OnPaint();
  virtual PRBool OnResize(nsRect &aWindowRect);

protected:
  PRUint32 mNumMenuItems;
  nsIWidget * mPopUpParent;
  HMENU       mMenu;
};

#endif // nsPopUpMenu_h__
