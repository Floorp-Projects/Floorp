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

#ifndef nsIPopUpMenu_h__
#define nsIPopUpMenu_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIMenuItem.h"

class nsIMenu;
class nsIWidget;

// {F6CD4F21-53AF-11d2-8DC4-00609703C14E}
#define NS_IPOPUPMENU_IID      \
{ 0xf6cd4f21, 0x53af, 0x11d2, \
  { 0x8d, 0xc4, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }

/**
 * PopUpMenu widget
 */
class nsIPopUpMenu : public nsISupports {

  public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPOPUPMENU_IID)

   /**
    * Creates the PopUpMenu
    *
    */
    NS_IMETHOD Create(nsIWidget * aParent) = 0;
    
   /**
    * Adds a PopUpMenu Item
    *
    */
    NS_IMETHOD AddItem(const nsString &aText) = 0;
    
   /**
    * Adds a PopUpMenu Item
    *
    */
    NS_IMETHOD AddItem(nsIMenuItem * aMenuItem) = 0;
    
   /**
    * Adds a Cascading PopUpMenu 
    *
    */
    NS_IMETHOD AddMenu(nsIMenu * aMenu) = 0;
    
   /**
    * Adds Separator
    *
    */
    NS_IMETHOD AddSeparator() = 0;

   /**
    * Returns the number of menu items
    *
    */
    NS_IMETHOD GetItemCount(PRUint32 &aCount) = 0;

   /**
    * Returns a PopUpMenu Item at a specified Index
    *
    */
    NS_IMETHOD GetItemAt(const PRUint32 aCount, nsIMenuItem *& aMenuItem) = 0;

   /**
    * Inserts a PopUpMenu Item at a specified Index
    *
    */
    NS_IMETHOD InsertItemAt(const PRUint32 aCount, nsIMenuItem *& aMenuItem) = 0;

   /**
    * Creates and inserts a PopUpMenu Item at a specified Index
    *
    */
    NS_IMETHOD InsertItemAt(const PRUint32 aCount, const nsString & aMenuItemName) = 0;

   /**
    * Creates and inserts a Separator at a specified Index
    *
    */
    NS_IMETHOD InsertSeparator(const PRUint32 aCount) = 0;

   /**
    * Removes an PopUpMenu Item from a specified Index
    *
    */
    NS_IMETHOD RemoveItem(const PRUint32 aCount) = 0;

   /**
    * Removes all the PopUpMenu Items
    *
    */
    NS_IMETHOD RemoveAll() = 0;

   /**
    * Shows menu and waits for action
    *
    */
    NS_IMETHOD ShowMenu(PRInt32 aX, PRInt32 aY) = 0;

   /**
    * Gets Native MenuHandle
    *
    */
    NS_IMETHOD  GetNativeData(void*& aData) = 0;

   /**
    * Gets parent widget
    *
    */
    NS_IMETHOD  GetParent(nsIWidget *& aParent) = 0;

};

#endif
