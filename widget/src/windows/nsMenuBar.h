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

#ifndef nsMenuBar_h__
#define nsMenuBar_h__

#include "nsdefs.h"
#include "nsWindow.h"
#include "nsSwitchToUIThread.h"

#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsIWebShell.h"

#include "nsIMenuBar.h"
#include "nsIMenuListener.h"
#include "nsVoidArray.h"

class nsIWidget;

/**
 * Native Win32 button wrapper
 */

class nsMenuBar : public nsIMenuBar, public nsIMenuListener
{

public:
  nsMenuBar();
  virtual ~nsMenuBar();

  // nsIMenuListener interface
  nsEventStatus MenuItemSelected(const nsMenuEvent & aMenuEvent);
  nsEventStatus MenuSelected(const nsMenuEvent & aMenuEvent);
  nsEventStatus MenuDeselected(const nsMenuEvent & aMenuEvent);
  nsEventStatus MenuConstruct(
    const nsMenuEvent & aMenuEvent,
    nsIWidget         * aParentWindow, 
    void              * menubarNode,
	void              * aWebShell);
  nsEventStatus MenuDestruct(const nsMenuEvent & aMenuEvent);
  
  NS_DECL_ISUPPORTS

  // nsIMenuBar Methods
  NS_IMETHOD Create(nsIWidget * aParent);
  NS_IMETHOD GetParent(nsIWidget *&aParent);
  NS_IMETHOD SetParent(nsIWidget * aParent);
  NS_IMETHOD AddMenu(nsIMenu * aMenu);
  NS_IMETHOD GetMenuCount(PRUint32 &aCount);
  NS_IMETHOD GetMenuAt(const PRUint32 aPos, nsIMenu *& aMenu);
  NS_IMETHOD InsertMenuAt(const PRUint32 aPos, nsIMenu *& aMenu);
  NS_IMETHOD RemoveMenu(const PRUint32 aPos);
  NS_IMETHOD RemoveAll();
  NS_IMETHOD GetNativeData(void*& aData);
  NS_IMETHOD Paint();
  NS_IMETHOD SetNativeData(void* aData);

  //NS_IMETHOD ConstructMenuBar(nsIDOMElement * menubarElement);

protected:
  PRUint32    mNumMenus;
  HMENU       mMenu;
  nsIWidget * mParent;

  PRBool      mIsMenuBarAdded;

  nsVoidArray * mItems;
  
  nsIDOMNode    * mDOMNode;
  nsIDOMElement * mDOMElement;
  nsIWebShell   * mWebShell;
  bool            mConstructed;
};

#endif // nsMenuBar_h__
