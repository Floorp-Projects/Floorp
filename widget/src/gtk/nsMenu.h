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

#ifndef nsMenu_h__
#define nsMenu_h__

#include "nsIMenu.h"
#include "nsVoidArray.h"
#include "nsIMenuListener.h"

class nsIMenuBar;
class nsIMenuListener;

#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIWebShell.h"

/**
 * Native GTK+ Menu wrapper
 */

class nsMenu : public nsIMenu, public nsIMenuListener
{

public:
  nsMenu();
  virtual ~nsMenu();

  NS_DECL_ISUPPORTS
  
  // nsIMenuListener methods
  nsEventStatus MenuItemSelected(const nsMenuEvent & aMenuEvent);
  nsEventStatus MenuSelected(const nsMenuEvent & aMenuEvent); 
  nsEventStatus MenuDeselected(const nsMenuEvent & aMenuEvent); 
  nsEventStatus MenuConstruct(
    const nsMenuEvent & aMenuEvent,
    nsIWidget         * aParentWindow, 
    void              * menuNode,
	void              * aWebShell);
  nsEventStatus MenuDestruct(const nsMenuEvent & aMenuEvent); 
  
  NS_IMETHOD Create(nsISupports * aParent, const nsString &aLabel);

  // nsIMenu Methods
  NS_IMETHOD GetParent(nsISupports *&aParent);
  NS_IMETHOD GetLabel(nsString &aText);
  NS_IMETHOD SetLabel(const nsString &aText);
  NS_IMETHOD AddItem(nsISupports * aItem);
  NS_IMETHOD AddMenuItem(nsIMenuItem * aMenuItem);
  NS_IMETHOD AddMenu(nsIMenu * aMenu);
  NS_IMETHOD AddSeparator();
  NS_IMETHOD GetItemCount(PRUint32 &aCount);
  NS_IMETHOD GetItemAt(const PRUint32 aPos, nsISupports *& aMenuItem);
  NS_IMETHOD InsertItemAt(const PRUint32 aPos, nsISupports * aMenuItem);
  NS_IMETHOD InsertSeparator(const PRUint32 aPos);
  NS_IMETHOD RemoveItem(const PRUint32 aPos);
  NS_IMETHOD RemoveAll();
  NS_IMETHOD GetNativeData(void** aData);
  NS_IMETHOD AddMenuListener(nsIMenuListener * aMenuListener);
  NS_IMETHOD RemoveMenuListener(nsIMenuListener * aMenuListener);

  NS_IMETHOD SetDOMNode(nsIDOMNode * aMenuNode);
  NS_IMETHOD SetDOMElement(nsIDOMElement * aMenuElement);
  NS_IMETHOD SetWebShell(nsIWebShell * aWebShell);
  
protected:
  void LoadMenuItem(
    nsIMenu       * pParentMenu,
    nsIDOMElement * menuitemElement,
    nsIDOMNode    * menuitemNode,
    unsigned short  menuitemIndex,
    nsIWebShell   * aWebShell);
  
  void LoadSubMenu(
    nsIMenu       * pParentMenu,
    nsIDOMElement * menuElement,
    nsIDOMNode    * menuNode);
    
  void       Create(GtkWidget *aParent, const nsString &aLabel);
  GtkWidget  *GetNativeParent();

  nsString   mLabel;
  PRUint32   mNumMenuItems;
  GtkWidget  *mMenu;
  
  nsVoidArray mMenuItemVoidArray;

  nsIMenu    *mMenuParent;
  nsIMenuBar *mMenuBarParent;
  nsIMenuListener * mListener;
  
  PRBool mConstructCalled;
  nsIDOMNode    * mDOMNode;
  nsIWebShell   * mWebShell;
  nsIDOMElement * mDOMElement;
};

#endif // nsMenu_h__
