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
    nscoord         parx = 0, pary = 0;
    nsIWidget       *pwidget = nsnull;
  
    pwidget = GetOffsetFromWidget(&parx, &pary);
    NS_IF_RELEASE(pwidget);
    
    mWindow->Move(NS_TO_INT_ROUND((x + parx) * scale),
                  NS_TO_INT_ROUND((y + pary) * scale));

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
  
    mWindow->Resize(NS_TO_INT_ROUND(t2p * width), NS_TO_INT_ROUND(t2p * height),
                    PR_TRUE);

    NS_RELEASE(px);
  }
}

class CornerView : public nsView
{
public:
  CornerView();
  ~CornerView();
  void ShowQuality(PRBool aShow);
  void SetQuality(nsContentQuality aQuality);
  void Show(PRBool aShow);
  PRBool Paint(nsIRenderingContext& rc, const nsRect& rect,
               PRUint32 aPaintFlags, nsIView *aBackstop = nsnull);

  PRBool            mShowQuality;
  nsContentQuality  mQuality;
  PRBool            mShow;
};

CornerView :: CornerView()
{
  mShowQuality = PR_FALSE;
  mQuality = nsContentQuality_kGood;
  mShow = PR_FALSE;
}

CornerView :: ~CornerView()
{
}

void CornerView :: ShowQuality(PRBool aShow)
{
  if (mShowQuality != aShow)
  {
    mShowQuality = aShow;

    if (mShow == PR_FALSE)
    {
      if (mVis == nsViewVisibility_kShow)
        mViewManager->SetViewVisibility(this, nsViewVisibility_kHide);
      else
        mViewManager->SetViewVisibility(this, nsViewVisibility_kShow);

      nscoord dimx, dimy;

      //this will force the scrolling view to recalc the scrollbar sizes... MMP

      mParent->GetDimensions(&dimx, &dimy);
      mParent->SetDimensions(dimx, dimy);
    }

    mViewManager->UpdateView(this, nsnull, NS_VMREFRESH_IMMEDIATE);
  }
}

void CornerView :: SetQuality(nsContentQuality aQuality)
{
  if (mQuality != aQuality)
  {
    mQuality = aQuality;

    if (mVis == nsViewVisibility_kShow)
      mViewManager->UpdateView(this, nsnull, NS_VMREFRESH_IMMEDIATE);
  }
}

void CornerView :: Show(PRBool aShow)
{
  if (mShow != aShow)
  {
    mShow = aShow;

    if (mShow == PR_TRUE)
      mViewManager->SetViewVisibility(this, nsViewVisibility_kShow);
    else if (mShowQuality == PR_FALSE)
      mViewManager->SetViewVisibility(this, nsViewVisibility_kHide);

    nscoord dimx, dimy;

    //this will force the scrolling view to recalc the scrollbar sizes... MMP

    mParent->GetDimensions(&dimx, &dimy);
    mParent->SetDimensions(dimx, dimy);
  }
}

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

