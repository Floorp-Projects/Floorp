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

#ifndef nsIMenuBar_h__
#define nsIMenuBar_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIMenu.h"

class nsIWidget;

// {BC658C81-4BEB-11d2-8DBB-00609703C14E}
#define NS_IMENUBAR_IID      \
{ 0xbc658c81, 0x4beb, 0x11d2, \
  { 0x8d, 0xbb, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }

/**
 * MenuBar widget
 */
class nsIMenuBar : public nsISupports {

  public:
     static const nsIID& GetIID() { static nsIID iid = NS_IMENUBAR_IID; return iid; }

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
    * Draw the menubar
    *
    */
    NS_IMETHOD  Paint() = 0;
};

#endif
