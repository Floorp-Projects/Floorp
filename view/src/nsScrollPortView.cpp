/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsScrollPortView.h"
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
#include "nsILookAndFeel.h"
#include "nsIClipView.h"
#include "nsISupportsArray.h"
#include "nsIScrollPositionListener.h"

static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);
static NS_DEFINE_IID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kIClipViewIID, NS_ICLIPVIEW_IID);

/*
class ScrollBarView : public nsView
{
public:
  ScrollBarView(nsScrollPortView *aScrollingView);
  ~ScrollBarView();

  NS_IMETHOD  HandleEvent(nsGUIEvent *aEvent, PRUint32 aEventFlags, nsEventStatus &aStatus, PRBool& handled);

  // Do not set the visibility of the ScrollbarView using SetVisibility. Instead it 
  // must be marked as visible or hidden using SetEnabled. 
  // The nsScrollPortView::UpdateComponentVisibility looks at both the enabled flag and the 
  // scrolling views visibility to determine if the ScrollBarView is visible or hidden and calls
  // SetVisibility. KMM
 
  void SetEnabled(PRBool aEnabled);
  PRBool GetEnabled();

public:
  nsScrollPortView *mScrollingView;

protected:
  PRBool mEnabled;

};

static inline PRBool
ViewIsShowing(ScrollBarView *aView)
{
  return(aView->GetEnabled());
}

ScrollBarView::ScrollBarView(nsScrollPortView *aScrollingView)
{
  mScrollingView = aScrollingView;
  mEnabled = PR_FALSE;
}

ScrollBarView::~ScrollBarView()
{
}

NS_IMETHODIMP ScrollBarView::HandleEvent(nsGUIEvent *aEvent, PRUint32 aEventFlags,
                                           nsEventStatus &aStatus, PRBool& aHandled)
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

void ScrollBarView::SetEnabled(PRBool aEnabled) 
{
  mEnabled = aEnabled;
}

PRBool ScrollBarView::GetEnabled()
{
  return(mEnabled);
}


class CornerView : public nsView
{
public:
  CornerView();
  ~CornerView();

  NS_IMETHOD  ShowQuality(PRBool aShow);
  NS_IMETHOD  SetQuality(nsContentQuality aQuality);
  NS_IMETHOD  Paint(nsIRenderingContext& rc, const nsRect& rect,
                    PRUint32 aPaintFlags, PRBool &Result);

  void  Show(PRBool aShow, PRBool aRethink);

  PRBool            mShowQuality;
  nsContentQuality  mQuality;
  PRBool            mShow;
  nsILookAndFeel    *mLookAndFeel;
};

CornerView::CornerView()
{
  mShowQuality = PR_FALSE;
  mQuality = nsContentQuality_kGood;
  mShow = PR_FALSE;
  mLookAndFeel = nsnull;
}

CornerView::~CornerView()
{
  NS_IF_RELEASE(mLookAndFeel);
}

NS_IMETHODIMP CornerView::ShowQuality(PRBool aShow)
{
  if (mShowQuality != aShow)
  {
    mShowQuality = aShow;

    if (mShow == PR_FALSE)
    {
      if (mShowQuality == PR_FALSE)
        mViewManager->SetViewVisibility(this, nsViewVisibility_kHide);
      else
        mViewManager->SetViewVisibility(this, nsViewVisibility_kShow);

      nsIScrollableView *par;
      if (NS_OK == mParent->QueryInterface(NS_GET_IID(nsIScrollableView), (void **)&par)) {
        par->ComputeScrollOffsets(PR_TRUE);
      }
    }

    mViewManager->UpdateView(this, nsnull, NS_VMREFRESH_IMMEDIATE);
  }
  return NS_OK;
}

NS_IMETHODIMP CornerView::SetQuality(nsContentQuality aQuality)
{
  if (mQuality != aQuality)
  {
    mQuality = aQuality;

    if (mVis == nsViewVisibility_kShow)
      mViewManager->UpdateView(this, nsnull, NS_VMREFRESH_IMMEDIATE);
  }
  return NS_OK;
}

void CornerView::Show(PRBool aShow, PRBool aRethink)
{
  if (mShow != aShow)
  {
    mShow = aShow;

    if (mShow == PR_TRUE)
      mViewManager->SetViewVisibility(this, nsViewVisibility_kShow);
    else if (mShowQuality == PR_FALSE)
      mViewManager->SetViewVisibility(this, nsViewVisibility_kHide);

    if (PR_TRUE == aRethink)
    {
      nsIScrollableView *par;
      if (NS_OK == mParent->QueryInterface(NS_GET_IID(nsIScrollableView), (void **)&par)) {
        par->ComputeScrollOffsets(PR_TRUE);
      }
    }
  }
}

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

NS_IMETHODIMP CornerView::Paint(nsIRenderingContext& rc, const nsRect& rect,
                                  PRUint32 aPaintFlags, PRBool &aResult)
{
  if (mVis == nsViewVisibility_kShow)
  {
    nsRect  brect;
    nscolor bgcolor;

    GetBounds(brect);

    brect.x = brect.y = 0;

    if (nsnull == mLookAndFeel)
    {
      nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull,
                                         NS_GET_IID(nsILookAndFeel), (void **)&mLookAndFeel);
    }

    if (nsnull != mLookAndFeel)
      mLookAndFeel->GetColor(nsILookAndFeel::eColor_WidgetBackground, bgcolor);
    else
      bgcolor = NS_RGB(192, 192, 192);

    rc.SetColor(bgcolor);
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
      else if (mQuality == nsContentQuality_kPoor)
        rc.SetColor(NS_RGB(255, 0, 0));
      else
        rc.SetColor(NS_RGB(0, 0, 255));

      //hey, notice that these numbers don't add up... that's because
      //something funny happens on windows when the *right* numbers are
      //used. MMP

      rc.FillEllipse(NSToCoordRound(brect.width * 0.23f),  // XXX should use NSToCoordCeil ??
                     NSToCoordRound(brect.height * 0.23f), // XXX should use NSToCoordCeil ??
                     nscoord(brect.width * 0.46f),
                     nscoord(brect.height * 0.46f));

      rc.GetColor(bcolor);
      tcolor = bcolor;

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

  aResult = PR_TRUE;

  return NS_OK;
}


class ClipView : public nsView, public nsIClipView
{
public:
  ClipView();
  ~ClipView();

  NS_IMETHOD QueryInterface(REFNSIID aIID,
                            void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);
};

ClipView::ClipView()
{
}

ClipView::~ClipView()
{
}

nsresult ClipView::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  
  if (aIID.Equals(NS_GET_IID(nsIClipView))) {
    *aInstancePtr = (void*)(nsIClipView*)this;
    return NS_OK;
  }

  return nsView::QueryInterface(aIID, aInstancePtr);
}

nsrefcnt ClipView::AddRef()
{
  NS_WARNING("not supported for views");
  return 1;
}

nsrefcnt ClipView::Release()
{
  NS_WARNING("not supported for views");
  return 1;
}
*/

