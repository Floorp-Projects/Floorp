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

#ifndef nsIContextMenu_h__
#define nsIContextMenu_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIWebShell.h"

class nsIMenuBar;
class nsIMenu;
class nsIMenuItem;
class nsIMenuListener;

// {ab6cea80-00ff-11d5-bb6f-f432a43ead7c}
#define NS_ICONTEXTMENU_IID      \
{ 0xab6cea80, 0x00ff, 0x11d5,    \
  { 0xbb, 0x6f, 0xf4, 0x32, 0xa4, 0x3e, 0xad, 0x7c } };

/**
 * Menu widget
 */
class nsIContextMenu : public nsISupports {

  public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICONTEXTMENU_IID)

  /**
    * Creates the context menu
    *
    */
    NS_IMETHOD Create(nsISupports * aParent, const nsString& anAlignment, const nsString& anAnchorAlignment) = 0;

   /**
    * Get the context menu's Parent
    *
    */
    NS_IMETHOD GetParent(nsISupports *&aParent) = 0;

   /**
    * Adds a context menu Item
    *
    */
    NS_IMETHOD AddItem(nsISupports* aItem) = 0;

   /**
    * Adds a separator
    *
    */
    NS_IMETHOD AddSeparator() = 0;

   /**
    * Returns the number of context menu items
    * This does count separators as items
    */
    NS_IMETHOD GetItemCount(PRUint32 &aCount) = 0;

   /**
    * Returns a Menu or Menu Item at a specified Index
    *
    */
    NS_IMETHOD GetItemAt(const PRUint32 aPos, nsISupports *& aMenuItem) = 0;

   /**
    * Inserts a Menu Item at a specified Index
    *
    */
    NS_IMETHOD InsertItemAt(const PRUint32 aPos, nsISupports * aMenuItem) = 0;

   /**
    * Removes an Menu Item from a specified Index
    *
    */
    NS_IMETHOD RemoveItem(const PRUint32 aPos) = 0;

   /**
    * Removes all the Menu Items
    *
    */
    NS_IMETHOD RemoveAll() = 0;

   /**
    * Gets Native MenuHandle
    *
    */
    NS_IMETHOD  GetNativeData(void** aData) = 0;

   /**
    * Adds menu listener for dynamic construction
    *
    */
    NS_IMETHOD AddMenuListener(nsIMenuListener * aMenuListener) = 0;

   /**
    * Removes menu listener for dynamic construction
    *
    */
    NS_IMETHOD RemoveMenuListener(nsIMenuListener * aMenuListener) = 0;

   /**
    * Set location
    *
    */
    NS_IMETHOD SetLocation(PRInt32 aX, PRInt32 aY) = 0;

   /**
    * Set DOMNode
    *
    */
    NS_IMETHOD SetDOMNode(nsIDOMNode * aMenuNode) = 0;

	/**
    * Set DOMElement
    *
    */
    NS_IMETHOD SetDOMElement(nsIDOMElement * aMenuElement) = 0;

	/**
    * Set WebShell
    *
    */
    NS_IMETHOD SetWebShell(nsIWebShell * aWebShell) = 0;
};

#endif
