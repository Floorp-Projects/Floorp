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

#include "nsScrollingView.h"
#include "nsIWidget.h"
#include "nsUnitConversion.h"
#include "nsIViewManager.h"
#include "nsIPresContext.h"
#include "nsIScrollbar.h"
#include "nsIDeviceContext.h"
#include "nsGUIEvent.h"
#include "nsWidgetsCID.h"
#include "nsViewsCID.h"
#include "nsIScrollableView.h"

class ScrollBarView : public nsView
{
public:
  ScrollBarView();
  ~ScrollBarView();
  void SetPosition(nscoord x, nscoord y);
  void SetDimensions(nscoord width, nscoord height);
};

ScrollBarView :: ScrollBarView()
{
}

ScrollBarView :: ~ScrollBarView()
{
}

void ScrollBarView :: SetPosition(nscoord x, nscoord y)
{
  mBounds.MoveTo(x, y);

  if (nsnull != mWindow)
  {
    nsIPresContext  *px = mViewManager->GetPresContext();
    float           scale = px->GetTwipsToPixels();
    
    mWindow->Move(NS_TO_INT_ROUND(x * scale), NS_TO_INT_ROUND(y * scale));

    NS_RELEASE(px);
  }
}

void ScrollBarView :: SetDimensions(nscoord width, nscoord height)
{
  mBounds.SizeTo(width, height);

  if (nsnull != mWindow)
  {
    nsIPresContext  *px = mViewManager->GetPresContext();
    float           t2p = px->GetTwipsToPixels();
  
    mWindow->Resize(NS_TO_INT_ROUND(t2p * width), NS_TO_INT_ROUND(t2p * height));

    NS_RELEASE(px);
  }
}

static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);

nsScrollingView :: nsScrollingView()
{
  mSizeX = mSizeY = 0;
  mOffsetX = mOffsetY = 0;
  mVScrollBarView = nsnull;
  mHScrollBarView = nsnull;
}

nsScrollingView :: ~nsScrollingView()
{
  if (nsnull != mVScrollBarView)
  {
    NS_RELEASE(mVScrollBarView);
    mVScrollBarView = nsnull;
  }

  if (nsnull != mHScrollBarView)
  {
    NS_RELEASE(mHScrollBarView);
    mHScrollBarView = nsnull;
  }
}

nsresult nsScrollingView :: QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  
  static NS_DEFINE_IID(kClassIID, NS_ISCROLLABLEVIEW_IID);

  if (aIID.Equals(kClassIID)) {
    *aInstancePtr = (void*)(nsIScrollableView*)this;
    AddRef();
    return NS_OK;
  }

  return nsView::QueryInterface(aIID, aInstancePtr);
}

nsrefcnt nsScrollingView :: AddRef()
{
  return nsView::AddRef();
}

nsrefcnt nsScrollingView :: Release()
{
  return nsView::Release();
}

nsresult nsScrollingView :: Init(nsIViewManager* aManager,
					const nsRect &aBounds,
					nsIView *aParent,
					const nsIID *aWindowIID,
          void *aWidgetInitData,
					nsNativeWindow aNative,
					PRInt32 aZIndex,
					const nsViewClip *aClip,
					float aOpacity,
					nsViewVisibility aVisibilityFlag)
{
  nsresult rv;
  
  rv = nsView :: Init(aManager, aBounds, aParent, aWindowIID, aWidgetInitData, aNative, aZIndex, aClip, aOpacity, aVisibilityFlag);

  if (rv == NS_OK)
  {
    nsIPresContext    *cx = mViewManager->GetPresContext();
    nsIDeviceContext  *dx = cx->GetDeviceContext();

    // Create a view

    mVScrollBarView = new ScrollBarView();

    if (nsnull != mVScrollBarView)
    {
      NS_ADDREF(mVScrollBarView);

      nsRect trect = aBounds;

      trect.width = NS_TO_INT_ROUND(dx->GetScrollBarWidth());
      trect.x = aBounds.XMost() - trect.width;

      static NS_DEFINE_IID(kCScrollbarIID, NS_VERTSCROLLBAR_CID);

      rv = mVScrollBarView->Init(mViewManager, trect, this, &kCScrollbarIID);

      mViewManager->InsertChild(this, mVScrollBarView, 0);
    }

    NS_RELEASE(dx);
    NS_RELEASE(cx);
  }

  return rv;
}