nsScrollPortView::nsScrollPortView()
  //: mInsets(0, 0, 0, 0)
{
 // mSizeX = mSizeY = 0;
  mOffsetX = mOffsetY = 0;
  mOffsetXpx = mOffsetYpx = 0;
  /*
  mClipView = nsnull;
  mVScrollBarView = nsnull;
  mHScrollBarView = nsnull;
  mCornerView = nsnull;
  mScrollPref = nsScrollPreference_kAuto;
  mScrollingTimer = nsnull;
  mLineHeight = 240;
  */

  mListeners = nsnull;
}

nsScrollPortView::~nsScrollPortView()
{
    /*
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
*/
    
  if (nsnull != mListeners) {
    mListeners->Clear();
    NS_RELEASE(mListeners);
  }

}

nsresult nsScrollPortView::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  *aInstancePtr = nsnull;
  if (aIID.Equals(NS_GET_IID(nsIScrollableView))) {
    *aInstancePtr = (void*)(nsIScrollableView*)this;
    return NS_OK;
  }

  return nsView::QueryInterface(aIID, aInstancePtr);
}

nsrefcnt nsScrollPortView::AddRef()
{
  NS_WARNING("not supported for views");
  return 1;
}

nsrefcnt nsScrollPortView::Release()
{
  NS_WARNING("not supported for views");
  return 1;
}


NS_IMETHODIMP nsScrollPortView::Init(nsIViewManager* aManager,
                                      const nsRect &aBounds,
                                      const nsIView *aParent,
                                      const nsViewClip *aClip,
                                      nsViewVisibility aVisibilityFlag)
{
    /*
  nsIDeviceContext  *dx = nsnull;

  aManager->GetDeviceContext(dx);

  if (dx)
  {
    float t2d, d2a;

    dx->GetTwipsToDevUnits(t2d);
    dx->GetDevUnitsToAppUnits(d2a);

    mLineHeight = NSToCoordRound(240.0f * t2d * d2a);

    NS_RELEASE(dx);
  }

  */
  return nsView::Init(aManager, aBounds, aParent, aClip, aVisibilityFlag);
}

NS_IMETHODIMP nsScrollPortView::SetDimensions(nscoord width, nscoord height, PRBool aPaint)
{
    /*
  nsIDeviceContext  *dx;
  mViewManager->GetDeviceContext(dx);
  float             scrollWidth, scrollHeight;
  dx->GetScrollBarDimensions(scrollWidth, scrollHeight);
  nscoord           showHorz = 0, showVert = 0;
  nsRect            clipRect;

  */

  // Set our bounds and size our widget if we have one
  nsView::SetDimensions(width, height, aPaint);

  /*
#if 0
  //this will fix the size of the thumb when we resize the root window,
  //but unfortunately it will also cause scrollbar flashing. so long as
  //all resize operations happen through the viewmanager, this is not
  //an issue. we'll see. MMP

  ComputeScrollOffsets();
#endif

  // Determine how much space is actually taken up by the scrollbars
  if (mHScrollBarView && ViewIsShowing((ScrollBarView *)mHScrollBarView))
    showHorz = NSToCoordRound(scrollHeight);

  if (mVScrollBarView && ViewIsShowing((ScrollBarView *)mVScrollBarView))
    showVert = NSToCoordRound(scrollWidth);

  // Compute the clip view rect
  clipRect.SetRect(0, 0, PR_MAX((width - showVert), 0), PR_MAX((height - showHorz), 0));
  clipRect.Deflate(mInsets);

  // Size and position the clip view
  if (nsnull != mClipView)
    mClipView->SetBounds(clipRect, aPaint);

  NS_RELEASE(dx);
  */
  return NS_OK;
}

NS_IMETHODIMP nsScrollPortView::SetPosition(nscoord aX, nscoord aY)
{
  // If we have a widget then there's no need to adjust child widgets,
  // because they're relative to our window
    /*
  if (nsnull != mWindow)
  {
  */

    nsView::SetPosition(aX, aY);

    /*
  }
  else
  {
    nsIDeviceContext  *dx;
    nsIWidget         *thiswin;
    GetWidget(thiswin);
    float             t2p;
    nsIView           *scrolledView;
  
    if (nsnull == thiswin)
      GetOffsetFromWidget(nsnull, nsnull, thiswin);
  
    if (nsnull != thiswin)
      thiswin->BeginResizingChildren();
  
    nsView::SetPosition(aX, aY);
  
    mViewManager->GetDeviceContext(dx);
    dx->GetAppUnitsToDevUnits(t2p);

    GetScrolledView(scrolledView);
  
    if (scrolledView)
    {
      // Adjust the positions of the scrollbars and clip view's widget
      AdjustChildWidgets(this, this, 0, 0, t2p);
    }
  
    if (nsnull != thiswin)
    {
      thiswin->EndResizingChildren();
      NS_RELEASE(thiswin);
    }
  
    NS_RELEASE(dx);
  }
  */
  return NS_OK;
}

