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

#include "nsViewManager.h"
#include "nsUnitConversion.h"
#include "nsIPresShell.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsGfxCIID.h"
#include "nsIScrollableView.h"
#include "nsIRegion.h"
#include "nsView.h"

static NS_DEFINE_IID(kIScrollableViewIID, NS_ISCROLLABLEVIEW_IID);

static const PRBool gsDebug = PR_FALSE;

#define UPDATE_QUANTUM  1000 / 40

//#define NO_DOUBLE_BUFFER

static void vm_timer_callback(nsITimer *aTimer, void *aClosure)
{
  nsViewManager *vm = (nsViewManager *)aClosure;

  //restart the timer
  
  PRUint32 fr;
  vm->GetFrameRate(fr);

  vm->mFrameRate = 0;
  vm->SetFrameRate(fr);
//printf("timer composite...\n");
#ifndef XP_MAC
	//XXX temporary: The Mac doesn't need the timer to repaint but
	// obviously this is not the good method to disable the thing.
	// It's that way for now because the proper solutions
	// (set UPDATE_QUANTUM to 0, or simply not create the timer)
	// don't work for now. We'll fix that and then disable the
	// Mac timers as we should.
  vm->Composite();
#endif
}

PRUint32 nsViewManager::mVMCount = 0;
nsDrawingSurface nsViewManager::mDrawingSurface = nsnull;
nsRect nsViewManager::mDSBounds = nsRect(0, 0, 0, 0);

static NS_DEFINE_IID(knsViewManagerIID, NS_IVIEWMANAGER_IID);

nsViewManager :: nsViewManager()
{
  mVMCount++;
}

nsViewManager :: ~nsViewManager()
{
  if (nsnull != mTimer)
  {
    mTimer->Cancel();     //XXX this should not be necessary. MMP
    NS_RELEASE(mTimer);
  }

  NS_IF_RELEASE(mRootWindow);

  --mVMCount;

  NS_ASSERTION(!(mVMCount < 0), "underflow of viewmanagers");

  if ((0 == mVMCount) && (nsnull != mDrawingSurface))
  {
    nsIRenderingContext *rc;

    static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
    static NS_DEFINE_IID(kIRenderingContextIID, NS_IRENDERING_CONTEXT_IID);

    nsresult rv = nsRepository::CreateInstance(kRenderingContextCID, 
                                       nsnull, 
                                       kIRenderingContextIID, 
                                       (void **)&rc);

    if (NS_OK == rv)
    {
      rc->DestroyDrawingSurface(mDrawingSurface);
      NS_RELEASE(rc);
    }
    
    mDrawingSurface = nsnull;
  }

  mObserver = nsnull;
  mContext = nsnull;
}

NS_IMPL_QUERY_INTERFACE(nsViewManager, knsViewManagerIID)

nsrefcnt nsViewManager::AddRef(void)
{
  return ++mRefCnt;
}

nsrefcnt nsViewManager::Release(void)
{
  mRefCnt--;

  if (mRefCnt == 0)
  {
    if (nsnull != mRootView) {
      // Destroy any remaining views
      mRootView->Destroy();
      mRootView = nsnull;
    }
    delete this;
    return 0;
  }
  return mRefCnt;
}

// We don't hold a reference to the presentation context because it
// holds a reference to us.
NS_IMETHODIMP nsViewManager :: Init(nsIDeviceContext* aContext)
{
  nsresult rv;

  NS_PRECONDITION(nsnull != aContext, "null ptr");

  if (nsnull == aContext) {
    return NS_ERROR_NULL_POINTER;
  }
  if (nsnull != mContext) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }
  mContext = aContext;

  mDSBounds.Empty();
  mTimer = nsnull;
  mFrameRate = 0;
  mTransCnt = 0;

  rv = SetFrameRate(UPDATE_QUANTUM);

  mLastRefresh = PR_IntervalNow();

  mRefreshEnabled = PR_TRUE;

  mMouseGrabber = nsnull;
  mKeyGrabber = nsnull;

  return rv;
}

