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

  NS_IMETHOD  Init(nsIViewManager* aManager,
        					 const nsRect &aBounds,
        					 nsIView *aParent,
        					 const nsIID *aWindowIID = nsnull,
                   nsWidgetInitData *aWidgetInitData = nsnull,
        					 nsNativeWidget aNative = nsnull,
        					 PRInt32 aZIndex = 0,
        					 const nsViewClip *aClip = nsnull,
        					 float aOpacity = 1.0f,
        					 nsViewVisibility aVisibilityFlag = nsViewVisibility_kShow);

  //overrides
  NS_IMETHOD  SetDimensions(nscoord width, nscoord height, PRBool aPaint = PR_TRUE);
  NS_IMETHOD  SetPosition(nscoord aX, nscoord aY);
  NS_IMETHOD  HandleEvent(nsGUIEvent *aEvent, PRUint32 aEventFlags, nsEventStatus &aStatus);
  NS_IMETHOD  Paint(nsIRenderingContext& rc, const nsRect& rect,
                    PRUint32 aPaintFlags, PRBool &aResult);

  //nsIScrollableView interface
  NS_IMETHOD  ComputeContainerSize();
  NS_IMETHOD  GetContainerSize(nscoord *aWidth, nscoord *aHeight);
  // XXX TROY
#if 0
  NS_IMETHOD  SetVisibleOffset(nscoord aOffsetX, nscoord aOffsetY);
  NS_IMETHOD  GetVisibleOffset(nscoord *aOffsetX, nscoord *aOffsetY);
#endif
  NS_IMETHOD  SetScrolledView(nsIView *aScrolledView);
  NS_IMETHOD  GetScrolledView(nsIView *&aScrolledView);

  NS_IMETHOD  ShowQuality(PRBool aShow);
  NS_IMETHOD  GetShowQuality(PRBool &aShow);
  NS_IMETHOD  SetQuality(nsContentQuality aQuality);

  NS_IMETHOD  SetScrollPreference(nsScrollPreference aPref);
  NS_IMETHOD  GetScrollPreference(nsScrollPreference &aScrollPreference);
  NS_IMETHOD  ScrollTo(nscoord aX, nscoord aY, PRUint32 aUpdateFlags);
  NS_IMETHOD  GetClipSize(nscoord *aX, nscoord *aY);

  //private
  void ComputeScrollArea(nsIView *aView, nsRect &aRect, nscoord aOffX, nscoord aOffY);
  virtual void HandleScrollEvent(nsGUIEvent *aEvent, PRUint32 aEventFlags);
  virtual void AdjustChildWidgets(nsScrollingView *aScrolling, nsIView *aView, nscoord aDx, nscoord aDy, float aScale);

private:
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

protected:
  virtual ~nsScrollingView();

  // nsITimerCallback Interface
  virtual void Notify(nsITimer *timer);


protected:
  nscoord             mSizeX, mSizeY;
  nscoord             mOffsetX, mOffsetY;
  nsIView             *mClipView;
  nsIView             *mVScrollBarView;
  nsIView             *mHScrollBarView;
  nsIView             *mCornerView;
  nsScrollPreference  mScrollPref;
  nscoord             mClipX, mClipY;

  nsITimer            *mScrollingTimer;
  nscoord             mScrollingDelta;
};

#endif