/*
nsresult
nsScrollPortView::SetComponentVisibility(nsIView* aView, nsViewVisibility aViewVisibility) 
{
  nsresult rv = NS_OK;
  if (nsnull != aView) {
      // Only set visibility if it's not currently set.
    nsViewVisibility componentVisibility;
    aView->GetVisibility(componentVisibility);
    if (aViewVisibility != componentVisibility) {
      rv = aView->SetVisibility(aViewVisibility);
    }
  }

  return rv;
}
*/

// Set the visibility of the scrolling view's components (ClipView, CornerView, ScrollBarView's)
/*
nsresult
nsScrollPortView::UpdateComponentVisibility(nsViewVisibility aScrollingViewVisibility)
{
  nsresult rv = NS_OK;
  if (nsViewVisibility_kHide == aScrollingViewVisibility) {

      
     // Hide Clip View 
    rv = SetComponentVisibility(mClipView, nsViewVisibility_kHide);

     // Hide horizontal scrollbar
    if (NS_SUCCEEDED(rv)) {
      rv = SetComponentVisibility(mHScrollBarView, nsViewVisibility_kHide);
    }
   
    // Hide vertical scrollbar
    if (NS_SUCCEEDED(rv)) {
      rv = SetComponentVisibility(mVScrollBarView, nsViewVisibility_kHide);
    }
   
    // Hide the corner view
    if (NS_SUCCEEDED(rv)) { 
      rv = SetComponentVisibility(mCornerView, nsViewVisibility_kHide);
    }
    

  } else if (nsViewVisibility_kShow == aScrollingViewVisibility) {
    
      // Show clip view if if the scrolling view is visible
     rv = SetComponentVisibility(mClipView, nsViewVisibility_kShow);
    
     PRBool horizEnabled = PR_FALSE; 
     PRBool vertEnabled = PR_FALSE; 

     // Show horizontal scrollbar if it is enabled otherwise hide it
     if ((NS_SUCCEEDED(rv)) && (nsnull != mHScrollBarView)) {
       horizEnabled = ((ScrollBarView *)mHScrollBarView)->GetEnabled();
       rv = SetComponentVisibility(mHScrollBarView, horizEnabled ? nsViewVisibility_kShow : nsViewVisibility_kHide);
     }
      
     // Show vertical scrollbar view if it is enabled otherwise hide it
     if ((NS_SUCCEEDED(rv)) &&  (nsnull != mVScrollBarView)) {
       vertEnabled = ((ScrollBarView *)mVScrollBarView)->GetEnabled(); 
       rv = SetComponentVisibility(mVScrollBarView, vertEnabled ? nsViewVisibility_kShow : nsViewVisibility_kHide);
     }

     // Show the corner view if both the horizontal and vertical scrollbars are enabled otherwise hide it
     if (NS_SUCCEEDED(rv)) {
       rv = SetComponentVisibility(mCornerView, (horizEnabled && vertEnabled) ? nsViewVisibility_kShow : nsViewVisibility_kHide);
     }
    }

   return rv;
}
*/


NS_IMETHODIMP nsScrollPortView::SetVisibility(nsViewVisibility aVisibility)
{
  nsresult rv; /// = UpdateComponentVisibility(aVisibility);
  //if (NS_SUCCEEDED(rv)) {
    rv = nsView::SetVisibility(aVisibility);
  //}
  return rv;
}
  
NS_IMETHODIMP nsScrollPortView::GetClipView(const nsIView** aClipView) const
{
 // NS_PRECONDITION(aClipView, "null pointer");
  *aClipView = this; //mClipView;
  return NS_OK;
}

NS_IMETHODIMP nsScrollPortView::AddScrollPositionListener(nsIScrollPositionListener* aListener)
{
	if (nsnull == mListeners) {
		nsresult rv = NS_NewISupportsArray(&mListeners);
		if (NS_FAILED(rv))
			return rv;
	}
	return mListeners->AppendElement(aListener);
}

NS_IMETHODIMP nsScrollPortView::RemoveScrollPositionListener(nsIScrollPositionListener* aListener)
{
	if (nsnull != mListeners) {
		return mListeners->RemoveElement(aListener);
	}
	return NS_ERROR_FAILURE;
}