void nsScrollingView :: SetDimensions(nscoord width, nscoord height)
{
  nsRect            trect;
  nsIPresContext    *cx;
  nsIDeviceContext  *dx;

  nsView :: SetDimensions(width, height);

  if (nsnull != mVScrollBarView)
  {
    cx = mViewManager->GetPresContext();
    dx = cx->GetDeviceContext();

    mVScrollBarView->GetDimensions(&trect.width, &trect.height);

    trect.height = height;
    trect.x = width - NS_TO_INT_ROUND(dx->GetScrollBarWidth());
    trect.y = 0;

    mVScrollBarView->SetBounds(trect);

    //this will fix the size of the thumb when we resize the root window,
    //but unfortunately it will also cause scrollbar flashing. so long as
    //all resize operations happen through the viewmanager, this is not
    //an issue. we'll see. MMP
//    ComputeContainerSize();

    NS_RELEASE(dx);
    NS_RELEASE(cx);
  }
}

nsEventStatus nsScrollingView :: HandleEvent(nsGUIEvent *aEvent, PRUint32 aEventFlags)
{
  nsEventStatus  retval =  nsEventStatus_eIgnore; 

  switch (aEvent->message)
  {
    case NS_SCROLLBAR_POS:
    case NS_SCROLLBAR_PAGE_NEXT:
    case NS_SCROLLBAR_PAGE_PREV:
    case NS_SCROLLBAR_LINE_NEXT:
    case NS_SCROLLBAR_LINE_PREV:
    {
      nscoord         oy;
      nsIPresContext  *px = mViewManager->GetPresContext();
      nscoord         dy;
      float           scale = px->GetTwipsToPixels();

      oy = mOffsetY;

      //now, this horrible thing makes sure that as we scroll
      //the document a pixel at a time, we keep the logical position of
      //our scroll bar at the top edge of the same pixel that
      //is displayed.

      mOffsetY = NS_TO_INT_ROUND(NS_TO_INT_ROUND(((nsScrollbarEvent *)aEvent)->position * scale) * px->GetPixelsToTwips());

      dy = NS_TO_INT_ROUND(scale * (oy - mOffsetY));

      if (dy != 0)
      {
        nsRect  clip;
        nscoord sx, sy;

        mVScrollBarView->GetDimensions(&sx, &sy);

        clip.x = 0;
        clip.y = 0;
        clip.width = NS_TO_INT_ROUND(scale * (mBounds.width - sx));
        clip.height = NS_TO_INT_ROUND(scale * mBounds.height);

        mViewManager->ClearDirtyRegion();
        mWindow->Scroll(0, dy, &clip);

        //and now we make sure that the scrollbar thumb is in sync with the
        //numbers we came up with here, but only if we actually moved at least
        //a full pixel. if didn't adjust the thumb only if the delta is non-zero,
        //very slow scrolling would never actually work.

        ((nsScrollbarEvent *)aEvent)->position = mOffsetY;

        AdjustChildWidgets(0, dy);
      }

      retval = nsEventStatus_eConsumeNoDefault;

      NS_RELEASE(px);

      break;
    }

    default:
      retval = nsView::HandleEvent(aEvent, aEventFlags);
      break;
  }

  return retval;
}

