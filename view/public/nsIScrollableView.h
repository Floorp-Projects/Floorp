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

typedef enum
{
  nsScrollPreference_kAuto = 0,
  nsScrollPreference_kNeverScroll,
  nsScrollPreference_kAlwaysScroll
} nsScrollPreference;

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
  NS_IMETHOD  ComputeContainerSize(void) = 0;

  /**
   * Get the dimensions of the container
   * @param aWidth return value for width of container
   * @param aHeight return value for height of container
   */
  NS_IMETHOD  GetContainerSize(nscoord *aWidth, nscoord *aHeight) = 0;

  /**
   * Set the view that we are scrolling within the
   * scrolling view. 
   */
  NS_IMETHOD  SetScrolledView(nsIView *aScrolledView) = 0;

  /**
   * Get the view that we are scrolling within the
   * scrolling view. 
   * @result child view
   */
  NS_IMETHOD  GetScrolledView(nsIView *&aScrolledView) = 0;

  /**
   * Select whether quality level should be displayed in view frame
   * @param aShow if PR_TRUE, quality level will be displayed, else hidden
   */
  NS_IMETHOD  ShowQuality(PRBool aShow) = 0;

  /**
   * Query whether quality level should be displayed in view frame
   * @return if PR_TRUE, quality level will be displayed, else hidden
   */
  NS_IMETHOD  GetShowQuality(PRBool &aShow) = 0;

  /**
   * Select whether quality level should be displayed in view frame
   * @param aShow if PR_TRUE, quality level will be displayed, else hidden
   */
  NS_IMETHOD  SetQuality(nsContentQuality aQuality) = 0;

  /**
   * Select whether scroll bars should be displayed all the time, never or
   * only when necessary.
   * @param aPref desired scrollbar selection
   */
  NS_IMETHOD  SetScrollPreference(nsScrollPreference aPref) = 0;

  /**
   * Query whether scroll bars should be displayed all the time, never or
   * only when necessary.
   * @return current scrollbar selection
   */
  NS_IMETHOD  GetScrollPreference(nsScrollPreference& aScrollPreference) = 0;

  /**
   * Scroll the view to the given x,y, update's the scrollbar's thumb
   * positions and the view's offset. Clamps the values to be
   * legal. Updates the display based on aUpdateFlags.
   * @param aX left edge to scroll to
   * @param aY top edge to scroll to
   * @param aUpdateFlags passed onto nsIViewManager->UpdateView()
   * @return error status
   */
  NS_IMETHOD ScrollTo(nscoord aX, nscoord aY, PRUint32 aUpdateFlags) = 0;

  /**
   * get the size of the content display area in the view. it can be different
   * from the bounds of the view.
   * @param aWidth out parameter for width of clip area
   * @param aHeight out parameter for height of clip area
   * @return error status
   */
  NS_IMETHOD GetClipSize(nscoord *aWidth, nscoord *aHeight) = 0;

private:
  NS_IMETHOD_(nsrefcnt) AddRef(void) = 0;
  NS_IMETHOD_(nsrefcnt) Release(void) = 0;
};

#endif