PRBool CornerView :: Paint(nsIRenderingContext& rc, const nsRect& rect,
                           PRUint32 aPaintFlags, nsIView *aBackstop)
{
  PRBool  clipres = PR_FALSE;

  if (mVis == nsViewVisibility_kShow)
  {
    nscoord xoff, yoff;

    rc.PushState();

    GetScrollOffset(&xoff, &yoff);
    rc.Translate(xoff, yoff);

    clipres = rc.SetClipRect(mBounds, nsClipCombine_kIntersect);

    if (clipres == PR_FALSE)
    {
      rc.SetColor(NS_RGB(192, 192, 192));
      rc.FillRect(mBounds);

      if (PR_TRUE == mShowQuality)
      {
        nscolor tcolor, bcolor;

        //display quality indicator

        rc.Translate(mBounds.x, mBounds.y);

        rc.SetColor(NS_RGB(0, 0, 0));

        rc.FillEllipse(nscoord(mBounds.width * 0.15f),
                       nscoord(mBounds.height * 0.15f),
                       NS_TO_INT_ROUND(mBounds.width * 0.7f),
                       NS_TO_INT_ROUND(mBounds.height * 0.7f));

        if (mQuality == nsContentQuality_kGood)
          rc.SetColor(NS_RGB(0, 255, 0));
        else if (mQuality == nsContentQuality_kFair)
          rc.SetColor(NS_RGB(255, 176, 0));
        else
          rc.SetColor(NS_RGB(255, 0, 0));

        //hey, notice that these numbers don't add up... that's because
        //something funny happens on windows when the *right* numbers are
        //used. MMP

        rc.FillEllipse(NS_TO_INT_ROUND(mBounds.width * 0.23f),
                       NS_TO_INT_ROUND(mBounds.height * 0.23f),
                       nscoord(mBounds.width * 0.46f),
                       nscoord(mBounds.height * 0.46f));

        bcolor = tcolor = rc.GetColor();

        //this is inefficient, but compact...

        tcolor = NS_RGB((int)min(NS_GET_R(bcolor) + 40, 255), 
                        (int)min(NS_GET_G(bcolor) + 40, 255),
                        (int)min(NS_GET_B(bcolor) + 40, 255));

        rc.SetColor(tcolor);

        rc.FillEllipse(NS_TO_INT_ROUND(mBounds.width * 0.34f),
                       NS_TO_INT_ROUND(mBounds.height * 0.34f),
                       nscoord(mBounds.width * 0.28f),
                       nscoord(mBounds.height * 0.28f));

        tcolor = NS_RGB((int)min(NS_GET_R(bcolor) + 120, 255), 
                        (int)min(NS_GET_G(bcolor) + 120, 255),
                        (int)min(NS_GET_B(bcolor) + 120, 255));

        rc.SetColor(tcolor);

        rc.FillEllipse(NS_TO_INT_ROUND(mBounds.width * 0.32f),
                       NS_TO_INT_ROUND(mBounds.height * 0.32f),
                       nscoord(mBounds.width * 0.17f),
                       nscoord(mBounds.height * 0.17f));
      }
    }

    clipres = rc.PopState();

    if (clipres == PR_FALSE)
    {
      nsRect  xrect = mBounds;

      xrect.x += xoff;
      xrect.y += yoff;

      clipres = rc.SetClipRect(xrect, nsClipCombine_kSubtract);
    }
  }

  return clipres;
}

static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);