/*
void nsScrollPortView::HandleScrollEvent(nsGUIEvent *aEvent, PRUint32 aEventFlags)
{

  nsIView           *scview = nsView::GetViewFor(aEvent->widget);

  nsIDeviceContext  *px;
  float             t2p, p2t;
  nscoord           dx = 0, dy = 0;  // in device units


  nsSize            clipSize;
 
  mViewManager->GetDeviceContext(px);
  px->GetAppUnitsToDevUnits(t2p);
  px->GetDevUnitsToAppUnits(p2t);
  NS_RELEASE(px);

 
  // Get the size of the clip view
  mClipView->GetDimensions(&clipSize.width, &clipSize.height);


  // Is it a vertical scroll event or a horizontal scroll event?
  if ((nsnull != mVScrollBarView) && (scview == mVScrollBarView))
  {
    nscoord oldOffsetY = mOffsetY;
    nscoord newPos;

    // The new scrollbar position is in app units
    newPos = ((nsScrollbarEvent *)aEvent)->position;

    // Don't allow a scroll below the bottom of the scrolled view
    if ((newPos + clipSize.height) > mSizeY)
      newPos = mSizeY - clipSize.height;

    // Snap the new scrollbar position to the nearest pixel. This ensures that
    // as we scroll the view a pixel at a time the scrollbar position
    // is at the same pixel as the top edge of the scrolled view
    mOffsetY = NSIntPixelsToTwips(NSTwipsToIntPixels(newPos, t2p), p2t);

    // Compute the delta in device units. We need device units when scrolling
    // the window
    dy = NSTwipsToIntPixels((oldOffsetY - mOffsetY), t2p);
    if (dy != 0)
    {
      // Update the scrollbar position passed in with the scrollbar event.
      // This value will be used to update the scrollbar thumb, and we want
      // to make sure the scrollbar thumb is in sync with the offset we came
      // up with here.
      ((nsScrollbarEvent *)aEvent)->position = mOffsetY;
    }
  }
  else if ((nsnull != mHScrollBarView) && (scview == mHScrollBarView))
  {
    nscoord oldOffsetX = mOffsetX;
    nscoord newPos;

    // The new scrollbar position is in app units
    newPos = ((nsScrollbarEvent *)aEvent)->position;

    // Don't allow a scroll beyond the width of the scrolled view
    if ((newPos + clipSize.width) > mSizeX)
      newPos = mSizeX - clipSize.width;

    // Snap the new scrollbar position to the nearest pixel. This ensures that
    // as we scroll the view a pixel at a time the scrollbar position
    // is at the same pixel as the left edge of the scrolled view
    mOffsetX = NSIntPixelsToTwips(NSTwipsToIntPixels(newPos, t2p), p2t);

    // Compute the delta in device units. We need device units when scrolling
    // the window
    dx = NSTwipsToIntPixels((oldOffsetX - mOffsetX), t2p);
    if (dx != 0)
    {
      // Update the scrollbar position passed in with the scrollbar event.
      // This value will be used to update the scrollbar thumb, and we want
      // to make sure the scrollbar thumb is in sync with the offset we came
      // up with here.
      ((nsScrollbarEvent *)aEvent)->position = mOffsetX;
    }
  }

  // Position the scrolled view
  nsIView *scrolledView;
  GetScrolledView(scrolledView);
  scrolledView->SetPosition(-mOffsetX, -mOffsetY);
  
  Scroll(scrolledView, dx, dy, t2p, 0);
}
*/

/*
void nsScrollPortView::Notify(nsITimer * aTimer)
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
*/

NS_IMETHODIMP nsScrollPortView::CreateScrollControls(nsNativeWidget aNative)
{
  nsWidgetInitData  initData;
  initData.clipChildren = PR_TRUE;
  initData.clipSiblings = PR_TRUE;

  CreateWidget(kWidgetCID, &initData,
               mWindow ? nsnull : aNative);
  
  /*
  nsIDeviceContext  *dx;
  mViewManager->GetDeviceContext(dx);
  nsresult rv = NS_ERROR_FAILURE;

  // XXX Have the all widgets siblings. For the time being this is needed
  // for 'fixed' elements...
  nsWidgetInitData  initData;
  initData.clipChildren = PR_TRUE;
  initData.clipSiblings = PR_TRUE;

  // Create a clip view
  mClipView = new ClipView;

  if (nsnull != mClipView)
  {
    // The clip view needs a widget to clip any of the scrolled view's
    // child views with widgets. Note that the clip view has an opacity
    // of 0.0f (completely transparent)
    // XXX The clip widget should be created on demand only...
    rv = mClipView->Init(mViewManager, mBounds, this);
    mViewManager->InsertChild(this, mClipView, 1);
    mViewManager->SetViewOpacity(mClipView, 0.0f);
    rv = mClipView->CreateWidget(kWidgetCID, &initData,
                                 mWindow ? nsnull : aNative);
  }

  // Create a view for a corner cover
  mCornerView = new CornerView;

  if (nsnull != mCornerView)
  {
    nsRect trect;
    float  sbWidth, sbHeight;

    dx->GetScrollBarDimensions(sbWidth, sbHeight);
    trect.width = NSToCoordRound(sbWidth);
    trect.x = mBounds.x + mBounds.XMost() - trect.width;
    trect.height = NSToCoordRound(sbHeight);
    trect.y = mBounds.y + mBounds.YMost() - trect.height;

    rv = mCornerView->Init(mViewManager, trect, this,
                           nsnull, nsViewVisibility_kHide);
    mViewManager->InsertChild(this, mCornerView, 3);
    mCornerView->CreateWidget(kWidgetCID, &initData,
                              mWindow ? nsnull : aNative);
  }

  // Create a view for a vertical scrollbar
  mVScrollBarView = new ScrollBarView(this);

  if (nsnull != mVScrollBarView)
  {
    nsRect  trect = mBounds;
    float   sbWidth, sbHeight;

    dx->GetScrollBarDimensions(sbWidth, sbHeight);
    trect.width = NSToCoordRound(sbWidth);
    trect.x += mBounds.XMost() - trect.width;
    trect.height -= NSToCoordRound(sbHeight);

    static NS_DEFINE_IID(kCScrollbarIID, NS_VERTSCROLLBAR_CID);

    rv = mVScrollBarView->Init(mViewManager, trect, this);
    mViewManager->InsertChild(this, mVScrollBarView, 3);
    rv = mVScrollBarView->CreateWidget(kCScrollbarIID, &initData,
                                       mWindow ? nsnull : aNative);
    nsIView *scrolledView;
    GetScrolledView(scrolledView);

#if 1 // MOUSE WHEEL TRACKER CODE

    // XXX This code is to be reviewed by michealp
    // It gets the Window for the view and the gets the widget
    // for the vertical ScrollbarView and sets it into the window
    // this is need for platforms where the window receives 
    // scrollbar message that need to be sent to the vertical scrollbar
    // For example, the Mouse Wheel Tracker on MS-Windows

    // Find Parent view with window and remember the window
    nsIWidget * win  = nsnull;
    nsIView   * view = (nsIView *)this;
    view->GetWidget(win);
    while (win == nsnull) {
      nsIView * parent;
      view->GetParent(parent);
      if (nsnull == parent) {
        break;
      }
      parent->GetWidget(win);
      view = parent;
    }
      
    // Set scrollbar widget into window
    if (nsnull != win) {
      nsIWidget * scrollbar;
      mVScrollBarView->GetWidget(scrollbar);
      if (nsnull != scrollbar) {
        win->SetVerticalScrollbar(scrollbar);
        NS_RELEASE(scrollbar);
      }
      NS_RELEASE(win);
    }
    // XXX done with the code that needs to be reviewed
#endif
  }

  // Create a view for a horizontal scrollbar
  mHScrollBarView = new ScrollBarView(this);

  if (nsnull != mHScrollBarView)
  {
    nsRect  trect = mBounds;
    float   sbWidth, sbHeight;

    dx->GetScrollBarDimensions(sbWidth, sbHeight);
    trect.height = NSToCoordRound(sbHeight);
    trect.y += mBounds.YMost() - trect.height;
    trect.width -= NSToCoordRound(sbWidth);

    static NS_DEFINE_IID(kCHScrollbarIID, NS_HORZSCROLLBAR_CID);

    rv = mHScrollBarView->Init(mViewManager, trect, this);
    mViewManager->InsertChild(this, mHScrollBarView, 3);
    rv = mHScrollBarView->CreateWidget(kCHScrollbarIID, &initData,
                                       mWindow ? nsnull : aNative);
  }

  NS_RELEASE(dx);

  return rv;
  */
  return NS_OK;
}

