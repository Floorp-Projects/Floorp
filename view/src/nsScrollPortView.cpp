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
 *   Neil Cronin (neil@rackle.com)
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

#include "nsScrollPortView.h"
#include "nsIWidget.h"
#include "nsUnitConversion.h"
#include "nsIDeviceContext.h"
#include "nsGUIEvent.h"
#include "nsWidgetsCID.h"
#include "nsViewsCID.h"
#include "nsIScrollableView.h"
#include "nsILookAndFeel.h"
#include "nsISupportsArray.h"
#include "nsIScrollPositionListener.h"
#include "nsIRegion.h"
#include "nsViewManager.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsCOMPtr.h"
#include "nsIServiceManagerUtils.h"

#include <math.h>

static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);

#define SMOOTH_SCROLL_MSECS_PER_FRAME 10
#define SMOOTH_SCROLL_FRAMES    10

#define SMOOTH_SCROLL_PREF_NAME "general.smoothScroll"

class SmoothScroll {
public:
  SmoothScroll() {}
  ~SmoothScroll() {
    if (mScrollAnimationTimer) mScrollAnimationTimer->Cancel();
  }

  nsCOMPtr<nsITimer> mScrollAnimationTimer;
  PRInt32 mVelocities[SMOOTH_SCROLL_FRAMES*2];
  PRInt32 mFrameIndex;
  nscoord mDestinationX;
  nscoord mDestinationY;
};

nsScrollPortView::nsScrollPortView()
{
  mOffsetX = mOffsetY = 0;
  mOffsetXpx = mOffsetYpx = 0;
  mLineHeight = NSIntPointsToTwips(12);

  mListeners = nsnull;
  mSmoothScroll = nsnull;

  SetClipChildrenToBounds(PR_TRUE);
}

