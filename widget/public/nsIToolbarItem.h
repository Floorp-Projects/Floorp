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

#ifndef nsIToolbarItem_h___
#define nsIToolbarItem_h___

#include "nsISupports.h"

struct nsRect;

// {1F9EE621-5234-11d2-8DC1-00609703C14E}
#define NS_ITOOLBARITEM_IID      \
{ 0x1f9ee621, 0x5234, 0x11d2, \
  { 0x8d, 0xc1, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }

class nsIToolbarItem : public nsISupports
{

public:
  static const nsIID& IID() { static nsIID iid = NS_ITOOLBARITEM_IID; return iid; }

    /**
     * Forces the item to repaint 
     *
     */
    NS_IMETHOD Repaint(PRBool aIsSynchronous) = 0;

    /**
     * Get this widget's dimension
     *
     * @param aRect on return it holds the  x. y, width and height of this widget
     *
     */
    NS_IMETHOD GetBounds(nsRect &aRect) = 0;

    /**
     * Show or hide this widget
     *
     * @param aState PR_TRUE to show the Widget, PR_FALSE to hide it
     *
     */
    NS_IMETHOD SetVisible(PRBool aState) = 0;

    /**
     * Returns whether the window is visible
     *
     */
    NS_IMETHOD IsVisible(PRBool & aState) = 0;

    /**
     * Move this widget.
     *
     * @param aX the new x position expressed in the parent's coordinate system
     * @param aY the new y position expressed in the parent's coordinate system
     *
     **/
    NS_IMETHOD SetLocation(PRUint32 aX, PRUint32 aY) = 0;

    /**
     * Resize this widget. 
     *
     * @param aWidth  the new width expressed in the parent's coordinate system
     * @param aHeight the new height expressed in the parent's coordinate system
     * @param aRepaint whether the widget should be repainted
     *
     */
    NS_IMETHOD SetBounds(PRUint32 aWidth,
                         PRUint32 aHeight,
                         PRBool   aRepaint) = 0;

    /**
     * Move or resize this widget.
     *
     * @param aX       the new x position expressed in the parent's coordinate system
     * @param aY       the new y position expressed in the parent's coordinate system
     * @param aWidth   the new width expressed in the parent's coordinate system
     * @param aHeight  the new height expressed in the parent's coordinate system
     * @param aRepaint whether the widget should be repainted if the size changes
     *
     */
    NS_IMETHOD SetBounds(PRUint32 aX,
                         PRUint32 aY,
                         PRUint32 aWidth,
                         PRUint32 aHeight,
                         PRBool   aRepaint) = 0;

    /**
     * Returns the preferred width and height for the widget
     *
     */
    NS_IMETHOD GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight) = 0;

    /**
     * Set the preferred width and height for the widget
     *
     */
    NS_IMETHOD SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight) = 0;


};

#endif /* nsIToolbarItem_h___ */