NS_IMETHODIMP nsScrollPortView::SetWidget(nsIWidget *aWidget)
{
  if (nsnull != aWidget) {
    NS_ASSERTION(PR_FALSE, "please don't try and set a widget here");
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP nsScrollPortView::ComputeScrollOffsets(PRBool aAdjustWidgets)
{
  NS_WARNING("nsScrollPortView::ComputeScrollOffsets() not implemented");

  /*
  nsIView       *scrolledView;
  GetScrolledView(scrolledView);

  nsIScrollbar  *scrollv = nsnull, *scrollh = nsnull;
  PRBool		hasVertical = PR_TRUE, hasHorizontal = PR_FALSE;
  nsIWidget     *win;


  if (nsnull != scrolledView)
  {
    nscoord           dx = 0, dy = 0;
    nsIDeviceContext  *px;
    nscoord           hwidth, hheight;
    nscoord           vwidth, vheight;
    PRUint32          oldsizey = mSizeY, oldsizex = mSizeX;
    nscoord           offx, offy;
    float             scale;
    nsRect            controlRect(0, 0, mBounds.width, mBounds.height);

    controlRect.Deflate(mInsets);

    mViewManager->GetDeviceContext(px);
    px->GetAppUnitsToDevUnits(scale);

    scrolledView->GetDimensions(&mSizeX, &mSizeY);

 
    if (nsnull != mHScrollBarView) {
      mHScrollBarView->GetDimensions(&hwidth, &hheight);
      mHScrollBarView->GetWidget(win);
      
      if (NS_OK == win->QueryInterface(NS_GET_IID(nsIScrollbar), (void **)&scrollh)) {
        if (((mSizeX > controlRect.width) &&
            (mScrollPref != nsScrollPreference_kNeverScroll)) ||
            (mScrollPref == nsScrollPreference_kAlwaysScroll))
        {
          hasHorizontal = PR_TRUE;
        }
        NS_RELEASE(scrollh);
      }

      NS_RELEASE(win);
    }
    

    if (nsnull != mVScrollBarView) {
      mVScrollBarView->GetDimensions(&vwidth, &vheight);
      offy = mOffsetY;

      mVScrollBarView->GetWidget(win);

      if (NS_OK == win->QueryInterface(NS_GET_IID(nsIScrollbar), (void **)&scrollv)) {
        if ((mSizeY > (controlRect.height - (hasHorizontal ? hheight : 0)))) {
          // if we are scrollable
          if (mScrollPref != nsScrollPreference_kNeverScroll) {
            //we need to be able to scroll

            ((ScrollBarView *)mVScrollBarView)->SetEnabled(PR_TRUE);
            win->Enable(PR_TRUE);

            //now update the scroller position for the new size

            PRUint32  oldpos = 0;
            float     p2t;
            PRInt32   availheight;

            scrollv->GetPosition(oldpos);
            px->GetDevUnitsToAppUnits(p2t);

            availheight = controlRect.height - (hasHorizontal ? hheight : 0);

            // XXX Check for 0 initial size. This is really indicative
            // of a problem. 
            if (0 == oldsizey)
              mOffsetY = 0;
              else
            {
              mOffsetY = NSIntPixelsToTwips(NSTwipsToIntPixels(nscoord(oldpos), scale), p2t);

              if ((mSizeY - mOffsetY) < availheight)
              {
                mOffsetY = mSizeY - availheight;

                if (mOffsetY < 0)
                  mOffsetY = 0;
              }
            }

            dy = NSTwipsToIntPixels((offy - mOffsetY), scale);

            scrollv->SetParameters(mSizeY, availheight,
                                   mOffsetY, mLineHeight);
          }
        } else {
          // The scrolled view is entirely visible vertically. Either hide the
          // vertical scrollbar or disable it
          mOffsetY = 0;
          dy = NSTwipsToIntPixels(offy, scale);

          scrollv->SetPosition(0);  // make sure thumb is at the top

          if (mScrollPref == nsScrollPreference_kAlwaysScroll)
          {
            ((ScrollBarView *)mVScrollBarView)->SetEnabled(PR_TRUE);
            win->Enable(PR_FALSE);
          }
          else
          {
            ((ScrollBarView *)mVScrollBarView)->SetEnabled(PR_FALSE);
            win->Enable(PR_TRUE);
            hasVertical = PR_FALSE;
          }
        }

        NS_RELEASE(scrollv);
      }

      NS_RELEASE(win);
    }

    if (nsnull != mHScrollBarView) {
      offx = mOffsetX;
      mHScrollBarView->GetWidget(win);
      
      if (NS_OK == win->QueryInterface(NS_GET_IID(nsIScrollbar), (void **)&scrollh)) {
        if ((mSizeX > (controlRect.width - (hasVertical ? vwidth : 0)))) {
          if (mScrollPref != nsScrollPreference_kNeverScroll) {
            //we need to be able to scroll

            ((ScrollBarView *)mHScrollBarView)->SetEnabled(PR_TRUE);
            win->Enable(PR_TRUE);

            //now update the scroller position for the new size

            PRUint32  oldpos = 0;
            float     p2t;
            PRInt32   availwidth;

            scrollh->GetPosition(oldpos);
            px->GetDevUnitsToAppUnits(p2t);

            availwidth = controlRect.width - (hasVertical ? vwidth : 0);

            // XXX Check for 0 initial size. This is really indicative
            // of a problem. 
            if (0 == oldsizex)
              mOffsetX = 0;
            else
            {
              mOffsetX = NSIntPixelsToTwips(NSTwipsToIntPixels(nscoord(oldpos), scale), p2t);

              if ((mSizeX - mOffsetX) < availwidth)
              {
                mOffsetX = mSizeX - availwidth;

                if (mOffsetX < 0)
                  mOffsetX = 0;
              }
            }

            dx = NSTwipsToIntPixels((offx - mOffsetX), scale);

            scrollh->SetParameters(mSizeX, availwidth,
                                   mOffsetX, mLineHeight);
          }
        } else {
          // The scrolled view is entirely visible horizontally. Either hide the
          // horizontal scrollbar or disable it
          mOffsetX = 0;
          dx = NSTwipsToIntPixels(offx, scale);

          scrollh->SetPosition(0);  // make sure thumb is all the way to the left

          if (mScrollPref == nsScrollPreference_kAlwaysScroll)
          {
            ((ScrollBarView *)mHScrollBarView)->SetEnabled(PR_TRUE);
            win->Enable(PR_FALSE);
          }
          else
          {
            ((ScrollBarView *)mHScrollBarView)->SetEnabled(PR_FALSE);
            win->Enable(PR_TRUE);
          }
        }

        NS_RELEASE(scrollh);
      }

      NS_RELEASE(win);
    }

    // Adjust the size of the clip view to account for scrollbars that are
    // showing
    if (mHScrollBarView && ViewIsShowing((ScrollBarView *)mHScrollBarView)) {
      controlRect.height -= hheight;
      controlRect.height = PR_MAX(controlRect.height, 0);
    }

    if (mVScrollBarView && ViewIsShowing((ScrollBarView *)mVScrollBarView)) {
      controlRect.width -= vwidth;
      controlRect.width = PR_MAX(controlRect.width, 0);
    }

  
    mClipView->SetDimensions(controlRect.width, controlRect.height, PR_FALSE);

    // Position the scrolled view
    scrolledView->SetPosition(-mOffsetX, -mOffsetY);

    if (mCornerView)
    {
      if (mHScrollBarView && ViewIsShowing((ScrollBarView *)mHScrollBarView) &&
          mVScrollBarView && ViewIsShowing((ScrollBarView *)mVScrollBarView))
        ((CornerView *)mCornerView)->Show(PR_TRUE, PR_FALSE);
      else
        ((CornerView *)mCornerView)->Show(PR_FALSE, PR_FALSE);
    }

    if ((dx != 0) || (dy != 0) && aAdjustWidgets)
      AdjustChildWidgets(this, scrolledView, 0, 0, scale);

    NS_RELEASE(px);
  }
  else
  {
    // There's no scrolled view so hide the scrollbars and corner view
    if (nsnull != mHScrollBarView)
    {
      ((ScrollBarView *)mHScrollBarView)->SetEnabled(PR_FALSE);

      mHScrollBarView->GetWidget(win);
      if (NS_OK == win->QueryInterface(NS_GET_IID(nsIScrollbar), (void **)&scrollh)) {
        scrollh->SetParameters(0, 0, 0, 0);
        NS_RELEASE(scrollh);
      }
      NS_RELEASE(win);
    }

    if (nsnull != mVScrollBarView)
    {
      ((ScrollBarView *)mVScrollBarView)->SetEnabled(PR_FALSE);

      mVScrollBarView->GetWidget(win);
      if (NS_OK == win->QueryInterface(NS_GET_IID(nsIScrollbar), (void **)&scrollv))
      {
        scrollv->SetParameters(0, 0, 0, 0);
        NS_RELEASE(scrollv);
      }
      NS_RELEASE(win);
    }

    if (nsnull != mCornerView)
      ((CornerView *)mCornerView)->Show(PR_FALSE, PR_FALSE);

    mOffsetX = mOffsetY = 0;
    mSizeX = mSizeY = 0;
  }

  UpdateScrollControls(PR_TRUE);
*/
  return NS_OK;
}

NS_IMETHODIMP nsScrollPortView::GetContainerSize(nscoord *aWidth, nscoord *aHeight) const
{
  GetDimensions(aWidth, aHeight);

  return NS_OK;
}

NS_IMETHODIMP nsScrollPortView::ShowQuality(PRBool aShow)
{
  //((CornerView *)mCornerView)->ShowQuality(aShow);
  NS_WARNING("nsScrollPortView::ShowQuality() not implemented");
  return NS_OK;
}

NS_IMETHODIMP nsScrollPortView::GetShowQuality(PRBool &aShow) const
{
  //aShow = ((CornerView *)mCornerView)->mShowQuality;
  NS_ERROR("nsScrollPortView::GetShowQuality() ot implemented");
  return NS_OK;
}

NS_IMETHODIMP nsScrollPortView::SetQuality(nsContentQuality aQuality)
{
  //((CornerView *)mCornerView)->SetQuality(aQuality);
  NS_WARNING("nsScrollPortView::SetQuality() ot implemented");
  return NS_OK;
}

NS_IMETHODIMP nsScrollPortView::SetScrollPreference(nsScrollPreference aPref)
{
  NS_WARNING("nsScrollPortView::SetScrollPreference() not supported");
  return NS_OK;
}

NS_IMETHODIMP nsScrollPortView::GetScrollPreference(nsScrollPreference &aScrollPreference) const
{
  return nsScrollPreference_kNeverScroll;
}

// XXX doesn't smooth scroll

NS_IMETHODIMP nsScrollPortView::ScrollTo(nscoord aX, nscoord aY, PRUint32 aUpdateFlags)
{

    // do nothing if the we aren't scrolling.
    if (aX == mOffsetX && aY == mOffsetY)
        return NS_OK;

	nsSize            scrolledSize;
    

	PRInt32           dxPx = 0, dyPx = 0;
    nsIView           *scrolledView;

    // convert to pixels
    nsIDeviceContext  *dev;
	float             t2p;
	float             p2t;
 
	// notify the listeners.
	PRUint32 listenerCount;
        const nsIID& kScrollPositionListenerIID = NS_GET_IID(nsIScrollPositionListener);
	nsIScrollPositionListener* listener;
	if (nsnull != mListeners) {
		if (NS_SUCCEEDED(mListeners->Count(&listenerCount))) {
			for (PRUint32 i = 0; i < listenerCount; i++) {
				if (NS_SUCCEEDED(mListeners->QueryElementAt(i, kScrollPositionListenerIID, (void**)&listener))) {
					listener->ScrollPositionWillChange(this, aX, aY);
					NS_RELEASE(listener);
				}
			}
		}
	}

    mViewManager->GetDeviceContext(dev);
	dev->GetAppUnitsToDevUnits(t2p); 
	dev->GetDevUnitsToAppUnits(p2t);
  
  	NS_RELEASE(dev);

	// Update the scrolled view's position

    // make sure the new position in in bounds
	GetScrolledView(scrolledView);
	scrolledView->GetDimensions(&scrolledSize.width, &scrolledSize.height);

    nsSize portSize;
	GetDimensions(&portSize.width, &portSize.height);

    nscoord maxX = scrolledSize.width - portSize.width;
    nscoord maxY = scrolledSize.height - portSize.height;

    if (aX > maxX)
        aX = maxX;

    if (aY > maxY)
        aY = maxY;

    if (aX < 0)
        aX = 0;

    if (aY < 0)
        aY = 0;

    // convert aX and aY in pixels
    nscoord aXpx = NSTwipsToIntPixels(aX, t2p);
    nscoord aYpx = NSTwipsToIntPixels(aY, t2p);

    // figure out the diff by comparing old pos to new
    dxPx = mOffsetXpx - aXpx;
    dyPx = mOffsetYpx - aYpx;
    
	if (nsnull != scrolledView)
	{
        // move the scrolled view to the new location
		scrolledView->SetPosition(-aX, -aY);

        // store old position in pixels. We need to do this to make sure there is no
        // round off errors. This could cause weird scrolling.
		mOffsetXpx = aXpx;
		mOffsetYpx = aYpx;

        // store the new position
        mOffsetX = aX;
        mOffsetY = aY;
	}

    Scroll(scrolledView, dxPx, dyPx, t2p, 0);

	// notify the listeners.
	if (nsnull != mListeners) {
		if (NS_SUCCEEDED(mListeners->Count(&listenerCount))) {
			for (PRUint32 i = 0; i < listenerCount; i++) {
				if (NS_SUCCEEDED(mListeners->QueryElementAt(i, kScrollPositionListenerIID, (void**)&listener))) {
					listener->ScrollPositionDidChange(this, aX, aY);
					NS_RELEASE(listener);
				}
			}
		}
	}

	return NS_OK;
}

NS_IMETHODIMP nsScrollPortView::SetControlInsets(const nsMargin &aInsets)
{
    NS_WARNING("nsScrollPortView::SetControlInsets() not implemented");
    return NS_OK;
}

NS_IMETHODIMP nsScrollPortView::GetControlInsets(nsMargin &aInsets) const
{
  NS_ERROR("nsScrollPortView::GetControlInsets() not implemented");
  return NS_OK;
}

NS_IMETHODIMP nsScrollPortView::GetScrollbarVisibility(PRBool *aVerticalVisible,
                                                        PRBool *aHorizontalVisible) const
{ 
  NS_ERROR("nsScrollPortView::GetScrollbarVisibility() not implemented");
  return NS_OK;
}


void nsScrollPortView::AdjustChildWidgets(nsScrollPortView *aScrolling, nsIView *aView, nscoord aDx, nscoord aDy, float scale)
{

  PRInt32           numkids;
  aView->GetChildCount(numkids);
  nscoord           offx, offy;
 // PRBool            isscroll = PR_FALSE;

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

#if 0
      win->BeginResizingChildren();
#endif
      kid->GetBounds(bounds);

      /*
      if (!isscroll ||
          (isscroll &&
          (kid != ((nsScrollPortView *)aView)->mVScrollBarView) &&
          (kid != ((nsScrollPortView *)aView)->mHScrollBarView)))
          */
        win->Move(NSTwipsToIntPixels((bounds.x + aDx), scale), NSTwipsToIntPixels((bounds.y + aDy), scale));
  //    else
  //      win->Move(NSTwipsToIntPixels((bounds.x + aDx + offx), scale), NSTwipsToIntPixels((bounds.y + aDy + offy), scale));
    }

    // Don't recurse if the view has a widget, because we adjusted the view's
    // widget position, and its child widgets are relative to its positon
    if (nsnull == win)
      AdjustChildWidgets(aScrolling, kid, aDx, aDy, scale);

    if (nsnull != win)
    {
#if 0
      win->EndResizingChildren();
#endif
      NS_RELEASE(win);
    }
  }

}


