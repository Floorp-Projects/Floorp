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
#include "nsIViewManager.h"
#include "nsIView.h"

class nsIView;
class nsIScrollPositionListener;
struct nsMargin;

typedef enum {
  nsScrollPreference_kAuto = 0,
  nsScrollPreference_kNeverScroll,
  nsScrollPreference_kAlwaysScroll,
  nsScrollPreference_kAlwaysScrollHorizontal,
  nsScrollPreference_kAlwaysScrollVertical
} nsScrollPreference;

// the percentage of the page that is scrolled on a page up or down
#define PAGE_SCROLL_PERCENT 0.9

// IID for the nsIScrollableView interface
#define NS_ISCROLLABLEVIEW_IID    \
{ 0xc95f1830, 0xc376, 0x11d1, \
{ 0xb7, 0x21, 0x0, 0x60, 0x8, 0x91, 0xd8, 0xc9 } }

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
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISCROLLABLEVIEW_IID)

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) = 0;

  /**
   * Create the controls used to allow scrolling. Call this method
   * before anything else is done with the scrollable view.
   * @param aNative native widget to use as parent for control widgets
   * @return error status
   */
  NS_IMETHOD  CreateScrollControls(nsNativeWidget aNative = nsnull) = 0;

  /**
   * Compute the values for the scroll bars and adjust the position
   * of the scrolled view as necessary.
   * @param aAdjustWidgets if any widgets that are children of the
   *        scrolled view should be repositioned after rethinking
   *        the scroll parameters, set this ot PR_TRUE. in general
   *        this should be true unless you intended to vists the
   *        child widgets manually.
   * @return error status
   */
  NS_IMETHOD  ComputeScrollOffsets(PRBool aAdjustWidgets = PR_TRUE) = 0;

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
  NS_IMETHOD  GetScrollPreference(nsScrollPreference& aScrollPreference) const = 0;

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
   * @param aUpdateFlags passed onto nsIViewManager->UpdateView()
   * @return error status
   */
  NS_IMETHOD ScrollTo(nscoord aX, nscoord aY, PRUint32 aUpdateFlags) = 0;

  /**
   * Get information about whether the vertical and horizontal scrollbars
   * are currently visible
   */
  NS_IMETHOD GetScrollbarVisibility(PRBool *aVerticalVisible,
                                    PRBool *aHorizontalVisible) const = 0;

  /**
   * Set the properties describing how scrolling can be performed
   * in this scrollable.
   * @param aProperties new properties
   * @return error status
   */
  NS_IMETHOD SetScrollProperties(PRUint32 aProperties) = 0;

  /**
   * Get the properties describing how scrolling can be performed
   * in this scrollable.
   * @param aProperties out parameter for current properties
   * @return error status
   */
  NS_IMETHOD GetScrollProperties(PRUint32 *aProperties) = 0;

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
   * @return error status
   */
  NS_IMETHOD ScrollByLines(PRInt32 aNumLinesX, PRInt32 aNumLinexY) = 0;

  /**
   * Scroll the view left or right by aNumPagesX pages. Positive values move right. 
   * Scroll the view up or down by aNumPagesY pages. Positive values move down. 
   * A page is considered to be the amount displayed by the clip view.
   * Prevents scrolling off the end of the view.
   * @param aNumPagesX number of pages to scroll the view horizontally
   * @param aNumPagesY number of pages to scroll the view vertically
   * @return error status
   */
  NS_IMETHOD ScrollByPages(PRInt32 aNumPagesX, PRInt32 aNumPagesY) = 0;

  /**
   * Scroll the view to the top or bottom of the document depending
   * on the value of aTop.
   * @param aForward indicates whether to scroll to top or bottom
   * @return error status
   */
  NS_IMETHOD ScrollByWhole(PRBool aTop) = 0;

  /**
   * Returns the clip view
   * XXX Obsolete; with nsScrollingView gone, this always returns 'this'
   */
  NS_IMETHOD GetClipView(const nsIView** aClipView) const = 0;

  /**
   * Adds a scroll position listener.
   */
  NS_IMETHOD AddScrollPositionListener(nsIScrollPositionListener* aListener) = 0;
  
  /**
   * Removes a scroll position listener.
   */
  NS_IMETHOD RemoveScrollPositionListener(nsIScrollPositionListener* aListener) = 0;
};

//regardless of the transparency or opacity settings
//for this view, it can always be scrolled via a blit
#define NS_SCROLL_PROPERTY_ALWAYS_BLIT    0x0001

//regardless of the transparency or opacity settings
//for this view, it can never be scrolled via a blit
#define NS_SCROLL_PROPERTY_NEVER_BLIT     0x0002

#endif
