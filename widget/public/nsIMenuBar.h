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

#ifndef nsIMenuBar_h__
#define nsIMenuBar_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIMenu.h"
#include "nsIWebShell.h"

class nsIWidget;

// {f2e79601-1700-11d5-bb6f-90f240fe493c}
#define NS_IMENUBAR_IID      \
{ 0xf2e79601, 0x1700, 0x11d5, \
  { 0xbb, 0x6f, 0x90, 0xf2, 0x40, 0xfe, 0x49, 0x3c } };

/**
 * MenuBar widget
 */
class nsIMenuBar : public nsISupports {

  public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMENUBAR_IID)

   /**
    * Creates the MenuBar
    *
    */
    NS_IMETHOD Create(nsIWidget * aParent) = 0;

   /**
    * Get the MenuBar's Parent
    *
    */
    NS_IMETHOD GetParent(nsIWidget *&aParent) = 0;

   /**
    * Set the MenuBar's Parent
    *
    */
    NS_IMETHOD SetParent(nsIWidget *aParent) = 0;

   /**
    * Adds the Menu 
    *
    */
    NS_IMETHOD AddMenu(nsIMenu * aMenu) = 0;
    
   /**
    * Returns the number of menus
    *
    */
    NS_IMETHOD GetMenuCount(PRUint32 &aCount) = 0;

   /**
    * Returns a Menu Item at a specified Index
    *
    */
    NS_IMETHOD GetMenuAt(const PRUint32 aCount, nsIMenu *& aMenu) = 0;

   /**
    * Inserts a Menu at a specified Index
    *
    */
    NS_IMETHOD InsertMenuAt(const PRUint32 aCount, nsIMenu *& aMenu) = 0;

   /**
    * Removes an Menu from a specified Index
    *
    */
    NS_IMETHOD RemoveMenu(const PRUint32 aCount) = 0;

   /**
    * Removes all the Menus
    *
    */
    NS_IMETHOD RemoveAll() = 0;

   /**
    * Gets Native MenuHandle
    *
    */
    NS_IMETHOD  GetNativeData(void*& aData) = 0;

   /**
    * Sets Native MenuHandle. Temporary hack for mac until 
    * nsMenuBar does it's own construction
    */
    NS_IMETHOD  SetNativeData(void* aData) = 0;
    
   /**
    * Draw the menubar
    *
    */
    NS_IMETHOD  Paint() = 0;
   
};

#endif
