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

#include "nsIMenuBar.h"
#include "nsIMenuListener.h"
#include "nsVoidArray.h"

#include "nsIWebShell.h"
#include "nsIDOMNode.h"

class nsIWidget;

/**
 * Native GTK+ MenuBar wrapper
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
    void              * menuNode,
	void              * aWebShell);
  nsEventStatus MenuDestruct(const nsMenuEvent & aMenuEvent);
  
  NS_DECL_ISUPPORTS

  
  NS_IMETHOD Create(nsIWidget * aParent);

  // nsIMenuBar Methods
  NS_IMETHOD GetParent(nsIWidget *&aParent);
  NS_IMETHOD SetParent(nsIWidget * aParent);
  NS_IMETHOD AddMenu(nsIMenu * aMenu);
  NS_IMETHOD GetMenuCount(PRUint32 &aCount);
  NS_IMETHOD GetMenuAt(const PRUint32 aCount, nsIMenu *& aMenu);
  NS_IMETHOD InsertMenuAt(const PRUint32 aCount, nsIMenu *& aMenu);
  NS_IMETHOD RemoveMenu(const PRUint32 aCount);
  NS_IMETHOD RemoveAll();
  NS_IMETHOD GetNativeData(void*& aData);
  NS_IMETHOD Paint();
  NS_IMETHOD SetNativeData(void* aData);
protected:
  GtkWidget * mMenuBar;
  nsIWidget * mParent;
  PRBool      mIsMenuBarAdded;
  
  nsIWebShell * mWebShell;
  nsIDOMNode  * mDOMNode;
  
  nsVoidArray mMenusVoidArray;
  PRUint32    mNumMenus;
};

#endif // nsMenuBar_h__
