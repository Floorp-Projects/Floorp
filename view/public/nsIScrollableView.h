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

#ifndef nsIScrollableView_h___
#define nsIScrollableView_h___

#include "nsISupports.h"
#include "nsCoord.h"
#include "nsIViewManager.h"
class nsIView;

// IID for the nsIView interface
#define NS_ISCROLLABLEVIEW_IID    \
{ 0xc95f1830, 0xc376, 0x11d1, \
{ 0xb7, 0x21, 0x0, 0x60, 0x8, 0x91, 0xd8, 0xc9 } }

class nsIScrollableView : public nsISupports
{
public:
  /**
   * Compute the size of the scrolled contanier.
   */
  virtual void ComputeContainerSize(void) = 0;

  /**
   * Get the dimensions of the container
   * @param aWidth return value for width of container
   * @param aHeight return value for height of container
   */
  virtual void GetContainerSize(nscoord *aWidth, nscoord *aHeight) = 0;

  /**
   * Set the offset into the container of the
   * top/left most visible coordinate
   * @param aOffsetX X offset in twips
   * @param aOffsetY Y offset in twips
   */
  virtual void SetVisibleOffset(nscoord aOffsetX, nscoord aOffsetY) = 0;

  /**
   * Get the offset of the top/left most visible coordinate
   * @param aOffsetX return value for X coordinate in twips
   * @param aOffsetY return value for Y coordinate in twips
   */
  virtual void GetVisibleOffset(nscoord *aOffsetX, nscoord *aOffsetY) = 0;

  /**
   * Get the view that we are scrolling within the
   * scrolling view. 
   * @result child view
   */
  virtual nsIView * GetScrolledView(void) = 0;

  /**
   * Select whether quality level should be displayed in view frame
   * @param aShow if PR_TRUE, quality level will be displayed, else hidden
   */
  virtual void ShowQuality(PRBool aShow) = 0;

  /**
   * Query whether quality level should be displayed in view frame
   * @return if PR_TRUE, quality level will be displayed, else hidden
   */
  virtual PRBool GetShowQuality(void) = 0;

  /**
   * Select whether quality level should be displayed in view frame
   * @param aShow if PR_TRUE, quality level will be displayed, else hidden
   */
  virtual void SetQuality(nsContentQuality aQuality) = 0;
};

#endif