NS_IMETHODIMP nsScrollPortView::SetScrolledView(nsIView *aScrolledView)
{
  return mViewManager->InsertChild(this, aScrolledView, 0);
}

NS_IMETHODIMP nsScrollPortView::GetScrolledView(nsIView *&aScrolledView) const
{
  return GetChild(0, aScrolledView);
}

NS_IMETHODIMP nsScrollPortView::GetScrollPosition(nscoord &aX, nscoord &aY) const
{
  aX = mOffsetX;
  aY = mOffsetY;

  return NS_OK;
}

NS_IMETHODIMP nsScrollPortView::SetScrollProperties(PRUint32 aProperties)
{
  mScrollProperties = aProperties;
  return NS_OK;
}

NS_IMETHODIMP nsScrollPortView::GetScrollProperties(PRUint32 *aProperties)
{
  *aProperties = mScrollProperties;
  return NS_OK;
}

NS_IMETHODIMP nsScrollPortView::SetLineHeight(nscoord aHeight)
{
  mLineHeight = aHeight;
  return NS_OK;
}

NS_IMETHODIMP nsScrollPortView::GetLineHeight(nscoord *aHeight)
{
  *aHeight = mLineHeight;
  return NS_OK;
}

NS_IMETHODIMP nsScrollPortView::ScrollByLines(PRInt32 aNumLines)
{
    nscoord dy = mLineHeight*aNumLines;

    ScrollTo(mOffsetX, mOffsetY + dy, 0);

	return NS_OK;
}

