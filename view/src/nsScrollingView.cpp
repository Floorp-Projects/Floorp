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
          nsWidgetInitData *aWidgetInitData,
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
      trect.height -= NS_TO_INT_ROUND(dx->GetScrollBarHeight());

      static NS_DEFINE_IID(kCScrollbarIID, NS_VERTSCROLLBAR_CID);

      rv = mVScrollBarView->Init(mViewManager, trect, this, &kCScrollbarIID);

      mViewManager->InsertChild(this, mVScrollBarView, 0);
    }

    // Create a view

    mHScrollBarView = new ScrollBarView();

    if (nsnull != mHScrollBarView)
    {
      NS_ADDREF(mHScrollBarView);

      nsRect trect = aBounds;

      trect.height = NS_TO_INT_ROUND(dx->GetScrollBarHeight());
      trect.y = aBounds.YMost() - trect.height;
      trect.width -= NS_TO_INT_ROUND(dx->GetScrollBarWidth());

      static NS_DEFINE_IID(kCHScrollbarIID, NS_HORZSCROLLBAR_CID);

      rv = mHScrollBarView->Init(mViewManager, trect, this, &kCHScrollbarIID);

      mViewManager->InsertChild(this, mHScrollBarView, 0);
    }

    NS_RELEASE(dx);
    NS_RELEASE(cx);
  }

  return rv;
}

void nsScrollingView :: SetDimensions(nscoord width, nscoord height)
{
  nsRect            trect;
  nsIPresContext    *cx = mViewManager->GetPresContext();
  nsIDeviceContext  *dx = cx->GetDeviceContext();

  nsView :: SetDimensions(width, height);

  if (nsnull != mVScrollBarView)
  {
    mVScrollBarView->GetDimensions(&trect.width, &trect.height);

    trect.height = height;

    if (mHScrollBarView && (mHScrollBarView->GetVisibility() == nsViewVisibility_kShow))
      trect.height -= NS_TO_INT_ROUND(dx->GetScrollBarHeight());

    trect.x = width - NS_TO_INT_ROUND(dx->GetScrollBarWidth());
    trect.y = 0;

    mVScrollBarView->SetBounds(trect);
  }

  if (nsnull != mHScrollBarView)
  {
    mHScrollBarView->GetDimensions(&trect.width, &trect.height);

    trect.width = width;

    if (mVScrollBarView && (mVScrollBarView->GetVisibility() == nsViewVisibility_kShow))
      trect.width -= NS_TO_INT_ROUND(dx->GetScrollBarHeight());

    trect.y = height - NS_TO_INT_ROUND(dx->GetScrollBarWidth());
    trect.x = 0;

    mHScrollBarView->SetBounds(trect);
  }

  //this will fix the size of the thumb when we resize the root window,
  //but unfortunately it will also cause scrollbar flashing. so long as
  //all resize operations happen through the viewmanager, this is not
  //an issue. we'll see. MMP
//  ComputeContainerSize();

  NS_RELEASE(dx);
  NS_RELEASE(cx);
}

