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

#ifndef nsIToolbar_h___
#define nsIToolbar_h___

#include "nsISupports.h"
#include "nsString.h"
#include "nsGUIEvent.h"

class nsIWidget;
class nsIThrobber;
#if GRIPPYS_NOT_WIDGETS
  class nsIToolbarManager;
#endif
class nsIToolbarItem;

// deb24690-35f8-11d2-9248-00805f8a7ab6
#define NS_ITOOLBAR_IID      \
 { 0xdeb24690, 0x35f8, 0x11d2, \
   {0x92, 0x48, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6} }

enum nsToolbarBorderType {   
                  // no border
                eToolbarBorderType_none,
                  // draws partial border
                eToolbarBorderType_partial,
                  // draws border on all sides
                eToolbarBorderType_full
              }; 

class nsIToolbar : public nsISupports
{
public:
  static const nsIID& IID() { static nsIID iid = NS_ITOOLBAR_IID; return iid; }

 /**
  * Adds a widget to the toolbar and indicates the left side gap
  *
  */
  NS_IMETHOD AddItem(nsIToolbarItem* anItem, PRInt32 aLeftGap, PRBool aStretchable) = 0;

 /**
  * Inserts a item into the toolbar at a specified position
  *
  */
  NS_IMETHOD InsertItemAt(nsIToolbarItem* anItem, 
                          PRInt32         aLeftGap, 
                          PRBool          aStretchable, 
                          PRInt32         anIndex) = 0;

 /**
  * Returns an item from the toolbar at a specified position
  *
  */
  NS_IMETHOD GetItemAt(nsIToolbarItem*& anItem, PRInt32 anIndex) = 0;

 /**
  * Get the preferred size of the toolbar
  *
  */
  NS_IMETHOD GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight) = 0;

 /**
  * Returns whether the toolbar is visible
  *
  */
  NS_IMETHOD IsVisible(PRBool & aIsVisible) = 0;

 /**
  * Sets the horizontal gap in between the last item and the margin
  *
  */
  NS_IMETHOD SetHGap(PRInt32 aGap) = 0;

 /**
  * Sets the vertical gap (if the toolbar "wraps")
  *
  */
  NS_IMETHOD SetVGap(PRInt32 aGap) = 0;

 /**
  * Set the "margin", the space from the edge of the toolbar to the top of the widgets
  *
  */
  NS_IMETHOD SetMargin(PRInt32 aMargin) = 0;

 /**
  * Forces the toolbar to layout
  *
  */
  NS_IMETHOD DoLayout() = 0;

 /**
  * Forces the toolbar to layout horizontally
  *
  */
  NS_IMETHOD SetHorizontalLayout(PRBool aDoHorizontalLayout) = 0;

 /**
  * Indicates that the last item is right justifed
  *
  */
  NS_IMETHOD SetLastItemIsRightJustified(const PRBool & aState) = 0;

 /**
  * Indicates that the next to the last item will be stretched
  *
  */
  NS_IMETHOD SetNextLastItemIsStretchy(const PRBool & aState) = 0;

#if GRIPPYS_NOT_WIDGETS
 /**
  * Sets the Toolbar manager for this toolbar
  *
  */
  NS_IMETHOD SetToolbarManager(nsIToolbarManager * aToolbarManager) = 0;

 /**
  * Gets the Toolbar manager for this toolbar
  *
  */
  NS_IMETHOD GetToolbarManager(nsIToolbarManager *& aToolbarManager) = 0;
#endif

 /**
  * Tells the toolbar to draw the border on all 4 sides, instead of just top and bottom
  *
  */
  NS_IMETHOD SetBorderType(nsToolbarBorderType aBorderType) = 0;

 /**
  * Tells the toolbar to wrap
  *
  */
  NS_IMETHOD SetWrapping(PRBool aDoWrap) = 0;

 /**
  * Indicates whether the toolbar is to wrap
  *
  */
  NS_IMETHOD GetWrapping(PRBool & aDoWrap) = 0;

 /**
  * Get the preferred size of the toolbar when constrainted horizontally or vertically
  *
  */
  NS_IMETHOD GetPreferredConstrainedSize(PRInt32& aSuggestedWidth, PRInt32& aSuggestedHeight, 
                                         PRInt32& aWidth,          PRInt32& aHeight) = 0;

 /**
  * HandleGUI Events
  *
  */
  NS_IMETHOD_(nsEventStatus) HandleEvent(nsGUIEvent *aEvent) = 0;

 /**
  * Handle OnPaint
  *
  */
  NS_IMETHOD_(nsEventStatus) OnPaint(nsIRenderingContext& aRenderingContext,
                                     const nsRect& aDirtyRect) = 0;


#if GRIPPYS_NOT_WIDGETS
 /**
  * Create a Tab on this toolbar
  *
  */
  NS_IMETHOD CreateTab(nsIWidget *& aTab) = 0;
#endif

};

#endif /* nsIToolbar_h___ */