NS_IMETHODIMP nsScrollPortView::ScrollByPages(PRInt32 aNumPages)
{
    nsSize size;
	GetDimensions(&size.width, &size.height);
    
    // scroll 80% of the window
    nscoord dy = nscoord(float(size.height)*0.8);

    // put in the number of pages.
    dy *= aNumPages;

    ScrollTo(mOffsetX, mOffsetY + dy, 0);
    

	return NS_OK;
}

NS_IMETHODIMP nsScrollPortView::ScrollByWhole(PRBool aTop)
{
  nscoord   newPos = 0;

  if (!aTop) {
    nsSize scrolledSize;
    nsIView* scrolledView = nsnull;
    GetScrolledView(scrolledView);
	scrolledView->GetDimensions(&scrolledSize.width, &scrolledSize.height);
    newPos = scrolledSize.height;
  }

	ScrollTo(0, newPos, 0);

	return NS_OK;
}

PRBool nsScrollPortView::CannotBitBlt(nsIView* aScrolledView)
{
  
  PRBool    trans;
  float     opacity;
  PRUint32  scrolledViewFlags;

  HasTransparency(trans);
  GetOpacity(opacity);
  aScrolledView->GetViewFlags(&scrolledViewFlags);

  return ((trans || opacity) && !(mScrollProperties & NS_SCROLL_PROPERTY_ALWAYS_BLIT)) ||
         (mScrollProperties & NS_SCROLL_PROPERTY_NEVER_BLIT) ||
         (scrolledViewFlags & NS_VIEW_PUBLIC_FLAG_DONT_BITBLT);
}