nsScrollPortView::~nsScrollPortView()
{    
  if (nsnull != mListeners) {
    mListeners->Clear();
    NS_RELEASE(mListeners);
  }

  if (nsnull != mViewManager) {
     nsIScrollableView* scrollingView;
     mViewManager->GetRootScrollableView(&scrollingView);
     if ((nsnull != scrollingView) && (this == scrollingView)) {
       mViewManager->SetRootScrollableView(nsnull);
     }
  }

  delete mSmoothScroll;
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

NS_IMETHODIMP_(nsIView*) nsScrollPortView::View()
{
  return this;
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

NS_IMETHODIMP nsScrollPortView::CreateScrollControls(nsNativeWidget aNative)
{
  nsWidgetInitData  initData;
  initData.clipChildren = PR_TRUE;
  initData.clipSiblings = PR_TRUE;

  CreateWidget(kWidgetCID, &initData,
               mWindow ? nsnull : aNative);
  
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
  return NS_OK;
}

NS_IMETHODIMP nsScrollPortView::GetContainerSize(nscoord *aWidth, nscoord *aHeight) const
{
  if (!aWidth || !aHeight)
    return NS_ERROR_NULL_POINTER;

  *aWidth  = 0;
  *aHeight = 0;

  nsView *scrolledView = GetScrolledView();

  if (!scrolledView)
    return NS_ERROR_FAILURE;

  nsSize sz;
  scrolledView->GetDimensions(sz);
  *aWidth = sz.width;
  *aHeight = sz.height;
  return NS_OK;
}

static void ComputeVelocities(PRInt32 aCurVelocity, nscoord aCurPos, nscoord aDstPos,
                              PRInt32* aVelocities, float aT2P, float aP2T) {
  // scrolling always works in units of whole pixels. So compute velocities
  // in pixels and then scale them up. This ensures, for example, that
  // a 1-pixel scroll isn't broken into N frames of 1/N pixels each, each
  // frame increment being rounded to 0 whole pixels.
  aCurPos = NSTwipsToIntPixels(aCurPos, aT2P);
  aDstPos = NSTwipsToIntPixels(aDstPos, aT2P);

  PRInt32 i;
  PRInt32 direction = (aCurPos < aDstPos ? 1 : -1);
  PRInt32 absDelta = (aDstPos - aCurPos)*direction;
  PRInt32 baseVelocity = absDelta/SMOOTH_SCROLL_FRAMES;

  for (i = 0; i < SMOOTH_SCROLL_FRAMES; i++) {
    aVelocities[i*2] = baseVelocity;
  }
  nscoord total = baseVelocity*SMOOTH_SCROLL_FRAMES;
  for (i = 0; i < SMOOTH_SCROLL_FRAMES; i++) {
    if (total < absDelta) {
      aVelocities[i*2]++;
      total++;
    }
  }
  NS_ASSERTION(total == absDelta, "Invalid velocity sum");

  PRInt32 scale = direction*((PRInt32)aP2T);
  for (i = 0; i < SMOOTH_SCROLL_FRAMES; i++) {
    aVelocities[i*2] *= scale;
  }
}
  
static nsresult ClampScrollValues(nscoord& aX, nscoord& aY, nsScrollPortView* aThis) {
  // make sure the new position in in bounds
  nsView* scrolledView = aThis->GetScrolledView();
  if (!scrolledView) return NS_ERROR_FAILURE;
  
  nsSize scrolledSize;
  scrolledView->GetDimensions(scrolledSize);
  
  nsSize portSize;
  aThis->GetDimensions(portSize);
  
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
  
  return NS_OK;
}
  
/*
 * this method wraps calls to ScrollToImpl(), either in one shot or incrementally,
 *  based on the setting of the smooth scroll pref
 */
NS_IMETHODIMP nsScrollPortView::ScrollTo(nscoord aDestinationX, nscoord aDestinationY,
                                         PRUint32 aUpdateFlags)
{
  // do nothing if the we aren't scrolling.
  if (aDestinationX == mOffsetX && aDestinationY == mOffsetY) {
    // kill any in-progress smooth scroll
    delete mSmoothScroll;
    mSmoothScroll = nsnull;
    return NS_OK;
  }
  
  if ((aUpdateFlags & NS_VMREFRESH_SMOOTHSCROLL) == 0
      || !IsSmoothScrollingEnabled()) {
    // Smooth scrolling is not allowed, so we'll kill any existing smooth-scrolling process
    // and do an instant scroll
    delete mSmoothScroll;
    mSmoothScroll = nsnull;
    return ScrollToImpl(aDestinationX, aDestinationY, aUpdateFlags);
  }

  PRInt32 currentVelocityX;
  PRInt32 currentVelocityY;

  if (mSmoothScroll) {
    currentVelocityX = mSmoothScroll->mVelocities[mSmoothScroll->mFrameIndex*2];
    currentVelocityY = mSmoothScroll->mVelocities[mSmoothScroll->mFrameIndex*2 + 1];
  } else {
    currentVelocityX = 0;
    currentVelocityY = 0;

    mSmoothScroll = new SmoothScroll;
    if (mSmoothScroll) {
      mSmoothScroll->mScrollAnimationTimer = do_CreateInstance("@mozilla.org/timer;1");
      if (!mSmoothScroll->mScrollAnimationTimer) {
        delete mSmoothScroll;
        mSmoothScroll = nsnull;
      }
    }
    if (!mSmoothScroll) {
      // some allocation failed. Scroll the normal way.
      return ScrollToImpl(aDestinationX, aDestinationY, aUpdateFlags);
    }
    mSmoothScroll->mScrollAnimationTimer->InitWithFuncCallback(
      SmoothScrollAnimationCallback, this, SMOOTH_SCROLL_MSECS_PER_FRAME,
      nsITimer::TYPE_REPEATING_PRECISE);
    mSmoothScroll->mDestinationX = mOffsetX;
    mSmoothScroll->mDestinationY = mOffsetY;
  }

  // need to store these so we know when to stop scrolling
  // Treat the desired scroll destination as an offset
  // relative to the current position. This makes things
  // work when someone starts a smooth scroll
  // while an existing smooth scroll has not yet been
  // completed.
  mSmoothScroll->mDestinationX += aDestinationX - mOffsetX;
  mSmoothScroll->mDestinationY += aDestinationY - mOffsetY;
  mSmoothScroll->mFrameIndex = 0;
  ClampScrollValues(mSmoothScroll->mDestinationX, mSmoothScroll->mDestinationY, this);

  nsCOMPtr<nsIDeviceContext> dev;
  mViewManager->GetDeviceContext(*getter_AddRefs(dev));
  float p2t, t2p;
  p2t = dev->DevUnitsToAppUnits(); 
  t2p = dev->AppUnitsToDevUnits();

  // compute velocity vectors
  ComputeVelocities(currentVelocityX, mOffsetX,
                    mSmoothScroll->mDestinationX, mSmoothScroll->mVelocities,
                    t2p, p2t);
  ComputeVelocities(currentVelocityY, mOffsetY,
                    mSmoothScroll->mDestinationY, mSmoothScroll->mVelocities + 1,
                    t2p, p2t);

  return NS_OK;
}

NS_IMETHODIMP nsScrollPortView::GetScrollbarVisibility(PRBool *aVerticalVisible,
                                                       PRBool *aHorizontalVisible) const
{ 
  NS_WARNING("Attempt to get scrollbars visibility this is not xp!");
  return NS_OK;
}

static void AdjustChildWidgets(nsView *aView,
  nsPoint aWidgetToParentViewOrigin, float aScale, PRBool aInvalidate)
{
  if (aView->HasWidget()) {
    nsIWidget* widget = aView->GetWidget();
    nsWindowType type;
    widget->GetWindowType(type);
    if (type != eWindowType_popup) {
      nsRect bounds = aView->GetBounds();
      nsPoint widgetOrigin = aWidgetToParentViewOrigin
        + nsPoint(bounds.x, bounds.y);
      widget->Move(NSTwipsToIntPixels(widgetOrigin.x, aScale),
                   NSTwipsToIntPixels(widgetOrigin.y, aScale));
      if (aInvalidate) {
        widget->Invalidate(PR_FALSE);
      }
    }
  } else {
    // Don't recurse if the view has a widget, because we adjusted the view's
    // widget position, and its child widgets are relative to its positon
    nsPoint widgetToViewOrigin = aWidgetToParentViewOrigin
      + aView->GetPosition();

    for (nsView* kid = aView->GetFirstChild(); kid; kid = kid->GetNextSibling())
    {
      AdjustChildWidgets(kid, widgetToViewOrigin, aScale, aInvalidate);
    }
  }
}


NS_IMETHODIMP nsScrollPortView::SetScrolledView(nsIView *aScrolledView)
{
  NS_ASSERTION(GetFirstChild() == nsnull || GetFirstChild()->GetNextSibling() == nsnull,
               "Error scroll port has too many children");

  // if there is already a child so remove it
  if (GetFirstChild() != nsnull)
  {
    mViewManager->RemoveChild(GetFirstChild());
  }

  return mViewManager->InsertChild(this, aScrolledView, 0);
}

NS_IMETHODIMP nsScrollPortView::GetScrolledView(nsIView *&aScrolledView) const
{
  aScrolledView = GetScrolledView();
  return NS_OK;
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

NS_IMETHODIMP nsScrollPortView::ScrollByLines(PRInt32 aNumLinesX, PRInt32 aNumLinesY)
{
  nscoord dx = mLineHeight*aNumLinesX;
  nscoord dy = mLineHeight*aNumLinesY;

  return ScrollTo(mOffsetX + dx, mOffsetY + dy, NS_VMREFRESH_SMOOTHSCROLL);
}

NS_IMETHODIMP nsScrollPortView::ScrollByPages(PRInt32 aNumPagesX, PRInt32 aNumPagesY)
{
  nsSize size;
  GetDimensions(size);
    
  // scroll % of the window
  nscoord dx = nscoord(float(size.width)*PAGE_SCROLL_PERCENT);
  nscoord dy = nscoord(float(size.height)*PAGE_SCROLL_PERCENT);

  // put in the number of pages.
  dx *= aNumPagesX;
  dy *= aNumPagesY;

  return ScrollTo(mOffsetX + dx, mOffsetY + dy, NS_VMREFRESH_SMOOTHSCROLL);
}

NS_IMETHODIMP nsScrollPortView::ScrollByWhole(PRBool aTop)
{
  nscoord   newPos = 0;

  if (!aTop) {
    nsSize scrolledSize;
    nsView* scrolledView = GetScrolledView();
    scrolledView->GetDimensions(scrolledSize);
    newPos = scrolledSize.height;
  }

  ScrollTo(0, newPos, 0);

  return NS_OK;
}

PRBool nsScrollPortView::CannotBitBlt(nsView* aScrolledView)
{
  PRUint32  scrolledViewFlags = aScrolledView->GetViewFlags();

  return (mScrollProperties & NS_SCROLL_PROPERTY_NEVER_BLIT) ||
         (scrolledViewFlags & NS_VIEW_FLAG_DONT_BITBLT) ||
         (!(mScrollProperties & NS_SCROLL_PROPERTY_ALWAYS_BLIT) && 
          !mViewManager->CanScrollWithBitBlt(aScrolledView));
}


void nsScrollPortView::Scroll(nsView *aScrolledView, PRInt32 aDx, PRInt32 aDy, float scale, PRUint32 aUpdateFlags)
{
  if ((aDx != 0) || (aDy != 0))
  {
    // Since we keep track of the dirty region as absolute screen coordintes,
    // we need to offset it by the amount we scrolled.
    nsCOMPtr<nsIRegion> dirtyRegion;
    GetDirtyRegion(*getter_AddRefs(dirtyRegion));
    dirtyRegion->Offset(aDx, aDy);

    nsIWidget *scrollWidget = GetWidget();
 
    if (!scrollWidget)
    {
      nsPoint offsetToWidget;
      GetNearestWidget(&offsetToWidget);
      // We're moving the child widgets because we are scrolling. But
      // the child widgets may stick outside our bounds, so their area
      // may include area that's not supposed to be scrolled. We need
      // to invalidate to ensure that any such area is properly
      // repainted back to the right rendering.
      AdjustChildWidgets(aScrolledView, offsetToWidget, scale, PR_TRUE);
      // If we don't have a scroll widget then we must just update.
      // We should call this after fixing up the widget positions to be
      // consistent with the view hierarchy.
      mViewManager->UpdateView(this, 0);
    } else if (CannotBitBlt(aScrolledView)) {
      // We can't blit for some reason.
      // Just update the view and adjust widgets
      // Recall that our widget's origin is at our bounds' top-left
      nsRect bounds(GetBounds());
      nsPoint topLeft(bounds.x, bounds.y);
      AdjustChildWidgets(aScrolledView,
                         GetPosition() - topLeft, scale, PR_FALSE);
      // We should call this after fixing up the widget positions to be
      // consistent with the view hierarchy.
      mViewManager->UpdateView(this, 0);
    } else { // if we can blit and have a scrollwidget then scroll.
      // Scroll the contents of the widget by the specfied amount, and scroll
      // the child widgets
      scrollWidget->Scroll(aDx, aDy, nsnull);
      mViewManager->UpdateViewAfterScroll(this, aDx, aDy);
    }
  }
}

NS_IMETHODIMP nsScrollPortView::Paint(nsIRenderingContext& rc, const nsRect& rect,
                                      PRUint32 aPaintFlags, PRBool &aResult)
{
  rc.PushState();
  nsRect bounds;
  GetDimensions(bounds);
  bounds.x = bounds.y = 0;
  rc.SetClipRect(bounds, nsClipCombine_kIntersect);

  nsresult rv = nsView::Paint(rc, rect, aPaintFlags, aResult);

  rc.PopState();
    
  return rv;
}

NS_IMETHODIMP nsScrollPortView::Paint(nsIRenderingContext& aRC, const nsIRegion& aRegion,
                                      PRUint32 aPaintFlags, PRBool &aResult)
{
  aRC.PushState();
  nsRect bounds;
  GetDimensions(bounds);
  bounds.x = bounds.y = 0;
  aRC.SetClipRect(bounds, nsClipCombine_kIntersect);

  nsresult rv = nsView::Paint(aRC, aRegion, aPaintFlags, aResult);

  aRC.PopState();
    
  return rv;
}

NS_IMETHODIMP nsScrollPortView::ScrollToImpl(nscoord aX, nscoord aY, PRUint32 aUpdateFlags)
{
  PRInt32           dxPx = 0, dyPx = 0;
  
  // convert to pixels
  nsIDeviceContext  *dev;
  float             t2p;
  float             p2t;

  mViewManager->GetDeviceContext(dev);
  t2p = dev->AppUnitsToDevUnits(); 
  p2t = dev->DevUnitsToAppUnits();
  
  NS_RELEASE(dev);

  // Update the scrolled view's position
  nsresult rv = ClampScrollValues(aX, aY, this);
  if (NS_FAILED(rv)) {
    return rv;
  }
  
  // convert aX and aY in pixels
  nscoord aXpx = NSTwipsToIntPixels(aX, t2p);
  nscoord aYpx = NSTwipsToIntPixels(aY, t2p);
  
  aX = NSIntPixelsToTwips(aXpx,p2t);
  aY = NSIntPixelsToTwips(aYpx,p2t);
  
  // do nothing if the we aren't scrolling.
  // this needs to be rechecked because of the clamping and
  // rounding
  if (aX == mOffsetX && aY == mOffsetY) {
    return NS_OK;
  }

  // figure out the diff by comparing old pos to new
  dxPx = mOffsetXpx - aXpx;
  dyPx = mOffsetYpx - aYpx;

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
  
  nsView* scrolledView = GetScrolledView();
  if (!scrolledView) return NS_ERROR_FAILURE;

  // move the scrolled view to the new location
  // Note that child widgets may be scrolled by the native widget scrolling,
  // so don't update their positions
  scrolledView->SetPositionIgnoringChildWidgets(-aX, -aY);
      
  // store old position in pixels. We need to do this to make sure there is no
  // round off errors. This could cause weird scrolling.
  mOffsetXpx = aXpx;
  mOffsetYpx = aYpx;
      
  // store the new position
  mOffsetX = aX;
  mOffsetY = aY;
  
  Scroll(scrolledView, dxPx, dyPx, t2p, 0);

  mViewManager->SynthesizeMouseMove(PR_TRUE);
  
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

/************************
 *
 * smooth scrolling methods
 *
 ***********************/

PRBool nsScrollPortView::IsSmoothScrollingEnabled() {
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    PRBool enabled;
    nsresult rv = prefs->GetBoolPref(SMOOTH_SCROLL_PREF_NAME, &enabled);
    if (NS_SUCCEEDED(rv)) {
      return enabled;
    }
  }
  return PR_FALSE;
}

/*
 * Callback function from timer used in nsScrollPortView::DoSmoothScroll
 * this cleans up the target coordinates and incrementally calls 
 * nsScrollPortView::ScrollTo
 */
void
nsScrollPortView::SmoothScrollAnimationCallback (nsITimer *aTimer, void* anInstance) 
{
  nsScrollPortView* self = NS_STATIC_CAST(nsScrollPortView*, anInstance);
  if (self) {
    self->IncrementalScroll();
  }
}

/*
 * manages data members and calls to ScrollTo from the (static) SmoothScrollAnimationCallback method
 */ 
void
nsScrollPortView::IncrementalScroll()
{
  if (!mSmoothScroll) {
    return;
  }

  if (mSmoothScroll->mFrameIndex < SMOOTH_SCROLL_FRAMES) {
    ScrollToImpl(mOffsetX + mSmoothScroll->mVelocities[mSmoothScroll->mFrameIndex*2],
                 mOffsetY + mSmoothScroll->mVelocities[mSmoothScroll->mFrameIndex*2 + 1],
                 0);
    mSmoothScroll->mFrameIndex++;
  } else {
    delete mSmoothScroll;
    mSmoothScroll = nsnull;
  }
}
