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

//#define USE_DIRTY_RECT
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
  vm->Composite();
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
  NS_IF_RELEASE(mDirtyRegion);

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
  mDrawingSurface = nsnull;
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

//printf("new dims: %4.2f %4.2f...\n", width / 20.0f, height / 20.0f);
  // Inform the presentation shell that we've been resized
  if (nsnull != mObserver)
    mObserver->ResizeReflow(mRootView, width, height);
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: GetWindowOffsets(nscoord *xoffset, nscoord *yoffset)
{
  if (nsnull != mRootView)
  {
    nsIScrollableView *scroller;

    if (NS_OK == mRootView->QueryInterface(kIScrollableViewIID, (void **)&scroller))
      scroller->GetVisibleOffset(xoffset, yoffset);
    else
      *xoffset = *yoffset = 0;
  }
  else
    *xoffset = *yoffset = 0;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: SetWindowOffsets(nscoord xoffset, nscoord yoffset)
{
  if (nsnull != mRootView)
  {
    nsIScrollableView *scroller;
    nsresult           retval;

    retval = mRootView->QueryInterface(kIScrollableViewIID, (void **)&scroller);
    if (NS_SUCCEEDED(retval)) {
      scroller->SetVisibleOffset(xoffset, yoffset);
    }

    return retval;
  }

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
  nscoord             xoff, yoff;
  float               scale;

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
    {
      return;
    }
  }
  else
    localcx = aContext;

  if (aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER)
  {
    mRootWindow->GetBounds(wrect);
    nsDrawingSurface  ds = GetDrawingSurface(*localcx, wrect);
    localcx->SelectOffScreenDrawingSurface(ds);
  }

  mContext->GetAppUnitsToDevUnits(scale);

  GetWindowOffsets(&xoff, &yoff);

  region->Offset(NSTwipsToIntPixels(-xoff, scale), NSTwipsToIntPixels(-yoff, scale));
//  localcx->SetClipRegion(*region, nsClipCombine_kIntersect);
  localcx->SetClipRegion(*region, nsClipCombine_kReplace);
  region->Offset(NSTwipsToIntPixels(xoff, scale), NSTwipsToIntPixels(yoff, scale));

  nsRect  trect;
  float   p2t;

  mContext->GetDevUnitsToAppUnits(p2t);

  region->GetBoundingBox(&trect.x, &trect.y, &trect.width, &trect.height);
  trect.ScaleRoundOut(p2t);

  PRBool  result;
  aView->Paint(*localcx, trect, 0, result);

  if (aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER)
    localcx->CopyOffScreenBits(wrect);

  if (localcx != aContext)
    NS_RELEASE(localcx);

  //is the dirty region the same as the region we just painted?

  if ((region == mDirtyRegion) || region->IsEqual(*mDirtyRegion))
    ClearDirtyRegion();
  else
    mDirtyRegion->Subtract(*region);

  mLastRefresh = PR_IntervalNow();

  mPainting = PR_FALSE;
}

void nsViewManager :: Refresh(nsIView *aView, nsIRenderingContext *aContext, nsRect *rect, PRUint32 aUpdateFlags)
{
  nsRect              wrect;
  nsIRenderingContext *localcx = nsnull;
  nscoord             xoff, yoff;

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
    {
      return;
    }
  }
  else
    localcx = aContext;

  if (aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER)
  {
    mRootWindow->GetBounds(wrect);
    nsDrawingSurface  ds = GetDrawingSurface(*localcx, wrect);
    localcx->SelectOffScreenDrawingSurface(ds);
  }

  if (aUpdateFlags & NS_VMREFRESH_SCREEN_RECT)
    localcx->SetClipRect(*rect, nsClipCombine_kReplace);

  GetWindowOffsets(&xoff, &yoff);

  nsRect trect = *rect;

  if (aUpdateFlags & NS_VMREFRESH_SCREEN_RECT)
    trect.MoveBy(xoff, yoff);
  else
  {
    trect.MoveBy(-xoff, -yoff);
    localcx->SetClipRect(trect, nsClipCombine_kReplace);
  }

  PRBool  result;
  aView->Paint(*localcx, trect, 0, result);

  if (aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER)
    localcx->CopyOffScreenBits(wrect);

  if (localcx != aContext)
    NS_RELEASE(localcx);