void nsScrollingView :: ComputeContainerSize()
{
  nsIView *scrollview = GetScrolledView();

  if (nsnull != scrollview)
  {
    if (nsnull != mVScrollBarView)
    {
      nsIPresContext  *px = mViewManager->GetPresContext();
      nscoord         width, height;
      nsIScrollbar    *scroll;
      nsIWidget       *win;
      PRUint32        oldsize = mSizeY;
      nsRect          area(0, 0, 0, 0);
      PRInt32         offy, dy;
      float           scale = px->GetTwipsToPixels();

      ComputeScrollArea(scrollview, area, 0, 0);

      mSizeY = area.YMost();

      mVScrollBarView->GetDimensions(&width, &height);
      offy = mOffsetY;

      win = mVScrollBarView->GetWidget();

      static NS_DEFINE_IID(kscroller, NS_ISCROLLBAR_IID);

      if (NS_OK == win->QueryInterface(kscroller, (void **)&scroll))
      {
        if (mSizeY > mBounds.height)
        {
          //we need to be able to scroll

          mVScrollBarView->SetVisibility(nsViewVisibility_kShow);

          //now update the scroller position for the new size

          PRUint32  oldpos = scroll->GetPosition();

          mOffsetY = NS_TO_INT_ROUND(NS_TO_INT_ROUND((((float)oldpos * mSizeY) / oldsize) * scale) * px->GetPixelsToTwips());

          dy = NS_TO_INT_ROUND(scale * (offy - mOffsetY));

          scroll->SetParameters(mSizeY, mBounds.height, mOffsetY, NS_POINTS_TO_TWIPS_INT(12));

          NS_RELEASE(px);
        }
        else
        {
          mOffsetY = 0;
          dy = NS_TO_INT_ROUND(scale * offy);
          mVScrollBarView->SetVisibility(nsViewVisibility_kHide);
        }

        if (dy != 0)
          AdjustChildWidgets(0, dy);

        scroll->Release();
      }

      NS_RELEASE(win);
    }

    NS_RELEASE(scrollview);
  }
}

void nsScrollingView :: GetContainerSize(nscoord *aWidth, nscoord *aHeight)
{
  *aWidth = mSizeX;
  *aHeight = mSizeY;
}

void nsScrollingView :: SetVisibleOffset(nscoord aOffsetX, nscoord aOffsetY)
{
  mOffsetX = aOffsetX;
  mOffsetY = aOffsetY;
}

void nsScrollingView :: GetVisibleOffset(nscoord *aOffsetX, nscoord *aOffsetY)
{
  *aOffsetX = mOffsetX;
  *aOffsetY = mOffsetY;
}

void nsScrollingView :: AdjustChildWidgets(nscoord aDx, nscoord aDy)
{
  PRInt32 numkids = GetChildCount();

  for (PRInt32 cnt = 0; cnt < numkids; cnt++)
  {
    nsIView   *kid = GetChild(cnt);

    if ((kid != mVScrollBarView) && (kid != mHScrollBarView))
    {
      nsIWidget *win = kid->GetWidget();

      if (nsnull != win)
      {
        nsRect  bounds;

        win->GetBounds(bounds);
        win->Move(bounds.x + aDx, bounds.y + aDy);

        NS_RELEASE(win);
      }

      kid->AdjustChildWidgets(aDx, aDy);
    }
  }
}

nsIView * nsScrollingView :: GetScrolledView(void)
{
  PRInt32 numkids;
  nsIView *retview = nsnull;

  numkids = GetChildCount();

  for (PRInt32 cnt = 0; cnt < numkids; cnt++)
  {
    retview = GetChild(cnt);

    if ((retview != mVScrollBarView) && (retview != mHScrollBarView))
      break;
    else
      retview = nsnull;
  }

  NS_IF_ADDREF(retview);

  return retview;
}

void nsScrollingView :: ComputeScrollArea(nsIView *aView, nsRect &aRect,
                                          nscoord aOffX, nscoord aOffY)
{
  nsRect  trect, vrect;

  aView->GetBounds(vrect);

  aOffX += vrect.x;
  aOffY += vrect.y;

  trect.x = aOffX;
  trect.y = aOffY;
  trect.width = vrect.width;
  trect.height = vrect.height;

  if (aRect.IsEmpty() == PR_TRUE)
    aRect = trect;
  else
    aRect.UnionRect(aRect, trect);

  PRInt32 numkids = aView->GetChildCount();
  
  for (PRInt32 cnt = 0; cnt < numkids; cnt++)
  {
    nsIView *view = aView->GetChild(cnt);
    ComputeScrollArea(view, aRect, aOffX, aOffY);
  }
}