nsScrollingView :: nsScrollingView()
{
  mSizeX = mSizeY = 0;
  mOffsetX = mOffsetY = 0;
  mVScrollBarView = nsnull;
  mHScrollBarView = nsnull;
  mCornerView = nsnull;
  mScrollPref = nsScrollPreference_kAuto;
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

  if (nsnull != mCornerView)
  {
    NS_RELEASE(mCornerView);
    mCornerView = nsnull;
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
					nsNativeWidget aNative,
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

    // Create a view for a corner cover

    mCornerView = new CornerView();

    if (nsnull != mCornerView)
    {
      NS_ADDREF(mCornerView);

      nsRect trect;

      trect.width = NS_TO_INT_ROUND(dx->GetScrollBarWidth());
      trect.x = aBounds.XMost() - trect.width;
      trect.height = NS_TO_INT_ROUND(dx->GetScrollBarHeight());
      trect.y = aBounds.YMost() - trect.height;

      rv = mCornerView->Init(mViewManager, trect, this, nsnull, nsnull, nsnull, -1, nsnull, 1.0f, nsViewVisibility_kHide);

      mViewManager->InsertChild(this, mCornerView, -1);
    }

    // Create a view for a vertical scrollbar

    mVScrollBarView = new ScrollBarView();

    if (nsnull != mVScrollBarView)
    {
      NS_ADDREF(mVScrollBarView);

      nsRect trect = aBounds;

      trect.width = NS_TO_INT_ROUND(dx->GetScrollBarWidth());
      trect.x = aBounds.XMost() - trect.width;
      trect.height -= NS_TO_INT_ROUND(dx->GetScrollBarHeight());

      static NS_DEFINE_IID(kCScrollbarIID, NS_VERTSCROLLBAR_CID);

      rv = mVScrollBarView->Init(mViewManager, trect, this, &kCScrollbarIID, nsnull, nsnull, -2);

      mViewManager->InsertChild(this, mVScrollBarView, -2);
    }

    // Create a view for a horizontal scrollbar

    mHScrollBarView = new ScrollBarView();

    if (nsnull != mHScrollBarView)
    {
      NS_ADDREF(mHScrollBarView);

      nsRect trect = aBounds;

      trect.height = NS_TO_INT_ROUND(dx->GetScrollBarHeight());
      trect.y = aBounds.YMost() - trect.height;
      trect.width -= NS_TO_INT_ROUND(dx->GetScrollBarWidth());

      static NS_DEFINE_IID(kCHScrollbarIID, NS_HORZSCROLLBAR_CID);

      rv = mHScrollBarView->Init(mViewManager, trect, this, &kCHScrollbarIID, nsnull, nsnull, -2);

      mViewManager->InsertChild(this, mHScrollBarView, -2);
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

  if (nsnull != mCornerView)
  {
    mCornerView->GetDimensions(&trect.width, &trect.height);

    trect.y = height - NS_TO_INT_ROUND(dx->GetScrollBarHeight());
    trect.x = width - NS_TO_INT_ROUND(dx->GetScrollBarWidth());

    mCornerView->SetBounds(trect);
  }

  nsView :: SetDimensions(width, height);

  if (nsnull != mVScrollBarView)
  {
    mVScrollBarView->GetDimensions(&trect.width, &trect.height);

    trect.height = height;

    if ((mHScrollBarView && (mHScrollBarView->GetVisibility() == nsViewVisibility_kShow)) ||
        (mCornerView && (mCornerView->GetVisibility() == nsViewVisibility_kShow)))
      trect.height -= NS_TO_INT_ROUND(dx->GetScrollBarHeight());

    trect.x = width - NS_TO_INT_ROUND(dx->GetScrollBarWidth());
    trect.y = 0;

    mVScrollBarView->SetBounds(trect);
  }

  if (nsnull != mHScrollBarView)
  {
    mHScrollBarView->GetDimensions(&trect.width, &trect.height);

    trect.width = width;

    if ((mVScrollBarView && (mVScrollBarView->GetVisibility() == nsViewVisibility_kShow)) ||
        (mCornerView && (mCornerView->GetVisibility() == nsViewVisibility_kShow)))
      trect.width -= NS_TO_INT_ROUND(dx->GetScrollBarWidth());

    trect.y = height - NS_TO_INT_ROUND(dx->GetScrollBarHeight());
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

void nsScrollingView :: SetPosition(nscoord aX, nscoord aY)
{
  nsIPresContext  *px = mViewManager->GetPresContext();
  nsIWidget       *thiswin = GetWidget();

  if (nsnull == thiswin)
    thiswin = GetOffsetFromWidget(nsnull, nsnull);

  if (nsnull != thiswin)
    thiswin->BeginResizingChildren();

  nsView::SetPosition(aX, aY);

  AdjustChildWidgets(this, this, 0, 0, px->GetTwipsToPixels());

  if (nsnull != thiswin)
  {
    thiswin->EndResizingChildren();
    NS_RELEASE(thiswin);
  }

  NS_RELEASE(px);
}

PRBool nsScrollingView :: Paint(nsIRenderingContext& rc, const nsRect& rect,
                                PRUint32 aPaintFlags, nsIView *aBackstop)
{
  PRBool  clipres = PR_FALSE;

  rc.PushState();

  if (mVis == nsViewVisibility_kShow)
    clipres = rc.SetClipRect(mBounds, nsClipCombine_kIntersect);

  if (clipres == PR_FALSE)
  {
    rc.Translate(-mOffsetX, -mOffsetY);
    clipres = nsView::Paint(rc, rect, aPaintFlags | NS_VIEW_FLAG_CLIP_SET, aBackstop);
  }

  clipres = rc.PopState();

  if ((clipres == PR_FALSE) && (mVis == nsViewVisibility_kShow) && (nsnull == mWindow))
    clipres = rc.SetClipRect(mBounds, nsClipCombine_kSubtract);

  return clipres;
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

          if (nsnull == thiswin)
            thiswin = GetOffsetFromWidget(nsnull, nsnull);

          if (nsnull != thiswin)
            thiswin->BeginResizingChildren();

          //and now we make sure that the scrollbar thumb is in sync with the
          //numbers we came up with here, but only if we actually moved at least
          //a full pixel. if didn't adjust the thumb only if the delta is non-zero,
          //very slow scrolling would never actually work.

          ((nsScrollbarEvent *)aEvent)->position = mOffsetY;

          if (dy != 0)
          {
            AdjustChildWidgets(this, this, 0, 0, px->GetTwipsToPixels());

            if (nsnull != mWindow)
              mWindow->Scroll(0, dy, &clip);
            else
              mViewManager->UpdateView(this, nsnull, 0);
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

          if (nsnull == thiswin)
            thiswin = GetOffsetFromWidget(nsnull, nsnull);

          if (nsnull != thiswin)
            thiswin->BeginResizingChildren();

          //and now we make sure that the scrollbar thumb is in sync with the
          //numbers we came up with here, but only if we actually moved at least
          //a full pixel. if didn't adjust the thumb only if the delta is non-zero,
          //very slow scrolling would never actually work.

          ((nsScrollbarEvent *)aEvent)->position = mOffsetX;

          if (dx != 0)
          {
            AdjustChildWidgets(this, this, 0, 0, px->GetTwipsToPixels());

            if (nsnull != mWindow)
              mWindow->Scroll(dx, 0, &clip);
            else
              mViewManager->UpdateView(this, nsnull, 0);
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
    nscoord         hwidth, hheight;
    nscoord         vwidth, vheight;
    PRUint32        oldsizey = mSizeY, oldsizex = mSizeX;
    nsRect          area(0, 0, 0, 0);
    nscoord         offx, offy;
    float           scale = px->GetTwipsToPixels();

    ComputeScrollArea(scrollview, area, 0, 0);

    mSizeY = area.YMost();
    mSizeX = area.XMost();

    if (nsnull != mHScrollBarView)
    {
      mHScrollBarView->GetDimensions(&hwidth, &hheight);
      win = mHScrollBarView->GetWidget();

      if (NS_OK == win->QueryInterface(kscroller, (void **)&scrollh))
      {
        if (((mSizeX > mBounds.width) &&
            (mScrollPref != nsScrollPreference_kNeverScroll)) ||
            (mScrollPref == nsScrollPreference_kAlwaysScroll))
          scrollh->Release(); //DO NOT USE NS_RELEASE()! MMP
        else
          NS_RELEASE(scrollh); //MUST USE NS_RELEASE()! MMP
      }

      NS_RELEASE(win);
    }

    if (nsnull != mVScrollBarView)
    {
      mVScrollBarView->GetDimensions(&vwidth, &vheight);
      offy = mOffsetY;

      win = mVScrollBarView->GetWidget();

      if (NS_OK == win->QueryInterface(kscroller, (void **)&scrollv))
      {
        if ((mSizeY > mBounds.height) && (mScrollPref != nsScrollPreference_kNeverScroll))
        {
          //we need to be able to scroll

          mVScrollBarView->SetVisibility(nsViewVisibility_kShow);
          win->Enable(PR_TRUE);

          //now update the scroller position for the new size

          PRUint32  oldpos = scrollv->GetPosition();

          mOffsetY = NS_TO_INT_ROUND(NS_TO_INT_ROUND((((float)oldpos * mSizeY) / oldsizey) * scale) * px->GetPixelsToTwips());

          dy = NS_TO_INT_ROUND(scale * (offy - mOffsetY));

          scrollv->SetParameters(mSizeY, mBounds.height - ((nsnull != scrollh) ? hheight : 0),
                                 mOffsetY, NS_POINTS_TO_TWIPS_INT(12));
        }
        else
        {
          mOffsetY = 0;
          dy = NS_TO_INT_ROUND(scale * offy);

          if (mScrollPref == nsScrollPreference_kAlwaysScroll)
          {
            mVScrollBarView->SetVisibility(nsViewVisibility_kShow);
            win->Enable(PR_FALSE);
          }
          else
          {
            mVScrollBarView->SetVisibility(nsViewVisibility_kHide);
            win->Enable(PR_TRUE);
            NS_RELEASE(scrollv);
          }
        }

        //don't release the vertical scroller here because if we need to
        //create a horizontal one, it will need to know that there is a vertical one
//        //create a horizontal one, it will need to tweak the vertical one
      }

      NS_RELEASE(win);
    }

    if (nsnull != mHScrollBarView)
    {
      offx = mOffsetX;

      win = mHScrollBarView->GetWidget();

      if (NS_OK == win->QueryInterface(kscroller, (void **)&scrollh))
      {
        if ((mSizeX > mBounds.width) && (mScrollPref != nsScrollPreference_kNeverScroll))
        {
          //we need to be able to scroll

          mHScrollBarView->SetVisibility(nsViewVisibility_kShow);
          win->Enable(PR_TRUE);

          //now update the scroller position for the new size

          PRUint32  oldpos = scrollh->GetPosition();

          mOffsetX = NS_TO_INT_ROUND(NS_TO_INT_ROUND((((float)oldpos * mSizeX) / oldsizex) * scale) * px->GetPixelsToTwips());

          dx = NS_TO_INT_ROUND(scale * (offx - mOffsetX));

          scrollh->SetParameters(mSizeX, mBounds.width - ((nsnull != scrollv) ? vwidth : 0),
                                 mOffsetX, NS_POINTS_TO_TWIPS_INT(12));

//          //now make the vertical scroll region account for this scrollbar
//
//          if (nsnull != scrollv)
//            scrollv->SetParameters(mSizeY, mBounds.height - hheight, mOffsetY, NS_POINTS_TO_TWIPS_INT(12));
        }
        else
        {
          mOffsetX = 0;
          dx = NS_TO_INT_ROUND(scale * offx);

          if (mScrollPref == nsScrollPreference_kAlwaysScroll)
          {
            mHScrollBarView->SetVisibility(nsViewVisibility_kShow);
            win->Enable(PR_FALSE);
          }
          else
          {
            mHScrollBarView->SetVisibility(nsViewVisibility_kHide);
            win->Enable(PR_TRUE);
          }
        }

        NS_RELEASE(scrollh);
      }

      NS_RELEASE(win);
    }

    if (mCornerView)
    {
      if ((mHScrollBarView && (mHScrollBarView->GetVisibility() == nsViewVisibility_kShow)) &&
          (mVScrollBarView && (mVScrollBarView->GetVisibility() == nsViewVisibility_kShow)))
        ((CornerView *)mCornerView)->Show(PR_TRUE);
      else
        ((CornerView *)mCornerView)->Show(PR_FALSE);
    }

    // now we can release the vertical scroller if there was one...

    NS_IF_RELEASE(scrollv);

    if ((dx != 0) || (dy != 0))
      AdjustChildWidgets(this, this, 0, 0, px->GetTwipsToPixels());

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

    if (nsnull != mCornerView)
      ((CornerView *)mCornerView)->Show(PR_FALSE);

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

void nsScrollingView :: ShowQuality(PRBool aShow)
{
  ((CornerView *)mCornerView)->ShowQuality(aShow);
}

PRBool nsScrollingView :: GetShowQuality(void)
{
  return ((CornerView *)mCornerView)->mShowQuality;
}

void nsScrollingView :: SetQuality(nsContentQuality aQuality)
{
  ((CornerView *)mCornerView)->SetQuality(aQuality);
}

void nsScrollingView :: SetScrollPreference(nsScrollPreference aPref)
{
  mScrollPref = aPref;
  ComputeContainerSize();
}

nsScrollPreference nsScrollingView :: GetScrollPreference(void)
{
  return mScrollPref;
}

void nsScrollingView :: AdjustChildWidgets(nsScrollingView *aScrolling, nsIView *aView, nscoord aDx, nscoord aDy, float scale)
{
  PRInt32           numkids = aView->GetChildCount();
  nsIScrollableView *scroller;
  nscoord           offx, offy;
  PRBool            isscroll = PR_FALSE;

  static NS_DEFINE_IID(kscroller, NS_ISCROLLABLEVIEW_IID);

  if (aScrolling == aView)
  {
    nsIWidget *widget = aScrolling->GetOffsetFromWidget(&aDx, &aDy);
    nsIView   *parview = aScrolling->GetParent();

    while (nsnull != parview)
    {
      nsIWidget *parwidget = parview->GetWidget();

      if (NS_OK == parview->QueryInterface(kscroller, (void **)&scroller))
      {
        scroller->GetVisibleOffset(&offx, &offy);

        aDx -= offx;
        aDy -= offy;

        NS_RELEASE(scroller);
      }

      if (parwidget == widget)
      {
        NS_IF_RELEASE(parwidget);
        break;
      }

      NS_IF_RELEASE(parwidget);

      parview = parview->GetParent();
    }

    NS_IF_RELEASE(widget);
  }

  aView->GetPosition(&offx, &offy);

  aDx += offx;
  aDy += offy;

  if (NS_OK == aView->QueryInterface(kscroller, (void **)&scroller))
  {
    scroller->GetVisibleOffset(&offx, &offy);

    aDx -= offx;
    aDy -= offy;

    NS_RELEASE(scroller);

    isscroll = PR_TRUE;
  }

  for (PRInt32 cnt = 0; cnt < numkids; cnt++)
  {
    nsIView   *kid = aView->GetChild(cnt);
    nsIWidget *win = kid->GetWidget();

    if (nsnull != win)
    {
      nsRect  bounds;

      win->BeginResizingChildren();
      kid->GetBounds(bounds);

      if (!isscroll ||
          (isscroll &&
          (kid != ((nsScrollingView *)aView)->mVScrollBarView) &&
          (kid != ((nsScrollingView *)aView)->mHScrollBarView)))
        win->Move(NS_TO_INT_ROUND((bounds.x + aDx) * scale), NS_TO_INT_ROUND((bounds.y + aDy) * scale));
      else
        win->Move(NS_TO_INT_ROUND((bounds.x + aDx + offx) * scale), NS_TO_INT_ROUND((bounds.y + aDy + offy) * scale));
    }

    AdjustChildWidgets(aScrolling, kid, aDx, aDy, scale);

    if (nsnull != win)
    {
      win->EndResizingChildren();
      NS_RELEASE(win);
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

    if ((retview != mVScrollBarView) &&
        (retview != mHScrollBarView) &&
        (retview != mCornerView))
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
