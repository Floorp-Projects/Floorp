/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsMenu_h__
#define nsMenu_h__

#include "nsWidget.h"
#include "nsIMenu.h"
#include "nsISupportsArray.h"
#include "nsIMenuListener.h"
class nsIContent;
class nsIDOMElement;
class nsIDOMNode;



class nsMenu : public nsIMenu, public nsIMenuListener
{

public:
  nsMenu();
  virtual ~nsMenu();

  NS_DECL_ISUPPORTS
  
  // nsIMenuListener interface
  nsEventStatus MenuItemSelected(const nsMenuEvent & aMenuEvent);
  nsEventStatus MenuSelected(const nsMenuEvent & aMenuEvent);
  nsEventStatus MenuDeselected(const nsMenuEvent & aMenuEvent);
  nsEventStatus MenuConstruct(const nsMenuEvent & aMenuEvent,
                              nsIWidget         * aParentWindow, 
                              void              * menubarNode,
                              void              * aWebShell);
  nsEventStatus MenuDestruct(const nsMenuEvent & aMenuEvent);

  
  // nsIMenu Methods
  NS_IMETHOD Create(nsISupports * aParent, const nsAReadableString &aLabel,
                    const nsAReadableString &aAccessKey, 
                    nsIChangeManager* aManager, nsIWebShell* aShell,
                    nsIContent* aContent );

  NS_IMETHOD GetParent(nsISupports *&aParent);
  NS_IMETHOD GetLabel(nsString &aText);
  NS_IMETHOD SetLabel(const nsAReadableString &aText);
  NS_IMETHOD GetAccessKey(nsString &aText);
  NS_IMETHOD SetAccessKey(const nsAReadableString &aText);

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
  NS_IMETHOD GetEnabled(PRBool* aIsEnabled);
  NS_IMETHOD IsHelpMenu(PRBool* aIsHelp);

  //
  NS_IMETHOD AddMenuItem(nsIMenuItem * aMenuItem);
  NS_IMETHOD AddMenu(nsIMenu * aMenu);
  NS_IMETHOD InsertSeparator(const PRUint32 aCount);

  NS_IMETHOD SetDOMNode(nsIDOMNode * menuNode);
  NS_IMETHOD SetDOMElement(nsIDOMElement * menuElement);
  NS_IMETHOD SetWebShell(nsIWebShell * aWebShell);


  // Native Impl Methods
  // These are not ref counted
  nsIMenu    * GetMenuParent()    { return mMenuParent;    } 
  nsIMenuBar * GetMenuBarParent() { return mMenuBarParent; }

protected:
  nsIMenuBar * GetMenuBar(nsIMenu * aMenu);
  nsIWidget *  GetParentWidget();

  nsString     mLabel;

  nsIMenuBar * mMenuBarParent;
  nsIMenu    * mMenuParent;

  nsISupportsArray * mItems;
  nsIMenuListener * mListener;

};

#endif // nsMenu_h__