nsEventStatus nsScrollingView :: HandleEvent(nsGUIEvent *aEvent, PRUint32 aEventFlags)
{
  nsEventStatus retval =  nsEventStatus_eIgnore; 
  nsIView       *scview = nsnull;

  static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);

  aEvent->widget->QueryInterface(kIViewIID, (void**)&scview);

  switch (aEvent->message)
  {
    case NS_SCROLLBAR_POS:
    case NS_SCROLLBAR_PAGE_NEXT:
    case NS_SCROLLBAR_PAGE_PREV:
    case NS_SCROLLBAR_LINE_NEXT:
    case NS_SCROLLBAR_LINE_PREV:
    {
      nsIPresContext  *px = mViewManager->GetPresContext();
      float           scale = px->GetTwipsToPixels();
      nscoord         dx = 0, dy = 0;

      if ((nsnull != mVScrollBarView) && (scview == mVScrollBarView))
      {
        nscoord oy;

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

          if ((nsnull != mHScrollBarView) && (mHScrollBarView->GetVisibility() == nsViewVisibility_kShow))
            mHScrollBarView->GetDimensions(&sx, &sy);
          else
            sy = 0;

          clip.height = NS_TO_INT_ROUND(scale * (mBounds.height - sy));

          mViewManager->ClearDirtyRegion();

          nsIWidget *thiswin = GetWidget();

          if (nsnull != thiswin)
            thiswin->BeginResizingChildren();

          //and now we make sure that the scrollbar thumb is in sync with the
          //numbers we came up with here, but only if we actually moved at least
          //a full pixel. if didn't adjust the thumb only if the delta is non-zero,
          //very slow scrolling would never actually work.

          ((nsScrollbarEvent *)aEvent)->position = mOffsetY;

          if (dy != 0)
          {
            AdjustChildWidgets(0, dy);
            mWindow->Scroll(0, dy, &clip);
          }

          if (nsnull != thiswin)
          {
            thiswin->EndResizingChildren();
            NS_RELEASE(thiswin);
          }
        }
      }
      else if ((nsnull != mHScrollBarView) && (scview == mHScrollBarView))
      {
        nscoord ox;

        ox = mOffsetX;

        //now, this horrible thing makes sure that as we scroll
        //the document a pixel at a time, we keep the logical position of
        //our scroll bar at the top edge of the same pixel that
        //is displayed.

        mOffsetX = NS_TO_INT_ROUND(NS_TO_INT_ROUND(((nsScrollbarEvent *)aEvent)->position * scale) * px->GetPixelsToTwips());

        dx = NS_TO_INT_ROUND(scale * (ox - mOffsetX));

        if (dx != 0)
        {
          nsRect  clip;
          nscoord sx, sy;

          clip.x = 0;
          clip.y = 0;

          if ((nsnull != mVScrollBarView) && (mVScrollBarView->GetVisibility() == nsViewVisibility_kShow))
            mVScrollBarView->GetDimensions(&sx, &sy);
          else
            sx = 0;

          clip.width = NS_TO_INT_ROUND(scale * (mBounds.width - sx));

          mHScrollBarView->GetDimensions(&sx, &sy);

          clip.height = NS_TO_INT_ROUND(scale * (mBounds.height - sy));

          mViewManager->ClearDirtyRegion();

          nsIWidget *thiswin = GetWidget();

          if (nsnull != thiswin)
            thiswin->BeginResizingChildren();

          //and now we make sure that the scrollbar thumb is in sync with the
          //numbers we came up with here, but only if we actually moved at least
          //a full pixel. if didn't adjust the thumb only if the delta is non-zero,
          //very slow scrolling would never actually work.

          ((nsScrollbarEvent *)aEvent)->position = mOffsetX;

          if (dx != 0)
          {
            AdjustChildWidgets(dx, 0);
            mWindow->Scroll(dx, 0, &clip);
          }

          if (nsnull != thiswin)
          {
            thiswin->EndResizingChildren();
            NS_RELEASE(thiswin);
          }
        }
      }

      retval = nsEventStatus_eConsumeNoDefault;

      NS_RELEASE(px);

      break;
    }

    default:
      retval = nsView::HandleEvent(aEvent, aEventFlags);
      break;
  }

  NS_IF_RELEASE(scview);

  return retval;
}

