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

#ifndef nsScrollPortView_h___
#define nsScrollPortView_h___

#include "nsView.h"
#include "nsIScrollableView.h"
#include "nsCOMPtr.h"
#include "nsITimer.h"

class nsISupportsArray;
class SmoothScroll;

//this is a class that acts as a container for other views and provides
//automatic management of scrolling of the views it contains.

class nsScrollPortView : public nsView, public nsIScrollableView
{
public:
  nsScrollPortView();

  NS_IMETHOD QueryInterface(REFNSIID aIID,
                            void** aInstancePtr);

  NS_IMETHOD  SetWidget(nsIWidget *aWidget);

  //nsIScrollableView interface
  NS_IMETHOD  CreateScrollControls(nsNativeWidget aNative = nsnull);
  NS_IMETHOD  ComputeScrollOffsets(PRBool aAdjustWidgets = PR_TRUE);
  NS_IMETHOD  GetContainerSize(nscoord *aWidth, nscoord *aHeight) const;
  NS_IMETHOD  SetScrolledView(nsIView *aScrolledView);
  NS_IMETHOD  GetScrolledView(nsIView *&aScrolledView) const;

  NS_IMETHOD  SetScrollPreference(nsScrollPreference aPref);
  NS_IMETHOD  GetScrollPreference(nsScrollPreference &aScrollPreference) const;
  NS_IMETHOD  GetScrollPosition(nscoord &aX, nscoord &aY) const;
  NS_IMETHOD  ScrollTo(nscoord aX, nscoord aY, PRUint32 aUpdateFlags);
  NS_IMETHOD  GetScrollbarVisibility(PRBool *aVerticalVisible,
                                     PRBool *aHorizontalVisible) const;
  NS_IMETHOD  SetScrollProperties(PRUint32 aProperties);
  NS_IMETHOD  GetScrollProperties(PRUint32 *aProperties);
  NS_IMETHOD  SetLineHeight(nscoord aHeight);
  NS_IMETHOD  GetLineHeight(nscoord *aHeight);
  NS_IMETHOD  ScrollByLines(PRInt32 aNumLinesX, PRInt32 aNumLinesY);
  NS_IMETHOD  ScrollByPages(PRInt32 aNumPagesX, PRInt32 aNumPagesY);
  NS_IMETHOD  ScrollByWhole(PRBool aTop);
  
  NS_IMETHOD  GetClipView(const nsIView** aClipView) const;

  NS_IMETHOD  AddScrollPositionListener(nsIScrollPositionListener* aListener);
  NS_IMETHOD  RemoveScrollPositionListener(nsIScrollPositionListener* aListener);

  // local to the view module

  NS_IMETHOD  Paint(nsIRenderingContext& rc, const nsRect& rect,
                    PRUint32 aPaintFlags, PRBool &Result);
  NS_IMETHOD  Paint(nsIRenderingContext& aRC, const nsIRegion& aRegion,
                    PRUint32 aPaintFlags, PRBool &Result);
  nsView*     GetScrolledView() const { return GetFirstChild(); }

private:
  NS_IMETHOD  ScrollToImpl(nscoord aX, nscoord aY, PRUint32 aUpdateFlags);

  // data members
  SmoothScroll* mSmoothScroll;

  // methods
  void        IncrementalScroll();
  PRBool      IsSmoothScrollingEnabled();
  static void SmoothScrollAnimationCallback(nsITimer *aTimer, void* aESM);

protected:
  virtual ~nsScrollPortView();

  //private
  void Scroll(nsView *aScrolledView, PRInt32 aDx, PRInt32 aDy, float scale, PRUint32 aUpdateFlags);
  PRBool CannotBitBlt(nsView* aScrolledView);

  nscoord             mOffsetX, mOffsetY;
  nscoord             mOffsetXpx, mOffsetYpx;
  PRUint32            mScrollProperties;
  nscoord             mLineHeight;
  nsISupportsArray   *mListeners;
};

#endif
