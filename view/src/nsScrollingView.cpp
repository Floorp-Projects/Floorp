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
#include "nsIFrame.h"

static inline PRBool
ViewIsShowing(nsIView *aView)
{
  nsViewVisibility  visibility;

  aView->GetVisibility(visibility);
  return nsViewVisibility_kShow == visibility;
}

static NS_DEFINE_IID(kIScrollbarIID, NS_ISCROLLBAR_IID);
static NS_DEFINE_IID(kIScrollableViewIID, NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);

class ScrollBarView : public nsView
{
public:
  ScrollBarView(nsScrollingView *aScrollingView);
  ~ScrollBarView();
  NS_IMETHOD  HandleEvent(nsGUIEvent *aEvent, PRUint32 aEventFlags, nsEventStatus &aStatus);
  NS_IMETHOD  SetPosition(nscoord x, nscoord y);
  NS_IMETHOD  SetDimensions(nscoord width, nscoord height, PRBool aPaint = PR_TRUE);

public:
  nsScrollingView *mScrollingView;
};

ScrollBarView :: ScrollBarView(nsScrollingView *aScrollingView)
{
  mScrollingView = aScrollingView;
}

ScrollBarView :: ~ScrollBarView()
{
}

NS_IMETHODIMP ScrollBarView :: HandleEvent(nsGUIEvent *aEvent, PRUint32 aEventFlags,
                                           nsEventStatus &aStatus)
{
  aStatus = nsEventStatus_eIgnore;

  switch (aEvent->message)
  {
    case NS_SCROLLBAR_POS:
    case NS_SCROLLBAR_PAGE_NEXT:
    case NS_SCROLLBAR_PAGE_PREV:
    case NS_SCROLLBAR_LINE_NEXT:
    case NS_SCROLLBAR_LINE_PREV:
      NS_ASSERTION((nsnull != mScrollingView), "HandleEvent() called after the ScrollingView has been destroyed.");
      if (nsnull != mScrollingView)
        mScrollingView->HandleScrollEvent(aEvent, aEventFlags);
      aStatus = nsEventStatus_eConsumeNoDefault;
      break;

    default:
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP ScrollBarView :: SetPosition(nscoord x, nscoord y)
{
  mBounds.MoveTo(x, y);

  if (nsnull != mWindow)
  {
    nsIDeviceContext  *dx;
    float             twipToPix;
    nscoord           parx = 0, pary = 0;
    nsIWidget         *pwidget = nsnull;

    mViewManager->GetDeviceContext(dx);
    dx->GetAppUnitsToDevUnits(twipToPix);  

    GetOffsetFromWidget(&parx, &pary, pwidget);
    NS_IF_RELEASE(pwidget);
    
    mWindow->Move(NSTwipsToIntPixels((x + parx), twipToPix),
                  NSTwipsToIntPixels((y + pary), twipToPix));

    NS_RELEASE(dx);
  }
  return NS_OK;
}

NS_IMETHODIMP ScrollBarView :: SetDimensions(nscoord width, nscoord height, PRBool aPaint)
{
  mBounds.SizeTo(width, height);

  if (nsnull != mWindow)
  {
    nsIDeviceContext  *dx;
    float             t2p;
  
    mViewManager->GetDeviceContext(dx);
    dx->GetAppUnitsToDevUnits(t2p);

    mWindow->Resize(NSTwipsToIntPixels(width, t2p), NSTwipsToIntPixels(height, t2p),
                    aPaint);

    NS_RELEASE(dx);
  }
  return NS_OK;
}

#if 0
class nsICornerWidget : public nsISupports {
public:
  NS_IMETHOD Init(nsIWidget* aParent, const nsRect& aBounds) = 0;
  NS_IMETHOD MoveTo(PRInt32 aX, PRInt32 aY) = 0;
  NS_IMETHOD Show() = 0;
  NS_IMETHOD Hide() = 0;
  NS_IMETHOD Start() = 0;
  NS_IMETHOD Stop() = 0;
};
#endif

class CornerView : public nsView
{
public:
  CornerView();
  ~CornerView();
  NS_IMETHOD  ShowQuality(PRBool aShow);
  NS_IMETHOD  SetQuality(nsContentQuality aQuality);
  NS_IMETHOD  Paint(nsIRenderingContext& rc, const nsRect& rect,
                    PRUint32 aPaintFlags, PRBool &Result);

  void  Show(PRBool aShow);

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

NS_IMETHODIMP CornerView :: ShowQuality(PRBool aShow)
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
  return NS_OK;
}

NS_IMETHODIMP CornerView :: SetQuality(nsContentQuality aQuality)
{
  if (mQuality != aQuality)
  {
    mQuality = aQuality;

    if (mVis == nsViewVisibility_kShow)
      mViewManager->UpdateView(this, nsnull, NS_VMREFRESH_IMMEDIATE);
  }
  return NS_OK;
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

NS_IMETHODIMP CornerView :: Paint(nsIRenderingContext& rc, const nsRect& rect,
                                  PRUint32 aPaintFlags, PRBool &aResult)
{
  PRBool  clipres = PR_FALSE;

  if (mVis == nsViewVisibility_kShow)
  {
    nsRect  brect;

    rc.PushState();
    GetBounds(brect);

    clipres = rc.SetClipRect(brect, nsClipCombine_kIntersect);

    if (clipres == PR_FALSE)
    {
      rc.SetColor(NS_RGB(192, 192, 192));
      rc.FillRect(brect);

      if (PR_TRUE == mShowQuality)
      {
        nscolor tcolor, bcolor;

        //display quality indicator

        rc.Translate(brect.x, brect.y);

        rc.SetColor(NS_RGB(0, 0, 0));

        rc.FillEllipse(NSToCoordFloor(brect.width * 0.15f),
                       NSToCoordFloor(brect.height * 0.15f),
                       NSToCoordRound(brect.width * 0.7f),    // XXX should use NSToCoordCeil ??
                       NSToCoordRound(brect.height * 0.7f));  // XXX should use NSToCoordCeil ??

        if (mQuality == nsContentQuality_kGood)
          rc.SetColor(NS_RGB(0, 255, 0));
        else if (mQuality == nsContentQuality_kFair)
          rc.SetColor(NS_RGB(255, 176, 0));
        else
          rc.SetColor(NS_RGB(255, 0, 0));

        //hey, notice that these numbers don't add up... that's because
        //something funny happens on windows when the *right* numbers are
        //used. MMP

        rc.FillEllipse(NSToCoordRound(brect.width * 0.23f),  // XXX should use NSToCoordCeil ??
                       NSToCoordRound(brect.height * 0.23f), // XXX should use NSToCoordCeil ??
                       nscoord(brect.width * 0.46f),
                       nscoord(brect.height * 0.46f));

        bcolor = tcolor = rc.GetColor();

        //this is inefficient, but compact...

        tcolor = NS_RGB((int)min(NS_GET_R(bcolor) + 40, 255), 
                        (int)min(NS_GET_G(bcolor) + 40, 255),
                        (int)min(NS_GET_B(bcolor) + 40, 255));

        rc.SetColor(tcolor);

        rc.FillEllipse(NSToCoordRound(brect.width * 0.34f),  // XXX should use NSToCoordCeil ??
                       NSToCoordRound(brect.height * 0.34f), // XXX should use NSToCoordCeil ??
                       nscoord(brect.width * 0.28f),
                       nscoord(brect.height * 0.28f));

        tcolor = NS_RGB((int)min(NS_GET_R(bcolor) + 120, 255), 
                        (int)min(NS_GET_G(bcolor) + 120, 255),
                        (int)min(NS_GET_B(bcolor) + 120, 255));

        rc.SetColor(tcolor);

        rc.FillEllipse(NSToCoordRound(brect.width * 0.32f),  // XXX should use NSToCoordCeil ??
                       NSToCoordRound(brect.height * 0.32f), // XXX should use NSToCoordCeil ??
                       nscoord(brect.width * 0.17f),
                       nscoord(brect.height * 0.17f));
      }
    }

    clipres = rc.PopState();

    if (clipres == PR_FALSE)
    {
      clipres = rc.SetClipRect(brect, nsClipCombine_kSubtract);
    }
  }

  aResult = clipres;
  return NS_OK;
}

static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);

nsScrollingView :: nsScrollingView()
{
  mSizeX = mSizeY = 0;
  mOffsetX = mOffsetY = 0;
  mClipView = nsnull;
  mVScrollBarView = nsnull;
  mHScrollBarView = nsnull;
  mCornerView = nsnull;
  mScrollPref = nsScrollPreference_kAuto;
  mClipX = mClipY = 0;
  mScrollingTimer = nsnull;
}

nsScrollingView :: ~nsScrollingView()
{
  if (nsnull != mVScrollBarView)
  {
    // Clear the back-pointer from the scrollbar...
    ((ScrollBarView*)mVScrollBarView)->mScrollingView = nsnull;
  }

  if (nsnull != mHScrollBarView)
  {
    // Clear the back-pointer from the scrollbar...
    ((ScrollBarView*)mHScrollBarView)->mScrollingView = nsnull;
  }

  mClipView = nsnull;
  mCornerView = nsnull;

  if (nsnull != mScrollingTimer)
  {
    mScrollingTimer->Cancel();
    NS_RELEASE(mScrollingTimer);
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
    return NS_OK;
  }

  return nsView::QueryInterface(aIID, aInstancePtr);
}

nsrefcnt nsScrollingView :: AddRef()
{
  NS_WARNING("not supported for views");
  return 1;
}

nsrefcnt nsScrollingView :: Release()
{
  NS_WARNING("not supported for views");
  return 1;
}

NS_IMETHODIMP nsScrollingView :: Init(nsIViewManager* aManager,
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

  mClipX = aBounds.width;
  mClipY = aBounds.height;
  
  rv = nsView :: Init(aManager, aBounds, aParent, aWindowIID, aWidgetInitData, aNative, aZIndex, aClip, aOpacity, aVisibilityFlag);

  if (rv == NS_OK)
  {
    nsIDeviceContext  *dx;
    mViewManager->GetDeviceContext(dx);

    // Create a clip view
    mClipView = new nsView;

    if (nsnull != mClipView)
    {
      rv = mClipView->Init(mViewManager, aBounds, this, nsnull, nsnull, nsnull, -1, nsnull, aOpacity);
      mViewManager->InsertChild(this, mClipView, -1);
    }

    // Create a view for a corner cover
    mCornerView = new CornerView;

    if (nsnull != mCornerView)
    {
      nsRect trect;
      float  sbWidth, sbHeight;

      dx->GetScrollBarDimensions(sbWidth, sbHeight);
      trect.width = NSToCoordRound(sbWidth);
      trect.x = aBounds.x + aBounds.XMost() - trect.width;
      trect.height = NSToCoordRound(sbHeight);
      trect.y = aBounds.y + aBounds.YMost() - trect.height;

      rv = mCornerView->Init(mViewManager, trect, this, nsnull, nsnull, nsnull, -1, nsnull, 1.0f, nsViewVisibility_kHide);

      mViewManager->InsertChild(this, mCornerView, -1);
    }

    // Create a view for a vertical scrollbar

    mVScrollBarView = new ScrollBarView(this);

    if (nsnull != mVScrollBarView)
    {
      nsRect  trect = aBounds;
      float   sbWidth, sbHeight;

      dx->GetScrollBarDimensions(sbWidth, sbHeight);
      trect.width = NSToCoordRound(sbWidth);
      trect.x += aBounds.XMost() - trect.width;
      trect.height -= NSToCoordRound(sbHeight);

      static NS_DEFINE_IID(kCScrollbarIID, NS_VERTSCROLLBAR_CID);

      rv = mVScrollBarView->Init(mViewManager, trect, this, &kCScrollbarIID, nsnull, aNative, -3);

      mViewManager->InsertChild(this, mVScrollBarView, -3);
    }

    // Create a view for a horizontal scrollbar

    mHScrollBarView = new ScrollBarView(this);

    if (nsnull != mHScrollBarView)
    {
      nsRect  trect = aBounds;
      float   sbWidth, sbHeight;

      dx->GetScrollBarDimensions(sbWidth, sbHeight);
      trect.height = NSToCoordRound(sbHeight);
      trect.y += aBounds.YMost() - trect.height;
      trect.width -= NSToCoordRound(sbWidth);

      static NS_DEFINE_IID(kCHScrollbarIID, NS_HORZSCROLLBAR_CID);

      rv = mHScrollBarView->Init(mViewManager, trect, this, &kCHScrollbarIID, nsnull, aNative, -3);

      mViewManager->InsertChild(this, mHScrollBarView, -3);
    }

    NS_RELEASE(dx);
  }

  return rv;
}

NS_IMETHODIMP nsScrollingView :: SetDimensions(nscoord width, nscoord height, PRBool aPaint)
{
  nsRect            trect;
  nsIDeviceContext  *dx;
  mViewManager->GetDeviceContext(dx);
  nscoord           showHorz = 0, showVert = 0;
  float             scrollWidthFloat, scrollHeightFloat;
  dx->GetScrollBarDimensions(scrollWidthFloat, scrollHeightFloat);
  nscoord           scrollWidth = NSToCoordRound(scrollWidthFloat);
  nscoord           scrollHeight = NSToCoordRound(scrollHeightFloat);

  if (nsnull != mCornerView)
  {
    mCornerView->GetDimensions(&trect.width, &trect.height);

    trect.y = height - scrollHeight;
    trect.x = width - scrollWidth;

    mCornerView->SetBounds(trect, aPaint);
  }

  if (mHScrollBarView && ViewIsShowing(mHScrollBarView))
    showHorz = scrollHeight;

  if (mVScrollBarView && ViewIsShowing(mVScrollBarView))
    showVert = scrollWidth;

//  nsView :: SetDimensions(width, height, aPaint);

  mBounds.SizeTo(width, height);

  if (nsnull != mWindow)
  {
    float t2p;

    dx->GetAppUnitsToDevUnits(t2p);

    mClipX = width - showVert;
    mClipY = height - showHorz;
  
    mWindow->Resize(NSTwipsToIntPixels((width - showVert), t2p),
                    NSTwipsToIntPixels((height - showHorz), t2p),
                    aPaint);
  }
  else
  {
    mClipX = width;
    mClipY = height;
  }

  if (nsnull != mClipView)
  {
    mClipView->SetDimensions(width - showVert, height - showHorz);
  }

  if (nsnull != mVScrollBarView)
  {
    mVScrollBarView->GetDimensions(&trect.width, &trect.height);

    trect.height = height;

    //if (showHorz || (mCornerView && (mCornerView->GetVisibility() == nsViewVisibility_kShow)))
    if (showHorz)
      trect.height -= scrollHeight;

    trect.x = width - scrollWidth;
    trect.y = 0;

    mVScrollBarView->SetBounds(trect, aPaint);
  }

  if (nsnull != mHScrollBarView)
  {
    mHScrollBarView->GetDimensions(&trect.width, &trect.height);

    trect.width = width;

    //if (showVert || (mCornerView && (mCornerView->GetVisibility() == nsViewVisibility_kShow)))
    if (showVert)
      trect.width -= scrollWidth;

    trect.y = height - scrollHeight;
    trect.x = 0;

    mHScrollBarView->SetBounds(trect, aPaint);
  }

  //this will fix the size of the thumb when we resize the root window,
  //but unfortunately it will also cause scrollbar flashing. so long as
  //all resize operations happen through the viewmanager, this is not
  //an issue. we'll see. MMP
//  ComputeContainerSize();

  NS_RELEASE(dx);
  return NS_OK;
}

NS_IMETHODIMP nsScrollingView :: SetPosition(nscoord aX, nscoord aY)
{
  nsIDeviceContext  *dx;
  nsIWidget         *thiswin;
  GetWidget(thiswin);
  float             t2p;

  if (nsnull == thiswin)
    GetOffsetFromWidget(nsnull, nsnull, thiswin);

  if (nsnull != thiswin)
    thiswin->BeginResizingChildren();

  nsView::SetPosition(aX, aY);

  mViewManager->GetDeviceContext(dx);
  dx->GetAppUnitsToDevUnits(t2p);

  AdjustChildWidgets(this, this, 0, 0, t2p);

  if (nsnull != thiswin)
  {
    thiswin->EndResizingChildren();
    NS_RELEASE(thiswin);
  }

  NS_RELEASE(dx);
  return NS_OK;
}

NS_IMETHODIMP nsScrollingView :: Paint(nsIRenderingContext& rc, const nsRect& rect,
                                       PRUint32 aPaintFlags, PRBool &aResult)
{
  PRBool  clipres = PR_FALSE;
  nsRect  brect;

  rc.PushState();

  GetBounds(brect);

  //don't clip if we have a widget
  if ((mVis == nsViewVisibility_kShow) && (nsnull == mWindow))
    clipres = rc.SetClipRect(brect, nsClipCombine_kIntersect);

  if (clipres == PR_FALSE)
  {
#if 0
    rc.Translate(-mOffsetX, -mOffsetY);
#endif
    nsView::Paint(rc, rect, aPaintFlags | NS_VIEW_FLAG_CLIP_SET, clipres);
  }

  clipres = rc.PopState();

  if ((clipres == PR_FALSE) && (mVis == nsViewVisibility_kShow) && (nsnull == mWindow))
    clipres = rc.SetClipRect(brect, nsClipCombine_kSubtract);

  aResult = clipres;
  return NS_OK;
}

void nsScrollingView :: HandleScrollEvent(nsGUIEvent *aEvent, PRUint32 aEventFlags)
{
  nsIView           *scview = nsView::GetViewFor(aEvent->widget);
  nsIDeviceContext  *px;
  float             scale;
  nscoord           dx = 0, dy = 0;
  nsRect            bounds;

  mViewManager->GetDeviceContext(px);
  px->GetAppUnitsToDevUnits(scale);

  GetBounds(bounds);

  if ((nsnull != mVScrollBarView) && (scview == mVScrollBarView))
  {
    nscoord oy = mOffsetY;
    nscoord newpos;
    float   p2t;

    px->GetDevUnitsToAppUnits(p2t);

    //now, this horrible thing makes sure that as we scroll
    //the document a pixel at a time, we keep the logical position of
    //our scroll bar at the top edge of the same pixel that
    //is displayed.

    newpos = ((nsScrollbarEvent *)aEvent)->position;

    if ((newpos + bounds.height) > mSizeY)
      newpos = mSizeY - bounds.height;

    mOffsetY = NSIntPixelsToTwips(NSTwipsToIntPixels(newpos, scale), p2t);

    dy = NSTwipsToIntPixels((oy - mOffsetY), scale);

    if (dy != 0)
    {
      // Slide the scrolled view
      nsIView *scrolledView;
      nscoord  x, y;
      GetScrolledView(scrolledView);
      scrolledView->GetPosition(&x, &y);
      scrolledView->SetPosition(x, -mOffsetY);

      // XXX Clearing everything isn't correct, but maybe we should clear it for
      // our view...
#if 0
      mViewManager->ClearDirtyRegion();
#endif

      nsIWidget *thiswin;
      GetWidget(thiswin);

      if (nsnull == thiswin)
        GetOffsetFromWidget(nsnull, nsnull, thiswin);

      if (nsnull != thiswin)
        thiswin->BeginResizingChildren();

      //and now we make sure that the scrollbar thumb is in sync with the
      //numbers we came up with here, but only if we actually moved at least
      //a full pixel. if didn't adjust the thumb only if the delta is non-zero,
      //very slow scrolling would never actually work.

      ((nsScrollbarEvent *)aEvent)->position = mOffsetY;

      if (dy != 0)
      {
        if (nsnull != mWindow)
          mWindow->Scroll(0, dy, nsnull);
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
    nscoord ox = mOffsetX;
    nscoord newpos;
    float   p2t;

    px->GetDevUnitsToAppUnits(p2t);

    //now, this horrible thing makes sure that as we scroll
    //the document a pixel at a time, we keep the logical position of
    //our scroll bar at the top edge of the same pixel that
    //is displayed.

    newpos = ((nsScrollbarEvent *)aEvent)->position;

    if ((newpos + bounds.width) > mSizeX)
      newpos = mSizeX - bounds.width;

    mOffsetX = NSIntPixelsToTwips(NSTwipsToIntPixels(newpos, scale), p2t);

    dx = NSTwipsToIntPixels((ox - mOffsetX), scale);

    if (dx != 0)
    {
      // Slide the scrolled view
      nsIView *scrolledView;
      nscoord  x, y;
      GetScrolledView(scrolledView);
      scrolledView->GetPosition(&x, &y);
      scrolledView->SetPosition(-mOffsetX, y);

      // XXX Clearing everything isn't correct, but maybe we should clear it for
      // our view...
#if 0
      mViewManager->ClearDirtyRegion();
#endif

      nsIWidget *thiswin;
      GetWidget(thiswin);

      if (nsnull == thiswin)
        GetOffsetFromWidget(nsnull, nsnull, thiswin);

      if (nsnull != thiswin)
        thiswin->BeginResizingChildren();

      //and now we make sure that the scrollbar thumb is in sync with the
      //numbers we came up with here, but only if we actually moved at least
      //a full pixel. if didn't adjust the thumb only if the delta is non-zero,
      //very slow scrolling would never actually work.

      ((nsScrollbarEvent *)aEvent)->position = mOffsetX;

      if (dx != 0)
      {
        if (nsnull != mWindow)
          mWindow->Scroll(dx, 0, nsnull);
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

  NS_RELEASE(px);
}

void nsScrollingView :: Notify(nsITimer * aTimer)
{
  nscoord xoff, yoff;
  nsIView *view;
  GetScrolledView(view);

  // First do the scrolling of the view
  xoff = mOffsetX;
  yoff = mOffsetY;

  nscoord newPos = yoff + mScrollingDelta;

  if (newPos < 0)
    newPos = 0;

  ScrollTo(0, newPos, 0);

  // Now fake a mouse event so the frames can process the selection event

  nsRect        rect;
  nsGUIEvent    event;
  nsEventStatus retval;

  event.message = NS_MOUSE_MOVE;

  GetBounds(rect);

  event.point.x = rect.x;
  event.point.y = (mScrollingDelta > 0) ? (rect.height - rect.y - 1) : 135;

  //printf("timer %d %d\n", event.point.x, event.point.y);

  nsIViewObserver *obs;

  if (NS_OK == mViewManager->GetViewObserver(obs))
  {
    obs->HandleEvent((nsIView *)this, &event, retval);
    NS_RELEASE(obs);
  }
  
  NS_RELEASE(mScrollingTimer);

  if (NS_OK == NS_NewTimer(&mScrollingTimer))
    mScrollingTimer->Init(this, 25);
}

NS_IMETHODIMP nsScrollingView :: HandleEvent(nsGUIEvent *aEvent, PRUint32 aEventFlags,
                                             nsEventStatus &aStatus)
{
nsIWidget 				*win;

  switch (aEvent->message)
  {
    case NS_MOUSE_LEFT_BUTTON_DOWN:
    case NS_MOUSE_MIDDLE_BUTTON_DOWN:
    case NS_MOUSE_RIGHT_BUTTON_DOWN: 
    {
      GetWidget(win);

      if (nsnull != win) 
      {
        win->SetFocus();
        NS_RELEASE(win);
      }

      break;
    }

    case NS_KEY_DOWN:
    {
      nsKeyEvent * keyEvent = (nsKeyEvent *)aEvent;
      switch (keyEvent->keyCode) {
        case NS_VK_PAGE_DOWN : 
        case NS_VK_PAGE_UP   : {
          nsIScrollbar  *scrollv = nsnull, *scrollh = nsnull;
          nsIWidget     *win;
          mVScrollBarView->GetWidget(win);

          if (NS_OK == win->QueryInterface(kIScrollbarIID, (void **)&scrollv))
          {
            PRUint32  oldpos = 0;
            scrollv->GetPosition(oldpos);
            nsRect rect;
            GetBounds(rect);
            nscoord newPos = 0;
            if (keyEvent->keyCode == NS_VK_PAGE_DOWN) {
              newPos = oldpos+rect.height;
            } else {
              newPos = oldpos-rect.height;
              newPos = (newPos < 0 ? 0 : newPos);
            }
            ScrollTo(0, newPos, 0);
          }

        } break;

        case NS_VK_DOWN : 
        case NS_VK_UP   : {
          nsIScrollbar  *scrollv = nsnull, *scrollh = nsnull;
          mVScrollBarView->GetWidget(win);

          if (NS_OK == win->QueryInterface(kIScrollbarIID, (void **)&scrollv))
          {
            PRUint32  oldpos  = 0;
            scrollv->GetPosition(oldpos);
            PRUint32  lineInc = 0;
            scrollv->GetLineIncrement(lineInc);
            nscoord newPos = 0;
            if (keyEvent->keyCode == NS_VK_DOWN) {
              newPos = oldpos+lineInc;
            } else {
              newPos = oldpos-lineInc;
              newPos = (newPos < 0 ? 0 : newPos);
            }
            ScrollTo(0, newPos, 0);
          }

        } break;

        default:
          break;

      } // switch
    } break;

    case NS_MOUSE_MOVE:
    {
      nsRect  brect;
      nscoord lx, ly;

      GetWidget(win);
		
      GetBounds(brect);
      
      // XXX Huh. We shouldn't just be doing this for any mouse move.
      // If this is for auto-scrolling then only on mouse press and drag...
      lx = aEvent->point.x - (brect.x);
      ly = aEvent->point.y - (brect.y);

      //nscoord         xoff, yoff;
      //GetScrolledView()->GetScrollOffset(&xoff, &yoff);
      //printf("%d %d   %d\n", trect.y, trect.height, yoff);
      //printf("mouse %d %d \n", aEvent->point.x, aEvent->point.y);

      if (!brect.Contains(lx, ly))
      {
        if (mScrollingTimer == nsnull)
        {
          if (nsnull != mClientData)
          {
            if (ly < 0 || ly > brect.y)
            {
              mScrollingDelta = ly < 0 ? -100 : 100;
              NS_NewTimer(&mScrollingTimer);
              mScrollingTimer->Init(this, 25);
            }
          }
        }
      }
      else if (mScrollingTimer != nsnull)
      {
        mScrollingTimer->Cancel();
        NS_RELEASE(mScrollingTimer);
      }
      break;
    }

    case NS_MOUSE_LEFT_BUTTON_UP:
    case NS_MOUSE_MIDDLE_BUTTON_UP:
    case NS_MOUSE_RIGHT_BUTTON_UP: 
    {
      if (mScrollingTimer != nsnull)
      {
        mScrollingTimer->Cancel();
        NS_RELEASE(mScrollingTimer);
        mScrollingTimer = nsnull;
      }

      nsRect  trect;
      nscoord lx, ly;

      GetBounds(trect);

      //GetWidget(win);
      //mViewManager->GetDeviceContext(dx);
      //dx->GetDevUnitsToAppUnits(p2t);
      //offX = offY = 0;
      //win->ConvertToDeviceCoordinates(offX,offY);
      //offX = NSIntPixelsToTwips(offX, p2t);
      //offY = NSIntPixelsToTwips(offY, p2t);

      lx = aEvent->point.x - (trect.x);
      ly = aEvent->point.y - (trect.y);

      if (!trect.Contains(lx, ly))
      {
        nsEventStatus retval;

        if (nsnull != mClientData)
        {
          nsIViewObserver *obs;

          if (NS_OK == mViewManager->GetViewObserver(obs))
          {
            obs->HandleEvent((nsIView *)this, aEvent, retval);
            NS_RELEASE(obs);
          }
        }
      }
      break;
    }

    default:
      break;
  }

  return nsView::HandleEvent(aEvent, aEventFlags, aStatus);
}

NS_IMETHODIMP nsScrollingView :: ComputeContainerSize()
{
  nsIView       *scrolledView;
  GetScrolledView(scrolledView);
  nsIScrollbar  *scrollv = nsnull, *scrollh = nsnull;
  nsIWidget     *win;

  if (nsnull != scrolledView)
  {
    nscoord           dx = 0, dy = 0;
    nsIDeviceContext  *px;
    nscoord           hwidth, hheight;
    nscoord           vwidth, vheight;
    PRUint32          oldsizey = mSizeY, oldsizex = mSizeX;
    nsRect            area(0, 0, 0, 0);
    nscoord           offx, offy;
    float             scale;

    mViewManager->GetDeviceContext(px);
    px->GetAppUnitsToDevUnits(scale);

#if 0
    ComputeScrollArea(scrolledView, area, 0, 0);

    mSizeY = area.YMost();
    mSizeX = area.XMost();
#else
    scrolledView->GetDimensions(&mSizeX, &mSizeY);
#endif

    if (nsnull != mHScrollBarView)
    {
      mHScrollBarView->GetDimensions(&hwidth, &hheight);
      mHScrollBarView->GetWidget(win);

      if (NS_OK == win->QueryInterface(kIScrollbarIID, (void **)&scrollh))
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

      mVScrollBarView->GetWidget(win);

      if (NS_OK == win->QueryInterface(kIScrollbarIID, (void **)&scrollv))
      {
        if ((mSizeY > mBounds.height) && (mScrollPref != nsScrollPreference_kNeverScroll))
        {
          //we need to be able to scroll

          mVScrollBarView->SetVisibility(nsViewVisibility_kShow);
          win->Enable(PR_TRUE);

          //now update the scroller position for the new size

          PRUint32  oldpos = 0;
          scrollv->GetPosition(oldpos);
          float     p2t;

          px->GetDevUnitsToAppUnits(p2t);

          mOffsetY = NSIntPixelsToTwips(NSTwipsToIntPixels(nscoord(((float)oldpos * mSizeY) / oldsizey), scale), p2t);

          dy = NSTwipsToIntPixels((offy - mOffsetY), scale);

          scrollv->SetParameters(mSizeY, mBounds.height - ((nsnull != scrollh) ? hheight : 0),
                                 mOffsetY, NSIntPointsToTwips(12));
        }
        else
        {
          mOffsetY = 0;
          dy = NSTwipsToIntPixels(offy, scale);

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

      mHScrollBarView->GetWidget(win);

      if (NS_OK == win->QueryInterface(kIScrollbarIID, (void **)&scrollh))
      {
        if ((mSizeX > mBounds.width) && (mScrollPref != nsScrollPreference_kNeverScroll))
        {
          //we need to be able to scroll

          mHScrollBarView->SetVisibility(nsViewVisibility_kShow);
          win->Enable(PR_TRUE);

          //now update the scroller position for the new size

          PRUint32  oldpos = 0;
          scrollh->GetPosition(oldpos);
          float     p2t;

          px->GetDevUnitsToAppUnits(p2t);

          mOffsetX = NSIntPixelsToTwips(NSTwipsToIntPixels(nscoord(((float)oldpos * mSizeX) / oldsizex), scale), p2t);

          dx = NSTwipsToIntPixels((offx - mOffsetX), scale);

          scrollh->SetParameters(mSizeX, mBounds.width - ((nsnull != scrollv) ? vwidth : 0),
                                 mOffsetX, NSIntPointsToTwips(12));

//          //now make the vertical scroll region account for this scrollbar
//
//          if (nsnull != scrollv)
//            scrollv->SetParameters(mSizeY, mBounds.height - hheight, mOffsetY, NSIntPointsToTwips(12));
        }
        else
        {
          mOffsetX = 0;
          dx = NSTwipsToIntPixels(offx, scale);

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

    // Position the scrolled view
    scrolledView->SetPosition(-mOffsetX, -mOffsetY);

    if (mCornerView)
    {
      if (mHScrollBarView && ViewIsShowing(mHScrollBarView) &&
          mVScrollBarView && ViewIsShowing(mVScrollBarView))
        ((CornerView *)mCornerView)->Show(PR_TRUE);
      else
        ((CornerView *)mCornerView)->Show(PR_FALSE);
    }

    // now we can release the vertical scroller if there was one...

    NS_IF_RELEASE(scrollv);

//    if ((dx != 0) || (dy != 0))
//      AdjustChildWidgets(this, this, 0, 0, px->GetTwipsToPixels());

    NS_RELEASE(px);
  }
  else
  {
    if (nsnull != mHScrollBarView)
    {
      mHScrollBarView->SetVisibility(nsViewVisibility_kHide);

      mHScrollBarView->GetWidget(win);

      if (NS_OK == win->QueryInterface(kIScrollbarIID, (void **)&scrollh))
      {
        scrollh->SetParameters(0, 0, 0, 0);
        NS_RELEASE(scrollh);
      }

      NS_RELEASE(win);
    }

    if (nsnull != mVScrollBarView)
    {
      mVScrollBarView->SetVisibility(nsViewVisibility_kHide);

      mVScrollBarView->GetWidget(win);

      if (NS_OK == win->QueryInterface(kIScrollbarIID, (void **)&scrollv))
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
  return NS_OK;
}

NS_IMETHODIMP nsScrollingView :: GetContainerSize(nscoord *aWidth, nscoord *aHeight)
{
  *aWidth = mSizeX;
  *aHeight = mSizeY;
  return NS_OK;
}

NS_IMETHODIMP nsScrollingView :: ShowQuality(PRBool aShow)
{
  ((CornerView *)mCornerView)->ShowQuality(aShow);
  return NS_OK;
}

NS_IMETHODIMP nsScrollingView :: GetShowQuality(PRBool &aShow)
{
  aShow = ((CornerView *)mCornerView)->mShowQuality;
  return NS_OK;
}

NS_IMETHODIMP nsScrollingView :: SetQuality(nsContentQuality aQuality)
{
  ((CornerView *)mCornerView)->SetQuality(aQuality);
  return NS_OK;
}

NS_IMETHODIMP nsScrollingView :: SetScrollPreference(nsScrollPreference aPref)
{
  mScrollPref = aPref;
  ComputeContainerSize();
  return NS_OK;
}

NS_IMETHODIMP nsScrollingView :: GetScrollPreference(nsScrollPreference &aScrollPreference)
{
  aScrollPreference = mScrollPref;
  return NS_OK;
}

// XXX This doesn't do X scrolling yet

// XXX This doesn't let the scrolling code slide the bits on the
// screen and damage only the appropriate area

// XXX doesn't smooth scroll

NS_IMETHODIMP
nsScrollingView :: ScrollTo(nscoord aX, nscoord aY, PRUint32 aUpdateFlags)
{
  nsIDeviceContext  *dx;
  float             t2p;
  float             p2t;

  mViewManager->GetDeviceContext(dx);
  dx->GetAppUnitsToDevUnits(t2p);
  dx->GetDevUnitsToAppUnits(p2t);

  NS_RELEASE(dx);

  nsIWidget*      win;
  mVScrollBarView->GetWidget(win);
  if (nsnull != win)
  {
    nsIScrollbar* scrollv;
    if (NS_OK == win->QueryInterface(kIScrollbarIID, (void **)&scrollv))
    {
      // Clamp aY
      nsRect r;
      GetBounds(r);
      if (aY + r.height > mSizeY) {
        aY = mSizeY - r.height;
        if (aY < 0) {
          aY = 0;
        }
      }

      // Move the scrollbar's thumb

      PRUint32  oldpos = mOffsetY;
      nscoord dy;

      PRUint32 newpos =
        NSIntPixelsToTwips(NSTwipsToIntPixels(aY, t2p), p2t);
      scrollv->SetPosition(newpos);

      dy = oldpos - newpos;

      // Update the scrolled view's position
      nsIView* scrolledView;
      GetScrolledView(scrolledView);
      if (nsnull != scrolledView)
      {
        scrolledView->SetPosition(-aX, -aY);
        mOffsetX = aX;
        mOffsetY = aY;
      }

      AdjustChildWidgets(this, this, 0, 0, t2p);

      // Damage the updated area
      r.x = 0;
      r.y = aY;
      if (nsnull != scrolledView)
      {
        mViewManager->UpdateView(scrolledView, r, aUpdateFlags);
      }

      NS_RELEASE(scrollv);
    }
    NS_RELEASE(win);
  }
  return NS_OK;
}

NS_IMETHODIMP nsScrollingView :: GetClipSize(nscoord *aX, nscoord *aY)
{
  *aX = mClipX;
  *aY = mClipY;

  return NS_OK;
}

void nsScrollingView :: AdjustChildWidgets(nsScrollingView *aScrolling, nsIView *aView, nscoord aDx, nscoord aDy, float scale)
{
  PRInt32           numkids;
  aView->GetChildCount(numkids);
  nsIScrollableView *scroller;
  nscoord           offx, offy;
  PRBool            isscroll = PR_FALSE;

  if (aScrolling == aView)
  {
    nsIWidget *widget;
    aScrolling->GetOffsetFromWidget(&aDx, &aDy, widget);
    NS_IF_RELEASE(widget);
  }

  aView->GetPosition(&offx, &offy);

  aDx += offx;
  aDy += offy;

  for (PRInt32 cnt = 0; cnt < numkids; cnt++)
  {
    nsIView   *kid;
    aView->GetChild(cnt, kid);
    nsIWidget *win;
    kid->GetWidget(win);

    if (nsnull != win)
    {
      nsRect  bounds;

      win->BeginResizingChildren();
      kid->GetBounds(bounds);

      if (!isscroll ||
          (isscroll &&
          (kid != ((nsScrollingView *)aView)->mVScrollBarView) &&
          (kid != ((nsScrollingView *)aView)->mHScrollBarView)))
        win->Move(NSTwipsToIntPixels((bounds.x + aDx), scale), NSTwipsToIntPixels((bounds.y + aDy), scale));
      else
        win->Move(NSTwipsToIntPixels((bounds.x + aDx + offx), scale), NSTwipsToIntPixels((bounds.y + aDy + offy), scale));
    }

    AdjustChildWidgets(aScrolling, kid, aDx, aDy, scale);

    if (nsnull != win)
    {
      win->EndResizingChildren();
      NS_RELEASE(win);
    }
  }
}

NS_IMETHODIMP nsScrollingView :: SetScrolledView(nsIView *aScrolledView)
{
  return mViewManager->InsertChild(mClipView, aScrolledView, 0);
}

NS_IMETHODIMP nsScrollingView :: GetScrolledView(nsIView *&aScrolledView)
{
  return mClipView->GetChild(0, aScrolledView);
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

  PRInt32 numkids;
  aView->GetChildCount(numkids);
  
  for (PRInt32 cnt = 0; cnt < numkids; cnt++)
  {
    nsIView *view;
    aView->GetChild(cnt, view);
    ComputeScrollArea(view, aRect, aOffX, aOffY);
  }
}
