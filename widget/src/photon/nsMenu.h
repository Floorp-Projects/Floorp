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

#ifndef nsMenu_h__
#define nsMenu_h__

#include "Pt.h"
#include "nsIMenu.h"
#include "nsISupportsArray.h"
#include "nsIMenuListener.h"

class nsIMenuBar;
class nsIMenuListener;

/**
 * Native Photon Menu wrapper
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
  

  // nsIMenu Methods
  NS_IMETHOD Create(nsISupports * aParent, const nsString &aLabel);
  NS_IMETHOD GetParent(nsISupports *&aParent);
  NS_IMETHOD GetLabel(nsString &aText);
  NS_IMETHOD SetLabel(const nsString &aText);
  NS_IMETHOD GetAccessKey(nsString &aText);
  NS_IMETHOD SetAccessKey(const nsString &aText);
  NS_IMETHOD AddItem(nsISupports * aItem);

  NS_IMETHOD AddSeparator();
  NS_IMETHOD GetItemCount(PRUint32 &aCount);
  NS_IMETHOD GetItemAt(const PRUint32 aPos, nsISupports *& aMenuItem);
  NS_IMETHOD InsertItemAt(const PRUint32 aPos, nsISupports * aMenuItem);
  NS_IMETHOD RemoveItem(const PRUint32 aPos);
  NS_IMETHOD RemoveAll();
  NS_IMETHOD GetNativeData(void** aData);
  NS_IMETHOD SetNativeData(void* aData);

  NS_IMETHOD AddMenuListener(nsIMenuListener * aMenuListener);
  NS_IMETHOD RemoveMenuListener(nsIMenuListener * aMenuListener);
  NS_IMETHOD SetEnabled(PRBool aIsEnabled);

  //
  NS_IMETHOD AddMenuItem(nsIMenuItem * aMenuItem);
  NS_IMETHOD AddMenu(nsIMenu * aMenu);
  NS_IMETHOD InsertSeparator(const PRUint32 aPos);
  
  NS_IMETHOD SetDOMNode(nsIDOMNode * aMenuNode);
  NS_IMETHOD SetDOMElement(nsIDOMElement * aMenuElement);
  NS_IMETHOD SetWebShell(nsIWebShell * aWebShell);
  
  // Native Photon Support Methods
  static int TopLevelMenuItemArmCb (
    PtWidget_t *widget, void *aNSMenu, PtCallbackInfo_t *cbinfo);
  static int SubMenuMenuItemArmCb (
    PtWidget_t *widget, void *aNSMenu, PtCallbackInfo_t *cbinfo);
  static int SubMenuMenuItemMenuCb (
    PtWidget_t *widget, void *aNSMenu, PtCallbackInfo_t *cbinfo);
  static int MenuRealizedCb (
    PtWidget_t *widget, void *aNSMenu, PtCallbackInfo_t *cbinfo);
 static int MenuUnRealizedCb (
    PtWidget_t *widget, void *aNSMenu, PtCallbackInfo_t *cbinfo);

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

  void       Create(PtWidget_t *aParent, const nsString &aLabel);
  PtWidget_t *GetNativeParent();

  nsString   mLabel;
  PtWidget_t *mMenu;
  PtWidget_t *mMenuButton;
  
  nsISupportsArray     *mItems;
  nsIMenuListener      *mListener;

  nsIMenu         *mMenuParent;
  nsIMenuBar      *mMenuBarParent;

  nsIWidget     * mParentWindow;
  nsIDOMNode    * mDOMNode;
  nsIDOMElement * mDOMElement;
  nsIWebShell   * mWebShell;
  PRBool          mConstruct;
  PRBool          mIsSubMenu;
};

#endif // nsMenu_h__
