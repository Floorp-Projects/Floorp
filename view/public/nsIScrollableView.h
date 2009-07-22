/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIScrollableView_h___
#define nsIScrollableView_h___

#include "nsCoord.h"
#include "nsNativeWidget.h"

class nsIView;
class nsIScrollPositionListener;
struct nsSize;

// IID for the nsIScrollableView interface
#define NS_ISCROLLABLEVIEW_IID    \
  { 0x74254899, 0xccc9, 0x4e67, \
    { 0xa6, 0x78, 0x6b, 0x79, 0xfa, 0x72, 0xb4, 0x86 } }

/**
 * A scrolling view allows an arbitrary view that you supply to be scrolled
 * vertically or horizontally (or both). The scrolling view creates and
 * manages the scrollbars.
 *
 * You must use SetScrolledView() to specify the view that is to be scrolled,
 * because the scrolled view is made a child of the clip view (an internal
 * child view created by the scrolling view).
 *
 */
class nsIScrollableView {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCROLLABLEVIEW_IID)

  /**
   * Get the dimensions of the container
   * @param aWidth return value for width of container
   * @param aHeight return value for height of container
   */
  NS_IMETHOD  GetContainerSize(nscoord *aWidth, nscoord *aHeight) const = 0;

  /**
   * Set the view that we are scrolling within the scrolling view. 
   */
  NS_IMETHOD  SetScrolledView(nsIView *aScrolledView) = 0;

  /**
   * Get the view that we are scrolling within the scrolling view. 
   * @result child view
   */
  NS_IMETHOD  GetScrolledView(nsIView *&aScrolledView) const = 0;

  /**
   * Get the position of the scrolled view.
   */
  NS_IMETHOD  GetScrollPosition(nscoord &aX, nscoord& aY) const = 0;

  /**
   * Scroll the view to the given x,y, update's the scrollbar's thumb
   * positions and the view's offset. Clamps the values to be
   * legal. Updates the display based on aUpdateFlags.
   * @param aX left edge to scroll to
   * @param aY top edge to scroll to
   * @param aUpdateFlags indicate smooth or async scrolling
   * @return error status
   */
  NS_IMETHOD ScrollTo(nscoord aX, nscoord aY, PRUint32 aUpdateFlags) = 0;

  /**
   * Set the height of a line used for line scrolling.
   * @param aHeight new line height in app units. the default
   *        height is 12 points.
   * @return error status
   */
  NS_IMETHOD SetLineHeight(nscoord aHeight) = 0;

  /**
   * Get the height of a line used for line scrolling.
   * @param aHeight out parameter for line height
   * @return error status
   */
  NS_IMETHOD GetLineHeight(nscoord *aHeight) = 0;

  /**
   * Scroll the view left or right by aNumLinesX columns. Positive values move right. 
   * Scroll the view up or down by aNumLinesY lines. Positive values move down. 
   * Prevents scrolling off the end of the view.
   * @param aNumLinesX number of lines to scroll the view horizontally
   * @param aNumLinesY number of lines to scroll the view vertically
   * @param aUpdateFlags indicate smooth or async scrolling
   * @return error status
   */
  NS_IMETHOD ScrollByLines(PRInt32 aNumLinesX, PRInt32 aNumLinexY,
                           PRUint32 aUpdateFlags = 0) = 0;
  /**
   * Identical to ScrollByLines while also returning overscroll values.
   * @param aNumLinesX number of lines to scroll the view horizontally
   * @param aNumLinesY number of lines to scroll the view vertically
   * @param aOverflowX returns the number of pixels that could not
   *                   be scrolled due to scroll bounds.
   * @param aOverflowY returns the number of pixels that could not
   *                   be scrolled due to scroll bounds.
   * @param aUpdateFlags indicate smooth or async scrolling
   * @return error status
   */
  NS_IMETHOD ScrollByLinesWithOverflow(PRInt32 aNumLinesX, PRInt32 aNumLinexY,
                                       PRInt32& aOverflowX, PRInt32& aOverflowY,
                                       PRUint32 aUpdateFlags = 0) = 0;

  /**
   * Get the desired size of a page scroll in each dimension.
   * ScrollByPages will scroll by independent multiples of these amounts
   * unless it hits the edge of the view.
   */
  NS_IMETHOD GetPageScrollDistances(nsSize *aDistances) = 0;

  /**
   * Scroll the view left or right by aNumPagesX pages. Positive values move right. 
   * Scroll the view up or down by aNumPagesY pages. Positive values move down. 
   * A page is considered to be the amount displayed by the clip view.
   * Prevents scrolling off the end of the view.
   * @param aNumPagesX number of pages to scroll the view horizontally
   * @param aNumPagesY number of pages to scroll the view vertically
   * @param aUpdateFlags indicate smooth or async scrolling
   * @return error status
   */
  NS_IMETHOD ScrollByPages(PRInt32 aNumPagesX, PRInt32 aNumPagesY,
                           PRUint32 aUpdateFlags = 0) = 0;

  /**
   * Scroll the view to the top or bottom of the document depending
   * on the value of aTop.
   * @param aForward indicates whether to scroll to top or bottom
   * @param aUpdateFlags indicate smooth or async scrolling
   * @return error status
   */
  NS_IMETHOD ScrollByWhole(PRBool aTop, PRUint32 aUpdateFlags = 0) = 0;

  /**
   * Scroll the view left or right by aNumLinesX pixels.  Positive values move 
   * right.  Scroll the view up or down by aNumLinesY pixels.  Positive values
   * move down.  Prevents scrolling off the end of the view.
   * @param aNumPixelsX number of pixels to scroll the view horizontally
   * @param aNumPixelsY number of pixels to scroll the view vertically
   * @param aOverflowX returns the number of pixels that could not
   *                   be scrolled due to scroll bounds.
   * @param aOverflowY returns the number of pixels that could not
   *                   be scrolled due to scroll bounds.
   * @param aUpdateFlags indicate smooth, async, or immediate scrolling
   * @return error status
   */
  NS_IMETHOD ScrollByPixels(PRInt32 aNumPixelsX, PRInt32 aNumPixelsY,
                            PRInt32& aOverflowX, PRInt32& aOverflowY,
                            PRUint32 aUpdateFlags = 0) = 0;

  /**
   * Check the view can scroll from current offset.
   * @param aHorizontal If checking to Left or to Right, true. Otherwise, false.
   * @param aForward    If checking to Right or Bottom, true. Otherwise, false.
   * @param aResult     If the view can scroll, true. Otherwise, false.
   * @return            error status
   */
  NS_IMETHOD CanScroll(PRBool aHorizontal, PRBool aForward, PRBool &aResult) = 0;

  /**
   * Returns the view as an nsIView*
   */
  NS_IMETHOD_(nsIView*) View() = 0;

  /**
   * Adds a scroll position listener.
   */
  NS_IMETHOD AddScrollPositionListener(nsIScrollPositionListener* aListener) = 0;
  
  /**
   * Removes a scroll position listener.
   */
  NS_IMETHOD RemoveScrollPositionListener(nsIScrollPositionListener* aListener) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScrollableView, NS_ISCROLLABLEVIEW_IID)

#endif
