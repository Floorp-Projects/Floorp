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

#ifndef nsScrollingView_h___
#define nsScrollingView_h___

#include "nsView.h"
#include "nsIScrollableView.h"
#include "nsITimer.h"
#include "nsITimerCallback.h"

//this is a class that acts as a container for other views and provides
//automatic management of scrolling of the views it contains.

class nsScrollingView : public nsView, public nsIScrollableView, public nsITimerCallback
{
public:
  nsScrollingView();

  NS_IMETHOD QueryInterface(REFNSIID aIID,
                            void** aInstancePtr);

  //overrides
  NS_IMETHOD  Init(nsIViewManager* aManager,
      						 const nsRect &aBounds,
                   const nsIView *aParent,
      						 const nsViewClip *aClip = nsnull,
      						 nsViewVisibility aVisibilityFlag = nsViewVisibility_kShow);
  NS_IMETHOD  SetDimensions(nscoord width, nscoord height, PRBool aPaint = PR_TRUE);
  NS_IMETHOD  SetPosition(nscoord aX, nscoord aY);
   // SetVisibility is overriden so that it will set it's components visibility (ClipView, 
   // CornerView, ScrollBarView's),as well as it's own visibility.
  NS_IMETHOD  SetVisibility(nsViewVisibility visibility);
  NS_IMETHOD  HandleEvent(nsGUIEvent *aEvent, PRUint32 aEventFlags, nsEventStatus &aStatus);
  NS_IMETHOD  SetWidget(nsIWidget *aWidget);

  //nsIScrollableView interface
  NS_IMETHOD  CreateScrollControls(nsNativeWidget aNative = nsnull);
  NS_IMETHOD  ComputeScrollOffsets(PRBool aAdjustWidgets = PR_TRUE);
  NS_IMETHOD  GetContainerSize(nscoord *aWidth, nscoord *aHeight) const;
  NS_IMETHOD  SetScrolledView(nsIView *aScrolledView);
  NS_IMETHOD  GetScrolledView(nsIView *&aScrolledView) const;

  NS_IMETHOD  ShowQuality(PRBool aShow);
  NS_IMETHOD  GetShowQuality(PRBool &aShow) const;
  NS_IMETHOD  SetQuality(nsContentQuality aQuality);

  NS_IMETHOD  SetScrollPreference(nsScrollPreference aPref);
  NS_IMETHOD  GetScrollPreference(nsScrollPreference &aScrollPreference) const;
  NS_IMETHOD  GetScrollPosition(nscoord &aX, nscoord &aY) const;
  NS_IMETHOD  ScrollTo(nscoord aX, nscoord aY, PRUint32 aUpdateFlags);
  NS_IMETHOD  SetControlInsets(const nsMargin &aInsets);
  NS_IMETHOD  GetControlInsets(nsMargin &aInsets) const;
  NS_IMETHOD  GetScrollbarVisibility(PRBool *aVerticalVisible,
                                     PRBool *aHorizontalVisible) const;
  NS_IMETHOD  SetScrollProperties(PRUint32 aProperties);
  NS_IMETHOD  GetScrollProperties(PRUint32 *aProperties);
  NS_IMETHOD  SetLineHeight(nscoord aHeight);
  NS_IMETHOD  GetLineHeight(nscoord *aHeight);
  NS_IMETHOD  ScrollByLines(PRInt32 aNumLines);
  NS_IMETHOD  ScrollByPages(PRInt32 aNumPages);
  
  NS_IMETHOD  GetClipView(const nsIView** aClipView) const;

  //locals
  void HandleScrollEvent(nsGUIEvent *aEvent, PRUint32 aEventFlags);
 
   // Set the visibility of a nsScrollingView's component
   // @param aView nsScrollingView component to set visibility for or nsnull.
   // @param aViewVisibility new setting for the component view If not the same as the current setting 
   // @returns the result of calling the SetVisibility on the component.
  nsresult SetComponentVisibility(nsIView* aView, nsViewVisibility aViewVisibility); 

   // Update the visibility of the nsScrollingView's components (ClipView, CornerView, ScrollBarView's)
   // @param aScrollingViewVisibility Visibility setting of the ScrollingView to consider when
   // setting the visibility of the components. If aScrollingViewVisibility == nsViewVisibility_kInherit then 
   // UpdateComponent has no effect.
   // @returns the result of calling SetComponentVisibility for each component.
  nsresult UpdateComponentVisibility(nsViewVisibility aScrollingViewVisibility);


private:
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

protected:
  virtual ~nsScrollingView();

  // nsITimerCallback Interface
  virtual void Notify(nsITimer *timer);

  //private
  void AdjustChildWidgets(nsScrollingView *aScrolling, nsIView *aView, nscoord aDx, nscoord aDy, float aScale);
  void UpdateScrollControls(PRBool aPaint);
  void Scroll(nsIView *aScrolledView, PRInt32 aDx, PRInt32 aDy, float scale, PRUint32 aUpdateFlags);


protected:
  nscoord             mSizeX, mSizeY;
  nscoord             mOffsetX, mOffsetY;
  nsIView             *mClipView;
  nsIView             *mVScrollBarView;
  nsIView             *mHScrollBarView;
  nsIView             *mCornerView;
  nsScrollPreference  mScrollPref;
  nsMargin            mInsets;
  nsITimer            *mScrollingTimer;
  nscoord             mScrollingDelta;
  PRUint32            mScrollProperties;
  nscoord             mLineHeight;
};

#endif