void nsScrollingView :: ComputeContainerSize()
{
  nsIView       *scrollview = GetScrolledView();
  nsIScrollbar  *scrollv = nsnull, *scrollh = nsnull;
  nsIWidget     *win;

  static NS_DEFINE_IID(kscroller, NS_ISCROLLBAR_IID);

  if (nsnull != scrollview)
  {
    nscoord         dx = 0, dy = 0;
    nsIPresContext  *px = mViewManager->GetPresContext();
    nscoord         width, height;
    nscoord         vwidth, vheight;
    PRUint32        oldsizey = mSizeY, oldsizex = mSizeX;
    nsRect          area(0, 0, 0, 0);
    nscoord         offx, offy;
    float           scale = px->GetTwipsToPixels();

    ComputeScrollArea(scrollview, area, 0, 0);

    mSizeY = area.YMost();
    mSizeX = area.XMost();

    if (nsnull != mVScrollBarView)
    {
      mVScrollBarView->GetDimensions(&vwidth, &vheight);
      offy = mOffsetY;

      win = mVScrollBarView->GetWidget();

      if (NS_OK == win->QueryInterface(kscroller, (void **)&scrollv))
      {
        if (mSizeY > mBounds.height)
        {
          //we need to be able to scroll

          mVScrollBarView->SetVisibility(nsViewVisibility_kShow);

          //now update the scroller position for the new size

          PRUint32  oldpos = scrollv->GetPosition();

          mOffsetY = NS_TO_INT_ROUND(NS_TO_INT_ROUND((((float)oldpos * mSizeY) / oldsizey) * scale) * px->GetPixelsToTwips());

          dy = NS_TO_INT_ROUND(scale * (offy - mOffsetY));

          scrollv->SetParameters(mSizeY, mBounds.height, mOffsetY, NS_POINTS_TO_TWIPS_INT(12));
        }
        else
        {
          mOffsetY = 0;
          dy = NS_TO_INT_ROUND(scale * offy);
          mVScrollBarView->SetVisibility(nsViewVisibility_kHide);
        }

        //don't release the vertical scroller here because if we need to
        //create a horizontal one, it will need to tweak the vertical one
      }

      NS_RELEASE(win);
    }

    if (nsnull != mHScrollBarView)
    {
      mHScrollBarView->GetDimensions(&width, &height);
      offx = mOffsetX;

      win = mHScrollBarView->GetWidget();

      if (NS_OK == win->QueryInterface(kscroller, (void **)&scrollh))
      {
        if (mSizeX > mBounds.width)
        {
          //we need to be able to scroll

          mHScrollBarView->SetVisibility(nsViewVisibility_kShow);

          //now update the scroller position for the new size

          PRUint32  oldpos = scrollh->GetPosition();

          mOffsetX = NS_TO_INT_ROUND(NS_TO_INT_ROUND((((float)oldpos * mSizeX) / oldsizex) * scale) * px->GetPixelsToTwips());

          dx = NS_TO_INT_ROUND(scale * (offx - mOffsetX));

          scrollh->SetParameters(mSizeX, mBounds.width - ((nsnull != scrollv) ? vwidth : 0),
                                 mOffsetX, NS_POINTS_TO_TWIPS_INT(12));

          //now make the vertical scroll region account for this scrollbar

          if (nsnull != scrollv)
            scrollv->SetParameters(mSizeY, mBounds.height - height, mOffsetY, NS_POINTS_TO_TWIPS_INT(12));
        }
        else
        {
          mOffsetX = 0;
          dx = NS_TO_INT_ROUND(scale * offx);
          mHScrollBarView->SetVisibility(nsViewVisibility_kHide);
        }

        NS_RELEASE(scrollh);
      }

      NS_RELEASE(win);
    }

    // now we can release the vertical srcoller if there was one...

    NS_IF_RELEASE(scrollv);

    if ((dx != 0) || (dy != 0))
      AdjustChildWidgets(dx, dy);

    NS_RELEASE(px);
    NS_RELEASE(scrollview);
  }
  else
  {
    if (nsnull != mHScrollBarView)
    {
      mHScrollBarView->SetVisibility(nsViewVisibility_kHide);

      win = mHScrollBarView->GetWidget();

      if (NS_OK == win->QueryInterface(kscroller, (void **)&scrollh))
      {
        scrollh->SetParameters(0, 0, 0, 0);
        NS_RELEASE(scrollh);
      }

      NS_RELEASE(win);
    }

    if (nsnull != mVScrollBarView)
    {
      mVScrollBarView->SetVisibility(nsViewVisibility_kHide);

      win = mVScrollBarView->GetWidget();

      if (NS_OK == win->QueryInterface(kscroller, (void **)&scrollv))
      {
        scrollv->SetParameters(0, 0, 0, 0);
        NS_RELEASE(scrollv);
      }

      NS_RELEASE(win);
    }

    mOffsetX = mOffsetY = 0;
    mSizeX = mSizeY = 0;
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
  PRInt32   numkids = GetChildCount();

  for (PRInt32 cnt = 0; cnt < numkids; cnt++)
  {
    nsIView   *kid = GetChild(cnt);

    if ((kid != mVScrollBarView) && (kid != mHScrollBarView))
    {
      nsIWidget *win = kid->GetWidget();

      if (nsnull != win)
      {
        nsRect  bounds;

        win->BeginResizingChildren();
        win->GetBounds(bounds);
        win->Move(bounds.x + aDx, bounds.y + aDy);
      }

      kid->AdjustChildWidgets(aDx, aDy);

      if (nsnull != win)
      {
        win->EndResizingChildren();
        NS_RELEASE(win);
      }
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