NS_IMETHODIMP nsViewManager :: GetRootView(nsIView *&aView)
{
  aView = mRootView;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: SetRootView(nsIView *aView)
{
  UpdateTransCnt(mRootView, aView);
  // Do NOT destroy the current root view. It's the caller's responsibility
  // to destroy it
  mRootView = aView;

  //now get the window too.
  NS_IF_RELEASE(mRootWindow);

  if (nsnull != mRootView)
    mRootView->GetWidget(mRootWindow);

  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: GetFrameRate(PRUint32 &aRate)
{
  aRate = mFrameRate;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: SetFrameRate(PRUint32 aFrameRate)
{
  nsresult  rv;

  if (aFrameRate != mFrameRate)
  {
    NS_IF_RELEASE(mTimer);

    mFrameRate = aFrameRate;

    if (mFrameRate != 0)
    {
      rv = NS_NewTimer(&mTimer);

      if (NS_OK == rv)
        mTimer->Init(vm_timer_callback, this, 1000 / mFrameRate);
    }
    else
      rv = NS_OK;
  }
  else
    rv = NS_OK;

  return rv;
}

NS_IMETHODIMP nsViewManager :: GetWindowDimensions(nscoord *width, nscoord *height)
{
  if (nsnull != mRootView)
    mRootView->GetDimensions(width, height);
  else
  {
    *width = 0;
    *height = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: SetWindowDimensions(nscoord width, nscoord height)
{
  // Resize the root view
  if (nsnull != mRootView)
    mRootView->SetDimensions(width, height);

//printf("new dims: %d %d\n", width, height);
  // Inform the presentation shell that we've been resized
  if (nsnull != mObserver)
    mObserver->ResizeReflow(mRootView, width, height);
//printf("reflow done\n");

  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: ResetScrolling(void)
{
  if (nsnull != mRootView)
  {
    nsIScrollableView *scroller;
    nsresult           retval;

    retval = mRootView->QueryInterface(kIScrollableViewIID, (void **)&scroller);
    if (NS_SUCCEEDED(retval)) {
      scroller->ComputeContainerSize();
    }

    return retval;
  }

  return NS_OK;
}

void nsViewManager :: Refresh(nsIView *aView, nsIRenderingContext *aContext, nsIRegion *region, PRUint32 aUpdateFlags)
{
  nsRect              wrect;
  nsIRenderingContext *localcx = nsnull;
  nsDrawingSurface    ds = nsnull;

  if (PR_FALSE == mRefreshEnabled)
    return;

  NS_ASSERTION(!(PR_TRUE == mPainting), "recursive painting not permitted");

  mPainting = PR_TRUE;

//printf("refreshing region...\n");
  //force double buffering because of non-opaque views?

  if (mTransCnt > 0)
    aUpdateFlags |= NS_VMREFRESH_DOUBLE_BUFFER;

#ifdef NO_DOUBLE_BUFFER
    aUpdateFlags &= ~NS_VMREFRESH_DOUBLE_BUFFER;
#endif

  if (nsnull == aContext)
  {
    localcx = CreateRenderingContext(*aView);

    //couldn't get rendering context. this is ok at init time atleast
    if (nsnull == localcx)
      return;
  }
  else
    localcx = aContext;

  if (aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER)
  {
    nsIWidget*  widget;
    aView->GetWidget(widget);
    widget->GetClientBounds(wrect);
    wrect.x = wrect.y = 0;
    NS_RELEASE(widget);
    ds = GetDrawingSurface(*localcx, wrect);
    localcx->SelectOffScreenDrawingSurface(ds);
  }

  PRBool  result;
  nsRect  trect;

  if (nsnull != region)
    localcx->SetClipRegion(*region, nsClipCombine_kUnion, result);

  aView->GetBounds(trect);

  localcx->SetClipRect(trect, nsClipCombine_kIntersect, result);

  // Paint the view. The clipping rect was set above set don't clip again.
  aView->Paint(*localcx, trect, NS_VIEW_FLAG_CLIP_SET, result);

  if (aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER)
    localcx->CopyOffScreenBits(ds, wrect.x, wrect.y, wrect, NS_COPYBITS_USE_SOURCE_CLIP_REGION);

  if (localcx != aContext)
    NS_RELEASE(localcx);

  // Subtract the area we just painted from the dirty region
  if ((nsnull != region) && !region->IsEmpty())
  {
    nsRect  pixrect = trect;
    float   t2p;

    mContext->GetAppUnitsToDevUnits(t2p);

    pixrect.ScaleRoundIn(t2p);
    region->Subtract(pixrect.x, pixrect.y, pixrect.width, pixrect.height);
  }

  mLastRefresh = PR_IntervalNow();

  mPainting = PR_FALSE;
}

void nsViewManager :: Refresh(nsIView *aView, nsIRenderingContext *aContext, const nsRect *rect, PRUint32 aUpdateFlags)
{
  nsRect              wrect;
  nsIRenderingContext *localcx = nsnull;
  nsDrawingSurface    ds = nsnull;

  if (PR_FALSE == mRefreshEnabled)
    return;

  NS_ASSERTION(!(PR_TRUE == mPainting), "recursive painting not permitted");

  mPainting = PR_TRUE;

  //force double buffering because of non-opaque views?

//printf("refreshing rect... ");
//stdout << *rect;
//printf("\n");
  if (mTransCnt > 0)
    aUpdateFlags |= NS_VMREFRESH_DOUBLE_BUFFER;

#ifdef NO_DOUBLE_BUFFER
    aUpdateFlags &= ~NS_VMREFRESH_DOUBLE_BUFFER;
#endif

  if (nsnull == aContext)
  {
    localcx = CreateRenderingContext(*aView);

    //couldn't get rendering context. this is ok if at startup
    if (nsnull == localcx)
      return;
  }
  else
    localcx = aContext;

  if (aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER)
  {
    nsIWidget*  widget;
    aView->GetWidget(widget);
    widget->GetClientBounds(wrect);
    wrect.x = wrect.y = 0;
    NS_RELEASE(widget);
    ds = GetDrawingSurface(*localcx, wrect);
    localcx->SelectOffScreenDrawingSurface(ds);
  }

  nsRect trect = *rect;

  PRBool  result;

  localcx->SetClipRect(trect, nsClipCombine_kReplace, result);

  // Paint the view. The clipping rect was set above set don't clip again.
  aView->Paint(*localcx, trect, NS_VIEW_FLAG_CLIP_SET, result);

  if (aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER)
    localcx->CopyOffScreenBits(ds, wrect.x, wrect.y, wrect, NS_COPYBITS_USE_SOURCE_CLIP_REGION);

  if (localcx != aContext)
    NS_RELEASE(localcx);

  // Subtract the area we just painted from the dirty region
  nsIRegion *dirtyRegion;
  aView->GetDirtyRegion(dirtyRegion);

  if ((nsnull != dirtyRegion) && !dirtyRegion->IsEmpty())
  {
    nsRect  pixrect = trect;
    float   t2p;

    mContext->GetAppUnitsToDevUnits(t2p);

    pixrect.ScaleRoundIn(t2p);
    dirtyRegion->Subtract(pixrect.x, pixrect.y, pixrect.width, pixrect.height);
    NS_RELEASE(dirtyRegion);
  }

  mLastRefresh = PR_IntervalNow();

  mPainting = PR_FALSE;
}

void nsViewManager :: UpdateDirtyViews(nsIView *aView, nsRect *aParentRect) const
{
  nsRect    pardamage;
  nsRect    bounds;

  aView->GetBounds(bounds);

  //translate parent region into child coords.

  if (nsnull != aParentRect)
  {
    pardamage = *aParentRect;

    pardamage.IntersectRect(bounds, pardamage);
    pardamage.MoveBy(-bounds.x, -bounds.y);
  }
  else
    pardamage = bounds;

  if (PR_FALSE == pardamage.IsEmpty())
  {
    nsIWidget *widget;

    aView->GetWidget(widget);

    if (nsnull != widget)
    {
      float scale;
      nsRect pixrect = pardamage;

      mContext->GetAppUnitsToDevUnits(scale);
      pixrect.ScaleRoundOut(scale);

//printf("invalidating: view %x (pix) %d, %d\n", aView, pixrect.width, pixrect.height);
      widget->Invalidate(pixrect, PR_FALSE);

      NS_RELEASE(widget);
    }
  }

  // Check our child views
  nsIView *child;

  aView->GetChild(0, child);

  while (nsnull != child)
  {
    UpdateDirtyViews(child, &pardamage);
    child->GetNextSibling(child);
  }
}

#if 0

  nsRect    bounds;
  nsIRegion *dirtyRegion, *damager;
  PRInt32   posx, posy;

  aView->GetBounds(bounds);

  //translate parent region into child coords.

  if (nsnull != aParentDamage)
  {
    float     scale;

    mContext->GetAppUnitsToDevUnits(scale);

    posx = NSTwipsToIntPixels(bounds.x, scale);
    posy = NSTwipsToIntPixels(bounds.y, scale);

    aParentDamage->Offset(-posx, -posy);
  }

  // See if the view has a non-empty dirty region

  aView->GetDirtyRegion(dirtyRegion);

  if (nsnull != dirtyRegion)
  {
    if (PR_FALSE == dirtyRegion->IsEmpty())
    {
      if (nsnull != aParentDamage)
        dirtyRegion->Union(*aParentDamage);

      damager = dirtyRegion;
    }
    else
      damager = aParentDamage;
  }
  else
    damager = aParentDamage;

  nsIWidget *widget;

  aView->GetWidget(widget);

  if (nsnull != widget)
  {
    //have we not yet figured out the rootmost
    //view for the damage repair? if not, keep
    //recording widgets as we find them as the
    //topmost widget containing the topmost
    //damaged view.

    if (nsnull == *aTopDamaged)
      *aTopWidget = widget;

    if (nsnull != damager)
    {
      nsRegionComplexity cplx;

      damager->GetRegionComplexity(cplx);

      if (cplx == eRegionComplexity_rect)
      {
        nsRect trect;

        damager->GetBoundingBox(&trect.x, &trect.y, &trect.width, &trect.height);
        widget->Invalidate(trect, PR_FALSE);
      }
      else if (cplx == eRegionComplexity_complex)
        widget->Invalidate(*damager, PR_FALSE);
    }

    NS_RELEASE(widget);
  }

  //record the topmost damaged view

  if ((nsnull == *aTopDamaged) && (damager == dirtyRegion))
    *aTopDamaged = aView;

  // Check our child views
  nsIView *child;

  aView->GetChild(0, child);

  while (nsnull != child)
  {
    UpdateDirtyViews(child, damager, aTopDamaged, aTopWidget);
    child->GetNextSibling(child);
  }

  //translate the parent damage region back to where it used to be

  if (nsnull != aParentDamage)
    aParentDamage->Offset(posx, posy);

  if (nsnull != dirtyRegion)
  {
    // Clear our dirty region

    dirtyRegion->SetTo(0, 0, 0, 0);
    NS_RELEASE(dirtyRegion);
  }

#endif

NS_IMETHODIMP nsViewManager :: Composite()
{
  if (mUpdateCnt > 0)
  {
    if (nsnull != mRootWindow)
      mRootWindow->Update();

    mUpdateCnt = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: UpdateView(nsIView *aView, nsIRegion *aRegion, PRUint32 aUpdateFlags)
{
  // XXX Huh. What about the case where aRegion isn't nsull?
  // XXX yeah? what about it?
  if (aRegion == nsnull)
  {
    nsRect  trect;

    // Mark the entire view as damaged
    aView->GetBounds(trect);
    trect.x = trect.y = 0;
    UpdateView(aView, trect, aUpdateFlags);
  }

  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: UpdateView(nsIView *aView, const nsRect &aRect, PRUint32 aUpdateFlags)
{
  NS_PRECONDITION(nsnull != aView, "null view");

  // Ignore any silly requests...
  if ((aRect.width == 0) || (aRect.height == 0))
    return NS_OK;

  if (gsDebug)
  {
    printf("ViewManager::UpdateView: %x, rect ", aView);
    stdout << aRect;
    printf("\n");
  }

  // Find the nearest view (including this view) that has a widget
  nsRect  trect = aRect;
  nsIView *par = aView;
  nscoord x, y;
  nsIView *widgetView = aView;

  do
  {
    nsIWidget *widget;

    // See if this view has a widget
    widgetView->GetWidget(widget);

    if (nsnull != widget)
    {
      NS_RELEASE(widget);
      break;
    }

    // Get the parent view
    widgetView->GetParent(widgetView);
  } while (nsnull != widgetView);
//  NS_ASSERTION(nsnull != widgetView, "no widget");  this assertion not valid for printing...MMP

  if (nsnull != widgetView)
  {
    mUpdateCnt++;

    // Convert damage rect to coordinate space of containing view with
    // a widget
    // XXX Consolidate this code with the code above...
    if (aView != widgetView)
    {
      do
      {
        par->GetPosition(&x, &y);
        trect.x += x;
        trect.y += y;

        par->GetParent(par);
      }
      while ((nsnull != par) && (par != widgetView));
    }

    // Add this rect to the widgetView's dirty region.

    if (nsnull != widgetView)
      UpdateDirtyViews(widgetView, &trect);

    // See if we should do an immediate refresh or wait

    if (aUpdateFlags & NS_VMREFRESH_IMMEDIATE)
    {
      Composite();
    }
    else if ((mFrameRate > 0) && !(aUpdateFlags & NS_VMREFRESH_NO_SYNC))
    {
      // or if a sync paint is allowed and it's time for the compositor to
      // do a refresh

      PRInt32 deltams = PR_IntervalToMilliseconds(PR_IntervalNow() - mLastRefresh);

      if (deltams > (1000 / (PRInt32)mFrameRate))
        Composite();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: DispatchEvent(nsGUIEvent *aEvent, nsEventStatus &aStatus)
{
  aStatus = nsEventStatus_eIgnore;

  switch(aEvent->message)
  {
    case NS_SIZE:
    {
      nsIView*      view = nsView::GetViewFor(aEvent->widget);

      if (nsnull != view)
      {
        nscoord width = ((nsSizeEvent*)aEvent)->windowSize->width;
        nscoord height = ((nsSizeEvent*)aEvent)->windowSize->height;
        width = ((nsSizeEvent*)aEvent)->mWinWidth;
        height = ((nsSizeEvent*)aEvent)->mWinHeight;

        // The root view may not be set if this is the resize associated with
        // window creation

        if (view == mRootView)
        {
          // Convert from pixels to twips
          float p2t;
          mContext->GetDevUnitsToAppUnits(p2t);

//printf("resize: (pix) %d, %d\n", width, height);
          SetWindowDimensions(NSIntPixelsToTwips(width, p2t),
                              NSIntPixelsToTwips(height, p2t));
          aStatus = nsEventStatus_eConsumeNoDefault;
        }
      }

      break;
    }

    case NS_PAINT:
    {
      nsIView *view = nsView::GetViewFor(aEvent->widget);

      if (nsnull != view)
      {
        // The rect is in device units, and it's in the coordinate space of its
        // associated window.
        nsRect  trect = *((nsPaintEvent*)aEvent)->rect;

        float   p2t;
        mContext->GetDevUnitsToAppUnits(p2t);
        trect.ScaleRoundOut(p2t);

        // Do an immediate refresh
        if (nsnull != mContext)
        {
          nsRect  vrect;
          float   varea;

          // Check that there's actually something to paint
          view->GetBounds(vrect);
          varea = (float)vrect.width * vrect.height;

          if (varea > 0.0000001f)
          {
            nsRect     rrect = trect;
            PRUint32   updateFlags = 0;

            // Auto double buffering logic.
            // See if the paint region is greater than .25 the area of our view.
            // If so, enable double buffered painting.

            rrect.IntersectRect(rrect, vrect);
  
            if ((((float)rrect.width * rrect.height) / varea) >  0.25f)
              updateFlags |= NS_VMREFRESH_DOUBLE_BUFFER;

//printf("refreshing: view: %x, %d, %d, %d, %d\n", view, trect.x, trect.y, trect.width, trect.height);
            // Refresh the view
            Refresh(view, ((nsPaintEvent*)aEvent)->renderingContext, &trect, updateFlags);
          }
        }

        aStatus = nsEventStatus_eConsumeNoDefault;
      }

      break;
    }

    case NS_DESTROY:
      aStatus = nsEventStatus_eConsumeNoDefault;
      break;

    default:
    {
      nsIView* view;
        
      if (nsnull != mMouseGrabber && NS_IS_MOUSE_EVENT(aEvent))
        view = mMouseGrabber;
      else if (nsnull != mKeyGrabber && NS_IS_KEY_EVENT(aEvent))
        view = mKeyGrabber;
      else
        view = nsView::GetViewFor(aEvent->widget);

      if (nsnull != view)
      {
        float p2t, t2p;

        // pass on to view somewhere else to deal with

        mContext->GetDevUnitsToAppUnits(p2t);
        mContext->GetAppUnitsToDevUnits(t2p);

        aEvent->point.x = NSIntPixelsToTwips(aEvent->point.x, p2t);
        aEvent->point.y = NSIntPixelsToTwips(aEvent->point.y, p2t);

        
        view->HandleEvent(aEvent, NS_VIEW_FLAG_CHECK_CHILDREN | 
                                  NS_VIEW_FLAG_CHECK_PARENT |
                                  NS_VIEW_FLAG_CHECK_SIBLINGS,
                          aStatus);

        aEvent->point.x = NSTwipsToIntPixels(aEvent->point.x, t2p);
        aEvent->point.y = NSTwipsToIntPixels(aEvent->point.y, t2p);
      }

      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: GrabMouseEvents(nsIView *aView, PRBool &aResult)
{
  mMouseGrabber = aView;
  aResult = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: GrabKeyEvents(nsIView *aView, PRBool &aResult)
{
  mKeyGrabber = aView;
  aResult = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: GetMouseEventGrabber(nsIView *&aView)
{
  aView = mMouseGrabber;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: GetKeyEventGrabber(nsIView *&aView)
{
  aView = mKeyGrabber;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: InsertChild(nsIView *parent, nsIView *child, nsIView *sibling,
                                           PRBool above)
{
  NS_PRECONDITION(nsnull != parent, "null ptr");
  NS_PRECONDITION(nsnull != child, "null ptr");

  if ((nsnull != parent) && (nsnull != child))
  {
    nsIView *kid;
    nsIView *prev = nsnull;

    //verify that the sibling exists...

    parent->GetChild(0, kid);

    while (nsnull != kid)
    {
      if (kid == sibling)
        break;

      //get the next sibling view

      prev = kid;
      kid->GetNextSibling(kid);
    }

    if (nsnull != kid)
    {
      //it's there, so do the insertion

      if (PR_TRUE == above)
        parent->InsertChild(child, prev);
      else
        parent->InsertChild(child, sibling);
    }

    UpdateTransCnt(nsnull, child);

    //and mark this area as dirty if the view is visible...

    nsViewVisibility  visibility;
    child->GetVisibility(visibility);

    if (nsViewVisibility_kHide != visibility)
      UpdateView(child, nsnull, NS_VMREFRESH_NO_SYNC);
  }
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: InsertChild(nsIView *parent, nsIView *child, PRInt32 zindex)
{
  NS_PRECONDITION(nsnull != parent, "null ptr");
  NS_PRECONDITION(nsnull != child, "null ptr");

  if ((nsnull != parent) && (nsnull != child))
  {
    nsIView *kid;
    nsIView *prev = nsnull;

    //find the right insertion point...

    parent->GetChild(0, kid);

    while (nsnull != kid)
    {
      PRInt32 idx;

      kid->GetZIndex(idx);

      if (zindex >= idx)
        break;

      //get the next sibling view

      prev = kid;
      kid->GetNextSibling(kid);
    }

    //in case this hasn't been set yet... maybe we should not do this? MMP

    child->SetZIndex(zindex);
    parent->InsertChild(child, prev);

    UpdateTransCnt(nsnull, child);

    //and mark this area as dirty if the view is visible...
    nsViewVisibility  visibility;
    child->GetVisibility(visibility);
    if (nsViewVisibility_kHide != visibility)
      UpdateView(child, nsnull, NS_VMREFRESH_NO_SYNC);
  }
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: RemoveChild(nsIView *parent, nsIView *child)
{
  NS_PRECONDITION(nsnull != parent, "null ptr");
  NS_PRECONDITION(nsnull != child, "null ptr");

  if ((nsnull != parent) && (nsnull != child))
  {
    UpdateTransCnt(child, nsnull);
    UpdateView(child, nsnull, NS_VMREFRESH_NO_SYNC);
    parent->RemoveChild(child);
  }
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: MoveViewBy(nsIView *aView, nscoord aX, nscoord aY)
{
  nscoord x, y;

  aView->GetPosition(&x, &y);
  MoveViewTo(aView, aX + x, aY + y);
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: MoveViewTo(nsIView *aView, nscoord aX, nscoord aY)
{
  nscoord oldX;
  nscoord oldY;

  aView->GetPosition(&oldX, &oldY);
  aView->SetPosition(aX, aY);

  // only do damage control if the view is visible

  if ((aX != oldX) || (aY != oldY))
  {
    nsViewVisibility  visibility;
    aView->GetVisibility(visibility);
    if (visibility != nsViewVisibility_kHide)
    {
      nsRect  bounds;
      aView->GetBounds(bounds);
    	nsRect oldArea(oldX, oldY, bounds.width, bounds.height);
    	nsIView* parent;
      aView->GetParent(parent);
  	  UpdateView(parent, oldArea, NS_VMREFRESH_NO_SYNC);
  	  nsRect newArea(aX, aY, bounds.width, bounds.height); 
  	  UpdateView(parent, newArea, NS_VMREFRESH_NO_SYNC);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: ResizeView(nsIView *aView, nscoord width, nscoord height)
{
  nscoord twidth, theight, left, top, right, bottom, x, y;

  aView->GetPosition(&x, &y);
  aView->GetDimensions(&twidth, &theight);

  if (width < twidth)
  {
    left = width;
    right = twidth;
  }
  else
  {
    left = twidth;
    right = width;
  }

  if (height < theight)
  {
    top = height;
    bottom = theight;
  }
  else
  {
    top = theight;
    bottom = height;
  }

  aView->SetDimensions(width, height);

	nsIView *parent;
  aView->GetParent(parent);

  if (nsnull == parent)
  {
    parent = aView;
    x = y = 0;
  }

  //now damage the right edge of the view,
  //and the bottom edge of the view,

  nsRect  trect;

  //right edge...

  trect.x = left;
  trect.y = y;
  trect.width = right - left;
  trect.height = bottom - y;

  UpdateView(parent, trect, NS_VMREFRESH_NO_SYNC);

  //bottom edge...

  trect.x = x;
  trect.y = top;
  trect.width = right - x;
  trect.height = bottom - top;

  UpdateView(parent, trect, NS_VMREFRESH_NO_SYNC);
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: SetViewClip(nsIView *aView, nsRect *aRect)
{
  NS_ASSERTION(!(nsnull == aView), "no view");
  NS_ASSERTION(!(nsnull == aRect), "no clip");

  aView->SetClip(aRect->x, aRect->y, aRect->XMost(), aRect->YMost());

  UpdateView(aView, *aRect, NS_VMREFRESH_NO_SYNC);

  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: SetViewVisibility(nsIView *aView, nsViewVisibility aVisible)
{
  aView->SetVisibility(aVisible);
  UpdateView(aView, nsnull, NS_VMREFRESH_NO_SYNC);
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: SetViewZIndex(nsIView *aView, PRInt32 aZIndex)
{
  nsresult  rv;

  NS_ASSERTION(!(nsnull == aView), "no view");

  PRInt32 oldidx;

  aView->GetZIndex(oldidx);

  if (oldidx != aZIndex)
  {
    nsIView *parent;

    aView->GetParent(parent);

    if (nsnull != parent)
    {
      //we don't just call the view manager's RemoveChild()
      //so that we can avoid two trips trough the UpdateView()
      //code (one for removal, one for insertion). MMP

      parent->RemoveChild(aView);
      UpdateTransCnt(aView, nsnull);
      rv = InsertChild(parent, aView, aZIndex);
    }
    else
      rv = NS_OK;
  }
  else
    rv = NS_OK;

  return rv;
}

NS_IMETHODIMP nsViewManager :: MoveViewAbove(nsIView *aView, nsIView *aOther)
{
  nsresult  rv;

  NS_ASSERTION(!(nsnull == aView), "no view");
  NS_ASSERTION(!(nsnull == aOther), "no view");

  nsIView *nextview;

  aView->GetNextSibling(nextview);
 
  if (nextview != aOther)
  {
    nsIView *parent;

    aView->GetParent(parent);

    if (nsnull != parent)
    {
      //we don't just call the view manager's RemoveChild()
      //so that we can avoid two trips trough the UpdateView()
      //code (one for removal, one for insertion). MMP

      parent->RemoveChild(aView);
      UpdateTransCnt(aView, nsnull);
      rv = InsertChild(parent, aView, aOther, PR_TRUE);
    }
    else
      rv = NS_OK;
  }
  else
    rv = NS_OK;

  return rv;
}

NS_IMETHODIMP nsViewManager :: MoveViewBelow(nsIView *aView, nsIView *aOther)
{
  nsresult  rv;

  NS_ASSERTION(!(nsnull == aView), "no view");
  NS_ASSERTION(!(nsnull == aOther), "no view");

  nsIView *nextview;

  aOther->GetNextSibling(nextview);
 
  if (nextview != aView)
  {
    nsIView *parent;

    aView->GetParent(parent);

    if (nsnull != parent)
    {
      //we don't just call the view manager's RemoveChild()
      //so that we can avoid two trips trough the UpdateView()
      //code (one for removal, one for insertion). MMP

      parent->RemoveChild(aView);
      UpdateTransCnt(aView, nsnull);
      rv = InsertChild(parent, aView, aOther, PR_FALSE);
    }
    else
      rv = NS_OK;
  }
  else
    rv = NS_OK;

  return rv;
}

NS_IMETHODIMP nsViewManager :: IsViewShown(nsIView *aView, PRBool &aResult)
{
  aResult = PR_TRUE;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsViewManager :: GetViewClipAbsolute(nsIView *aView, nsRect *rect, PRBool &aResult)
{
  aResult = PR_TRUE;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsViewManager :: SetViewContentTransparency(nsIView *aView, PRBool aTransparent)
{
  PRBool trans;

  aView->HasTransparency(trans);

  if (trans != aTransparent)
  {
    UpdateTransCnt(aView, nsnull);
    aView->SetContentTransparency(aTransparent);
    UpdateTransCnt(nsnull, aView);
    UpdateView(aView, nsnull, NS_VMREFRESH_NO_SYNC);
  }

  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: SetViewOpacity(nsIView *aView, float aOpacity)
{
  float opacity;

  aView->GetOpacity(opacity);

  if (opacity != aOpacity)
  {
    UpdateTransCnt(aView, nsnull);
    aView->SetOpacity(aOpacity);
    UpdateTransCnt(nsnull, aView);
    UpdateView(aView, nsnull, NS_VMREFRESH_NO_SYNC);
  }

  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: SetViewObserver(nsIViewObserver *aObserver)
{
  mObserver = aObserver;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: GetViewObserver(nsIViewObserver *&aObserver)
{
  if (nsnull != mObserver)
  {
    aObserver = mObserver;
    NS_ADDREF(mObserver);
    return NS_OK;
  }
  else
    return NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP nsViewManager :: GetDeviceContext(nsIDeviceContext *&aContext)
{
  NS_IF_ADDREF(mContext);
  aContext = mContext;
  return NS_OK;
}

nsDrawingSurface nsViewManager :: GetDrawingSurface(nsIRenderingContext &aContext, nsRect& aBounds)
{
  if ((nsnull == mDrawingSurface) ||
      (mDSBounds.width < aBounds.width) || (mDSBounds.height < aBounds.height))
  {
    if (nsnull != mDrawingSurface)
    {
      //destroy existing DS
      aContext.DestroyDrawingSurface(mDrawingSurface);
    }

    aContext.CreateDrawingSurface(&aBounds, 0, mDrawingSurface);
    mDSBounds = aBounds;
  }

  return mDrawingSurface;
}

NS_IMETHODIMP nsViewManager :: ShowQuality(PRBool aShow)
{
  nsIScrollableView *scroller;
  nsresult           retval;

  retval = mRootView->QueryInterface(kIScrollableViewIID, (void **)&scroller);
  if (NS_SUCCEEDED(retval)) {
    scroller->ShowQuality(aShow);
  }

  return retval;
}

NS_IMETHODIMP nsViewManager :: GetShowQuality(PRBool &aResult)
{
  nsIScrollableView *scroller;
  nsresult           retval;

  retval = mRootView->QueryInterface(kIScrollableViewIID, (void **)&scroller);
  if (NS_SUCCEEDED(retval)) {
    scroller->GetShowQuality(aResult);
  }

  return retval;
}

NS_IMETHODIMP nsViewManager :: SetQuality(nsContentQuality aQuality)
{
  nsIScrollableView *scroller;
  nsresult           retval;

  retval = mRootView->QueryInterface(kIScrollableViewIID, (void **)&scroller);
  if (NS_SUCCEEDED(retval))
  {
    scroller->SetQuality(aQuality);
  }

  return retval;
}

nsIRenderingContext * nsViewManager :: CreateRenderingContext(nsIView &aView)
{
  nsIView             *par = &aView;
  nsIWidget           *win;
  nsIRenderingContext *cx = nsnull;
  nscoord             x, y, ax = 0, ay = 0;

  do
  {
    par->GetWidget(win);

    if (nsnull != win)
      break;

    //get absolute coordinates of view, but don't
    //add in view pos since the first thing you ever
    //need to do when painting a view is to translate
    //the rendering context by the views pos and other parts
    //of the code do this for us...

    if (par != &aView)
    {
      par->GetPosition(&x, &y);

      ax += x;
      ay += y;
    }

    par->GetParent(par);
  }
  while (nsnull != par);

  if (nsnull != win)
  {
    mContext->CreateRenderingContext(&aView, cx);

    if (nsnull != cx)
      cx->Translate(ax, ay);

    NS_RELEASE(win);
  }

  return cx;
}

void nsViewManager :: AddRectToDirtyRegion(nsIView* aView, const nsRect &aRect) const
{
  // Get the dirty region associated with the view
  nsIRegion *dirtyRegion;

  aView->GetDirtyRegion(dirtyRegion);

  if (nsnull == dirtyRegion)
  {
    static NS_DEFINE_IID(kRegionCID, NS_REGION_CID);
    static NS_DEFINE_IID(kIRegionIID, NS_IREGION_IID);

    // The view doesn't have a dirty region so create one
    nsresult rv = nsRepository::CreateInstance(kRegionCID, 
                                       nsnull, 
                                       kIRegionIID, 
                                       (void **)&dirtyRegion);

    dirtyRegion->Init();
    aView->SetDirtyRegion(dirtyRegion);
  }

  // Dirty regions are in device units, and aRect is in app units so
  // we need to convert to device units
  nsRect  trect = aRect;
  float   t2p;

  mContext->GetAppUnitsToDevUnits(t2p);

  trect.ScaleRoundOut(t2p);
  dirtyRegion->Union(trect.x, trect.y, trect.width, trect.height);
  NS_IF_RELEASE(dirtyRegion);
}

void nsViewManager :: UpdateTransCnt(nsIView *oldview, nsIView *newview)
{
  if (nsnull != oldview)
  {
    PRBool  hasTransparency;
    float   opacity;

    oldview->HasTransparency(hasTransparency);
    oldview->GetOpacity(opacity);

    if (hasTransparency || (1.0f != opacity))
      mTransCnt--;
  }

  if (nsnull != newview)
  {
    PRBool  hasTransparency;
    float   opacity;

    newview->HasTransparency(hasTransparency);
    newview->GetOpacity(opacity);

    if (hasTransparency || (1.0f != opacity))
      mTransCnt++;
  }
}

NS_IMETHODIMP nsViewManager :: DisableRefresh(void)
{
  mRefreshEnabled = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: EnableRefresh(void)
{
  mRefreshEnabled = PR_TRUE;

  if (mFrameRate > 0)
  {
    PRInt32 deltams = PR_IntervalToMilliseconds(PR_IntervalNow() - mLastRefresh);

    if (deltams > (1000 / (PRInt32)mFrameRate))
      Composite();
  }

  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: Display(nsIView* aView)
{
  nsRect              wrect;
  nsIRenderingContext *localcx = nsnull;
  nsDrawingSurface    ds = nsnull;
  nsRect              trect;

  if (PR_FALSE == mRefreshEnabled)
    return NS_OK;

  NS_ASSERTION(!(PR_TRUE == mPainting), "recursive painting not permitted");

  mPainting = PR_TRUE;

  mContext->CreateRenderingContext(localcx);

  //couldn't get rendering context. this is ok if at startup
  if (nsnull == localcx)
  {
    return NS_ERROR_FAILURE;
  }

  aView->GetBounds(trect);
  nscoord x = trect.x, y = trect.y;

  // XXX Temporarily reset the position to (0, 0), that way when we paint
  // we won't end up translating incorrectly
  aView->SetPosition(0, 0);

  PRBool  result;

  trect.x = trect.y = 0;
  localcx->SetClipRect(trect, nsClipCombine_kReplace, result);

  // Paint the view. The clipping rect was set above set don't clip again.
  aView->Paint(*localcx, trect, NS_VIEW_FLAG_CLIP_SET, result);

  // XXX Reset the view's origin
  aView->SetPosition(x, y);

  NS_RELEASE(localcx);

  mPainting = PR_FALSE;

  return NS_OK;
}