void nsScrollPortView::Scroll(nsIView *aScrolledView, PRInt32 aDx, PRInt32 aDy, float scale, PRUint32 aUpdateFlags)
{
  if ((aDx != 0) || (aDy != 0))
  {
    
    nsIWidget *scrollWidget = nsnull;

    GetWidget(scrollWidget);
    
    if (nsnull == scrollWidget)
    {
      // if we don't have a scroll widget then we must just update.
      mViewManager->UpdateView(this, 0);

    } else if (CannotBitBlt(aScrolledView)) {
        // we can't blit for some reason just update the view and adjust any heavy weight widgets
        mViewManager->UpdateView(this, 0);
        AdjustChildWidgets(this, aScrolledView, 0, 0, scale);
    } else { // if we can blit and have a scrollwidget then scroll.
      // Scroll the contents of the widget by the specfied amount, and scroll
      // the child widgets
      scrollWidget->Scroll(aDx, aDy, nsnull);
    }
    
    NS_IF_RELEASE(scrollWidget);
  }
}

NS_IMETHODIMP nsScrollPortView::Paint(nsIRenderingContext& rc, const nsRect& rect,
                                  PRUint32 aPaintFlags, PRBool &aResult)
{
    
    nsSize size;

    //PRBool clipState;


    // Clip so we don't render outside the inner rect
    //rc.PushState();

    GetDimensions(&size.width, &size.height);

    //rc.SetClipRect(nsRect(0,0,size.width, size.height), nsClipCombine_kIntersect, clipState);
    

    //rc.SetColor(NS_RGB(0,255,0));
   // rc.FillRect(0,0,size.width,size.height);
    nsresult rv = nsView::Paint(rc, rect, aPaintFlags, aResult);

    /*
    rc.PopState(clipState);


    */
    return rv;
}
