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

//this is a class that acts as a container for other views and provides
//automatic management of scrolling of the views it contains.

class nsScrollingView : public nsView, public nsIScrollableView
{
public:
  nsScrollingView();

  NS_IMETHOD QueryInterface(REFNSIID aIID,
                            void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  virtual nsresult Init(nsIViewManager* aManager,
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
  virtual void SetDimensions(nscoord width, nscoord height);
  virtual void SetPosition(nscoord aX, nscoord aY);
  virtual void HandleScrollEvent(nsGUIEvent *aEvent, PRUint32 aEventFlags);
  virtual nsEventStatus HandleEvent(nsGUIEvent *aEvent, PRUint32 aEventFlags);
  virtual void AdjustChildWidgets(nsScrollingView *aScrolling, nsIView *aView, nscoord aDx, nscoord aDy, float aScale);
  virtual PRBool Paint(nsIRenderingContext& rc, const nsRect& rect,
                     PRUint32 aPaintFlags, nsIView *aBackstop = nsnull);

  //nsIScrollableView interface
  virtual void ComputeContainerSize();
  virtual void GetContainerSize(nscoord *aWidth, nscoord *aHeight);
  virtual void SetVisibleOffset(nscoord aOffsetX, nscoord aOffsetY);
  virtual void GetVisibleOffset(nscoord *aOffsetX, nscoord *aOffsetY);
  virtual nsIView * GetScrolledView(void);

  virtual void ShowQuality(PRBool aShow);
  virtual PRBool GetShowQuality(void);
  virtual void SetQuality(nsContentQuality aQuality);

  virtual void SetScrollPreference(nsScrollPreference aPref);
  virtual nsScrollPreference GetScrollPreference(void);

  //private
  void ComputeScrollArea(nsIView *aView, nsRect &aRect, nscoord aOffX, nscoord aOffY);

protected:
  virtual ~nsScrollingView();

protected:
  nscoord             mSizeX, mSizeY;
  nscoord             mOffsetX, mOffsetY;
  nsIView             *mVScrollBarView;
  nsIView             *mHScrollBarView;
  nsIView             *mCornerView;
  nsScrollPreference  mScrollPref;
};

#endif