#ifdef USE_DIRTY_RECT

  nsRect  updaterect = *rect;

  //does our dirty rect intersect the rect we just painted?

  if (updaterect.IntersectRect(updaterect, mDirtyRect))
  {
    //does the update rect fully contain the dirty rect?
    //if so, then clear the dirty rect.

    if (updaterect.Contains(mDirtyRect))
      ClearDirtyRegion();
  }

#else

  //subtract the area we just painted from the dirty region

  if ((nsnull != mDirtyRegion) && !mDirtyRegion->IsEmpty())
  {
    nsRect  pixrect = trect;
    float   t2p;

    mContext->GetAppUnitsToDevUnits(t2p);

    pixrect.ScaleRoundIn(t2p);
    mDirtyRegion->Subtract(pixrect.x, pixrect.y, pixrect.width, pixrect.height);
  }

#endif

  mLastRefresh = PR_IntervalNow();

  mPainting = PR_FALSE;
}

NS_IMETHODIMP nsViewManager :: Composite()
{
#ifdef USE_DIRTY_RECT

  if ((nsnull != mRootView) && !mDirtyRect.IsEmpty())
    Refresh(mRootView, nsnull, &mDirtyRect, NS_VMREFRESH_DOUBLE_BUFFER);

#else

  if ((nsnull != mRootView) && (nsnull != mDirtyRegion) && !mDirtyRegion->IsEmpty())
    Refresh(mRootView, nsnull, mDirtyRegion, NS_VMREFRESH_DOUBLE_BUFFER);

#endif
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: UpdateView(nsIView *aView, nsIRegion *aRegion, PRUint32 aUpdateFlags)
{
  if (aRegion == nsnull)
  {
    nsRect  trect;

    aView->GetBounds(trect);
    trect.x = trect.y = 0;
    UpdateView(aView, trect, aUpdateFlags);
  }
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: UpdateView(nsIView *aView, const nsRect &aRect, PRUint32 aUpdateFlags)
{
  nsRect  trect = aRect;
  nsIView *par = aView;
  nscoord x, y;

  if ((aRect.width == 0) || (aRect.height == 0))
    return NS_OK;

  if (gsDebug)
  {
    printf("ViewManager::UpdateView: %x, rect ", aView);
    stdout << trect;
    printf("\n");
  }

  if (aUpdateFlags & NS_VMREFRESH_SCREEN_RECT)
  {
    nscoord xoff, yoff;

    GetWindowOffsets(&xoff, &yoff);

    trect.x += xoff;
    trect.y += yoff;
  }
  else if (nsnull != aView)
  {
    do
    {
      //get absolute coordinates of view

      par->GetPosition(&x, &y);

      trect.x += x;
      trect.y += y;

      par->GetParent(par);
    }
    while (nsnull != par);
  }

//printf("updating... ");
//stdout << trect;
//printf("\n");
#ifdef USE_DIRTY_RECT

  if (mDirtyRect.IsEmpty())
    mDirtyRect = trect;
  else
    mDirtyRect.UnionRect(mDirtyRect, trect);

#else

  AddRectToDirtyRegion(trect);

#endif

  if (nsnull != mContext)
  {
    if (aUpdateFlags & NS_VMREFRESH_IMMEDIATE)
    {
      if (aUpdateFlags & NS_VMREFRESH_AUTO_DOUBLE_BUFFER)
      {
        nsRect  vrect, rrect;
        nscoord varea;

        //see if the paint region is greater than .75 the area of our root view.
        //if so, enable double buffered painting.

        mRootView->GetBounds(vrect);

        varea = vrect.width * vrect.height;

        if (varea == 0)
          return NS_OK;

#ifdef USE_DIRTY_RECT
        rrect = mDirtyRect;
#else
        mDirtyRegion->GetBoundingBox(&rrect.x, &rrect.y, &rrect.width, &rrect.height);
        float   p2t;
        mContext->GetDevUnitsToAppUnits(p2t);
        rrect.ScaleRoundOut(p2t);
#endif
        rrect.IntersectRect(rrect, vrect);

        if ((((float)rrect.width * rrect.height) / (float)varea) >  0.25f)
          aUpdateFlags |= NS_VMREFRESH_DOUBLE_BUFFER;
        else
          aUpdateFlags &= ~NS_VMREFRESH_DOUBLE_BUFFER;

        //now clear the bit that got us here...

        aUpdateFlags &= ~NS_VMREFRESH_AUTO_DOUBLE_BUFFER;
      }

#ifdef USE_DIRTY_RECT
      Refresh(mRootView, nsnull, &mDirtyRect, aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER);
#else
      Refresh(mRootView, nsnull, mDirtyRegion, aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER);
#endif
    }
    else if ((mFrameRate > 0) && !(aUpdateFlags & NS_VMREFRESH_NO_SYNC))
    {
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
      if (nsnull != view) {

        nscoord width = ((nsSizeEvent*)aEvent)->windowSize->width;
        nscoord height = ((nsSizeEvent*)aEvent)->windowSize->height;

        // The root view may not be set if this is the resize associated with
        // window creation

        if (view == mRootView)
        {
          // Convert from pixels to twips
          float             p2t;
          nsIScrollableView *scrollView;

          mContext->GetDevUnitsToAppUnits(p2t);

          //XXX hey look, a hack! :) i'm not proud of it, but it does
          //work. the purpose is to prevent resizes of the view if the
          //clip size (assumed to be the size of this window) is the same
          //as the new size we get here. MMP

          if (NS_OK == mRootView->QueryInterface(kIScrollableViewIID, (void **)&scrollView))
          {
            nscoord sizex, sizey;
            float   t2p;

            mContext->GetAppUnitsToDevUnits(t2p);

            scrollView->GetClipSize(&sizex, &sizey);

            if ((width == NSTwipsToIntPixels(sizex, t2p)) &&
                (height == NSTwipsToIntPixels(sizey, t2p)))
            {
              break;
            }
          }

          SetWindowDimensions(NSIntPixelsToTwips(width, p2t),
                              NSIntPixelsToTwips(height, p2t));
          aStatus = nsEventStatus_eConsumeNoDefault;
        }
      }

      break;
    }

    case NS_PAINT:
    {
      nsIView*      view = nsView::GetViewFor(aEvent->widget);
      if (nsnull != view) {

        float             convert;
        nsRect            trect = *((nsPaintEvent*)aEvent)->rect;

        mContext->GetDevUnitsToAppUnits(convert);

        trect *= convert;

//printf("damage repair...\n");

        UpdateView(view, trect,
                   NS_VMREFRESH_SCREEN_RECT |
                   NS_VMREFRESH_IMMEDIATE |
                   NS_VMREFRESH_AUTO_DOUBLE_BUFFER);

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
        
      if (nsnull != mMouseGrabber && NS_IS_MOUSE_EVENT(aEvent)) {
        view = mMouseGrabber;
      }
      else if (nsnull != mKeyGrabber && NS_IS_KEY_EVENT(aEvent)) {
        view = mKeyGrabber;
      }
      else {
        view = nsView::GetViewFor(aEvent->widget);
      }

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
    PRInt32 numkids;
    parent->GetChildCount(numkids);
    nsIView *kid = nsnull, *prev = nsnull;

    //verify that the sibling exists...

    for (PRInt32 cnt = 0; cnt < numkids; cnt++)
    {
      // XXX This is extremely inefficient...
      parent->GetChild(cnt, kid);

      if (kid == sibling)
        break;

      prev = kid;
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
      UpdateView(child, nsnull, 0);
  }
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: InsertChild(nsIView *parent, nsIView *child, PRInt32 zindex)
{
  NS_PRECONDITION(nsnull != parent, "null ptr");
  NS_PRECONDITION(nsnull != child, "null ptr");

  if ((nsnull != parent) && (nsnull != child))
  {
    PRInt32 numkids;
    parent->GetChildCount(numkids);
    nsIView *kid = nsnull, *prev = nsnull;

    //find the right insertion point...

    for (PRInt32 cnt = 0; cnt < numkids; cnt++)
    {
      PRInt32 idx;
    
      // XXX This is extremely inefficient...
      parent->GetChild(cnt, kid);

      kid->GetZIndex(idx);

      if (zindex < idx)
        break;

      prev = kid;
    }

    //in case this hasn't been set yet... maybe we should not do this? MMP

    child->SetZIndex(zindex);
    parent->InsertChild(child, prev);

    UpdateTransCnt(nsnull, child);

    //and mark this area as dirty if the view is visible...
    nsViewVisibility  visibility;
    child->GetVisibility(visibility);
    if (nsViewVisibility_kHide != visibility)
      UpdateView(child, nsnull, 0);
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
  	  UpdateView(parent, oldArea, 0);
  	  nsRect newArea(aX, aY, bounds.width, bounds.height); 
  	  UpdateView(parent, newArea, 0);
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

  UpdateView(parent, trect, 0);

  //bottom edge...

  trect.x = x;
  trect.y = top;
  trect.width = right - x;
  trect.height = bottom - top;

  UpdateView(parent, trect, 0);
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: SetViewClip(nsIView *aView, nsRect *rect)
{
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: SetViewVisibility(nsIView *aView, nsViewVisibility visible)
{
  aView->SetVisibility(visible);
  UpdateView(aView, nsnull, 0);
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: SetViewZindex(nsIView *aView, PRInt32 zindex)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsViewManager :: MoveViewAbove(nsIView *aView, nsIView *other)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsViewManager :: MoveViewBelow(nsIView *aView, nsIView *other)
{
  return NS_ERROR_NOT_IMPLEMENTED;
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
  PRBool  hasTransparency;
  aView->HasTransparency(hasTransparency);
  if (aTransparent != hasTransparency)
  {
    if (aTransparent == PR_FALSE)
    {
      //going opaque
      mTransCnt--;
    }
    else
    {
      //going transparent
      mTransCnt++;
    }

    aView->SetContentTransparency(aTransparent);
  }
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: SetViewOpacity(nsIView *aView, float aOpacity)
{
  PRBool  newopaque, oldopaque;
  float   oldopacity;

  if ((aOpacity == 1.0f) || (aOpacity == 0.0f))
    newopaque = PR_TRUE;
  else
    newopaque = PR_FALSE;

  aView->GetOpacity(oldopacity);

  if ((oldopacity == 1.0f) || (oldopacity == 0.0f))
    oldopaque = PR_TRUE;
  else
    oldopaque = PR_FALSE;

  if (newopaque != oldopaque)
  {
    if (newopaque == PR_FALSE)
    {
      //going transparent
      mTransCnt++;
    }
    else
    {
      //going opaque
      mTransCnt--;
    }
  }

  aView->SetOpacity(aOpacity);
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

    mDrawingSurface = aContext.CreateDrawingSurface(&aBounds);
    mDSBounds = aBounds;
  }

  return mDrawingSurface;
}

NS_IMETHODIMP nsViewManager :: ClearDirtyRegion(void)
{
#ifdef USE_DIRTY_RECT

  mDirtyRect.x = 0;
  mDirtyRect.y = 0;
  mDirtyRect.width = 0;
  mDirtyRect.height = 0;

#else

  if (nsnull != mDirtyRegion)
    mDirtyRegion->SetTo(0, 0, 0, 0);

#endif
  return NS_OK;
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

void nsViewManager :: AddRectToDirtyRegion(nsRect &aRect)
{
  if (nsnull == mDirtyRegion)
  {
    static NS_DEFINE_IID(kRegionCID, NS_REGION_CID);
    static NS_DEFINE_IID(kIRegionIID, NS_IREGION_IID);

    nsresult rv = nsRepository::CreateInstance(kRegionCID, 
                                       nsnull, 
                                       kIRegionIID, 
                                       (void **)&mDirtyRegion);

    if (NS_OK != rv)
      return;
    else
      mDirtyRegion->Init();
  }

  nsRect  trect = aRect;
  float   t2p;

  mContext->GetAppUnitsToDevUnits(t2p);

  trect.ScaleRoundOut(t2p);
  mDirtyRegion->Union(trect.x, trect.y, trect.width, trect.height);
}

void nsViewManager :: UpdateTransCnt(nsIView *oldview, nsIView *newview)
{
  if (nsnull != oldview) {
    PRBool  hasTransparency;
    float   opacity;

    oldview->HasTransparency(hasTransparency);
    oldview->GetOpacity(opacity);

    if (hasTransparency || (1.0f != opacity))
      mTransCnt--;
  }

  if (nsnull != newview) {
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

